#include <QtTest>
#include "core/SmiHal.h"
#include "core/HalProvider.h"
#include <cmath>

using namespace Margin::Plugins::LlamaPet;

class TstSmiParse : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testNormalLine();
    void testWhitespaceAndMissingUnits();
    void testNaValues();
    void testTooFewColumns();
    void testAutoHalSmoke();
};

void TstSmiParse::testNormalLine() {
    const QString sample = QStringLiteral("NVIDIA GeForce RTX 4090, 4096, 24576, 52, 120.50, 450.00, 32\n");
    auto res = parseSmiCsv(sample);
    QVERIFY(res.isOk());
    const GpuMetrics m = res.value();
    QCOMPARE(m.deviceName, QStringLiteral("NVIDIA GeForce RTX 4090"));
    QVERIFY(std::abs(m.vramUsedMb - 4096.0f) < 0.01f);
    QVERIFY(std::abs(m.vramTotalMb - 24576.0f) < 0.01f);
    QCOMPARE(m.tempC, 52u);
    QVERIFY(std::abs(m.powerW - 120.50f) < 0.01f);
    QVERIFY(std::abs(m.powerLimitW - 450.00f) < 0.01f);
    QCOMPARE(m.gpuUtil, 32u);
}

void TstSmiParse::testWhitespaceAndMissingUnits() {
    const QString sample = QStringLiteral("   NVIDIA RTX 4080 SUPER  ,  29232 , 32760 , 29 , 8.29 , 320.00 , 0  ");
    auto res = parseSmiCsv(sample);
    QVERIFY(res.isOk());
    const GpuMetrics m = res.value();
    QCOMPARE(m.deviceName, QStringLiteral("NVIDIA RTX 4080 SUPER"));
    QCOMPARE(m.tempC, 29u);
    QCOMPARE(m.gpuUtil, 0u);
}

void TstSmiParse::testNaValues() {
    const QString sample = QStringLiteral("Tesla T4, N/A, 15360, [N/A], -5.0, 70.0, [Not Supported]");
    auto res = parseSmiCsv(sample);
    QVERIFY(res.isOk());
    const GpuMetrics m = res.value();
    QCOMPARE(m.deviceName, QStringLiteral("Tesla T4"));
    QCOMPARE(m.vramUsedMb, 0.0f);
    QCOMPARE(m.tempC, 0u);
    QCOMPARE(m.powerW, 0.0f);
    QCOMPARE(m.gpuUtil, 0u);
}

void TstSmiParse::testTooFewColumns() {
    const QString sample = QStringLiteral("GeForce RTX, 1024, 8192");
    auto res = parseSmiCsv(sample);
    QVERIFY(res.isErr());
}

void TstSmiParse::testAutoHalSmoke() {
    EngineConfig::Hardware cfg;
    cfg.backend = QStringLiteral("auto");
    cfg.gpuIndex = 0;
    auto hal = createHal(cfg);
    if (!hal) {
        QSKIP("No NVIDIA GPU available on this system");
    }
    QVERIFY(hal != nullptr);
    auto pollRes = hal->pollMetrics();
    QVERIFY2(pollRes.isOk(), qPrintable(pollRes.error()));
    const GpuMetrics m = pollRes.value();
    QVERIFY(!m.deviceName.isEmpty());
    QVERIFY(m.vramTotalMb > 0.0f);
}

QTEST_MAIN(TstSmiParse)
#include "tst_smi_parse.moc"
