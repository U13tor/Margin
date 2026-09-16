#include "HalProvider.h"
#include "NvmlHal.h"
#include "SmiHal.h"

namespace Margin::Plugins::LlamaPet {

std::unique_ptr<HalProvider> createHal(const EngineConfig::Hardware& cfg) {
    if (cfg.backend == QStringLiteral("nvml")) {
        auto p = std::make_unique<NvmlHal>(cfg.gpuIndex);
        if (p->isAvailable()) {
            return p;
        }
        return nullptr;
    }

    if (cfg.backend == QStringLiteral("nvidia-smi")) {
        auto p = std::make_unique<SmiHal>(cfg.gpuIndex);
        if (p->isAvailable()) {
            return p;
        }
        return nullptr;
    }

    if (cfg.backend == QStringLiteral("auto")) {
        auto nvml = std::make_unique<NvmlHal>(cfg.gpuIndex);
        if (nvml->isAvailable()) {
            return nvml;
        }
        auto smi = std::make_unique<SmiHal>(cfg.gpuIndex);
        if (smi->isAvailable()) {
            return smi;
        }
        return nullptr;
    }

    return nullptr;
}

} // namespace Margin::Plugins::LlamaPet
