#pragma once

#include "core/TelemetryTypes.h"

namespace Margin::Plugins::LlamaPet {

class ItProvider {
public:
    virtual ~ItProvider() = default;
    virtual const char* engineName() const = 0;
    virtual LlmTelemetry pollTelemetry() = 0;
};

} // namespace Margin::Plugins::LlamaPet
