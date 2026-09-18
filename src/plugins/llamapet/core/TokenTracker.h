#pragma once

#include "core/TelemetryTypes.h"
#include <QObject>
#include <optional>
#include <unordered_map>

namespace Margin::Plugins::LlamaPet {

struct TokenDelta {
    quint64 completionTokens{0};
    quint64 promptTokens{0};
    quint32 activeSeconds{0};
    bool isFromMetrics{false};
};

class TokenTracker {
public:
    TokenTracker();
    ~TokenTracker();

    /// Process a telemetry snapshot, calculating the token deltas since last poll.
    TokenDelta process(const LlmTelemetry& inf);

    /// Reset internal state (e.g. on disconnect) so baseline is re-established.
    void reset();

    bool isUsingMetrics() const { return m_usingMetrics; }

private:
    bool m_usingMetrics{false};

    // Channel A: Prometheus monotonic counters
    std::optional<quint64> m_lastPredictedTotal;
    std::optional<quint64> m_lastPromptTotal;

    // Channel B: Slot watermarks fallback
    struct SlotWatermark {
        quint64 lastDecoded{0};
        quint64 lastPrompt{0};
        bool wasActive{false};
    };
    std::unordered_map<quint32, SlotWatermark> m_slotWatermarks;
    bool m_slotsInitialized{false};
};

} // namespace Margin::Plugins::LlamaPet

