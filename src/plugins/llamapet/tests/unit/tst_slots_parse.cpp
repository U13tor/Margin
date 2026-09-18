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
    void testTpsTracker();
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

void TstSlotsParse::testTpsTracker() {
    TpsTracker tracker;

    SlotRaw s0;
    s0.id = 0;
    s0.isActive = true;
    s0.isDecoding = true;
    s0.decodedTokens = 10;

    // 1. 首次调用记录基准，由于此前 smoothedTps 为 0，返回 nullopt
    auto res0 = tracker.update({s0}, true, true, false);
    QVERIFY(!res0.has_value());

    // 2. 210ms 后生成 20 个 token (decodedTokens = 30)
    QTest::qSleep(210);
    s0.decodedTokens = 30;
    auto res1 = tracker.update({s0}, true, true, false);
    QVERIFY(res1.has_value());
    // 首次从空闲启动：应直接响应瞬时 TPS (20 / ~0.21s ≈ 95 t/s)，无滞后延迟
    float initialTps = *res1;
    QVERIFY(initialTps > 50.0f);

    // 3. 后续采样做低通 EMA 平滑滤波
    QTest::qSleep(210);
    s0.decodedTokens = 40; // 增量较小 (10 tokens)
    auto res2 = tracker.update({s0}, true, true, false);
    QVERIFY(res2.has_value());
    // EMA 平滑后的值在 [10/0.21, initialTps] 之间
    QVERIFY(*res2 > 0.0f);

    // 4. 槽位完成任务并重置 (s0.decodedTokens = 0, isActive = false)，同时新槽位 s1 启动 (15 tokens)
    // 验证不会因 s0 归零发生负差跌落，单调增量累加正常工作
    QTest::qSleep(210);
    s0.isActive = false;
    s0.isDecoding = false;
    s0.decodedTokens = 0;

    SlotRaw s1;
    s1.id = 1;
    s1.isActive = true;
    s1.isDecoding = true;
    s1.decodedTokens = 15;

    auto res3 = tracker.update({s0, s1}, true, true, false);
    QVERIFY(res3.has_value());
    QVERIFY(*res3 > 0.0f);
    // 验证内部单调计数器累加正确: 20 (s0初增量) + 10 (s0次增量) + 15 (s1增量) = 45
    QCOMPARE(tracker.monotonicTotal, 45ull);

    // 5. 任务全部结束进入完全空闲：验证软着陆优雅衰减 (Soft Decay)，杜绝垂直砸地
    QTest::qSleep(210);
    s1.isActive = false;
    s1.isDecoding = false;
    s1.decodedTokens = 0;
    auto res4 = tracker.update({s0, s1}, false, false, false);
    QVERIFY(res4.has_value());
    // 衰减为此前值的 ~50%，大于 0 但平稳下降
    QVERIFY(*res4 < *res3);
    QVERIFY(*res4 > 0.0f);
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
