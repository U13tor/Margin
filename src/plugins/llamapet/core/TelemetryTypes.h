#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QtGlobal>
#include <optional>

#if defined(slots)
#undef slots
#endif

namespace Margin::Plugins::LlamaPet {

enum class StatusLevel { Healthy, Warning, Danger };
enum class Emotion { Idle, Sleeping, EatingPrefill, SpittingTokens,
                     HappyCache, PanickingOom, ConfusedOffline };

struct GpuMetrics {                 // 原生单位 MB/W/°C/%，与 UI 换算分离
    QString deviceName;
    float vramUsedMb{0.0f};
    float vramTotalMb{0.0f};
    quint32 tempC{0};
    float powerW{0.0f};
    float powerLimitW{0.0f};
    quint32 gpuUtil{0};
};

struct SlotInfo {
    quint32 id{0};
    bool isActive{false};
    quint64 promptTokens{0};
    quint64 cachedTokens{0};
    quint64 decodedTokens{0};
};

struct LlmTelemetry {               // 给不出的一律 std::nullopt / null
    QByteArray engine;
    bool connected{false};
    std::optional<quint32> activeSlots;
    std::optional<quint32> totalSlots;
    std::optional<float> currentTps;
    std::optional<float> cacheHitRatePct;
    std::optional<bool> speculativeActive;
    std::optional<QList<SlotInfo>> slots;
    bool anyPrefill{false};
    bool anyDecoding{false};
    bool slotsDisabled{false};
};

struct TelemetryPayload {           // EventBus JSON 用；Q_PROPERTY 暴露用同字段
    qint64 timestamp{0};
    quint32 healthScore{0};
    StatusLevel status{StatusLevel::Healthy};
    GpuMetrics hw;
    LlmTelemetry inf;
    Emotion emotion{Emotion::Idle};
    QString dialogText;
};

} // namespace Margin::Plugins::LlamaPet
