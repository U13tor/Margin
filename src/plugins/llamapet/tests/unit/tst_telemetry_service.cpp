#include <QtTest>
#include <QSignalSpy>
#include "app/TelemetryService.h"

using namespace Margin::Plugins::LlamaPet;

namespace {

class MockHal : public HalProvider {
public:
    const char* providerName() const override { return "mock-hal"; }
    Margin::Result<GpuMetrics, QString> pollMetrics() override {
        return Margin::Result<GpuMetrics, QString>::ok(metrics);
    }
    GpuMetrics metrics;
};

class MockIt : public ItProvider {
public:
    const char* engineName() const override { return "mock-it"; }
    LlmTelemetry pollTelemetry() override {
        return telemetry;
    }
    LlmTelemetry telemetry;
};

} // namespace

class TstTelemetryService : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testInitialTickAndProperties();
    void testOfflineHysteresis();
    void testIdleTicksAndSleeping();
    void testHappyCacheTransition();
    void testJsonPayloadContent();
};

void TstTelemetryService::testInitialTickAndProperties() {
    TelemetryService service;
    auto hal = std::make_unique<MockHal>();
    hal->metrics.deviceName = QStringLiteral("Mock RTX 4090");
    hal->metrics.vramUsedMb = 8192.0f;
    hal->metrics.vramTotalMb = 24576.0f;
    hal->metrics.tempC = 55;
    hal->metrics.powerW = 150.0f;
    hal->metrics.powerLimitW = 450.0f;
    hal->metrics.gpuUtil = 30;
    service.setHalProvider(std::move(hal));

    auto it = std::make_unique<MockIt>();
    it->telemetry.engine = "llama.cpp";
    it->telemetry.connected = true;
    service.setItProvider(std::move(it));

    QSignalSpy changeSpy(&service, &TelemetryService::telemetryChanged);
    QSignalSpy jsonSpy(&service, &TelemetryService::telemetryPayload);

    service.start(1000);
    QVERIFY(service.isRunning());
    QCOMPARE(changeSpy.count(), 1);
    QCOMPARE(jsonSpy.count(), 1);

    QCOMPARE(service.deviceName(), QStringLiteral("Mock RTX 4090"));
    QCOMPARE(service.tempC(), 55u);
    QVERIFY(service.connected());
    QCOMPARE(service.emotion(), static_cast<int>(Emotion::Idle));
    QCOMPARE(service.status(), static_cast<int>(StatusLevel::Healthy));
    QVERIFY(service.healthScore() >= 80);
    QCOMPARE(service.dialogText(), QStringLiteral("随时准备推理！"));

    service.stop();
    QVERIFY(!service.isRunning());
}

void TstTelemetryService::testOfflineHysteresis() {
    TelemetryService service;
    auto hal = std::make_unique<MockHal>();
    hal->metrics.vramUsedMb = 4096.0f;
    hal->metrics.vramTotalMb = 16384.0f;
    service.setHalProvider(std::move(hal));

    auto it = std::make_unique<MockIt>();
    it->telemetry.engine = "llama.cpp";
    it->telemetry.connected = false;
    MockIt* itPtr = it.get();
    service.setItProvider(std::move(it));

    // 周期 1：单次断连仍在防抖期，输出 Idle
    service.tick();
    QCOMPARE(service.emotion(), static_cast<int>(Emotion::Idle));

    // 周期 2：连续 2 次断连，进入 ConfusedOffline
    service.tick();
    QCOMPARE(service.emotion(), static_cast<int>(Emotion::ConfusedOffline));
    QCOMPARE(service.status(), static_cast<int>(StatusLevel::Warning));
    QCOMPARE(service.healthScore(), 0u);
    QCOMPARE(service.dialogText(), QStringLiteral("未检测到本地服务"));

    // 周期 3：重新连通，立即退出离线态回到 Idle
    itPtr->telemetry.connected = true;
    service.tick();
    QCOMPARE(service.emotion(), static_cast<int>(Emotion::Idle));
    QVERIFY(service.healthScore() > 0);
}

void TstTelemetryService::testIdleTicksAndSleeping() {
    TelemetryService service;
    auto it = std::make_unique<MockIt>();
    it->telemetry.connected = true;
    MockIt* itPtr = it.get();
    service.setItProvider(std::move(it));

    service.tick();
    QCOMPARE(service.idleTicks(), 1u);
    QCOMPARE(service.emotion(), static_cast<int>(Emotion::Idle));

    // 快速累加到 300 ticks
    for (int i = 0; i < 298; ++i) {
        service.tick();
    }
    QCOMPARE(service.idleTicks(), 299u);
    QCOMPARE(service.emotion(), static_cast<int>(Emotion::Idle));

    // 到达 300 ticks，进入 Sleeping
    service.tick();
    QCOMPARE(service.idleTicks(), 300u);
    QCOMPARE(service.emotion(), static_cast<int>(Emotion::Sleeping));
    QCOMPARE(service.dialogText(), QStringLiteral("呼.. 模型浅睡中"));

    // 新请求唤醒：anyPrefill = true
    itPtr->telemetry.anyPrefill = true;
    service.tick();
    QCOMPARE(service.idleTicks(), 0u);
    QCOMPARE(service.emotion(), static_cast<int>(Emotion::EatingPrefill));
    QCOMPARE(service.dialogText(), QStringLiteral("正在吞入代码..."));
}

void TstTelemetryService::testHappyCacheTransition() {
    TelemetryService service;
    auto it = std::make_unique<MockIt>();
    it->telemetry.connected = true;
    it->telemetry.anyDecoding = true;
    it->telemetry.cacheHitRatePct = 75.0f;
    it->telemetry.currentTps = 42.0f;
    service.setItProvider(std::move(it));

    // 周期 1：虽然 >=70%，但第 1 周期为 SpittingTokens
    service.tick();
    QCOMPARE(service.emotion(), static_cast<int>(Emotion::SpittingTokens));
    QCOMPARE(service.dialogText(), QStringLiteral("全力生成中 (42.0 t/s)"));

    // 周期 2：持续第 2 周期进入 HappyCache
    service.tick();
    QCOMPARE(service.emotion(), static_cast<int>(Emotion::HappyCache));
    QCOMPARE(service.dialogText(), QStringLiteral("✨ KV 缓存复用达成！"));
}

void TstTelemetryService::testJsonPayloadContent() {
    TelemetryService service;
    auto hal = std::make_unique<MockHal>();
    hal->metrics.deviceName = QStringLiteral("NVIDIA RTX 4080");
    hal->metrics.vramUsedMb = 12000.0f;
    hal->metrics.vramTotalMb = 16384.0f;
    service.setHalProvider(std::move(hal));

    auto it = std::make_unique<MockIt>();
    it->telemetry.engine = "llama.cpp";
    it->telemetry.connected = true;
    it->telemetry.activeSlots = 2u;
    it->telemetry.totalSlots = 4u;
    it->telemetry.currentTps = 28.5f;
    it->telemetry.cacheHitRatePct = 68.0f;
    service.setItProvider(std::move(it));

    service.tick();

    const QJsonObject json = service.currentPayloadJson();
    QVERIFY(json.contains(QStringLiteral("timestamp")));
    QVERIFY(json.contains(QStringLiteral("health_score")));
    QVERIFY(json.contains(QStringLiteral("status")));
    QVERIFY(json.contains(QStringLiteral("emotion")));
    QVERIFY(json.contains(QStringLiteral("dialog_text")));
    QVERIFY(json.contains(QStringLiteral("hw")));
    QVERIFY(json.contains(QStringLiteral("inf")));

    const QJsonObject hw = json[QStringLiteral("hw")].toObject();
    QCOMPARE(hw[QStringLiteral("device_name")].toString(), QStringLiteral("NVIDIA RTX 4080"));

    const QJsonObject inf = json[QStringLiteral("inf")].toObject();
    QCOMPARE(inf[QStringLiteral("engine")].toString(), QStringLiteral("llama.cpp"));
    QCOMPARE(inf[QStringLiteral("active_slots")].toInt(), 2);
    QCOMPARE(inf[QStringLiteral("total_slots")].toInt(), 4);
    QVERIFY(inf[QStringLiteral("connected")].toBool());
}

QTEST_MAIN(TstTelemetryService)
#include "tst_telemetry_service.moc"
