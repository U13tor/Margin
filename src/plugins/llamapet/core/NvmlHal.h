#pragma once

#include "core/HalProvider.h"
#include <memory>

namespace Margin::Plugins::LlamaPet {

class NvmlHal : public HalProvider {
public:
    explicit NvmlHal(quint32 gpuIndex = 0);
    ~NvmlHal() override;

    bool isAvailable() const;
    const char* providerName() const override { return "nvml"; }
    Result<GpuMetrics, QString> pollMetrics() override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    quint32 m_gpuIndex{0};
};

} // namespace Margin::Plugins::LlamaPet
