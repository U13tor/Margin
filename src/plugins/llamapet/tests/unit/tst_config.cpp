#include <QtTest>
#include "core/EngineConfig.h"
#include <cmath>

using namespace Margin::Plugins::LlamaPet;

class TstConfig : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testDefaultConfigValuesMatchSpec();
    void testValidateConfigAcceptsValidAndRejectsInvalids();
    void testJsonRoundtrip();
    void testLoadFromToml();
};

void TstConfig::testDefaultConfigValuesMatchSpec() {
    EngineConfig cfg;
    QCOMPARE(cfg.inference.engine, QStringLiteral("llama.cpp"));
    QCOMPARE(cfg.inference.endpointUrl, QStringLiteral("http://127.0.0.1:8080"));
    QCOMPARE(cfg.inference.pollIntervalMs, 1000);
    QVERIFY(std::abs(cfg.hardware.vramDangerThreshold - 0.96f) < 0.001f);
    QVERIFY(std::abs(cfg.hardware.vramWarnThreshold - 0.90f) < 0.001f);
    QCOMPARE(cfg.appearance.defaultMode, QStringLiteral("dock"));
    QVERIFY(cfg.appearance.alwaysOnTop);
    QVERIFY(!cfg.appearance.clickThrough);
    QCOMPARE(cfg.shortcuts.toggleClickThrough, QStringLiteral("Ctrl+Alt+P"));
}

void TstConfig::testValidateConfigAcceptsValidAndRejectsInvalids() {
    EngineConfig cfg;
    QVERIFY(EngineConfig::validate(cfg).isOk());

    // 非法引擎
    cfg.inference.engine = QStringLiteral("invalid_engine");
    QVERIFY(EngineConfig::validate(cfg).isErr());
    cfg.inference.engine = QStringLiteral("llama.cpp");

    // 非法 URL
    cfg.inference.endpointUrl = QStringLiteral("ftp://localhost:8080");
    QVERIFY(EngineConfig::validate(cfg).isErr());
    cfg.inference.endpointUrl = QStringLiteral("http://127.0.0.1:8080");

    // 轮询间隔超限
    cfg.inference.pollIntervalMs = 50;
    QVERIFY(EngineConfig::validate(cfg).isErr());
    cfg.inference.pollIntervalMs = 20000;
    QVERIFY(EngineConfig::validate(cfg).isErr());
    cfg.inference.pollIntervalMs = 1000;

    // 阈值倒挂 (warn >= danger)
    cfg.hardware.vramWarnThreshold = 0.98f;
    cfg.hardware.vramDangerThreshold = 0.95f;
    QVERIFY(EngineConfig::validate(cfg).isErr());
    cfg.hardware.vramWarnThreshold = 0.90f;
    cfg.hardware.vramDangerThreshold = 0.96f;

    // 禁用热键 Ctrl+Shift+P
    cfg.shortcuts.toggleClickThrough = QStringLiteral("Ctrl+Shift+P");
    QVERIFY(EngineConfig::validate(cfg).isErr());
    cfg.shortcuts.toggleClickThrough = QStringLiteral("ctrl + shift + p");
    QVERIFY(EngineConfig::validate(cfg).isErr());
}

void TstConfig::testJsonRoundtrip() {
    EngineConfig cfg;
    cfg.inference.engine = QStringLiteral("ollama");
    cfg.inference.endpointUrl = QStringLiteral("http://192.168.1.100:11434");
    cfg.inference.pollIntervalMs = 2000;
    cfg.hardware.vramDangerThreshold = 0.95f;
    cfg.hardware.vramWarnThreshold = 0.85f;

    const QJsonObject json = cfg.toJson();
    const EngineConfig restored = EngineConfig::fromJson(json);

    QCOMPARE(restored.inference.engine, cfg.inference.engine);
    QCOMPARE(restored.inference.endpointUrl, cfg.inference.endpointUrl);
    QCOMPARE(restored.inference.pollIntervalMs, cfg.inference.pollIntervalMs);
    QVERIFY(std::abs(restored.hardware.vramDangerThreshold - cfg.hardware.vramDangerThreshold) < 0.001f);
    QVERIFY(std::abs(restored.hardware.vramWarnThreshold - cfg.hardware.vramWarnThreshold) < 0.001f);
}

void TstConfig::testLoadFromToml() {
    QTemporaryFile tmp;
    QVERIFY(tmp.open());
    const QByteArray tomlContent =
        "[inference]\n"
        "engine = \"llama.cpp\"\n"
        "endpoint_url = \"http://127.0.0.1:1802\"\n"
        "api_key = \"EwZ9LYLL0oq4N6gRtKb8sXluRqnzmET8WsIcSaLsnQU\"\n"
        "poll_interval_ms = 1000\n\n"
        "[hardware]\n"
        "backend = \"auto\"\n"
        "gpu_index = 0\n"
        "vram_danger_threshold = 0.96\n"
        "vram_warn_threshold = 0.90\n\n"
        "[appearance]\n"
        "default_mode = \"dock\"\n"
        "always_on_top = true\n"
        "click_through = false\n"
        "auto_dock_hide = true\n\n"
        "[shortcuts]\n"
        "toggle_click_through = \"Ctrl+Alt+P\"\n";
    tmp.write(tomlContent);
    tmp.close();

    auto opt = EngineConfig::loadFromLlamaPetToml(tmp.fileName());
    QVERIFY(opt.has_value());
    const EngineConfig cfg = *opt;
    QCOMPARE(cfg.inference.endpointUrl, QStringLiteral("http://127.0.0.1:1802"));
    QCOMPARE(cfg.inference.apiKey, QStringLiteral("EwZ9LYLL0oq4N6gRtKb8sXluRqnzmET8WsIcSaLsnQU"));
    QCOMPARE(cfg.inference.pollIntervalMs, 1000);
    QCOMPARE(cfg.appearance.defaultMode, QStringLiteral("dock"));
    QVERIFY(cfg.appearance.alwaysOnTop);
    QCOMPARE(cfg.shortcuts.toggleClickThrough, QStringLiteral("Ctrl+Alt+P"));
}

QTEST_MAIN(TstConfig)
#include "tst_config.moc"
