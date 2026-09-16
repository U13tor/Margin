#pragma once

#include "core/TelemetryTypes.h"
#include <QString>
#include <optional>

namespace Margin::Plugins::LlamaPet {

struct EmotionInputs {
    float vramPercent{0.0f};
    float vramDanger{96.0f};
    float vramWarn{90.0f};
    bool connected{false};
    bool anyPrefill{false};
    bool anyDecoding{false};
    std::optional<float> cacheHitRate;
    quint32 idleTicks{0};
};

class EmotionEngine {
public:
    EmotionEngine();

    Emotion tick(const EmotionInputs& input);
    Emotion current() const { return m_current; }

private:
    Emotion m_current{Emotion::Idle};
    quint32 m_oomRecoveryTicks{0};
    quint32 m_offlineFailTicks{0};
    quint32 m_happyCacheTicks{0};
};

QString dialogFor(Emotion emotion, std::optional<float> tps = std::nullopt);

} // namespace Margin::Plugins::LlamaPet
