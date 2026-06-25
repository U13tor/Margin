// tests/unit/test_rssi_smoother.cpp
//
// M1-C3: sliding-window RSSI averager. Pure algorithm — no Qt signals,
// no BLE hardware. Verifies empty/single/full-window/overflow/resize/reset
// behaviour per docs/11-roadmap.md M1-C3.

#include <QObject>
#include <QTest>

#include "plugins/aura_locker/RssiSmoother.h"

using Margin::Plugins::Aura::RssiSmoother;

class TestRssiSmoother : public QObject {
    Q_OBJECT

private slots:
    void testEmptyReturnsNullopt();
    void testSingleValue();
    void testFullWindow();
    void testOverflowDropsOldest();
    void testResizeShrinks();
    void testReset();
    void testNegativeWindowSizeIgnored();
};

void TestRssiSmoother::testEmptyReturnsNullopt() {
    RssiSmoother s;
    QVERIFY(!s.average().has_value());
    QCOMPARE(s.sampleCount(), 0);
    QCOMPARE(s.windowSize(), RssiSmoother::kDefaultWindowSize);
}

void TestRssiSmoother::testSingleValue() {
    RssiSmoother s;
    s.push(-60);
    QCOMPARE(s.sampleCount(), 1);
    QVERIFY(s.average().has_value());
    QCOMPARE(*s.average(), -60.0);
}

void TestRssiSmoother::testFullWindow() {
    RssiSmoother s(5);
    s.push(-60);
    s.push(-70);
    s.push(-65);
    s.push(-55);
    s.push(-50);
    QCOMPARE(s.sampleCount(), 5);
    double expected = (-60.0 + -70.0 + -65.0 + -55.0 + -50.0) / 5.0;
    QCOMPARE(*s.average(), expected);
}

void TestRssiSmoother::testOverflowDropsOldest() {
    RssiSmoother s(3);
    s.push(-60);
    s.push(-70);
    s.push(-65);
    s.push(-55);
    QCOMPARE(s.sampleCount(), 3);
    double expected = (-70.0 + -65.0 + -55.0) / 3.0;
    QCOMPARE(*s.average(), expected);
}

void TestRssiSmoother::testResizeShrinks() {
    RssiSmoother s(5);
    s.push(-60);
    s.push(-61);
    s.push(-62);
    s.push(-63);
    s.push(-64);
    s.setWindowSize(3);
    QCOMPARE(s.sampleCount(), 3);
    double expected = (-62.0 + -63.0 + -64.0) / 3.0;
    QCOMPARE(*s.average(), expected);
}

void TestRssiSmoother::testReset() {
    RssiSmoother s;
    s.push(-60);
    s.push(-65);
    s.reset();
    QCOMPARE(s.sampleCount(), 0);
    QVERIFY(!s.average().has_value());
}

void TestRssiSmoother::testNegativeWindowSizeIgnored() {
    RssiSmoother s(5);
    s.setWindowSize(-1);
    QCOMPARE(s.windowSize(), 5);
    s.setWindowSize(0);
    QCOMPARE(s.windowSize(), 5);
}

QTEST_MAIN(TestRssiSmoother)
#include "test_rssi_smoother.moc"
