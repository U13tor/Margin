#include "NvmlHal.h"

#include <QLibrary>
#include <vector>

namespace Margin::Plugins::LlamaPet {

namespace {

using nvmlReturn_t = int;
constexpr nvmlReturn_t NVML_SUCCESS = 0;

using nvmlDevice_t = void*;

struct nvmlMemory_t {
    unsigned long long total;
    unsigned long long free;
    unsigned long long used;
};

enum nvmlTemperatureSensors_t {
    NVML_TEMPERATURE_GPU = 0
};

struct nvmlUtilization_t {
    unsigned int gpu;
    unsigned int memory;
};

using nvmlInit_t = nvmlReturn_t (*)();
using nvmlShutdown_t = nvmlReturn_t (*)();
using nvmlDeviceGetHandleByIndex_t = nvmlReturn_t (*)(unsigned int, nvmlDevice_t*);
using nvmlDeviceGetName_t = nvmlReturn_t (*)(nvmlDevice_t, char*, unsigned int);
using nvmlDeviceGetMemoryInfo_t = nvmlReturn_t (*)(nvmlDevice_t, nvmlMemory_t*);
using nvmlDeviceGetTemperature_t = nvmlReturn_t (*)(nvmlDevice_t, nvmlTemperatureSensors_t, unsigned int*);
using nvmlDeviceGetPowerUsage_t = nvmlReturn_t (*)(nvmlDevice_t, unsigned int*);
using nvmlDeviceGetEnforcedPowerLimit_t = nvmlReturn_t (*)(nvmlDevice_t, unsigned int*);
using nvmlDeviceGetPowerManagementLimit_t = nvmlReturn_t (*)(nvmlDevice_t, unsigned int*);
using nvmlDeviceGetUtilizationRates_t = nvmlReturn_t (*)(nvmlDevice_t, nvmlUtilization_t*);

} // namespace

struct NvmlHal::Impl {
    QLibrary lib;
    bool initialized{false};
    nvmlDevice_t device{nullptr};

    nvmlInit_t initFn{nullptr};
    nvmlShutdown_t shutdownFn{nullptr};
    nvmlDeviceGetHandleByIndex_t getHandleFn{nullptr};
    nvmlDeviceGetName_t getNameFn{nullptr};
    nvmlDeviceGetMemoryInfo_t getMemoryInfoFn{nullptr};
    nvmlDeviceGetTemperature_t getTemperatureFn{nullptr};
    nvmlDeviceGetPowerUsage_t getPowerUsageFn{nullptr};
    nvmlDeviceGetEnforcedPowerLimit_t getEnforcedPowerLimitFn{nullptr};
    nvmlDeviceGetPowerManagementLimit_t getPowerManagementLimitFn{nullptr};
    nvmlDeviceGetUtilizationRates_t getUtilizationRatesFn{nullptr};

    Impl() : lib(QStringLiteral("nvml")) {}

    ~Impl() {
        if (initialized && shutdownFn) {
            shutdownFn();
        }
        if (lib.isLoaded()) {
            lib.unload();
        }
    }

    bool resolveSymbols() {
        if (!lib.load()) {
            return false;
        }

        initFn = reinterpret_cast<nvmlInit_t>(lib.resolve("nvmlInit_v2"));
        if (!initFn) {
            initFn = reinterpret_cast<nvmlInit_t>(lib.resolve("nvmlInit"));
        }

        shutdownFn = reinterpret_cast<nvmlShutdown_t>(lib.resolve("nvmlShutdown"));

        getHandleFn = reinterpret_cast<nvmlDeviceGetHandleByIndex_t>(lib.resolve("nvmlDeviceGetHandleByIndex_v2"));
        if (!getHandleFn) {
            getHandleFn = reinterpret_cast<nvmlDeviceGetHandleByIndex_t>(lib.resolve("nvmlDeviceGetHandleByIndex"));
        }

        getNameFn = reinterpret_cast<nvmlDeviceGetName_t>(lib.resolve("nvmlDeviceGetName"));
        getMemoryInfoFn = reinterpret_cast<nvmlDeviceGetMemoryInfo_t>(lib.resolve("nvmlDeviceGetMemoryInfo"));
        getTemperatureFn = reinterpret_cast<nvmlDeviceGetTemperature_t>(lib.resolve("nvmlDeviceGetTemperature"));
        getPowerUsageFn = reinterpret_cast<nvmlDeviceGetPowerUsage_t>(lib.resolve("nvmlDeviceGetPowerUsage"));
        getEnforcedPowerLimitFn = reinterpret_cast<nvmlDeviceGetEnforcedPowerLimit_t>(lib.resolve("nvmlDeviceGetEnforcedPowerLimit"));
        getPowerManagementLimitFn = reinterpret_cast<nvmlDeviceGetPowerManagementLimit_t>(lib.resolve("nvmlDeviceGetPowerManagementLimit"));
        getUtilizationRatesFn = reinterpret_cast<nvmlDeviceGetUtilizationRates_t>(lib.resolve("nvmlDeviceGetUtilizationRates"));

        return (initFn && shutdownFn && getHandleFn && getNameFn && getMemoryInfoFn &&
                getTemperatureFn && getPowerUsageFn && getUtilizationRatesFn);
    }
};

NvmlHal::NvmlHal(quint32 gpuIndex)
    : m_gpuIndex(gpuIndex), m_impl(std::make_unique<Impl>()) {
    if (m_impl->resolveSymbols()) {
        if (m_impl->initFn() == NVML_SUCCESS) {
            m_impl->initialized = true;
            nvmlDevice_t dev = nullptr;
            if (m_impl->getHandleFn(m_gpuIndex, &dev) == NVML_SUCCESS) {
                m_impl->device = dev;
            }
        }
    }
}

NvmlHal::~NvmlHal() = default;

bool NvmlHal::isAvailable() const {
    return m_impl && m_impl->initialized && (m_impl->device != nullptr);
}

Result<GpuMetrics, QString> NvmlHal::pollMetrics() {
    if (!isAvailable()) {
        return Result<GpuMetrics, QString>::err(QStringLiteral("NVML not available or GPU index invalid"));
    }

    GpuMetrics metrics;

    // Device Name
    char nameBuf[96] = {0};
    if (m_impl->getNameFn(m_impl->device, nameBuf, sizeof(nameBuf)) == NVML_SUCCESS) {
        metrics.deviceName = QString::fromUtf8(nameBuf);
    } else {
        metrics.deviceName = QStringLiteral("NVIDIA GPU");
    }

    // Memory Info
    nvmlMemory_t mem{};
    if (m_impl->getMemoryInfoFn(m_impl->device, &mem) == NVML_SUCCESS) {
        metrics.vramUsedMb = static_cast<float>(mem.used) / (1024.0f * 1024.0f);
        metrics.vramTotalMb = static_cast<float>(mem.total) / (1024.0f * 1024.0f);
    } else {
        return Result<GpuMetrics, QString>::err(QStringLiteral("Failed to read NVML memory info"));
    }

    // Temperature
    unsigned int tempC = 0;
    if (m_impl->getTemperatureFn(m_impl->device, NVML_TEMPERATURE_GPU, &tempC) == NVML_SUCCESS) {
        metrics.tempC = static_cast<quint32>(tempC);
    } else {
        metrics.tempC = 0;
    }

    // Power Usage
    unsigned int powerMilliwatts = 0;
    if (m_impl->getPowerUsageFn(m_impl->device, &powerMilliwatts) == NVML_SUCCESS) {
        metrics.powerW = static_cast<float>(powerMilliwatts) / 1000.0f;
    } else {
        metrics.powerW = 0.0f;
    }

    // Power Limit
    unsigned int limitMilliwatts = 0;
    bool limitOk = false;
    if (m_impl->getEnforcedPowerLimitFn &&
        m_impl->getEnforcedPowerLimitFn(m_impl->device, &limitMilliwatts) == NVML_SUCCESS) {
        limitOk = true;
    } else if (m_impl->getPowerManagementLimitFn &&
               m_impl->getPowerManagementLimitFn(m_impl->device, &limitMilliwatts) == NVML_SUCCESS) {
        limitOk = true;
    }
    if (limitOk) {
        metrics.powerLimitW = static_cast<float>(limitMilliwatts) / 1000.0f;
    } else {
        metrics.powerLimitW = 0.0f;
    }

    // GPU Utilization
    nvmlUtilization_t util{};
    if (m_impl->getUtilizationRatesFn(m_impl->device, &util) == NVML_SUCCESS) {
        metrics.gpuUtil = static_cast<quint32>(util.gpu);
    } else {
        metrics.gpuUtil = 0;
    }

    return Result<GpuMetrics, QString>::ok(metrics);
}

} // namespace Margin::Plugins::LlamaPet
