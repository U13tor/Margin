#pragma once

#include "core/HalProvider.h"

namespace Margin::Plugins::LlamaPet {

// 纯函数解析 CSV（单测与 SmiHal 共用）
Result<GpuMetrics, QString> parseSmiCsv(const QString& input);

class SmiHal : public HalProvider {
public:
    explicit SmiHal(quint32 gpuIndex = 0);

    bool isAvailable() const;
    const char* providerName() const override { return "nvidia-smi"; }
    Result<GpuMetrics, QString> pollMetrics() override;

private:
    quint32 m_gpuIndex{0};
};

} // namespace Margin::Plugins::LlamaPet
