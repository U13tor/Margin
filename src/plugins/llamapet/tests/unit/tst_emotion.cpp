#include <QtTest>
#include "core/EmotionEngine.h"
#include "core/Health.h"

using namespace Margin::Plugins::LlamaPet;

class TstEmotion : public QObject {
    Q_OBJECT

private:
    EmotionInputs defaultInput() {
        EmotionInputs in;
        in.vramPercent = 40.0f;
        in.vramDanger = 96.0f;
        in.vramWarn = 90.0f;
        in.connected = true;
        in.anyPrefill = false;
        in.anyDecoding = false;
        in.cacheHitRate = std::nullopt;
        in.idleTicks = 0;
        return in;
    }

private Q_SLOTS:
    void testInitialStateIsIdle();
    void testNormalStateTransitionsFlow();
    void testOomOverridesOfflineState();
    void testOomOverridesPrefillAndDecoding();
    void testOomOverridesHappyCache();
    void testOomHysteresisStayInOomAboveWarn();
    void testOomHysteresisExitRequiresTwoTicksBelowWarn();
    void testOfflineHysteresisRequiresTwoFailuresToEnter();
    void testOfflineHysteresisImmediateExitOnSingleSuccess();
    void testHappyCacheRequiresTwoTicksAboveSeventyPct();
    void testHappyCacheHysteresisStayBetweenSixtyAndSeventy();
    void testHappyCacheHysteresisExitBelowSixty();
    void testSleepingBoundaryAtThreeHundredTicks();
    void testDialogForAllEmotionsAndTpsFormatting();
    void testHealthScoreHealthyConditionMigrated();
    void testHealthScoreDisconnectedConditionMigrated();
    void testHealthScoreOomDangerConditionMigrated();
    void testHealthScorePerfectBoundary();
    void testHealthScoreTempBoundaries();
    void testHealthScorePowerLimitZeroFallback();
    void testStatusLevelDisconnectedWarningVsDanger();
    void testStatusLevelTempBoundaryAtSeventyFive();
    void testStatusLevelScoreBelowFiftyAndEighty();
};

void TstEmotion::testInitialStateIsIdle() {
    EmotionEngine engine;
    QCOMPARE(engine.current(), Emotion::Idle);
}

void TstEmotion::testNormalStateTransitionsFlow() {
    EmotionEngine engine;
    EmotionInputs input = defaultInput();

    // 初始空闲
    QCOMPARE(engine.tick(input), Emotion::Idle);

    // 吞入 Prompt Prefill
    input.anyPrefill = true;
    QCOMPARE(engine.tick(input), Emotion::EatingPrefill);

    // 切换到生成解码阶段，进入 SpittingTokens
    input.anyPrefill = false;
    input.anyDecoding = true;
    QCOMPARE(engine.tick(input), Emotion::SpittingTokens);

    // 解码结束，回到待命状态
    input.anyDecoding = false;
    QCOMPARE(engine.tick(input), Emotion::Idle);
}

void TstEmotion::testOomOverridesOfflineState() {
    EmotionEngine engine;
    EmotionInputs input = defaultInput();

    // 服务离线同时触发显存爆满红线，PanickingOom 应无条件压制离线状态
    input.connected = false;
    input.vramPercent = 97.0f;
    QCOMPARE(engine.tick(input), Emotion::PanickingOom);
}

void TstEmotion::testOomOverridesPrefillAndDecoding() {
    EmotionEngine engine;
    EmotionInputs input = defaultInput();

    // 无论正在 Prefill 还是 Decoding，OOM 均拥有最高仲裁优先级
    input.anyPrefill = true;
    input.vramPercent = 97.0f;
    QCOMPARE(engine.tick(input), Emotion::PanickingOom);

    input.anyPrefill = false;
    input.anyDecoding = true;
    QCOMPARE(engine.tick(input), Emotion::PanickingOom);
}

void TstEmotion::testOomOverridesHappyCache() {
    EmotionEngine engine;
    EmotionInputs input = defaultInput();
    input.anyDecoding = true;
    input.cacheHitRate = 80.0f;

    // 进 Happy 需 2 周期
    QCOMPARE(engine.tick(input), Emotion::SpittingTokens);
    QCOMPARE(engine.tick(input), Emotion::HappyCache);

    // 突发显存红线
    input.vramPercent = 98.0f;
    QCOMPARE(engine.tick(input), Emotion::PanickingOom);
}

void TstEmotion::testOomHysteresisStayInOomAboveWarn() {
    EmotionEngine engine;
    EmotionInputs input = defaultInput();
    input.vramPercent = 96.0f;
    QCOMPARE(engine.tick(input), Emotion::PanickingOom);

    // 显存回落到 92%（在 warn 90% 与 danger 96% 之间滞回带），保持 OOM
    input.vramPercent = 92.0f;
    QCOMPARE(engine.tick(input), Emotion::PanickingOom);
    QCOMPARE(engine.tick(input), Emotion::PanickingOom);
}

void TstEmotion::testOomHysteresisExitRequiresTwoTicksBelowWarn() {
    EmotionEngine engine;
    EmotionInputs input = defaultInput();
    input.vramPercent = 97.0f;
    QCOMPARE(engine.tick(input), Emotion::PanickingOom);

    // 回落到 88%（< 90%）第 1 周期，仍保持 OOM 防抖
    input.vramPercent = 88.0f;
    QCOMPARE(engine.tick(input), Emotion::PanickingOom);

    // 连续第 2 周期低于 90%，正式退出 OOM 回到 Idle
    QCOMPARE(engine.tick(input), Emotion::Idle);
}

void TstEmotion::testOfflineHysteresisRequiresTwoFailuresToEnter() {
    EmotionEngine engine;
    EmotionInputs input = defaultInput();

    QCOMPARE(engine.tick(input), Emotion::Idle);

    // 第 1 周期失败，仍防抖，输出 Idle
    input.connected = false;
    QCOMPARE(engine.tick(input), Emotion::Idle);

    // 第 2 周期连续失败，进入 ConfusedOffline
    QCOMPARE(engine.tick(input), Emotion::ConfusedOffline);
}

void TstEmotion::testOfflineHysteresisImmediateExitOnSingleSuccess() {
    EmotionEngine engine;
    EmotionInputs input = defaultInput();
    input.connected = false;

    // 触发进入离线态
    engine.tick(input);
    QCOMPARE(engine.tick(input), Emotion::ConfusedOffline);

    // 单次恢复连通，立即退出离线态
    input.connected = true;
    QCOMPARE(engine.tick(input), Emotion::Idle);
}

void TstEmotion::testHappyCacheRequiresTwoTicksAboveSeventyPct() {
    EmotionEngine engine;
    EmotionInputs input = defaultInput();
    input.anyDecoding = true;
    input.cacheHitRate = 75.0f;

    // 第 1 周期虽然 >=70%，但仍为 SpittingTokens
    QCOMPARE(engine.tick(input), Emotion::SpittingTokens);

    // 持续第 2 周期 >=70%，进入 HappyCache
    QCOMPARE(engine.tick(input), Emotion::HappyCache);
}

void TstEmotion::testHappyCacheHysteresisStayBetweenSixtyAndSeventy() {
    EmotionEngine engine;
    EmotionInputs input = defaultInput();
    input.anyDecoding = true;
    input.cacheHitRate = 85.0f;

    engine.tick(input);
    QCOMPARE(engine.tick(input), Emotion::HappyCache);

    // 回落到 65%（在 60% 与 70% 滞回带内），保持 HappyCache
    input.cacheHitRate = 65.0f;
    QCOMPARE(engine.tick(input), Emotion::HappyCache);
    QCOMPARE(engine.tick(input), Emotion::HappyCache);
}

void TstEmotion::testHappyCacheHysteresisExitBelowSixty() {
    EmotionEngine engine;
    EmotionInputs input = defaultInput();
    input.anyDecoding = true;
    input.cacheHitRate = 85.0f;

    engine.tick(input);
    QCOMPARE(engine.tick(input), Emotion::HappyCache);

    // 命中率跌破 60%，退出 HappyCache 回到 SpittingTokens
    input.cacheHitRate = 59.0f;
    QCOMPARE(engine.tick(input), Emotion::SpittingTokens);
}

void TstEmotion::testSleepingBoundaryAtThreeHundredTicks() {
    EmotionEngine engine;
    EmotionInputs input = defaultInput();

    input.idleTicks = 299;
    QCOMPARE(engine.tick(input), Emotion::Idle);

    input.idleTicks = 300;
    QCOMPARE(engine.tick(input), Emotion::Sleeping);

    input.idleTicks = 500;
    QCOMPARE(engine.tick(input), Emotion::Sleeping);

    // 新请求唤醒
    input.idleTicks = 0;
    input.anyPrefill = true;
    QCOMPARE(engine.tick(input), Emotion::EatingPrefill);
}

void TstEmotion::testDialogForAllEmotionsAndTpsFormatting() {
    QCOMPARE(dialogFor(Emotion::Sleeping, std::nullopt), QStringLiteral("呼.. 模型浅睡中"));
    QCOMPARE(dialogFor(Emotion::Idle, std::nullopt), QStringLiteral("随时准备推理！"));
    QCOMPARE(dialogFor(Emotion::EatingPrefill, std::nullopt), QStringLiteral("正在吞入代码..."));
    QCOMPARE(dialogFor(Emotion::SpittingTokens, 35.5f), QStringLiteral("全力生成中 (35.5 t/s)"));
    QCOMPARE(dialogFor(Emotion::SpittingTokens, std::nullopt), QStringLiteral("全力生成中"));
    QCOMPARE(dialogFor(Emotion::HappyCache, std::nullopt), QStringLiteral("✨ KV 缓存复用达成！"));
    QCOMPARE(dialogFor(Emotion::PanickingOom, std::nullopt), QStringLiteral("🚨 显存快撑爆啦！要掉速！"));
    QCOMPARE(dialogFor(Emotion::ConfusedOffline, std::nullopt), QStringLiteral("未检测到本地服务"));
}

void TstEmotion::testHealthScoreHealthyConditionMigrated() {
    quint32 s = health::score(33.3f, 45, 100.0f, 450.0f, true);
    StatusLevel level = health::statusLevel(s, 33.3f, 45, 0.96f, 0.90f, true);
    QVERIFY(s >= 80);
    QCOMPARE(level, StatusLevel::Healthy);
}

void TstEmotion::testHealthScoreDisconnectedConditionMigrated() {
    quint32 s = health::score(16.7f, 40, 50.0f, 450.0f, false);
    StatusLevel level = health::statusLevel(s, 16.7f, 40, 0.96f, 0.90f, false);
    QCOMPARE(s, 0u);
    QCOMPARE(level, StatusLevel::Warning);
}

void TstEmotion::testHealthScoreOomDangerConditionMigrated() {
    quint32 s = health::score(97.6f, 80, 420.0f, 450.0f, true);
    StatusLevel level = health::statusLevel(s, 97.6f, 80, 0.96f, 0.90f, true);
    QVERIFY(s < 50);
    QCOMPARE(level, StatusLevel::Danger);
}

void TstEmotion::testHealthScorePerfectBoundary() {
    quint32 s = health::score(0.0f, 45, 100.0f, 450.0f, true);
    QCOMPARE(s, 100u);
}

void TstEmotion::testHealthScoreTempBoundaries() {
    // <=50 满分
    quint32 sCool = health::score(0.0f, 50, 0.0f, 100.0f, true);
    QCOMPARE(sCool, 100u);

    // >=85 零分 (V=0, T=0, P=0, S=1 => score = 20)
    quint32 sHot = health::score(100.0f, 85, 100.0f, 100.0f, true);
    QCOMPARE(sHot, 20u);
}

void TstEmotion::testHealthScorePowerLimitZeroFallback() {
    quint32 s = health::score(50.0f, 50, 0.0f, 0.0f, true);
    QVERIFY(s > 0);
}

void TstEmotion::testStatusLevelDisconnectedWarningVsDanger() {
    // 断连 + 正常显存 => Warning
    StatusLevel lvl1 = health::statusLevel(0, 50.0f, 40, 96.0f, 90.0f, false);
    QCOMPARE(lvl1, StatusLevel::Warning);

    // 断连 + 爆显存 => Danger 优先
    StatusLevel lvl2 = health::statusLevel(0, 98.0f, 40, 96.0f, 90.0f, false);
    QCOMPARE(lvl2, StatusLevel::Danger);
}

void TstEmotion::testStatusLevelTempBoundaryAtSeventyFive() {
    // temp = 75°C 仍为 Healthy
    StatusLevel lvlOk = health::statusLevel(85, 50.0f, 75, 96.0f, 90.0f, true);
    QCOMPARE(lvlOk, StatusLevel::Healthy);

    // temp = 76°C 触发 Warning
    StatusLevel lvlWarn = health::statusLevel(85, 50.0f, 76, 96.0f, 90.0f, true);
    QCOMPARE(lvlWarn, StatusLevel::Warning);
}

void TstEmotion::testStatusLevelScoreBelowFiftyAndEighty() {
    StatusLevel lvlWarn = health::statusLevel(79, 40.0f, 50, 96.0f, 90.0f, true);
    QCOMPARE(lvlWarn, StatusLevel::Warning);

    StatusLevel lvlDanger = health::statusLevel(49, 40.0f, 50, 96.0f, 90.0f, true);
    QCOMPARE(lvlDanger, StatusLevel::Danger);
}

QTEST_MAIN(TstEmotion)
#include "tst_emotion.moc"
