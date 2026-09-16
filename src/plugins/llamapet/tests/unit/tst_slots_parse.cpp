#include <QtTest>
#include "core/LlamaCppIt.h"
#include <cmath>

using namespace Margin::Plugins::LlamaPet;
using namespace Margin::Plugins::LlamaPet::llama_slots;

class TstSlotsParse : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testFixtureSlotsParsingMatchesSpec();
    void testNumericCacheTokens();
    void testTpsDiffCalculation();
    void testEmptyAndZeroPrompt();
    void testParseHealthWithFixture();
    void testParsePropsWithFixture();
};

void TstSlotsParse::testFixtureSlotsParsingMatchesSpec() {
    QFile file(QStringLiteral(FIXTURES_DIR "/llamacpp_slots.json"));
    QVERIFY2(file.open(QIODevice::ReadOnly), "Failed to open llamacpp_slots.json fixture");
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QVERIFY(!doc.isNull());

    const QList<SlotRaw> rawSlots = parseSlots(doc);
    QCOMPARE(rawSlots.size(), 4);

    // 槽位 0：20/24 ≈ 83.33%
    QCOMPARE(rawSlots[0].id, 0u);
    QCOMPARE(rawSlots[0].promptTokens, 24ull);
    QCOMPARE(rawSlots[0].cachedTokens, 20ull);
    QCOMPARE(rawSlots[0].decodedTokens, 18ull);
    QVERIFY(rawSlots[0].isActive);
    QVERIFY(rawSlots[0].isDecoding);
    QVERIFY(rawSlots[0].cacheHitRatePct().has_value());
    QVERIFY(std::abs(*rawSlots[0].cacheHitRatePct() - (20.0f / 24.0f * 100.0f)) < 0.01f);

    // 槽位 1：0/20 = 0%
    QCOMPARE(rawSlots[1].id, 1u);
    QCOMPARE(rawSlots[1].promptTokens, 20ull);
    QCOMPARE(rawSlots[1].cachedTokens, 0ull);
    QCOMPARE(rawSlots[1].decodedTokens, 2ull);
    QVERIFY(rawSlots[1].isActive);
    QVERIFY(rawSlots[1].isDecoding);
    QVERIFY(rawSlots[1].cacheHitRatePct().has_value());
    QVERIFY(std::abs(*rawSlots[1].cacheHitRatePct() - 0.0f) < 0.01f);

    // 槽位 2：28/32 = 87.5%
    QCOMPARE(rawSlots[2].id, 2u);
    QCOMPARE(rawSlots[2].promptTokens, 32ull);
    QCOMPARE(rawSlots[2].cachedTokens, 28ull);
    QCOMPARE(rawSlots[2].decodedTokens, 0ull);
    QVERIFY(rawSlots[2].isActive);
    QVERIFY(rawSlots[2].isPrefill);
    QVERIFY(rawSlots[2].cacheHitRatePct().has_value());
    QVERIFY(std::abs(*rawSlots[2].cacheHitRatePct() - 87.5f) < 0.01f);

    // 槽位 3：空闲
    QCOMPARE(rawSlots[3].id, 3u);
    QCOMPARE(rawSlots[3].promptTokens, 0ull);
    QVERIFY(!rawSlots[3].isActive);
    QVERIFY(!rawSlots[3].cacheHitRatePct().has_value());

    // 聚合分析指标断言
    const SlotsAnalysis analysis = analyzeSlots(rawSlots);
    QCOMPARE(analysis.totalSlots, 4u);
    QCOMPARE(analysis.activeSlots, 3u);
    QVERIFY(analysis.anyPrefill);
    QVERIFY(analysis.anyDecoding);
    QCOMPARE(analysis.speculativeActive, std::optional<bool>(false));
    QCOMPARE(analysis.totalDecodedTokens, 20ull);

    // 整体命中率：48 / 76 ≈ 63.16%
    QVERIFY(analysis.cacheHitRatePct.has_value());
    QVERIFY(std::abs(*analysis.cacheHitRatePct - (48.0f / 76.0f * 100.0f)) < 0.01f);
}

void TstSlotsParse::testNumericCacheTokens() {
    const QString jsonStr = QStringLiteral(R"([
        {
            "id": 0,
            "state": 3,
            "prompt_tokens": 100,
            "cache_tokens": 75,
            "n_decoded": 30
        }
    ])");
    const QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
    const QList<SlotRaw> raw = parseSlots(doc);
    QCOMPARE(raw.size(), 1);
    QCOMPARE(raw[0].cachedTokens, 75ull);
    QCOMPARE(raw[0].promptTokens, 100ull);
    QCOMPARE(raw[0].decodedTokens, 30ull);
    QVERIFY(raw[0].cacheHitRatePct().has_value());
    QVERIFY(std::abs(*raw[0].cacheHitRatePct() - 75.0f) < 0.01f);
}

void TstSlotsParse::testTpsDiffCalculation() {
    QElapsedTimer timer;

    // 首次调用记录基准，输出 nullopt
    auto tps0 = calcTps(timer, 100);
    QVERIFY(!tps0.has_value());

    // 等待 210ms 后新增 40 token
    QTest::qSleep(210);
    auto tps1 = calcTps(timer, 140);
    QVERIFY(tps1.has_value());
    // 40 tokens / ~0.21s ≈ 190 t/s (> 0)
    QVERIFY(*tps1 > 0.0f);

    // 服务重启或模型重载：140 -> 20，差值为负，应复位基准并返回 nullopt
    QTest::qSleep(210);
    auto tps2 = calcTps(timer, 20);
    QVERIFY(!tps2.has_value());

    // 时间间隔过短 (< 200ms) 返回 nullopt
    auto tpsShort = calcTps(timer, 25);
    QVERIFY(!tpsShort.has_value());
}

void TstSlotsParse::testEmptyAndZeroPrompt() {
    const SlotsAnalysis emptyAnalysis = analyzeSlots({});
    QCOMPARE(emptyAnalysis.totalSlots, 0u);
    QCOMPARE(emptyAnalysis.activeSlots, 0u);
    QVERIFY(!emptyAnalysis.cacheHitRatePct.has_value());

    SlotRaw zeroSlot;
    zeroSlot.id = 0;
    zeroSlot.promptTokens = 0;
    const SlotsAnalysis zeroAnalysis = analyzeSlots({zeroSlot});
    QVERIFY(!zeroAnalysis.cacheHitRatePct.has_value());
}

void TstSlotsParse::testParseHealthWithFixture() {
    QFile file(QStringLiteral(FIXTURES_DIR "/llamacpp_health.json"));
    QVERIFY2(file.open(QIODevice::ReadOnly), "Failed to open llamacpp_health.json");
    const QString content = QString::fromUtf8(file.readAll());
    QVERIFY(parseHealth(content));
}

void TstSlotsParse::testParsePropsWithFixture() {
    QFile file(QStringLiteral(FIXTURES_DIR "/llamacpp_props.json"));
    QVERIFY2(file.open(QIODevice::ReadOnly), "Failed to open llamacpp_props.json");
    const QString content = QString::fromUtf8(file.readAll());
    const PropsInfo props = parseProps(content);
    QCOMPARE(props.modelAlias, std::optional<QString>(QStringLiteral("qwen3-14b")));
    QCOMPARE(props.nCtx, std::optional<quint64>(16384ull));
    QCOMPARE(props.totalSlots, std::optional<quint32>(4u));
}

QTEST_MAIN(TstSlotsParse)
#include "tst_slots_parse.moc"
