#include <QtTest>
#include "core/OllamaIt.h"

using namespace Margin::Plugins::LlamaPet;
using namespace Margin::Plugins::LlamaPet::ollama;

class TstOllamaParse : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testParsePsWithFixture();
    void testParsePsEmptyAndCorrupt();
    void testParsePsToleratesMissingFields();
};

void TstOllamaParse::testParsePsWithFixture() {
    QFile file(QStringLiteral(FIXTURES_DIR "/ollama_ps.json"));
    QVERIFY2(file.open(QIODevice::ReadOnly), "Failed to open ollama_ps.json fixture");
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QVERIFY(!doc.isNull());

    const QList<OllamaModel> models = parsePs(doc);
    QCOMPARE(models.size(), 2);
    QCOMPARE(models[0].model, QStringLiteral("qwen3:14b"));
    QCOMPARE(models[0].sizeVram, 8954829312ull);
    QCOMPARE(models[1].model, QStringLiteral("nomic-embed-text:latest"));
    QCOMPARE(models[1].sizeVram, 271044608ull);

    quint64 totalVram = 0;
    for (const auto& m : models) {
        totalVram += m.sizeVram;
    }
    QCOMPARE(totalVram, 9225873920ull);
}

void TstOllamaParse::testParsePsEmptyAndCorrupt() {
    QCOMPARE(parsePs(QJsonDocument::fromJson("{\"models\": []}")).size(), 0);
    QCOMPARE(parsePs(QJsonDocument::fromJson("{}")).size(), 0);
    QCOMPARE(parsePs(QJsonDocument::fromJson("invalid json")).size(), 0);
}

void TstOllamaParse::testParsePsToleratesMissingFields() {
    const QString jsonStr = QStringLiteral(R"({
        "models": [
            { "name": "only-name-model" },
            { "model": "only-model-id", "size_vram": 1024 }
        ]
    })");
    const QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
    const QList<OllamaModel> models = parsePs(doc);
    QCOMPARE(models.size(), 2);
    QCOMPARE(models[0].model, QStringLiteral("only-name-model"));
    QCOMPARE(models[0].sizeVram, 0ull);
    QCOMPARE(models[1].model, QStringLiteral("only-model-id"));
    QCOMPARE(models[1].sizeVram, 1024ull);
}

QTEST_MAIN(TstOllamaParse)
#include "tst_ollama_parse.moc"
