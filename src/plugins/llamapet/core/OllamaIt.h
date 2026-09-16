#pragma once

#include "core/ItProvider.h"
#include <QJsonDocument>
#include <QList>
#include <QString>

namespace Margin::Plugins::LlamaPet {

namespace ollama {

struct OllamaModel {
    QString model;
    quint64 sizeVram{0};
};

QList<OllamaModel> parsePs(const QJsonDocument& doc);

} // namespace ollama

class OllamaIt : public ItProvider {
public:
    explicit OllamaIt(const QString& endpoint = QStringLiteral("http://127.0.0.1:11434"));

    void setMockTelemetry(const LlmTelemetry& telemetry);

    const char* engineName() const override { return "ollama"; }
    LlmTelemetry pollTelemetry() override;

private:
    QString m_endpoint;
    bool m_hasMock{false};
    LlmTelemetry m_mockTelemetry;
};

} // namespace Margin::Plugins::LlamaPet
