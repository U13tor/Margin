#include "OllamaIt.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

namespace Margin::Plugins::LlamaPet {

namespace ollama {

QList<OllamaModel> parsePs(const QJsonDocument& doc) {
    QList<OllamaModel> list;
    if (!doc.isObject()) {
        return list;
    }

    const QJsonObject root = doc.object();
    if (!root.contains(QStringLiteral("models")) || !root[QStringLiteral("models")].isArray()) {
        return list;
    }

    const QJsonArray arr = root[QStringLiteral("models")].toArray();
    list.reserve(arr.size());

    for (const QJsonValue& val : arr) {
        if (!val.isObject()) continue;
        const QJsonObject obj = val.toObject();

        QString modelName;
        if (obj.contains(QStringLiteral("model")) && obj[QStringLiteral("model")].isString()) {
            modelName = obj[QStringLiteral("model")].toString();
        } else if (obj.contains(QStringLiteral("name")) && obj[QStringLiteral("name")].isString()) {
            modelName = obj[QStringLiteral("name")].toString();
        }

        quint64 sizeVram = 0;
        if (obj.contains(QStringLiteral("size_vram"))) {
            sizeVram = obj[QStringLiteral("size_vram")].toVariant().toULongLong();
        }

        list.append(OllamaModel{modelName, sizeVram});
    }

    return list;
}

} // namespace ollama

OllamaIt::OllamaIt(const QString& endpoint) : m_endpoint(endpoint) {
}

void OllamaIt::setMockTelemetry(const LlmTelemetry& telemetry) {
    m_hasMock = true;
    m_mockTelemetry = telemetry;
}

LlmTelemetry OllamaIt::pollTelemetry() {
    if (m_hasMock) {
        return m_mockTelemetry;
    }

    LlmTelemetry t;
    t.engine = "ollama";
    t.connected = false;
    return t;
}

} // namespace Margin::Plugins::LlamaPet
