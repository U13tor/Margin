#include "LlamaCppIt.h"
#include "LoopbackGuard.h"

#include <QJsonArray>
#include <QUrl>
#include <QJsonObject>
#include <QJsonValue>
#include <mutex>
#include <unordered_map>

namespace Margin::Plugins::LlamaPet {

namespace llama_slots {

namespace {

static std::mutex s_tpsMutex;
static std::unordered_map<const QElapsedTimer*, quint64> s_lastDecoded;

quint64 getJsonU64(const QJsonObject& obj, const QStringList& keys, quint64 defaultVal = 0) {
    for (const QString& k : keys) {
        if (obj.contains(k)) {
            const QJsonValue v = obj[k];
            if (v.isDouble()) {
                return static_cast<quint64>(v.toVariant().toULongLong());
            }
        }
    }
    return defaultVal;
}

std::optional<SlotRaw> parseSingleSlot(const QJsonValue& val) {
    if (!val.isObject()) {
        return std::nullopt;
    }
    const QJsonObject v = val.toObject();

    quint32 id = static_cast<quint32>(v.value(QStringLiteral("id")).toVariant().toUInt());

    quint64 promptTokens = getJsonU64(
        v, {QStringLiteral("prompt_tokens"), QStringLiteral("n_prompt_tokens"), QStringLiteral("n_prompt")});

    quint64 cachedTokens = 0;
    const QStringList cacheKeys = {
        QStringLiteral("cache_tokens"),
        QStringLiteral("n_prompt_tokens_cache"),
        QStringLiteral("n_cache")
    };
    for (const QString& k : cacheKeys) {
        if (v.contains(k)) {
            const QJsonValue cv = v[k];
            if (cv.isArray()) {
                cachedTokens = static_cast<quint64>(cv.toArray().size());
                break;
            } else if (cv.isDouble()) {
                cachedTokens = static_cast<quint64>(cv.toVariant().toULongLong());
                break;
            }
        }
    }

    quint64 decodedTokens = getJsonU64(v, {QStringLiteral("decoded_tokens"), QStringLiteral("n_decoded")});
    if (decodedTokens == 0 && v.contains(QStringLiteral("next_token"))) {
        const QJsonArray nt = v[QStringLiteral("next_token")].toArray();
        if (!nt.isEmpty() && nt[0].isObject()) {
            decodedTokens = nt[0].toObject().value(QStringLiteral("n_decoded")).toVariant().toULongLong();
        }
    }

    bool isActive = false;
    bool isPrefill = false;
    bool isDecoding = false;

    if (v.contains(QStringLiteral("state"))) {
        int st = v[QStringLiteral("state")].toInt();
        isActive = (st != 0);
        isPrefill = (st == 2);
        isDecoding = (st == 3);
    } else if (v.value(QStringLiteral("is_processing")).toBool(false)) {
        isActive = true;
        if (decodedTokens > 0) {
            isDecoding = true;
        } else {
            isPrefill = true;
        }
    }

    bool speculative = v.value(QStringLiteral("speculative")).toBool(false);

    SlotRaw raw;
    raw.id = id;
    raw.isActive = isActive;
    raw.isPrefill = isPrefill;
    raw.isDecoding = isDecoding;
    raw.promptTokens = promptTokens;
    raw.cachedTokens = cachedTokens;
    raw.decodedTokens = decodedTokens;
    raw.speculative = speculative;

    return raw;
}

} // namespace

QList<SlotRaw> parseSlots(const QJsonDocument& doc) {
    QJsonArray slotsArray;
    if (doc.isArray()) {
        slotsArray = doc.array();
    } else if (doc.isObject()) {
        slotsArray = doc.object().value(QStringLiteral("slots")).toArray();
    }

    QList<SlotRaw> result;
    result.reserve(slotsArray.size());
    for (const QJsonValue& val : slotsArray) {
        auto opt = parseSingleSlot(val);
        if (opt) {
            result.append(*opt);
        }
    }
    return result;
}

SlotsAnalysis analyzeSlots(const QList<SlotRaw>& rawSlots) {
    if (rawSlots.isEmpty()) {
        return SlotsAnalysis{};
    }

    SlotsAnalysis analysis;
    analysis.totalSlots = static_cast<quint32>(rawSlots.size());
    analysis.slots.reserve(rawSlots.size());

    quint64 totalCached = 0;
    quint64 totalPrompt = 0;
    bool anySpeculative = false;

    for (const SlotRaw& s : rawSlots) {
        if (s.isActive) {
            analysis.activeSlots++;
        }
        if (s.isPrefill) {
            analysis.anyPrefill = true;
        }
        if (s.isDecoding) {
            analysis.anyDecoding = true;
        }
        if (s.speculative) {
            anySpeculative = true;
        }
        if (s.promptTokens > 0) {
            totalCached += s.cachedTokens;
            totalPrompt += s.promptTokens;
        }
        analysis.totalDecodedTokens += s.decodedTokens;

        SlotInfo info;
        info.id = s.id;
        info.isActive = s.isActive;
        info.promptTokens = s.promptTokens;
        info.cachedTokens = s.cachedTokens;
        info.decodedTokens = s.decodedTokens;
        analysis.slots.append(info);
    }

    if (totalPrompt > 0) {
        analysis.cacheHitRatePct = (static_cast<float>(totalCached) / static_cast<float>(totalPrompt)) * 100.0f;
    } else {
        analysis.cacheHitRatePct = std::nullopt;
    }

    analysis.speculativeActive = anySpeculative;

    return analysis;
}

std::optional<float> calcTps(QElapsedTimer& timer, quint64 totalDecoded) {
    std::lock_guard<std::mutex> lock(s_tpsMutex);
    if (!timer.isValid()) {
        timer.start();
        s_lastDecoded[&timer] = totalDecoded;
        return std::nullopt;
    }

    qint64 elapsedMs = timer.elapsed();
    if (elapsedMs < 200) {
        return std::nullopt;
    }

    auto it = s_lastDecoded.find(&timer);
    if (it == s_lastDecoded.end()) {
        timer.restart();
        s_lastDecoded[&timer] = totalDecoded;
        return std::nullopt;
    }

    quint64 last = it->second;
    timer.restart();
    s_lastDecoded[&timer] = totalDecoded;

    if (totalDecoded >= last) {
        float elapsedSec = static_cast<float>(elapsedMs) / 1000.0f;
        float diff = static_cast<float>(totalDecoded - last);
        return diff / elapsedSec;
    } else {
        // 重置基准
        return std::nullopt;
    }
}

void resetTps(const QElapsedTimer* timer) {
    std::lock_guard<std::mutex> lock(s_tpsMutex);
    s_lastDecoded.erase(timer);
}

bool parseHealth(const QString& body) {
    QJsonParseError err{};
    QJsonDocument doc = QJsonDocument::fromJson(body.toUtf8(), &err);
    if (err.error == QJsonParseError::NoError && doc.isObject()) {
        QString status = doc.object().value(QStringLiteral("status")).toString();
        if (status.compare(QStringLiteral("ok"), Qt::CaseInsensitive) == 0 ||
            status.compare(QStringLiteral("healthy"), Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return !body.trimmed().isEmpty();
}

PropsInfo parseProps(const QString& body) {
    PropsInfo info;
    QJsonParseError err{};
    QJsonDocument doc = QJsonDocument::fromJson(body.toUtf8(), &err);
    if (err.error == QJsonParseError::NoError && doc.isObject()) {
        QJsonObject obj = doc.object();
        if (obj.contains(QStringLiteral("model_alias"))) {
            info.modelAlias = obj[QStringLiteral("model_alias")].toString();
        } else if (obj.contains(QStringLiteral("model_path"))) {
            QString path = obj[QStringLiteral("model_path")].toString();
            int slash = std::max(path.lastIndexOf(QLatin1Char('/')), path.lastIndexOf(QLatin1Char('\\')));
            info.modelAlias = (slash >= 0) ? path.mid(slash + 1) : path;
        }
        if (obj.contains(QStringLiteral("n_ctx"))) {
            info.nCtx = obj[QStringLiteral("n_ctx")].toVariant().toULongLong();
        }
        if (obj.contains(QStringLiteral("total_slots"))) {
            info.totalSlots = static_cast<quint32>(obj[QStringLiteral("total_slots")].toVariant().toUInt());
        }
    }
    return info;
}

} // namespace llama_slots

std::atomic<bool> LlamaCppIt::s_slotsAuthWarned{false};

LlamaCppIt::LlamaCppIt(const QString& endpoint, const QString& apiKey, QObject* parent)
    : QObject(parent), m_endpoint(endpoint), m_apiKey(apiKey) {
    m_nam = std::make_unique<QNetworkAccessManager>(this);
    m_snapshot.engine = "llama.cpp";
    m_snapshot.connected = false;
}

LlamaCppIt::~LlamaCppIt() {
    llama_slots::resetTps(&m_tpsTimer);
}

void LlamaCppIt::setMockTelemetry(const LlmTelemetry& telemetry) {
    m_hasMock = true;
    m_mockTelemetry = telemetry;
}

void LlamaCppIt::setEndpoint(const QString& endpoint) {
    m_endpoint = endpoint;
    m_failCount = 0;
}

void LlamaCppIt::setApiKey(const QString& apiKey) {
    m_apiKey = apiKey;
    s_slotsAuthWarned.store(false);
}

QNetworkRequest LlamaCppIt::buildRequest(const QString& path) const {
    QString cleanEndpoint = m_endpoint.trimmed();
    while (cleanEndpoint.endsWith(QLatin1Char('/'))) {
        cleanEndpoint.chop(1);
    }
    QUrl url(cleanEndpoint + path);
    LoopbackGuard::validate(url);

    QNetworkRequest req(url);
    req.setTransferTimeout(2000);
    if (!m_apiKey.trimmed().isEmpty()) {
        req.setRawHeader("Authorization", "Bearer " + m_apiKey.trimmed().toUtf8());
    }
    return req;
}

LlmTelemetry LlamaCppIt::pollTelemetry() {
    if (m_hasMock) {
        return m_mockTelemetry;
    }

    triggerAsyncPoll();
    return m_snapshot;
}

void LlamaCppIt::triggerAsyncPoll() {
    if (m_inFlight) {
        return;
    }

    QString cleanEndpoint = m_endpoint.trimmed();
    while (cleanEndpoint.endsWith(QLatin1Char('/'))) {
        cleanEndpoint.chop(1);
    }
    QUrl slotsUrl(cleanEndpoint + QStringLiteral("/slots"));
    if (!LoopbackGuard::isLoopback(slotsUrl)) {
        handleDisconnect();
        return;
    }

    m_inFlight = true;
    fetchSlots();
}

void LlamaCppIt::fetchSlots() {
    QNetworkRequest req = buildRequest(QStringLiteral("/slots"));
    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        int httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (httpCode == 404) {
            // 404: 未带 --slots 启动，进入 slotsDisabled 降级
            m_snapshot.engine = "llama.cpp";
            m_snapshot.connected = true;
            m_snapshot.slotsDisabled = true;
            m_snapshot.activeSlots = 0;
            m_snapshot.totalSlots = std::nullopt;
            m_snapshot.currentTps = std::nullopt;
            m_snapshot.cacheHitRatePct = std::nullopt;
            m_snapshot.slots = std::nullopt;
            m_failCount = 0;
            m_inFlight = false;
            return;
        }

        if (httpCode == 401 || httpCode == 403) {
            if (!s_slotsAuthWarned.exchange(true)) {
                qWarning("LlamaCppIt: llama-server 拒绝访问 (401/403)，请检查 API Key");
            }
            m_snapshot.connected = false;
            m_failCount++;
            m_inFlight = false;
            return;
        }

        if (reply->error() != QNetworkReply::NoError || (httpCode < 200 || httpCode >= 300)) {
            handleDisconnect();
            return;
        }

        s_slotsAuthWarned.store(false);
        QByteArray data = reply->readAll();
        QJsonParseError err{};
        QJsonDocument doc = QJsonDocument::fromJson(data, &err);
        if (err.error == QJsonParseError::NoError) {
            auto rawSlots = llama_slots::parseSlots(doc);
            auto analysis = llama_slots::analyzeSlots(rawSlots);
            auto tps = llama_slots::calcTps(m_tpsTimer, analysis.totalDecodedTokens);

            m_snapshot.engine = "llama.cpp";
            m_snapshot.connected = true;
            m_snapshot.slotsDisabled = false;
            m_snapshot.activeSlots = analysis.activeSlots;
            m_snapshot.totalSlots = analysis.totalSlots;
            m_snapshot.currentTps = tps;
            m_snapshot.cacheHitRatePct = analysis.cacheHitRatePct;
            m_snapshot.speculativeActive = analysis.speculativeActive;
            m_snapshot.slots = analysis.slots;
            m_snapshot.anyPrefill = analysis.anyPrefill;
            m_snapshot.anyDecoding = analysis.anyDecoding;
            m_failCount = 0;
        }

        m_inFlight = false;
    });
}

void LlamaCppIt::handleDisconnect() {
    m_inFlight = false;
    m_failCount++;
    if (m_failCount < 3) {
        // 3 周期防抖：沿用上次快照，保持 connected 标记
        m_snapshot.connected = true;
    } else {
        m_snapshot.engine = "llama.cpp";
        m_snapshot.connected = false;
        m_snapshot.activeSlots = 0;
        m_snapshot.currentTps = std::nullopt;
        m_snapshot.cacheHitRatePct = std::nullopt;
        m_snapshot.speculativeActive = std::nullopt;
        // 保持 slots 结构存在并将 isActive 标记为 false，避免 UI 列表跳动
        if (m_snapshot.slots) {
            for (auto& s : *m_snapshot.slots) {
                s.isActive = false;
            }
        }
        m_snapshot.anyPrefill = false;
        m_snapshot.anyDecoding = false;
        llama_slots::resetTps(&m_tpsTimer);
    }
}

void LlamaCppIt::eraseIdleSlots(std::function<void(Result<int, QString>)> callback) {
    QString cleanEndpoint = m_endpoint.trimmed();
    while (cleanEndpoint.endsWith(QLatin1Char('/'))) {
        cleanEndpoint.chop(1);
    }
    QUrl slotsUrl(cleanEndpoint + QStringLiteral("/slots"));
    if (!LoopbackGuard::isLoopback(slotsUrl)) {
        if (callback) {
            callback(Result<int, QString>::err(QStringLiteral("地址不是有效的本地回环地址")));
        }
        return;
    }

    QNetworkRequest req = buildRequest(QStringLiteral("/slots"));
    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, callback]() {
        reply->deleteLater();
        int httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (httpCode == 404) {
            if (callback) {
                callback(Result<int, QString>::err(
                    QStringLiteral("当前 llama-server 未带 --slots 参数启动，无法管理槽位")));
            }
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            if (callback) {
                callback(Result<int, QString>::err(
                    QStringLiteral("获取槽位状态失败: %1").arg(reply->errorString())));
            }
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        auto rawSlots = llama_slots::parseSlots(doc);
        QList<quint32> idleSlotIds;
        for (const auto& slot : rawSlots) {
            if (!slot.isActive) {
                idleSlotIds.append(slot.id);
            }
        }

        if (idleSlotIds.isEmpty()) {
            if (callback) {
                callback(Result<int, QString>::ok(0));
            }
            return;
        }

        auto remaining = std::make_shared<int>(idleSlotIds.size());
        auto erased = std::make_shared<int>(0);
        auto hasError = std::make_shared<bool>(false);

        for (quint32 id : idleSlotIds) {
            QNetworkRequest postReq = buildRequest(QStringLiteral("/slots/%1?action=erase").arg(id));
            QNetworkReply* postReply = m_nam->post(postReq, QByteArray());
            connect(postReply, &QNetworkReply::finished, this, [this, postReply, remaining, erased, hasError, callback]() {
                postReply->deleteLater();
                int code = postReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                if (postReply->error() == QNetworkReply::NoError && code >= 200 && code < 300) {
                    (*erased)++;
                } else {
                    *hasError = true;
                }

                (*remaining)--;
                if (*remaining == 0 && callback) {
                    if (*hasError) {
                        callback(Result<int, QString>::err(
                            QStringLiteral("清空 KV 失败: llama-server 需以 --slot-save-path 启动以启用槽位存储与擦除管理")));
                    } else {
                        callback(Result<int, QString>::ok(*erased));
                    }
                }
            });
        }
    });
}

} // namespace Margin::Plugins::LlamaPet
