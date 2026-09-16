#include <QtTest>
#include "core/OpenAiIt.h"

using namespace Margin::Plugins::LlamaPet;
using namespace Margin::Plugins::LlamaPet::openai_compat;

class TstOpenAiParse : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testParseModelsWithFixture();
    void testParseModelsEmptyAndCorrupt();
    void testAuthHeaderGeneration();
    void testEngineNameMatchesSpec();
};

void TstOpenAiParse::testParseModelsWithFixture() {
    QFile file(QStringLiteral(FIXTURES_DIR "/openai_models.json"));
    QVERIFY2(file.open(QIODevice::ReadOnly), "Failed to open openai_models.json fixture");
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QVERIFY(!doc.isNull());

    const QStringList models = parseModels(doc);
    QCOMPARE(models.size(), 2);
    QCOMPARE(models[0], QStringLiteral("qwen2.5-14b-instruct"));
    QCOMPARE(models[1], QStringLiteral("embedding-model"));
}

void TstOpenAiParse::testParseModelsEmptyAndCorrupt() {
    QCOMPARE(parseModels(QJsonDocument::fromJson("{\"data\": []}")).size(), 0);
    QCOMPARE(parseModels(QJsonDocument::fromJson("{}")).size(), 0);
    QCOMPARE(parseModels(QJsonDocument::fromJson("invalid json")).size(), 0);
}

void TstOpenAiParse::testAuthHeaderGeneration() {
    QVERIFY(buildAuthHeader(QString{}).isEmpty());
    QVERIFY(buildAuthHeader(QStringLiteral("   ")).isEmpty());
    QCOMPARE(buildAuthHeader(QStringLiteral("sk-test-secret-12345")),
             QStringLiteral("Bearer sk-test-secret-12345"));
}

void TstOpenAiParse::testEngineNameMatchesSpec() {
    OpenAiIt provider(QStringLiteral("http://127.0.0.1:1234"), QString{});
    QCOMPARE(provider.engineName(), "openai_compatible");
}

QTEST_MAIN(TstOpenAiParse)
#include "tst_openai_parse.moc"
