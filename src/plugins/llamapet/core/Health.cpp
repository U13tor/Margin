#include "Health.h"
#include <algorithm>
#include <cmath>

namespace Margin::Plugins::LlamaPet::health {

namespace {

inline float clamp01(float x) {
    return std::max(0.0f, std::min(1.0f, x));
}

inline float normalizeRatio(float val) {
    if (val > 1.0f) {
        return val / 100.0f;
    }
    return std::max(0.0f, val);
}

} // namespace

quint32 score(float vramPercent, quint32 tempC, float powerW, float powerLimitW, bool connected) {
    if (!connected) {
        return 0;
    }

    float vramRatio = normalizeRatio(vramPercent);
    float v = clamp01((1.0f - vramRatio) / 0.4f);
    float t = clamp01((85.0f - static_cast<float>(tempC)) / 35.0f);
    float p = (powerLimitW > 0.0f) ? clamp01((powerLimitW - powerW) / (0.4f * powerLimitW)) : 1.0f;
    float s = 1.0f;

    float computed = std::round(100.0f * (0.4f * v + 0.2f * t + 0.2f * p + 0.2f * s));
    quint32 res = static_cast<quint32>(computed);
    return std::min(res, 100u);
}

quint32 score(const GpuMetrics& m, bool connected) {
    float vramPercent = (m.vramTotalMb > 0.0f) ? (m.vramUsedMb / m.vramTotalMb * 100.0f) : 0.0f;
    return score(vramPercent, m.tempC, m.powerW, m.powerLimitW, connected);
}

StatusLevel statusLevel(quint32 scoreVal, float vramPercent, quint32 tempC, float dangerVal, float warnVal, bool connected) {
    float vramRatio = normalizeRatio(vramPercent);
    float danger = normalizeRatio(dangerVal);
    float warn = normalizeRatio(warnVal);

    if (!connected) {
        if (vramRatio >= danger) {
            return StatusLevel::Danger;
        } else {
            return StatusLevel::Warning;
        }
    }

    if (vramRatio >= danger || scoreVal < 50) {
        return StatusLevel::Danger;
    } else if (vramRatio >= warn || tempC > 75 || scoreVal < 80) {
        return StatusLevel::Warning;
    } else {
        return StatusLevel::Healthy;
    }
}

StatusLevel statusLevel(quint32 scoreVal, const GpuMetrics& m, float danger, float warn, bool connected) {
    float vramPercent = (m.vramTotalMb > 0.0f) ? (m.vramUsedMb / m.vramTotalMb * 100.0f) : 0.0f;
    return statusLevel(scoreVal, vramPercent, m.tempC, danger, warn, connected);
}

StatusLevel statusLevel(quint32 scoreVal, const GpuMetrics& m, float danger, float warn) {
    return statusLevel(scoreVal, m, danger, warn, scoreVal > 0);
}

} // namespace Margin::Plugins::LlamaPet::health
