#pragma once

#include "core/ItProvider.h"
#include "Margin/Result.h"
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QString>
#include <atomic>
#include <functional>
#include <memory>
#include <optional>

namespace Margin::Plugins::LlamaPet {

namespace llama_slots {

struct SlotRaw {
    quint32 id{0};
    bool isActive{false};
    bool isPrefill{false};
    bool isDecoding{false};
    quint64 promptTokens{0};
    quint64 cachedTokens{0};
    quint64 decodedTokens{0};
    bool speculative{false};

    std::optional<float> cacheHitRatePct() const {
        if (promptTokens > 0) {
            return (static_cast<float>(cachedTokens) / static_cast<float>(promptTokens)) * 100.0f;
        }
        return std::nullopt;
    }
};

struct SlotsAnalysis {
    quint32 activeSlots{0};
    quint32 totalSlots{0};
    std::optional<float> cacheHitRatePct;
    std::optional<bool> speculativeActive;
    bool anyPrefill{false};
    bool anyDecoding{false};
    quint64 totalDecodedTokens{0};
    QList<SlotInfo> slots;
};

struct PropsInfo {
    std::optional<QString> modelAlias;
    std::optional<quint64> nCtx;
    std::optional<quint32> totalSlots;
};

struct TpsTracker {
    QElapsedTimer timer;
    std::unordered_map<quint32, quint64> slotLastDecoded;
    float smoothedTps{0.0f};
    quint64 monotonicTotal{0};

    void reset();
    std::optional<float> update(const QList<SlotRaw>& slots, bool anyActive, bool anyDecoding, bool anyPrefill);
};

QList<SlotRaw> parseSlots(const QJsonDocument& doc);
SlotsAnalysis analyzeSlots(const QList<SlotRaw>& rawSlots);
std::optional<float> calcTps(QElapsedTimer& timer, quint64 totalDecoded);
void resetTps(const QElapsedTimer* timer);
bool parseHealth(const QString& body);
PropsInfo parseProps(const QString& body);

struct MetricsInfo {
    std::optional<quint64> predictedTokensTotal;
    std::optional<quint64> promptTokensTotal;
};
MetricsInfo parsePrometheusMetrics(const QString& body);

} // namespace llama_slots

class LlamaCppIt : public QObject, public ItProvider {
public:
    explicit LlamaCppIt(const QString& endpoint = QStringLiteral("http://127.0.0.1:8080"),
                        const QString& apiKey = QString{},
                        QObject* parent = nullptr);
    ~LlamaCppIt() override;

    // Mock 注入支持（单测或无网络环境使用）
    void setMockTelemetry(const LlmTelemetry& telemetry);
    void setEndpoint(const QString& endpoint);
    void setApiKey(const QString& apiKey);

    const char* engineName() const override { return "llama.cpp"; }
    LlmTelemetry pollTelemetry() override;

    // 清空空闲槽位 KV (POST /slots/{id}?action=erase)
    void eraseIdleSlots(std::function<void(Result<int, QString>)> callback);

private:
    void triggerAsyncPoll();
    void fetchSlots();
    void fetchMetrics();
    void handleDisconnect();
    QNetworkRequest buildRequest(const QString& path) const;

    QString m_endpoint;
    QString m_apiKey;
    bool m_hasMock{false};
    LlmTelemetry m_mockTelemetry;
    LlmTelemetry m_snapshot;
    llama_slots::TpsTracker m_tpsTracker;
    std::unique_ptr<QNetworkAccessManager> m_nam;
    bool m_inFlight{false};
    bool m_metricsInFlight{false};
    int m_failCount{0};
    static std::atomic<bool> s_slotsAuthWarned;
};

} // namespace Margin::Plugins::LlamaPet
