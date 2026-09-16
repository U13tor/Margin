#pragma once

#include "core/TelemetryTypes.h"
#include "core/EngineConfig.h"
#include "core/EmotionEngine.h"
#include "core/HalProvider.h"
#include "core/ItProvider.h"

#include <QDateTime>
#include <QJsonObject>
#include <QObject>
#include <QTimer>
#include <memory>

namespace Margin::Plugins::LlamaPet {

class TelemetryService : public QObject {
    Q_OBJECT

    // Q_PROPERTY 暴露给 QML 与 UI 层
    Q_PROPERTY(float vramPercent READ vramPercent NOTIFY telemetryChanged)
    Q_PROPERTY(float vramUsedMb READ vramUsedMb NOTIFY telemetryChanged)
    Q_PROPERTY(float vramTotalMb READ vramTotalMb NOTIFY telemetryChanged)
    Q_PROPERTY(quint32 tempC READ tempC NOTIFY telemetryChanged)
    Q_PROPERTY(float powerW READ powerW NOTIFY telemetryChanged)
    Q_PROPERTY(float powerLimitW READ powerLimitW NOTIFY telemetryChanged)
    Q_PROPERTY(quint32 gpuUtil READ gpuUtil NOTIFY telemetryChanged)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY telemetryChanged)

    Q_PROPERTY(bool connected READ connected NOTIFY telemetryChanged)
    Q_PROPERTY(int activeSlots READ activeSlots NOTIFY telemetryChanged)
    Q_PROPERTY(int totalSlots READ totalSlots NOTIFY telemetryChanged)
    Q_PROPERTY(float currentTps READ currentTps NOTIFY telemetryChanged)
    Q_PROPERTY(float cacheHitRatePct READ cacheHitRatePct NOTIFY telemetryChanged)
    Q_PROPERTY(bool speculativeActive READ speculativeActive NOTIFY telemetryChanged)
    Q_PROPERTY(bool slotsDisabled READ slotsDisabled NOTIFY telemetryChanged)
    Q_PROPERTY(QVariantList slotsList READ slotsList NOTIFY telemetryChanged)
    Q_PROPERTY(QString engine READ engine NOTIFY telemetryChanged)

    Q_PROPERTY(int emotion READ emotion NOTIFY telemetryChanged)
    Q_PROPERTY(int status READ status NOTIFY telemetryChanged)
    Q_PROPERTY(quint32 healthScore READ healthScore NOTIFY telemetryChanged)
    Q_PROPERTY(QString dialogText READ dialogText NOTIFY telemetryChanged)
    Q_PROPERTY(quint32 idleTicks READ idleTicks NOTIFY telemetryChanged)

public:
    explicit TelemetryService(QObject* parent = nullptr);
    ~TelemetryService() override;

    void setHalProvider(std::unique_ptr<HalProvider> hal);
    void setItProvider(std::unique_ptr<ItProvider> it);
    void setConfig(const EngineConfig& cfg);
    const EngineConfig& config() const { return m_config; }

    bool isRunning() const;
    const TelemetryPayload& currentPayload() const { return m_payload; }
    QJsonObject currentPayloadJson() const;

    // 属性读取接口
    float vramPercent() const;
    float vramUsedMb() const { return m_payload.hw.vramUsedMb; }
    float vramTotalMb() const { return m_payload.hw.vramTotalMb; }
    quint32 tempC() const { return m_payload.hw.tempC; }
    float powerW() const { return m_payload.hw.powerW; }
    float powerLimitW() const { return m_payload.hw.powerLimitW; }
    quint32 gpuUtil() const { return m_payload.hw.gpuUtil; }
    QString deviceName() const { return m_payload.hw.deviceName; }

    bool connected() const { return m_payload.inf.connected; }
    int activeSlots() const { return m_payload.inf.activeSlots.value_or(0); }
    int totalSlots() const { return m_payload.inf.totalSlots.value_or(0); }
    float currentTps() const { return m_payload.inf.currentTps.value_or(0.0f); }
    float cacheHitRatePct() const { return m_payload.inf.cacheHitRatePct.value_or(0.0f); }
    bool speculativeActive() const { return m_payload.inf.speculativeActive.value_or(false); }
    bool slotsDisabled() const { return m_payload.inf.slotsDisabled; }
    QString engine() const { return QString::fromUtf8(m_payload.inf.engine); }
    QVariantList slotsList() const;
    void eraseIdleSlots(std::function<void(Result<int, QString>)> callback);

    int emotion() const { return static_cast<int>(m_payload.emotion); }
    int status() const { return static_cast<int>(m_payload.status); }
    quint32 healthScore() const { return m_payload.healthScore; }
    QString dialogText() const { return m_payload.dialogText; }
    quint32 idleTicks() const { return m_idleTicks; }

public Q_SLOTS:
    void start(int intervalMs = 1000);
    void stop();
    void tick();

Q_SIGNALS:
    void telemetryChanged();
    void telemetryPayload(const QJsonObject& payload);

private:
    EngineConfig m_config;
    std::unique_ptr<HalProvider> m_hal;
    std::unique_ptr<ItProvider> m_it;

    EmotionEngine m_emotionEngine;
    TelemetryPayload m_payload;
    quint32 m_idleTicks{0};
    mutable QVariantList m_lastSlotsList;

    QTimer m_timer;
};

} // namespace Margin::Plugins::LlamaPet
