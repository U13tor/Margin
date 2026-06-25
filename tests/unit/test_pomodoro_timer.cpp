// tests/unit/test_pomodoro_timer.cpp
//
// M3-C1: PomodoroTimer state machine — pure logic, no HostServices / no BLE
// hardware. Mirrors the test_rssi_smoother / test_proximity_detector pattern
// (compile the source directly into the test exe; plugins don't dllexport).
//
// Covers:
//   - happy-path state transitions: Idle → Working → BreakDue → BreakActive → Idle
//   - clamp helpers (loadSettings path SSOT)
//   - postponeBreak counter + cap
//   - skipBreak doesn't count as completed
//   - advance(seconds) drives the tick accurately

#include <QObject>
#include <QSignalSpy>
#include <QString>
#include <QTest>

#include "plugins/rhythm/PomodoroTimer.h"

using Margin::Plugins::Rhythm::PomodoroTimer;

class TestPomodoroTimer : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void initialStateIsIdle();
    void startTransitionsToWorking();
    void workingToBreakDueAfterWorkMinutes();
    void clampHelpersRoundTrip();
    void setWorkMinutesMidRunSnapsRemaining();
    void startBreakTransitionsToBreakActive();
    void breakActiveCompletesToIdle();
    void postponeBreakDecrementsCounter();
    void postponeBreakRefusesAtZero();
    void skipBreakDoesNotCountAsCompleted();
    void stopFromAnyStateGoesIdle();
    void startResetsPostponesCounter();
    void setPausedHaltsCountdown();
    void setUnpausedResumesCountdown();
    void setPausedIsIdempotent();
    void endBreakEarlyFromBreakActiveGoesIdle();
    void endBreakEarlyNoOpOutsideBreakActive();
    void postponeBudgetOfThreeReachesZero();
    void targetRoundsDefaultIsFive();
    void targetRoundsClampsToRange();
    void targetRoundsEmitsChangedOnSet();
};

void TestPomodoroTimer::initTestCase() {
    // Sanity: defaults match the spec's "45 min work, 5 min break".
    PomodoroTimer t;
    QCOMPARE(t.workMinutes(),    PomodoroTimer::kDefaultWorkMinutes);
    QCOMPARE(t.breakMinutes(),   PomodoroTimer::kDefaultBreakMinutes);
    QCOMPARE(t.maxPostpones(),   PomodoroTimer::kDefaultMaxPostpones);
    QCOMPARE(t.targetRounds(),   PomodoroTimer::kDefaultTargetRounds);
}

void TestPomodoroTimer::initialStateIsIdle() {
    PomodoroTimer t;
    QCOMPARE(t.state(),          PomodoroTimer::State::Idle);
    QCOMPARE(t.stateString(),    QStringLiteral("idle"));
    QCOMPARE(t.remainingSeconds(), 0);
    QCOMPARE(t.breaksCompleted(),   0);
}

void TestPomodoroTimer::startTransitionsToWorking() {
    PomodoroTimer t;
    QSignalSpy stateSpy(&t, &PomodoroTimer::stateChanged);
    QSignalSpy remainingSpy(&t, &PomodoroTimer::remainingChanged);

    t.start();

    QCOMPARE(t.state(),          PomodoroTimer::State::Working);
    QCOMPARE(stateSpy.count(),   1);
    QCOMPARE(t.remainingSeconds(), t.workMinutes() * 60);
    QVERIFY(remainingSpy.count() >= 1);
}

void TestPomodoroTimer::workingToBreakDueAfterWorkMinutes() {
    PomodoroTimer t;
    t.setWorkMinutes(1);  // 60s for test speed
    QCOMPARE(t.workMinutes(), 1);
    t.start();

    QSignalSpy breakDueSpy(&t, &PomodoroTimer::breakDue);
    QSignalSpy stateSpy(&t, &PomodoroTimer::stateChanged);

    // Advance one second short — should still be Working.
    t.advance(59);
    QCOMPARE(t.state(), PomodoroTimer::State::Working);
    QCOMPARE(breakDueSpy.count(), 0);

    // Final tick flips to BreakDue.
    t.advance(1);
    QCOMPARE(t.state(),        PomodoroTimer::State::BreakDue);
    QCOMPARE(breakDueSpy.count(), 1);
    QCOMPARE(t.remainingSeconds(), 0);
}

void TestPomodoroTimer::clampHelpersRoundTrip() {
    // Below floor snaps to floor.
    QCOMPARE(PomodoroTimer::clampWorkMinutes(0),  PomodoroTimer::kMinWorkMinutes);
    QCOMPARE(PomodoroTimer::clampBreakMinutes(0), PomodoroTimer::kMinBreakMinutes);
    QCOMPARE(PomodoroTimer::clampMaxPostpones(-1), PomodoroTimer::kMinPostpones);
    QCOMPARE(PomodoroTimer::clampTargetRounds(0), PomodoroTimer::kMinTargetRounds);
    // Above ceiling snaps to ceiling.
    QCOMPARE(PomodoroTimer::clampWorkMinutes(9999),  PomodoroTimer::kMaxWorkMinutes);
    QCOMPARE(PomodoroTimer::clampBreakMinutes(9999), PomodoroTimer::kMaxBreakMinutes);
    QCOMPARE(PomodoroTimer::clampMaxPostpones(9999), PomodoroTimer::kMaxPostpones);
    QCOMPARE(PomodoroTimer::clampTargetRounds(9999), PomodoroTimer::kMaxTargetRounds);
    // In-range passes through.
    QCOMPARE(PomodoroTimer::clampWorkMinutes(25),  25);
    QCOMPARE(PomodoroTimer::clampBreakMinutes(10), 10);
    QCOMPARE(PomodoroTimer::clampMaxPostpones(5),  5);
    QCOMPARE(PomodoroTimer::clampTargetRounds(8),  8);
}

void TestPomodoroTimer::setWorkMinutesMidRunSnapsRemaining() {
    PomodoroTimer t;
    t.setWorkMinutes(45);
    t.start();
    QCOMPARE(t.remainingSeconds(), 45 * 60);

    // Shorten work to 30 minutes — remaining should snap to the new cap.
    t.setWorkMinutes(30);
    QCOMPARE(t.remainingSeconds(), 30 * 60);
}

void TestPomodoroTimer::startBreakTransitionsToBreakActive() {
    PomodoroTimer t;
    t.setWorkMinutes(1);
    t.setBreakMinutes(1);
    t.start();
    t.advance(60);  // → BreakDue

    QSignalSpy startedSpy(&t, &PomodoroTimer::breakStarted);
    t.startBreak();
    QCOMPARE(t.state(),          PomodoroTimer::State::BreakActive);
    QCOMPARE(startedSpy.count(), 1);
    QCOMPARE(t.remainingSeconds(), t.breakMinutes() * 60);
}

void TestPomodoroTimer::breakActiveCompletesToIdle() {
    PomodoroTimer t;
    t.setWorkMinutes(1);
    t.setBreakMinutes(1);
    t.start();
    t.advance(60);     // → BreakDue
    t.startBreak();    // → BreakActive

    QSignalSpy endedSpy(&t, &PomodoroTimer::breakEnded);
    QSignalSpy completedSpy(&t, &PomodoroTimer::breaksCompletedChanged);

    t.advance(60);     // → Idle (natural completion)
    QCOMPARE(t.state(),          PomodoroTimer::State::Idle);
    QCOMPARE(endedSpy.count(),   1);
    QCOMPARE(completedSpy.count(), 1);
    QCOMPARE(t.breaksCompleted(), 1);
}

void TestPomodoroTimer::postponeBreakDecrementsCounter() {
    PomodoroTimer t;
    t.setWorkMinutes(1);
    t.start();
    t.advance(60);  // → BreakDue

    QSignalSpy postponedSpy(&t, &PomodoroTimer::postponed);
    const int before = t.postponesRemaining();
    t.postponeBreak();

    QCOMPARE(postponedSpy.count(), 1);
    QCOMPARE(t.postponesRemaining(), before - 1);
    QCOMPARE(t.state(), PomodoroTimer::State::Working);
}

void TestPomodoroTimer::postponeBreakRefusesAtZero() {
    PomodoroTimer t;
    t.setMaxPostpones(1);
    t.setWorkMinutes(1);
    t.start();
    t.advance(60);  // → BreakDue

    t.postponeBreak();  // → Working, postpones 0
    QCOMPARE(t.postponesRemaining(), 0);
    t.advance(60);     // → BreakDue again

    QSignalSpy spy(&t, &PomodoroTimer::postponed);
    t.postponeBreak();  // refused — stuck at BreakDue
    QCOMPARE(spy.count(), 0);
    QCOMPARE(t.state(), PomodoroTimer::State::BreakDue);
}

void TestPomodoroTimer::skipBreakDoesNotCountAsCompleted() {
    PomodoroTimer t;
    t.setWorkMinutes(1);
    t.start();
    t.advance(60);  // → BreakDue

    QSignalSpy skippedSpy(&t, &PomodoroTimer::skipped);
    t.skipBreak();
    QCOMPARE(skippedSpy.count(), 1);
    QCOMPARE(t.state(),          PomodoroTimer::State::Idle);
    QCOMPARE(t.breaksCompleted(), 0);
}

void TestPomodoroTimer::stopFromAnyStateGoesIdle() {
    PomodoroTimer t;
    t.setWorkMinutes(45);
    t.start();
    QCOMPARE(t.state(), PomodoroTimer::State::Working);

    t.stop();
    QCOMPARE(t.state(),          PomodoroTimer::State::Idle);
    QCOMPARE(t.remainingSeconds(), 0);

    // Stopping from Idle is a no-op (no signal noise).
    QSignalSpy spy(&t, &PomodoroTimer::stateChanged);
    t.stop();
    QCOMPARE(spy.count(), 0);
}

void TestPomodoroTimer::startResetsPostponesCounter() {
    PomodoroTimer t;
    t.setMaxPostpones(2);
    t.setWorkMinutes(1);
    t.start();
    t.advance(60);          // → BreakDue
    t.postponeBreak();      // postpones 2 → 1
    QCOMPARE(t.postponesRemaining(), 1);
    t.stop();               // → Idle
    t.start();              // fresh session → fresh budget
    QCOMPARE(t.postponesRemaining(), 2);
}

void TestPomodoroTimer::setPausedHaltsCountdown() {
    // M3-C2: pause via setPaused (called by InputMonitor::userIdleStateChanged).
    // advance() must not decrement while paused.
    PomodoroTimer t;
    t.setWorkMinutes(1);
    t.start();
    QCOMPARE(t.remainingSeconds(), 60);
    t.advance(10);
    QCOMPARE(t.remainingSeconds(), 50);

    QSignalSpy spy(&t, &PomodoroTimer::pausedChanged);
    t.setPaused(true);
    QCOMPARE(spy.count(),    1);
    QCOMPARE(t.paused(),     true);

    t.advance(40);  // would normally eat 40s — paused means no-op
    QCOMPARE(t.remainingSeconds(), 50);
}

void TestPomodoroTimer::setUnpausedResumesCountdown() {
    PomodoroTimer t;
    t.setWorkMinutes(1);
    t.start();
    t.advance(10);              // 60 → 50
    t.setPaused(true);
    t.advance(40);              // no-op
    QCOMPARE(t.remainingSeconds(), 50);

    t.setPaused(false);
    QCOMPARE(t.paused(), false);

    t.advance(10);              // 50 → 40
    QCOMPARE(t.remainingSeconds(), 40);
}

void TestPomodoroTimer::setPausedIsIdempotent() {
    // Setting the same value twice must not emit pausedChanged — QML would
    // otherwise get phantom change notifications.
    PomodoroTimer t;
    QCOMPARE(t.paused(), false);

    QSignalSpy spy(&t, &PomodoroTimer::pausedChanged);
    t.setPaused(false);
    QCOMPARE(spy.count(), 0);

    t.setPaused(true);
    QCOMPARE(spy.count(), 1);

    t.setPaused(true);
    QCOMPARE(spy.count(), 1);
}

void TestPomodoroTimer::endBreakEarlyFromBreakActiveGoesIdle() {
    // A1/B2: the overlay's 跳过 / Esc must end an active break. skipBreak()
    // guards BreakDue and is a no-op during BreakActive, so the overlay uses
    // endBreakEarly(). It must NOT count as a completed break.
    PomodoroTimer t;
    t.setWorkMinutes(1);
    t.setBreakMinutes(1);
    t.start();
    t.advance(60);     // → BreakDue
    t.startBreak();    // → BreakActive
    QCOMPARE(t.state(), PomodoroTimer::State::BreakActive);

    QSignalSpy skippedSpy(&t, &PomodoroTimer::skipped);
    QSignalSpy completedSpy(&t, &PomodoroTimer::breaksCompletedChanged);
    t.endBreakEarly();

    QCOMPARE(t.state(),            PomodoroTimer::State::Idle);
    QCOMPARE(skippedSpy.count(),   1);
    QCOMPARE(completedSpy.count(), 0);
    QCOMPARE(t.breaksCompleted(),  0);
}

void TestPomodoroTimer::endBreakEarlyNoOpOutsideBreakActive() {
    PomodoroTimer t;
    t.setWorkMinutes(1);
    t.start();                 // Working

    QSignalSpy workingSpy(&t, &PomodoroTimer::stateChanged);
    t.endBreakEarly();         // no-op — only BreakActive exits this way
    QCOMPARE(workingSpy.count(), 0);
    QCOMPARE(t.state(), PomodoroTimer::State::Working);

    t.advance(60);             // → BreakDue
    QSignalSpy dueSpy(&t, &PomodoroTimer::stateChanged);
    t.endBreakEarly();         // still no-op in BreakDue
    QCOMPARE(dueSpy.count(), 0);
    QCOMPARE(t.state(), PomodoroTimer::State::BreakDue);
}

void TestPomodoroTimer::postponeBudgetOfThreeReachesZero() {
    // DoD #4: postpone up to 3 times, then the budget is gone and the next
    // postpone is refused (UI then offers only 开始做操). Each postpone must
    // consume exactly ONE credit (B1: the plugin's window double-decrement is
    // a separate bug; the budget math itself lives here and is one-per-call).
    PomodoroTimer t;
    QCOMPARE(t.maxPostpones(), 3);
    t.setWorkMinutes(1);
    t.start();

    for (int remaining = 3; remaining > 0; --remaining) {
        t.advance(60);                                  // → BreakDue
        QCOMPARE(t.state(), PomodoroTimer::State::BreakDue);
        QCOMPARE(t.postponesRemaining(), remaining);
        t.postponeBreak();                              // spend one credit
        QCOMPARE(t.postponesRemaining(), remaining - 1);
        QCOMPARE(t.state(), PomodoroTimer::State::Working);
    }
    QCOMPARE(t.postponesRemaining(), 0);

    t.advance(60);                                      // → BreakDue, exhausted
    QSignalSpy postponedSpy(&t, &PomodoroTimer::postponed);
    t.postponeBreak();                                  // refused
    QCOMPARE(postponedSpy.count(), 0);
    QCOMPARE(t.state(), PomodoroTimer::State::BreakDue);
}

void TestPomodoroTimer::targetRoundsDefaultIsFive() {
    // M4-C12: "#N/M" header uses targetRounds as M. Default 5 = typical
    // work-day budget per design SSOT (docs/06 §4.4).
    PomodoroTimer t;
    QCOMPARE(t.targetRounds(), 5);
    QCOMPARE(t.targetRounds(), PomodoroTimer::kDefaultTargetRounds);
}

void TestPomodoroTimer::targetRoundsClampsToRange() {
    // Out-of-range values from a hand-edited settings.json must clamp via
    // the same SSOT as the QML SpinBox floor/ceiling.
    PomodoroTimer t;
    t.setTargetRounds(0);
    QCOMPARE(t.targetRounds(), 1);
    t.setTargetRounds(-5);
    QCOMPARE(t.targetRounds(), 1);
    t.setTargetRounds(99);
    QCOMPARE(t.targetRounds(), 12);
    t.setTargetRounds(7);  // in-range passes through
    QCOMPARE(t.targetRounds(), 7);
}

void TestPomodoroTimer::targetRoundsEmitsChangedOnSet() {
    PomodoroTimer t;
    QSignalSpy spy(&t, &PomodoroTimer::targetRoundsChanged);

    t.setTargetRounds(8);  // real change
    QCOMPARE(spy.count(), 1);
    QCOMPARE(t.targetRounds(), 8);

    spy.clear();
    t.setTargetRounds(8);  // same value — no signal noise
    QCOMPARE(spy.count(), 0);

    // Clamped back to current value also no-op (8 already, clampTargetRounds(0)=1).
    spy.clear();
    t.setTargetRounds(0);
    QCOMPARE(spy.count(), 1);  // 8 → 1 is a real change
    QCOMPARE(t.targetRounds(), 1);

    spy.clear();
    t.setTargetRounds(-1);  // clampTargetRounds(-1) = 1, already 1 — no-op
    QCOMPARE(spy.count(), 0);
}

QTEST_MAIN(TestPomodoroTimer)
#include "test_pomodoro_timer.moc"
