#pragma once

#include "core/ItProvider.h"
#include <QJsonDocument>
#include <QString>
#include <QStringList>

namespace Margin::Plugins::LlamaPet {

namespace openai_compat {

QStringList parseModels(const QJsonDocument& doc);
QString buildAuthHeader(const QString& apiKey);

} // namespace openai_compat

class OpenAiIt : public ItProvider {
public:
    explicit OpenAiIt(const QString& endpoint = QStringLiteral("http://127.0.0.1:1234"),
                      const QString& apiKey = QString{});

    void setMockTelemetry(const LlmTelemetry& telemetry);

    const char* engineName() const override { return "openai_compatible"; }
    LlmTelemetry pollTelemetry() override;

private:
    QString m_endpoint;
    QString m_apiKey;
    bool m_hasMock{false};
    LlmTelemetry m_mockTelemetry;
};

} // namespace Margin::Plugins::LlamaPet
