// tests/unit/test_proximity_detector.cpp
//
// M1 integration commit: ProximityDetector state machine. Pure logic
// (no BLE hardware, no EventBus). Synthetic RSSI samples + QSignalSpy
// drive the away → lock → cooldown round-trip.
//
// Cases:
//   - Paired state holds steady while samples stay above threshold
//   - awayDetected + lockRequested fire once after awayDelay below threshold
//   - Cooldown suppresses re-lock while the cooldown window is open
//   - After cooldown elapses, state returns to Paired
//   - pause() drops state to Inactive and ignores further samples
//   - Heartbeat fires when no sample arrives for 2× awayDelay (covers the
//     real-world case where the paired device stops broadcasting entirely
//     — e.g. smartwatch taken over by phone via BT Classic — so the
//     threshold path never sees a weak signal, only silence)

#include <QObject>
#include <QSignalSpy>
#include <QString>
#include <QTest>

#include "plugins/aura_locker/ProximityDetector.h"

using namespace Margin::Plugins::Aura;

namespace {
constexpr qint16 kThresh  = -65;
constexpr int    kAwaySec = 2;   // short for test; real default 30s
constexpr int    kCoolSec = 3;
} // namespace

class TestProximityDetector : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();

    void pairedStays_whenAboveThreshold();
    void awayTriggers_afterDelayBelowThreshold();
    void cooldownSuppressesRelock();
    void cooldownEnds_backToPaired();
    void pause_stopsStateMachine();
    void heartbeatFires_whenNoSampleArrives();
};

void TestProximityDetector::initTestCase() {
    // stateChanged is emitted cross-thread via AutoConnection in
    // production; register the metatype so QSignalSpy capture works.
    qRegisterMetaType<ProximityDetector::State>("ProximityDetector::State");
}

void TestProximityDetector::pairedStays_whenAboveThreshold() {
    ProximityDetector d;
    d.configure(kThresh, kAwaySec, kCoolSec);

    QSignalSpy lockSpy(&d, &ProximityDetector::lockRequested);
    QSignalSpy awaySpy(&d, &ProximityDetector::awayDetected);
    d.start();
    QCOMPARE(d.state(), ProximityDetector::State::Paired);

    // Pump above-threshold samples for longer than awayDelay. State must
    // stay Paired; no awayDetected, no lockRequested.
    for (int i = 0; i < 20; ++i) {
        d.onRssiSample(-50);
        QTest::qWait(150);
    }
    QCOMPARE(d.state(), ProximityDetector::State::Paired);
    QCOMPARE(awaySpy.count(), 0);
    QCOMPARE(lockSpy.count(), 0);
}

void TestProximityDetector::awayTriggers_afterDelayBelowThreshold() {
    ProximityDetector d;
    d.configure(kThresh, kAwaySec, kCoolSec);

    QSignalSpy lockSpy(&d, &ProximityDetector::lockRequested);
    QSignalSpy awaySpy(&d, &ProximityDetector::awayDetected);

    d.start();

    // Below threshold — arms awayTimer; once it elapses, awayDetected +
    // lockRequested fire exactly once each.
    for (int i = 0; i < 5; ++i) d.onRssiSample(-90);
    QTest::qWait((kAwaySec + 1) * 1000);

    QCOMPARE(d.state(), ProximityDetector::State::Away);
    QCOMPARE(awaySpy.count(), 1);
    QCOMPARE(lockSpy.count(), 1);
    QVERIFY(d.lastSmoothedDbm() < kThresh);
}

void TestProximityDetector::cooldownSuppressesRelock() {
    ProximityDetector d;
    d.configure(kThresh, kAwaySec, kCoolSec);

    QSignalSpy lockSpy(&d, &ProximityDetector::lockRequested);
    QSignalSpy backSpy(&d, &ProximityDetector::backDetected);

    d.start();
    // Drive into Away (lock #1).
    for (int i = 0; i < 5; ++i) d.onRssiSample(-90);
    QTest::qWait((kAwaySec + 1) * 1000);
    QCOMPARE(d.state(), ProximityDetector::State::Away);
    QCOMPARE(lockSpy.count(), 1);

    // Come back into range → Cooldown.
    for (int i = 0; i < 5; ++i) d.onRssiSample(-50);
    QTest::qWait(200);
    QCOMPARE(d.state(), ProximityDetector::State::Cooldown);
    QCOMPARE(backSpy.count(), 1);

    // While still inside the cooldown window, push below-threshold samples.
    // Cooldown state ignores samples entirely (awayTimer never arms), so
    // even after awayDelay elapses, no extra lock fires. Wait kCoolSec/2
    // (1.5s) — comfortably past awayDelay (2s... no, 1.5s < 2s; but we
    // have already pushed samples in Cooldown which don't arm the timer,
    // so even if the awayDelay would have elapsed, the timer was never
    // running). State stays Cooldown; lockSpy stays at 1.
    for (int i = 0; i < 10; ++i) d.onRssiSample(-90);
    QTest::qWait(kCoolSec * 1000 / 2);
    QCOMPARE(d.state(), ProximityDetector::State::Cooldown);
    QCOMPARE(lockSpy.count(), 1);
}

void TestProximityDetector::cooldownEnds_backToPaired() {
    ProximityDetector d;
    d.configure(kThresh, kAwaySec, kCoolSec);

    d.start();
    for (int i = 0; i < 5; ++i) d.onRssiSample(-90);
    QTest::qWait((kAwaySec + 1) * 1000);
    QCOMPARE(d.state(), ProximityDetector::State::Away);

    for (int i = 0; i < 5; ++i) d.onRssiSample(-50);
    QTest::qWait(200);
    QCOMPARE(d.state(), ProximityDetector::State::Cooldown);

    // Cooldown elapses → Paired.
    QTest::qWait((kCoolSec + 1) * 1000);
    QCOMPARE(d.state(), ProximityDetector::State::Paired);
}

void TestProximityDetector::pause_stopsStateMachine() {
    ProximityDetector d;
    d.configure(kThresh, kAwaySec, kCoolSec);

    QSignalSpy lockSpy(&d, &ProximityDetector::lockRequested);
    d.start();
    d.pause();
    QCOMPARE(d.state(), ProximityDetector::State::Inactive);

    // Paused: samples ignored, no lock fires.
    for (int i = 0; i < 5; ++i) d.onRssiSample(-90);
    QTest::qWait((kAwaySec + 1) * 1000);
    QCOMPARE(d.state(), ProximityDetector::State::Inactive);
    QCOMPARE(lockSpy.count(), 0);
}

void TestProximityDetector::heartbeatFires_whenNoSampleArrives() {
    // Reproduces the "watch got grabbed by phone over BT Classic, stopped
    // BLE advertising entirely" failure mode — the threshold path can't
    // see it because no sample ever arrives. The heartbeat timer must
    // fire awayDetected + lockRequested on its own.
    ProximityDetector d;
    d.configure(kThresh, kAwaySec, kCoolSec);

    QSignalSpy lockSpy(&d, &ProximityDetector::lockRequested);
    QSignalSpy awaySpy(&d, &ProximityDetector::awayDetected);

    d.start();

    // Silence. No samples. Wait past the heartbeat (2× kAwaySec = 4 s).
    QTest::qWait((kAwaySec * 2 + 1) * 1000);

    QCOMPARE(d.state(), ProximityDetector::State::Away);
    QCOMPARE(awaySpy.count(), 1);
    QCOMPARE(lockSpy.count(), 1);
}

QTEST_MAIN(TestProximityDetector)
#include "test_proximity_detector.moc"
