// tests/unit/test_bluetooth_proximity_tracker.cpp
//
// M1-C1+C2: passive BLE tracker abstraction. Real hardware isn't
// available in CI, so this exercises the contract via a fake impl
// (subclass + manual signal emit) and the platform factory on Win.
//
//   - BluetoothProximityTracker interface is implementable via MI
//   - rssiSampled / radioStateChanged signals emit and QSignalSpy captures them
//   - create() returns non-null on Win (factory picks the WinRT impl)
//
// The WinRT impl itself (BluetoothLEAdvertisementTrackerWin) is not
// started here — startMonitoring would invoke real BLE hardware and
// is exercised in tests/manual/M1_ACCEPTANCE.md instead.

#include <QObject>
#include <QSignalSpy>
#include <QString>
#include <QTest>

#include <memory>

#include "plugins/aura_locker/BluetoothProximityTracker.h"

using namespace Margin::Plugins::Aura;

namespace {

class FakeTracker : public BluetoothProximityTracker {
    Q_OBJECT
public:
    void startMonitoring(const QString&) override { m_active = true; }
    void stopMonitoring() override { m_active = false; }
    bool isActive() const override { return m_active; }
    RadioState radioState() const override { return m_radioState; }

    // Test-only helpers to drive the same signals the real impl emits.
    void emitSample(const RssiSample& s) { emit rssiSampled(s); }
    void emitRadio(RadioState s) { emit radioStateChanged(s); }

    bool m_active = false;
    RadioState m_radioState = RadioState::Unknown;
};

} // namespace

class TestBluetoothProximityTracker : public QObject {
    Q_OBJECT

private slots:
    void fakeTrackerEmitsSample();
    void fakeTrackerEmitsRadioState();
    void fakeTrackerActiveToggle();
    void factoryReturnsImplOnWin();
};

void TestBluetoothProximityTracker::fakeTrackerEmitsSample() {
    FakeTracker t;
    QSignalSpy spy(&t, &BluetoothProximityTracker::rssiSampled);
    QCOMPARE(spy.count(), 0);

    RssiSample s;
    s.deviceId = QStringLiteral("AABBCCDDEEFF");
    s.advertisedName = QStringLiteral("iPhone");
    s.rssiDbm = -65;
    s.timestampMs = 1718000000000;

    t.emitSample(s);
    QCOMPARE(spy.count(), 1);

    const auto args = spy.takeFirst();
    QCOMPARE(args.at(0).value<RssiSample>().deviceId,        s.deviceId);
    QCOMPARE(args.at(0).value<RssiSample>().advertisedName,  s.advertisedName);
    QCOMPARE(args.at(0).value<RssiSample>().rssiDbm,         s.rssiDbm);
    QCOMPARE(args.at(0).value<RssiSample>().timestampMs,     s.timestampMs);
}

void TestBluetoothProximityTracker::fakeTrackerEmitsRadioState() {
    FakeTracker t;
    QSignalSpy spy(&t, &BluetoothProximityTracker::radioStateChanged);

    t.emitRadio(RadioState::Off);
    t.emitRadio(RadioState::On);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(0).at(0).value<RadioState>(), RadioState::Off);
    QCOMPARE(spy.at(1).at(0).value<RadioState>(), RadioState::On);
}

void TestBluetoothProximityTracker::fakeTrackerActiveToggle() {
    FakeTracker t;
    QVERIFY(!t.isActive());
    t.startMonitoring(QString{});
    QVERIFY(t.isActive());
    t.stopMonitoring();
    QVERIFY(!t.isActive());
}

void TestBluetoothProximityTracker::factoryReturnsImplOnWin() {
    // Force the metatype registry — Qt::QueuedConnection across threads
    // requires these to be registered before the first cross-thread emit.
    // On same-thread tests the registration is still required for
    // QSignalSpy's QVariant capture.
    qRegisterMetaType<RssiSample>("RssiSample");
    qRegisterMetaType<RadioState>("RadioState");

    auto tracker = BluetoothProximityTracker::create();
#ifdef _WIN32
    QVERIFY(tracker);
    QCOMPARE(tracker->radioState(), RadioState::Unknown);
    QVERIFY(!tracker->isActive());
#else
    QVERIFY(!tracker);
#endif
}

QTEST_MAIN(TestBluetoothProximityTracker)
#include "test_bluetooth_proximity_tracker.moc"
