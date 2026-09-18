#include "TokenTracker.h"

namespace Margin::Plugins::LlamaPet {

TokenTracker::TokenTracker() = default;
TokenTracker::~TokenTracker() = default;

void TokenTracker::reset() {
    m_usingMetrics = false;
    m_lastPredictedTotal.reset();
    m_lastPromptTotal.reset();
    m_slotWatermarks.clear();
    m_slotsInitialized = false;
}

TokenDelta TokenTracker::process(const LlmTelemetry& inf) {
    if (!inf.connected) {
        reset();
        return TokenDelta{};
    }

    // ── Channel A: Prometheus /metrics (Primary authoritative source) ──
    if (inf.metricsSupported && inf.predictedTokensTotal.has_value()) {
        m_usingMetrics = true;
        quint64 curPred = *inf.predictedTokensTotal;
        quint64 curPrompt = inf.promptTokensTotal.value_or(0);

        if (!m_lastPredictedTotal.has_value()) {
            // First tick: establish baseline without attributing historical tokens
            m_lastPredictedTotal = curPred;
            m_lastPromptTotal = curPrompt;
            return TokenDelta{0, 0, 0, true};
        }

        quint64 deltaCompletion = 0;
        quint64 deltaPrompt = 0;

        if (curPred >= *m_lastPredictedTotal) {
            deltaCompletion = curPred - *m_lastPredictedTotal;
        } else {
            // llama-server restarted: curPred is the new total
            deltaCompletion = curPred;
        }

        if (curPrompt >= m_lastPromptTotal.value_or(0)) {
            deltaPrompt = curPrompt - m_lastPromptTotal.value_or(0);
        } else {
            deltaPrompt = curPrompt;
        }

        m_lastPredictedTotal = curPred;
        m_lastPromptTotal = curPrompt;

        quint32 activeSec = (deltaCompletion > 0 || inf.anyPrefill || inf.anyDecoding) ? 1 : 0;
        return TokenDelta{deltaCompletion, deltaPrompt, activeSec, true};
    }

    // ── Channel B: Slots state-machine tracking (Fallback source) ──
    m_usingMetrics = false;
    if (!inf.slots.has_value() || inf.slotsDisabled) {
        return TokenDelta{};
    }

    const auto& slots = *inf.slots;
    if (!m_slotsInitialized) {
        for (const auto& s : slots) {
            m_slotWatermarks[s.id] = SlotWatermark{s.decodedTokens, s.promptTokens, s.isActive};
        }
        m_slotsInitialized = true;
        return TokenDelta{0, 0, 0, false};
    }

    quint64 deltaCompletion = 0;
    quint64 deltaPrompt = 0;

    for (const auto& s : slots) {
        auto it = m_slotWatermarks.find(s.id);
        if (it == m_slotWatermarks.end()) {
            m_slotWatermarks[s.id] = SlotWatermark{s.decodedTokens, s.promptTokens, s.isActive};
            continue;
        }

        auto& wm = it->second;
        if (s.decodedTokens >= wm.lastDecoded) {
            deltaCompletion += (s.decodedTokens - wm.lastDecoded);
        } else {
            // Slot was reset for a new generation task
            deltaCompletion += s.decodedTokens;
        }

        if (s.promptTokens > wm.lastPrompt) {
            deltaPrompt += (s.promptTokens - wm.lastPrompt);
        }

        wm.lastDecoded = s.decodedTokens;
        wm.lastPrompt = s.promptTokens;
        wm.wasActive = s.isActive;
    }

    quint32 activeSec = (deltaCompletion > 0 || inf.anyPrefill || inf.anyDecoding) ? 1 : 0;
    return TokenDelta{deltaCompletion, deltaPrompt, activeSec, false};
}

} // namespace Margin::Plugins::LlamaPet

