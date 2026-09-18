#include "TelemetryService.h"
#include "core/Health.h"
#include "core/LlamaCppIt.h"
#include "core/TokenStore.h"
#include "Margin/Database.h"
#include <QVariantMap>

namespace Margin::Plugins::LlamaPet {

TelemetryService::TelemetryService(QObject* parent) : QObject(parent) {
    connect(&m_timer, &QTimer::timeout, this, &TelemetryService::tick);
}

TelemetryService::~TelemetryService() {
    stop();
    if (m_tokenStore && m_db) {
        m_tokenStore->flush(*m_db);
    }
}

void TelemetryService::setTokenStore(TokenStore* store, Margin::Database* db) {
    m_tokenStore = store;
    m_db = db;
}

void TelemetryService::setHalProvider(std::unique_ptr<HalProvider> hal) {
    m_hal = std::move(hal);
}

void TelemetryService::setItProvider(std::unique_ptr<ItProvider> it) {
    m_it = std::move(it);
}

void TelemetryService::setConfig(const EngineConfig& cfg) {
    m_config = cfg;
}

bool TelemetryService::isRunning() const {
    return m_timer.isActive();
}

void TelemetryService::start(int intervalMs) {
    if (intervalMs <= 0) {
        intervalMs = m_config.inference.pollIntervalMs;
    }
    m_timer.start(intervalMs);
    tick(); // 启动时立即触发一次初始采样
}

void TelemetryService::stop() {
    m_timer.stop();
}

float TelemetryService::vramPercent() const {
    if (m_payload.hw.vramTotalMb > 0.0f) {
        return (m_payload.hw.vramUsedMb / m_payload.hw.vramTotalMb) * 100.0f;
    }
    return 0.0f;
}

void TelemetryService::tick() {
    // 1. 采集硬件指标
    GpuMetrics hw;
    if (m_hal) {
        auto res = m_hal->pollMetrics();
        if (res.isOk()) {
            hw = res.value();
        } else {
            // 失败时保留基本信息或上次快照
            hw.deviceName = QStringLiteral("NVIDIA GPU (Offline)");
        }
    }

    // 2. 采集推理指标
    LlmTelemetry inf;
    if (m_it) {
        inf = m_it->pollTelemetry();
    } else {
        inf.connected = false;
    }

    // 3. 维护 idleTicks
    bool hasInferenceActivity = (inf.anyPrefill || inf.anyDecoding);
    if (hasInferenceActivity) {
        m_idleTicks = 0;
    } else {
        m_idleTicks++;
    }

    // 3.1 Token 增量计算与累加持久化 (双通道高精度)
    TokenDelta delta = m_tokenTracker.process(inf);
    if (m_tokenStore && (delta.completionTokens > 0 || delta.promptTokens > 0 || delta.activeSeconds > 0)) {
        m_tokenStore->recordTokens(delta.promptTokens, delta.completionTokens, delta.activeSeconds);
    }
    m_flushTickCount++;
    if (m_flushTickCount >= 5) {
        m_flushTickCount = 0;
        if (m_tokenStore && m_db) {
            m_tokenStore->flush(*m_db);
        }
    }

    // 4. 显存百分比换算与组装 EmotionInputs
    float vramPct = 0.0f;
    if (hw.vramTotalMb > 0.0f) {
        vramPct = (hw.vramUsedMb / hw.vramTotalMb) * 100.0f;
    }

    float vramDanger = m_config.hardware.vramDangerThreshold;
    if (vramDanger <= 1.0f) vramDanger *= 100.0f;
    float vramWarn = m_config.hardware.vramWarnThreshold;
    if (vramWarn <= 1.0f) vramWarn *= 100.0f;

    EmotionInputs in;
    in.vramPercent = vramPct;
    in.vramDanger = vramDanger;
    in.vramWarn = vramWarn;
    in.connected = inf.connected;
    in.anyPrefill = inf.anyPrefill;
    in.anyDecoding = inf.anyDecoding;
    in.cacheHitRate = inf.cacheHitRatePct;
    in.idleTicks = m_idleTicks;

    // 5. 状态机仲裁
    Emotion emotion = m_emotionEngine.tick(in);

    // 6. 计算健康评分与状态档位
    quint32 score = health::score(hw, inf.connected);
    StatusLevel status = health::statusLevel(
        score, hw, m_config.hardware.vramDangerThreshold, m_config.hardware.vramWarnThreshold, inf.connected);

    // 7. 生成对话气泡文案
    QString dialog = dialogFor(emotion, inf.currentTps);

    // 8. 构造最新快照
    m_payload.timestamp = QDateTime::currentMSecsSinceEpoch();
    m_payload.healthScore = score;
    m_payload.status = status;
    m_payload.hw = hw;
    m_payload.inf = inf;
    m_payload.emotion = emotion;
    m_payload.dialogText = dialog;

    // 9. 发射变更信号与 JSON Payload 信号
    Q_EMIT telemetryChanged();
    Q_EMIT telemetryPayload(currentPayloadJson());
}

QJsonObject TelemetryService::currentPayloadJson() const {
    QJsonObject obj;
    obj[QStringLiteral("timestamp")] = m_payload.timestamp;
    obj[QStringLiteral("health_score")] = static_cast<int>(m_payload.healthScore);
    obj[QStringLiteral("status")] = static_cast<int>(m_payload.status);
    obj[QStringLiteral("emotion")] = static_cast<int>(m_payload.emotion);
    obj[QStringLiteral("dialog_text")] = m_payload.dialogText;

    QJsonObject hw;
    hw[QStringLiteral("device_name")] = m_payload.hw.deviceName;
    hw[QStringLiteral("vram_used_mb")] = m_payload.hw.vramUsedMb;
    hw[QStringLiteral("vram_total_mb")] = m_payload.hw.vramTotalMb;
    hw[QStringLiteral("temp_c")] = static_cast<int>(m_payload.hw.tempC);
    hw[QStringLiteral("power_w")] = m_payload.hw.powerW;
    hw[QStringLiteral("power_limit_w")] = m_payload.hw.powerLimitW;
    hw[QStringLiteral("gpu_util")] = static_cast<int>(m_payload.hw.gpuUtil);
    obj[QStringLiteral("hw")] = hw;

    QJsonObject inf;
    inf[QStringLiteral("engine")] = QString::fromUtf8(m_payload.inf.engine);
    inf[QStringLiteral("connected")] = m_payload.inf.connected;
    if (m_payload.inf.activeSlots) {
        inf[QStringLiteral("active_slots")] = static_cast<int>(*m_payload.inf.activeSlots);
    }
    if (m_payload.inf.totalSlots) {
        inf[QStringLiteral("total_slots")] = static_cast<int>(*m_payload.inf.totalSlots);
    }
    if (m_payload.inf.currentTps) {
        inf[QStringLiteral("current_tps")] = *m_payload.inf.currentTps;
    }
    if (m_payload.inf.cacheHitRatePct) {
        inf[QStringLiteral("cache_hit_rate_pct")] = *m_payload.inf.cacheHitRatePct;
    }
    if (m_payload.inf.speculativeActive) {
        inf[QStringLiteral("speculative_active")] = *m_payload.inf.speculativeActive;
    }
    inf[QStringLiteral("any_prefill")] = m_payload.inf.anyPrefill;
    inf[QStringLiteral("any_decoding")] = m_payload.inf.anyDecoding;
    inf[QStringLiteral("slots_disabled")] = m_payload.inf.slotsDisabled;
    obj[QStringLiteral("inf")] = inf;

    return obj;
}

QVariantList TelemetryService::slotsList() const {
    if (!m_payload.inf.slots) {
        return m_lastSlotsList;
    }
    QVariantList list;
    for (const auto& slot : *m_payload.inf.slots) {
        QVariantMap map;
        map[QStringLiteral("id")] = slot.id;
        map[QStringLiteral("isActive")] = slot.isActive;
        map[QStringLiteral("promptTokens")] = static_cast<qulonglong>(slot.promptTokens);
        map[QStringLiteral("cachedTokens")] = static_cast<qulonglong>(slot.cachedTokens);
        map[QStringLiteral("decodedTokens")] = static_cast<qulonglong>(slot.decodedTokens);
        map[QStringLiteral("cacheHitRatePct")] = slot.promptTokens > 0
            ? (static_cast<double>(slot.cachedTokens) * 100.0 / slot.promptTokens)
            : 0.0;
        list.append(map);
    }
    m_lastSlotsList = list;
    return list;
}

void TelemetryService::eraseIdleSlots(std::function<void(Result<int, QString>)> callback) {
    if (auto* llama = dynamic_cast<LlamaCppIt*>(m_it.get())) {
        llama->eraseIdleSlots(callback);
    } else {
        if (callback) {
            callback(Result<int, QString>::err(QStringLiteral("当前引擎不支持槽位擦除管理")));
        }
    }
}

} // namespace Margin::Plugins::LlamaPet
