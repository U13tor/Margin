#pragma once

#include "core/TelemetryTypes.h"
#include "core/EngineConfig.h"
#include "Margin/Result.h"

#include <memory>
#include <QString>

namespace Margin::Plugins::LlamaPet {

class HalProvider {
public:
    virtual ~HalProvider() = default;
    virtual const char* providerName() const = 0;
    virtual Result<GpuMetrics, QString> pollMetrics() = 0;
};

std::unique_ptr<HalProvider> createHal(const EngineConfig::Hardware& cfg);

} // namespace Margin::Plugins::LlamaPet
