// tests/unit/test_idle_detector.cpp
//
// Validates IdleDetectorWin contract without depending on real key/mouse
// input (which CI runners can't synthesize). What we CAN verify:
//   1. startMonitoring(thresholdMs) installs both LL hooks + isActive() true.
//   2. stopMonitoring unhooks + isActive() false.
//   3. Double start is idempotent (just updates threshold + resets timer).
//   4. stop-before-start is safe.
//   5. Threshold getter/setter round-trip.
//   6. Idle timer fires naturally (no input → emit userIdleStateChanged(true)).
//      Uses a short threshold (50 ms) so the test runs fast.
//
// What we DO NOT verify here: real key/mouse → onInputEvent() →
// userIdleStateChanged(false) edge. That's a manual L4 test —
// see tests/manual/M2_ACCEPTANCE.md.

#include <QObject>
#include <QSignalSpy>
#include <QTest>

#include "host/platform/windows/IdleDetectorWin.h"

using Margin::IdleDetectorWin;

class TestIdleDetector : public QObject {
    Q_OBJECT

private slots:
    void testStartStopMonitoring();
    void testDoubleStartIsIdempotent();
    void testDoubleStopIsSafe();
    void testThresholdGetterSetter();
    void testIdleTimerFiresNaturally();
};

void TestIdleDetector::testStartStopMonitoring() {
    IdleDetectorWin det;
    QVERIFY2(!det.isActive(), "Fresh detector must not be active");

    QVERIFY2(det.startMonitoring(60000),
             "SetWindowsHookExW failed — see GetLastError()");
    QVERIFY(det.isActive());

    det.stopMonitoring();
    QVERIFY2(!det.isActive(), "stopMonitoring must clear isActive");
}

void TestIdleDetector::testDoubleStartIsIdempotent() {
    IdleDetectorWin det;
    QVERIFY(det.startMonitoring(60000));
    QVERIFY(det.startMonitoring(30000));  // no-op, just updates threshold
    QVERIFY(det.isActive());
    QCOMPARE(det.idleThresholdMs(), 30000);

    det.stopMonitoring();
}

void TestIdleDetector::testDoubleStopIsSafe() {
    IdleDetectorWin det;
    det.stopMonitoring();  // before start — must not crash
    QVERIFY(!det.isActive());

    QVERIFY(det.startMonitoring(60000));
    det.stopMonitoring();
    det.stopMonitoring();  // second stop — also safe
    QVERIFY(!det.isActive());
}

void TestIdleDetector::testThresholdGetterSetter() {
    IdleDetectorWin det;
    det.startMonitoring(60000);
    QCOMPARE(det.idleThresholdMs(), 60000);

    det.setIdleThresholdMs(120000);
    QCOMPARE(det.idleThresholdMs(), 120000);

    // Invalid threshold (0) is ignored, current value preserved.
    det.setIdleThresholdMs(0);
    QCOMPARE(det.idleThresholdMs(), 120000);

    det.stopMonitoring();
}

void TestIdleDetector::testIdleTimerFiresNaturally() {
    IdleDetectorWin det;
    QVERIFY(det.startMonitoring(50));  // 50ms threshold

    QVERIFY(!det.isUserIdle());
    QSignalSpy idleSpy(&det, &IdleDetectorWin::userIdleStateChanged);

    // Wait up to 500ms for the timer to fire. Under CI load 50ms can
    // easily take longer; the spy counts only matching emissions.
    QTest::qWait(500);

    // Expect at least one emission with arg=true (the active→idle edge).
    QVERIFY2(idleSpy.count() >= 1, "Idle timer should have fired by now");
    QVERIFY(det.isUserIdle());

    const QVariant firstArg = idleSpy.takeFirst().at(0);
    QCOMPARE(firstArg.toBool(), true);  // first edge is active→idle

    det.stopMonitoring();
}

QTEST_MAIN(TestIdleDetector)
#include "test_idle_detector.moc"
