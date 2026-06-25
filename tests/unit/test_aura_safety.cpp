// test_aura_safety — M1-C7 BT-off safety guard.
// Drives a FakeTracker + real ProximityDetector to verify that a
// radioStateChanged(Off) signal mid-away-window disarms the away
// timer so no lockRequested fires while the radio is gone.
//
// AUTOMOC is enabled by CMake — the .moc include at the bottom resolves
// the Q_OBJECT inside FakeTracker. We compile plugin sources directly
// into the test exe because plugins don't dllexport their C++ symbols.

#include "plugins/aura_locker/BluetoothProximityTracker.h"
#include "plugins/aura_locker/ProximityDetector.h"
#include "plugins/aura_locker/RssiSmoother.h"

#include <QObject>
#include <QSignalSpy>
#include <QTest>

#include <cstdio>
#include <memory>

using namespace Margin::Plugins::Aura;

namespace {

class FakeTracker : public BluetoothProximityTracker {
    Q_OBJECT
public:
    RadioState m_state = RadioState::On;
    RadioState radioState() const override { return m_state; }
    void startMonitoring(const QString&) override {}
    void stopMonitoring() override {}
    bool isActive() const override { return true; }

    void emitSample(qint16 rssi) {
        RssiSample s;
        s.deviceId = QStringLiteral("AA:BB:CC:DD:EE:FF");
        s.rssiDbm = rssi;
        s.timestampMs = QDateTime::currentMSecsSinceEpoch();
        emit rssiSampled(s);
    }
    void emitRadio(RadioState st) {
        m_state = st;
        emit radioStateChanged(st);
    }
};

} // namespace

class TestAuraSafety : public QObject {
    Q_OBJECT

private slots:
    void radioOnLockFires();
    void radioOffMidAwaySuppressesLock();
    void radioOffThenOnResumesLocking();
};

void TestAuraSafety::radioOnLockFires() {
    fprintf(stderr, "[radioOnLockFires] start\n");
    FakeTracker tracker;
    ProximityDetector detector;
    detector.configure(/*threshold*/ -50, /*awayDelaySec*/ 1, /*cooldownSec*/ 60);
    detector.start();

    QObject::connect(&tracker, &BluetoothProximityTracker::rssiSampled,
                     &detector, [&](const RssiSample& s) { detector.onRssiSample(s.rssiDbm); });

    bool canLock = true;
    QObject context;
    QObject::connect(&tracker, &BluetoothProximityTracker::radioStateChanged,
                     &context, [&](RadioState s) {
        if (s == RadioState::Off) {
            canLock = false;
            detector.pause();
        } else if (s == RadioState::On) {
            canLock = true;
        }
    });

    QSignalSpy lockSpy(&detector, &ProximityDetector::lockRequested);
    tracker.emitSample(-90);  // below threshold → arm 1s away timer
    QTest::qWait(1200);       // let it elapse
    fprintf(stderr, "[radioOnLockFires] canLock=%d lockSpy=%d\n",
            canLock, int(lockSpy.count()));

    QCOMPARE(canLock, true);
    QCOMPARE(lockSpy.count(), 1);
}

void TestAuraSafety::radioOffMidAwaySuppressesLock() {
    fprintf(stderr, "[radioOffMidAwaySuppressesLock] start\n");
    FakeTracker tracker;
    ProximityDetector detector;
    detector.configure(/*threshold*/ -50, /*awayDelaySec*/ 1, /*cooldownSec*/ 60);
    detector.start();

    QObject::connect(&tracker, &BluetoothProximityTracker::rssiSampled,
                     &detector, [&](const RssiSample& s) { detector.onRssiSample(s.rssiDbm); });

    bool canLock = true;
    QObject context;
    QObject::connect(&tracker, &BluetoothProximityTracker::radioStateChanged,
                     &context, [&](RadioState s) {
        if (s == RadioState::Off) {
            canLock = false;
            detector.pause();
        } else if (s == RadioState::On) {
            canLock = true;
        }
    });

    QSignalSpy lockSpy(&detector, &ProximityDetector::lockRequested);
    tracker.emitSample(-90);   // arm 1s away timer
    QTest::qWait(50);          // 5% in
    tracker.emitRadio(RadioState::Off);  // pause → disarm timer
    QTest::qWait(1200);        // would have elapsed by now
    fprintf(stderr, "[radioOffMidAwaySuppressesLock] canLock=%d lockSpy=%d\n",
            canLock, int(lockSpy.count()));

    QCOMPARE(canLock, false);
    QCOMPARE(lockSpy.count(), 0);  // never fired
}

void TestAuraSafety::radioOffThenOnResumesLocking() {
    fprintf(stderr, "[radioOffThenOnResumesLocking] start\n");
    FakeTracker tracker;
    ProximityDetector detector;
    detector.configure(/*threshold*/ -50, /*awayDelaySec*/ 1, /*cooldownSec*/ 60);
    detector.start();

    QObject::connect(&tracker, &BluetoothProximityTracker::rssiSampled,
                     &detector, [&](const RssiSample& s) { detector.onRssiSample(s.rssiDbm); });

    bool canLock = true;
    QObject context;
    QObject::connect(&tracker, &BluetoothProximityTracker::radioStateChanged,
                     &context, [&](RadioState s) {
        if (s == RadioState::Off) {
            canLock = false;
            detector.pause();
        } else if (s == RadioState::On) {
            canLock = true;
            detector.resume();
        }
    });

    QSignalSpy lockSpy(&detector, &ProximityDetector::lockRequested);

    // Drive into away, kill the radio, wait, bring it back, drive into
    // away again — second cycle must fire lockRequested.
    tracker.emitSample(-90);
    QTest::qWait(50);
    tracker.emitRadio(RadioState::Off);
    QTest::qWait(200);
    fprintf(stderr, "[off->on] after off: lockSpy=%d\n", int(lockSpy.count()));
    QCOMPARE(lockSpy.count(), 0);

    tracker.emitRadio(RadioState::On);
    tracker.emitSample(-90);
    QTest::qWait(1200);
    fprintf(stderr, "[off->on] after on+away: canLock=%d lockSpy=%d\n",
            canLock, int(lockSpy.count()));

    QCOMPARE(canLock, true);
    QCOMPARE(lockSpy.count(), 1);
}

QTEST_MAIN(TestAuraSafety)
#include "test_aura_safety.moc"
