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
    // PauseReason bitmask (2026-06-26 fix-forward). The legacy setPaused
    // cases above keep working — they exercise the User-bit path.
    void setPausedForReason_idle_setsPausedState();
    void setPausedForReason_userThenIdle_keepsPausedWhenUserClears();
    void setPausedForReason_auraAway_thenBack_resumes();
    void setPausedForReason_idleAndAura_resumesOnlyWhenBothClear();
    void setPausedForReason_isIdempotent_perReason();
    void pauseReasonsText_changesWithReason();
    void legacySetPaused_mapsToUserReason();
    void endBreakEarlyFromBreakActiveGoesIdle();
    void endBreakEarlyNoOpOutsideBreakActive();
    void postponeBudgetOfThreeReachesZero();
    void targetRoundsDefaultIsFive();
    void targetRoundsClampsToRange();
    void targetRoundsEmitsChangedOnSet();
    // ── 2026-06-27 lifecycle fix ──
    // forceResume: user explicit override clears entire pause mask (Bug A/H).
    // start clears mask: Bug F — entering Working with leftover pause bits
    // would freeze the countdown. autoContinue: Bug I — BreakActive natural
    // completion must seed the next work round or the timer visually freezes
    // at 00:00 with no obvious next step.
    void forceResume_clearsFullMask();
    void forceResume_isIdempotent();
    void start_clearsPauseMask();
    void breakActiveAutoContinuesToWorking();
    void breakActiveReturnsToIdleWhenAutoContinueFalse();
    void pauseReasonsText_priorityComplete();
    void autoContinueCycle_defaultAndSignal();
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
    // Note: with the 2026-06-27 lifecycle fix, BreakActive natural completion
    // auto-continues into Working by default. This existing case pins the
    // autoContinue=false branch — breakActiveAutoContinuesToWorking covers
    // the new default. Both paths share breakEnded + breaksCompleted signals.
    PomodoroTimer t;
    t.setAutoContinueCycle(false);
    t.setWorkMinutes(1);
    t.setBreakMinutes(1);
    t.start();
    t.advance(60);     // → BreakDue
    t.startBreak();    // → BreakActive

    QSignalSpy endedSpy(&t, &PomodoroTimer::breakEnded);
    QSignalSpy completedSpy(&t, &PomodoroTimer::breaksCompletedChanged);

    t.advance(60);     // → Idle (natural completion, manual pacing)
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

// ── PauseReason bitmask cases (2026-06-26 fix-forward) ──
// The historical setPaused(bool) latch was a single bit shared by 3 sources
// (User / Idle / AuraAway); any source clearing it would resume even if the
// others were still active. These tests pin the new mask semantics.

void TestPomodoroTimer::setPausedForReason_idle_setsPausedState() {
    PomodoroTimer t;
    t.setWorkMinutes(1);
    t.start();
    QCOMPARE(t.paused(), false);

    QSignalSpy spy(&t, &PomodoroTimer::pausedChanged);
    t.setPausedForReason(true, PomodoroTimer::PauseReason::Idle);

    QCOMPARE(spy.count(),    1);
    QCOMPARE(t.paused(),     true);
    QCOMPARE(int(t.pauseMask()), int(PomodoroTimer::PauseReason::Idle));

    // Countdown is frozen.
    t.advance(30);
    QCOMPARE(t.remainingSeconds(), 60);
}

void TestPomodoroTimer::setPausedForReason_userThenIdle_keepsPausedWhenUserClears() {
    // The core bitmask guarantee: clearing User does NOT resume while Idle
    // is still set. This is what kills the "Aura back while Idle still
    // active → false resume" bug pattern.
    PomodoroTimer t;
    t.setWorkMinutes(1);
    t.start();

    t.setPaused(true);  // legacy path → User bit
    t.setPausedForReason(true, PomodoroTimer::PauseReason::Idle);
    QCOMPARE(t.paused(), true);

    QSignalSpy spy(&t, &PomodoroTimer::pausedChanged);
    t.setPaused(false);  // clears User bit, Idle still set
    // paused() boolean stays true (no edge), but pausedChanged still fires
    // once — pauseReasonsText is bound to this signal so the UI chip can
    // refresh its text from "用户暂停" (User dominant) to "闲置" (Idle now
    // dominant alone). See PomodoroTimer.cpp setPausedForReason rationale.
    QCOMPARE(spy.count(),    1);
    QCOMPARE(t.paused(),     true);
    QCOMPARE(int(t.pauseMask()), int(PomodoroTimer::PauseReason::Idle));
}

void TestPomodoroTimer::setPausedForReason_auraAway_thenBack_resumes() {
    PomodoroTimer t;
    t.setWorkMinutes(1);
    t.start();

    t.setPausedForReason(true, PomodoroTimer::PauseReason::AuraAway);
    QCOMPARE(t.paused(), true);

    t.setPausedForReason(false, PomodoroTimer::PauseReason::AuraAway);
    QCOMPARE(t.paused(),     false);
    QCOMPARE(int(t.pauseMask()), 0);

    // Countdown resumes — tick advances remaining.
    t.advance(5);
    QCOMPARE(t.remainingSeconds(), 55);
}

void TestPomodoroTimer::setPausedForReason_idleAndAura_resumesOnlyWhenBothClear() {
    PomodoroTimer t;
    t.setWorkMinutes(1);
    t.start();

    t.setPausedForReason(true, PomodoroTimer::PauseReason::Idle);
    t.setPausedForReason(true, PomodoroTimer::PauseReason::AuraAway);
    QCOMPARE(t.paused(), true);

    // Clear AuraAway but Idle still holds — must stay paused.
    t.setPausedForReason(false, PomodoroTimer::PauseReason::AuraAway);
    QCOMPARE(t.paused(), true);
    QCOMPARE(int(t.pauseMask()), int(PomodoroTimer::PauseReason::Idle));

    // Now clear Idle too — fully resumed.
    t.setPausedForReason(false, PomodoroTimer::PauseReason::Idle);
    QCOMPARE(t.paused(),     false);
    QCOMPARE(int(t.pauseMask()), 0);
}

void TestPomodoroTimer::setPausedForReason_isIdempotent_perReason() {
    // Setting the same reason twice must not emit pausedChanged — same
    // guarantee as legacy setPausedIsIdempotent, scoped to the per-reason
    // path so QML chip text doesn't flicker.
    PomodoroTimer t;
    t.start();

    QSignalSpy spy(&t, &PomodoroTimer::pausedChanged);
    t.setPausedForReason(true, PomodoroTimer::PauseReason::Idle);
    QCOMPARE(spy.count(), 1);

    t.setPausedForReason(true, PomodoroTimer::PauseReason::Idle);  // same bit
    QCOMPARE(spy.count(), 1);

    // Adding a NEW reason still fires pausedChanged even though the boolean
    // didn't flip — so the UI chip binding (which reads pauseReasonsText)
    // gets the signal to re-evaluate.
    t.setPaused(true);  // legacy path adds User bit while Idle still set
    QCOMPARE(spy.count(), 2);
    QCOMPARE(t.paused(), true);
}

void TestPomodoroTimer::pauseReasonsText_changesWithReason() {
    PomodoroTimer t;
    t.start();

    QVERIFY(t.pauseReasonsText().isEmpty());

    t.setPausedForReason(true, PomodoroTimer::PauseReason::User);
    QCOMPARE(t.pauseReasonsText(), QStringLiteral("用户暂停"));

    t.setPausedForReason(true, PomodoroTimer::PauseReason::Idle);
    // Idle has higher priority than User → text flips.
    QCOMPARE(t.pauseReasonsText(), QStringLiteral("闲置"));

    t.setPausedForReason(true, PomodoroTimer::PauseReason::AuraAway);
    // AuraAway wins over Idle + User.
    QCOMPARE(t.pauseReasonsText(), QStringLiteral("离开中"));

    // Clear AuraAway → Idle dominates again.
    t.setPausedForReason(false, PomodoroTimer::PauseReason::AuraAway);
    QCOMPARE(t.pauseReasonsText(), QStringLiteral("闲置"));

    // Clear Idle too → only User left.
    t.setPausedForReason(false, PomodoroTimer::PauseReason::Idle);
    QCOMPARE(t.pauseReasonsText(), QStringLiteral("用户暂停"));

    // Clear User → empty.
    t.setPaused(false);
    QVERIFY(t.pauseReasonsText().isEmpty());
}

void TestPomodoroTimer::legacySetPaused_mapsToUserReason() {
    // Regression: legacy setPaused(true) used by tray + QML must continue
    // working and tag the User bit, not a synthetic "legacy" reason.
    PomodoroTimer t;
    t.start();

    t.setPaused(true);
    QCOMPARE(t.paused(),     true);
    QCOMPARE(int(t.pauseMask()), int(PomodoroTimer::PauseReason::User));
    QCOMPARE(t.pauseReasonsText(), QStringLiteral("用户暂停"));

    t.setPaused(false);
    QCOMPARE(t.paused(),     false);
    QCOMPARE(int(t.pauseMask()), 0);
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

// ── 2026-06-27 lifecycle fix cases ──
// See plan: recursive-crunching-pearl.md PR1. forceResume is the user-
// explicit override that clears the entire pauseMask in one shot — without
// it, paused-by-Aura-only states can never be cleared by the user when the
// BLE back signal never arrives (Microsoft peripherals sleep).

void TestPomodoroTimer::forceResume_clearsFullMask() {
    // The core fix for Bug A/H: an AuraAway-only pause (or any multi-bit
    // combination) must clear completely when the user explicitly resumes.
    // Legacy setPaused(false) only clears the User bit — useless when the
    // user didn't cause the pause in the first place.
    PomodoroTimer t;
    t.setWorkMinutes(1);
    t.start();
    QCOMPARE(t.remainingSeconds(), 60);

    t.setPausedForReason(true, PomodoroTimer::PauseReason::Idle);
    t.setPausedForReason(true, PomodoroTimer::PauseReason::AuraAway);
    QCOMPARE(t.paused(), true);
    QCOMPARE(int(t.pauseMask()),
             int(PomodoroTimer::PauseReason::Idle)
             | int(PomodoroTimer::PauseReason::AuraAway));

    QSignalSpy spy(&t, &PomodoroTimer::pausedChanged);
    t.forceResume();

    QCOMPARE(spy.count(),        1);
    QCOMPARE(t.paused(),         false);
    QCOMPARE(int(t.pauseMask()), 0);

    // Countdown actually resumes — the tick advances remaining.
    t.advance(5);
    QCOMPARE(t.remainingSeconds(), 55);
}

void TestPomodoroTimer::forceResume_isIdempotent() {
    // No-op when already running — no phantom pausedChanged signal. QML
    // bindings on `paused` would otherwise see spurious refreshes.
    PomodoroTimer t;
    t.start();
    QCOMPARE(t.paused(), false);

    QSignalSpy spy(&t, &PomodoroTimer::pausedChanged);
    t.forceResume();
    QCOMPARE(spy.count(), 0);
    QCOMPARE(int(t.pauseMask()), 0);
}

void TestPomodoroTimer::start_clearsPauseMask() {
    // Bug F: a prior session's leftover pause bits must not survive start().
    // Without this, entering Working with m_paused=true freezes the
    // countdown and the user sees a stuck timer with no pause chip
    // (pauseReasonsText is empty because the chip binding is gated on
    // rhythm.paused — but onTick refuses to advance). State machine must
    // not accept this contradictory configuration.
    PomodoroTimer t;
    t.setWorkMinutes(1);
    t.start();
    t.setPausedForReason(true, PomodoroTimer::PauseReason::User);
    t.setPausedForReason(true, PomodoroTimer::PauseReason::AuraAway);
    QCOMPARE(t.paused(), true);
    t.stop();  // → Idle. Mask is NOT cleared by stop — leftover bits persist.

    QSignalSpy spy(&t, &PomodoroTimer::pausedChanged);
    t.start();
    QCOMPARE(spy.count(),        1);
    QCOMPARE(t.paused(),         false);
    QCOMPARE(int(t.pauseMask()), 0);

    // After start, countdown actually advances.
    t.advance(5);
    QCOMPARE(t.remainingSeconds(), 55);
}

void TestPomodoroTimer::breakActiveAutoContinuesToWorking() {
    // Bug I fix — default behavior. BreakActive natural completion seeds
    // the next work round immediately. Without this, the timer froze at
    // 00:00 in Idle with no obvious next step (the "做完一次操然后计时器
    // 没开始" complaint). Postpones reset so the new work session has a
    // fresh budget.
    PomodoroTimer t;
    QCOMPARE(t.autoContinueCycle(), true);  // default
    t.setWorkMinutes(1);
    t.setBreakMinutes(1);
    t.setMaxPostpones(2);
    t.start();
    // Spend one postpone credit to verify reset happens on auto-continue.
    t.advance(60);  // → BreakDue
    t.postponeBreak();
    QCOMPARE(t.postponesRemaining(), 1);
    t.advance(60);  // → BreakDue again
    t.startBreak(); // → BreakActive

    QSignalSpy endedSpy(&t, &PomodoroTimer::breakEnded);
    QSignalSpy stateSpy(&t, &PomodoroTimer::stateChanged);

    t.advance(60);  // break completes

    QCOMPARE(t.state(),          PomodoroTimer::State::Working);
    QCOMPARE(endedSpy.count(),   1);
    QCOMPARE(t.breaksCompleted(), 1);
    // New work session — full budget, postpones reset to ceiling.
    QCOMPARE(t.remainingSeconds(), 60);
    QCOMPARE(t.postponesRemaining(), 2);
    // stateChanged fires: BreakActive → Working is one transition.
    // (No Idle intermediate — auto-continue skips it.)
    QVERIFY(stateSpy.count() >= 1);
}

void TestPomodoroTimer::breakActiveReturnsToIdleWhenAutoContinueFalse() {
    // Opt-out path: hosts with autostart disabled (or users who want manual
    // pacing) see the v1.0 behavior. The same breakEnded signal fires so
    // plugin / UI wiring doesn't need conditional branches.
    PomodoroTimer t;
    t.setAutoContinueCycle(false);
    QCOMPARE(t.autoContinueCycle(), false);
    t.setWorkMinutes(1);
    t.setBreakMinutes(1);
    t.start();
    t.advance(60);
    t.startBreak();

    QSignalSpy endedSpy(&t, &PomodoroTimer::breakEnded);
    t.advance(60);

    QCOMPARE(t.state(),          PomodoroTimer::State::Idle);
    QCOMPARE(endedSpy.count(),   1);
    QCOMPARE(t.breaksCompleted(), 1);
}

void TestPomodoroTimer::pauseReasonsText_priorityComplete() {
    // Pin the priority chain AuraAway > Idle > User exhaustively. The chip
    // text on RhythmTab depends on this staying stable — a regression that
    // reorders the chain would silently mislabel pause reasons to the user.
    PomodoroTimer t;
    t.start();

    // No reasons active → empty.
    QVERIFY(t.pauseReasonsText().isEmpty());

    // Each reason alone shows its own label.
    t.setPausedForReason(true, PomodoroTimer::PauseReason::User);
    QCOMPARE(t.pauseReasonsText(), QStringLiteral("用户暂停"));
    t.setPausedForReason(false, PomodoroTimer::PauseReason::User);

    t.setPausedForReason(true, PomodoroTimer::PauseReason::Idle);
    QCOMPARE(t.pauseReasonsText(), QStringLiteral("闲置"));
    t.setPausedForReason(false, PomodoroTimer::PauseReason::Idle);

    t.setPausedForReason(true, PomodoroTimer::PauseReason::AuraAway);
    QCOMPARE(t.pauseReasonsText(), QStringLiteral("离开中"));

    // Combinations — AuraAway dominates whenever set.
    t.setPausedForReason(true, PomodoroTimer::PauseReason::Idle);
    QCOMPARE(t.pauseReasonsText(), QStringLiteral("离开中"));
    t.setPausedForReason(true, PomodoroTimer::PauseReason::User);
    QCOMPARE(t.pauseReasonsText(), QStringLiteral("离开中"));

    // Clear AuraAway → Idle dominates.
    t.setPausedForReason(false, PomodoroTimer::PauseReason::AuraAway);
    QCOMPARE(t.pauseReasonsText(), QStringLiteral("闲置"));

    // Clear Idle → only User left.
    t.setPausedForReason(false, PomodoroTimer::PauseReason::Idle);
    QCOMPARE(t.pauseReasonsText(), QStringLiteral("用户暂停"));
}

void TestPomodoroTimer::autoContinueCycle_defaultAndSignal() {
    // Setting persistence SSOT — RhythmPlugin::loadSettings reads this on
    // boot. Default true so the "做完一次操然后计时器没开始" bug stays fixed
    // out of the box.
    PomodoroTimer t;
    QCOMPARE(t.autoContinueCycle(), true);

    QSignalSpy spy(&t, &PomodoroTimer::autoContinueCycleChanged);
    t.setAutoContinueCycle(false);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(t.autoContinueCycle(), false);

    spy.clear();
    t.setAutoContinueCycle(false);  // same value — no signal noise
    QCOMPARE(spy.count(), 0);

    spy.clear();
    t.setAutoContinueCycle(true);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(t.autoContinueCycle(), true);
}

QTEST_MAIN(TestPomodoroTimer)
#include "test_pomodoro_timer.moc"
