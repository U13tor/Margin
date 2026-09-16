#include "OpenAiIt.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

namespace Margin::Plugins::LlamaPet {

namespace openai_compat {

QStringList parseModels(const QJsonDocument& doc) {
    QStringList models;
    if (!doc.isObject()) {
        return models;
    }

    const QJsonObject root = doc.object();
    if (!root.contains(QStringLiteral("data")) || !root[QStringLiteral("data")].isArray()) {
        return models;
    }

    const QJsonArray arr = root[QStringLiteral("data")].toArray();
    for (const QJsonValue& val : arr) {
        if (!val.isObject()) continue;
        const QJsonObject obj = val.toObject();
        const QString id = obj.value(QStringLiteral("id")).toString();
        if (!id.isEmpty()) {
            models.append(id);
        }
    }

    return models;
}

QString buildAuthHeader(const QString& apiKey) {
    const QString trimmed = apiKey.trimmed();
    if (trimmed.isEmpty()) {
        return QString{};
    }
    return QStringLiteral("Bearer %1").arg(trimmed);
}

} // namespace openai_compat

OpenAiIt::OpenAiIt(const QString& endpoint, const QString& apiKey)
    : m_endpoint(endpoint), m_apiKey(apiKey) {
}

void OpenAiIt::setMockTelemetry(const LlmTelemetry& telemetry) {
    m_hasMock = true;
    m_mockTelemetry = telemetry;
}

LlmTelemetry OpenAiIt::pollTelemetry() {
    if (m_hasMock) {
        return m_mockTelemetry;
    }

    LlmTelemetry t;
    t.engine = "openai_compatible";
    t.connected = false;
    return t;
}

} // namespace Margin::Plugins::LlamaPet
