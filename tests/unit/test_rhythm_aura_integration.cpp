// test_rhythm_aura_integration — M3-C5: Aura away/back events pause/resume
// the PomodoroTimer via the EventBus subscription that RhythmPlugin sets up
// in onLoad. Mirrors the subscription shape from RhythmPlugin.cpp — but
// avoids loading the full plugin (which would need QmlService + InputMonitor).
//
// "away" pauses immediately; "back" arms a single-shot resume timer
// (resume_delay_sec, 60s in prod — DoD #3 "回座 60s 后恢复") rather than
// resuming instantly, and a second "away" before it fires cancels the resume.
// These tests mirror that wiring with a short delay.
//
// 2026-06-26 fix-forward: subscriptions now route through
// setPausedForReason(_, AuraAway) rather than the legacy setPaused(_) latch.
// Same semantics from the EventBus perspective; the change matters when
// another source (Idle / User) overlaps — covered in test_pomodoro_timer's
// bitmask cases.
//
// The test sets up the subscription directly with the real EventBus impl,
// then publishes margin.aura.away / margin.aura.back. EventBus dispatches
// via Qt::QueuedConnection, so we need a spinning event loop (QSignalSpy::wait
// or processEvents) for the handler to fire.

#include <QCoreApplication>
#include <QJsonObject>
#include <QObject>
#include <QSignalSpy>
#include <QString>
#include <QTest>
#include <QTimer>

#include "Margin/EventBus.h"
#include "plugins/rhythm/PomodoroTimer.h"

using Margin::EventBus;
using Margin::Plugins::Rhythm::PomodoroTimer;

class TestRhythmAuraIntegration : public QObject {
    Q_OBJECT

private slots:
    void init();
    void auraAwayPausesCountdown();
    void auraBackResumesAfterDelay();
    void auraReentryCancelsPendingResume();
    void auraPayloadIsIgnored();
    // 2026-06-27 lifecycle fix — RhythmPlugin wires InputMonitor's !idle
    // edge to clear a stuck AuraAway bit, and forceResume is the user-
    // explicit override that clears the entire mask. Both kill the
    // "paused · 离开中 stuck forever after BLE dropout" bug pattern.
    void inputNotIdle_clearsStuckAuraAway();
    void forceResume_fromAwayOnlyPause();

private:
    std::unique_ptr<EventBus> m_bus;
};

void TestRhythmAuraIntegration::init() {
    m_bus = EventBus::wire();
}

void TestRhythmAuraIntegration::auraAwayPausesCountdown() {
    PomodoroTimer t;
    t.setWorkMinutes(1);
    t.start();
    QCOMPARE(t.paused(), false);

    // Subscribe the same way RhythmPlugin does — direct lambda into
    // setPaused(true). subscriberIdentity is `this` (the test QObject) so
    // EventBus can track it; doesn't matter for this test since we don't
    // call unsubscribeAll.
    m_bus->subscribe(QStringLiteral("margin.aura.away"),
        [&t](const QJsonObject&) { t.setPausedForReason(true, PomodoroTimer::PauseReason::AuraAway); }, this);

    m_bus->publish(QStringLiteral("margin.aura.away"), QJsonObject{});
    // EventBus dispatches via QueuedConnection — spin the loop to drain.
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    QCoreApplication::processEvents(QEventLoop::AllEvents);

    QCOMPARE(t.paused(), true);

    // The pause halts the countdown. advance() respects m_paused.
    t.advance(30);
    QCOMPARE(t.remainingSeconds(), 60);  // unchanged
}

void TestRhythmAuraIntegration::auraBackResumesAfterDelay() {
    // DoD #3: "back" must NOT resume instantly — Rhythm arms a single-shot
    // resume timer (resume_delay_sec) so a brief walk-by doesn't unpause.
    // Mirror of RhythmPlugin's wiring with a short stand-in delay.
    PomodoroTimer t;
    t.setWorkMinutes(1);
    t.start();
    t.setPausedForReason(true, PomodoroTimer::PauseReason::AuraAway);

    QTimer resumeTimer;
    resumeTimer.setSingleShot(true);
    resumeTimer.setInterval(50);  // stands in for resume_delay_sec
    QObject::connect(&resumeTimer, &QTimer::timeout, &t,
                     [&t]() { t.setPausedForReason(false, PomodoroTimer::PauseReason::AuraAway); });

    m_bus->subscribe(QStringLiteral("margin.aura.away"),
        [&](const QJsonObject&) { resumeTimer.stop(); t.setPausedForReason(true, PomodoroTimer::PauseReason::AuraAway); }, this);
    m_bus->subscribe(QStringLiteral("margin.aura.back"),
        [&](const QJsonObject&) { resumeTimer.start(); }, this);

    m_bus->publish(QStringLiteral("margin.aura.back"), QJsonObject{});
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    QCoreApplication::processEvents(QEventLoop::AllEvents);

    // Resume is pending, not immediate — still paused right after "back".
    QCOMPARE(t.paused(), true);

    // Once the delay elapses, the countdown resumes.
    QTRY_COMPARE_WITH_TIMEOUT(t.paused(), false, 2000);
}

void TestRhythmAuraIntegration::auraReentryCancelsPendingResume() {
    // Step back, then leave again before the resume delay fires → the pending
    // resume is cancelled and the countdown stays paused.
    PomodoroTimer t;
    t.setWorkMinutes(1);
    t.start();
    t.setPausedForReason(true, PomodoroTimer::PauseReason::AuraAway);

    QTimer resumeTimer;
    resumeTimer.setSingleShot(true);
    resumeTimer.setInterval(200);
    QObject::connect(&resumeTimer, &QTimer::timeout, &t,
                     [&t]() { t.setPausedForReason(false, PomodoroTimer::PauseReason::AuraAway); });

    m_bus->subscribe(QStringLiteral("margin.aura.away"),
        [&](const QJsonObject&) { resumeTimer.stop(); t.setPausedForReason(true, PomodoroTimer::PauseReason::AuraAway); }, this);
    m_bus->subscribe(QStringLiteral("margin.aura.back"),
        [&](const QJsonObject&) { resumeTimer.start(); }, this);

    m_bus->publish(QStringLiteral("margin.aura.back"), QJsonObject{});
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    m_bus->publish(QStringLiteral("margin.aura.away"), QJsonObject{});  // re-leave
    QCoreApplication::processEvents(QEventLoop::AllEvents);

    // Wait past the original resume deadline — must still be paused.
    QTest::qWait(350);
    QCOMPARE(t.paused(), true);
}

void TestRhythmAuraIntegration::auraPayloadIsIgnored() {
    // Rhythm's handler doesn't read the payload — away/back are simple
    // presence signals. Verify by sending payload with garbage fields and
    // confirming the pause still triggers.
    PomodoroTimer t;
    t.setWorkMinutes(1);
    t.start();

    m_bus->subscribe(QStringLiteral("margin.aura.away"),
        [&t](const QJsonObject&) { t.setPausedForReason(true, PomodoroTimer::PauseReason::AuraAway); }, this);

    QJsonObject junk;
    junk["device"]    = "iPhone XYZ";
    junk["rssi"]      = -85;
    junk["timestamp"] = 12345;
    m_bus->publish(QStringLiteral("margin.aura.away"), junk);
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    QCoreApplication::processEvents(QEventLoop::AllEvents);

    QCOMPARE(t.paused(), true);
}

void TestRhythmAuraIntegration::inputNotIdle_clearsStuckAuraAway() {
    // Bug H fallback (2026-06-27): Aura's BLE inference can stick on "away"
    // when a Microsoft peripheral sleeps after the user walks back — no
    // margin.aura.back ever fires. RhythmPlugin wires the InputMonitor
    // !idle edge to also clear AuraAway, because physical input is ground
    // truth. This test mirrors that wiring.
    PomodoroTimer t;
    t.setWorkMinutes(1);
    t.start();

    // Simulate Aura away fired, back NEVER fired.
    t.setPausedForReason(true, PomodoroTimer::PauseReason::AuraAway);
    QCOMPARE(t.paused(), true);
    QCOMPARE(int(t.pauseMask()), int(PomodoroTimer::PauseReason::AuraAway));

    // Mirror RhythmPlugin's InputMonitor connect — !idle also clears AuraAway.
    auto onIdleChanged = [&t](bool idle) {
        t.setPausedForReason(idle, PomodoroTimer::PauseReason::Idle);
        if (!idle) {
            t.setPausedForReason(false, PomodoroTimer::PauseReason::AuraAway);
        }
    };

    // User moves the mouse — Idle clears AND stuck AuraAway clears.
    onIdleChanged(false);

    QCOMPARE(t.paused(),         false);
    QCOMPARE(int(t.pauseMask()), 0);
    // Countdown actually resumes — advance is no longer blocked.
    t.advance(5);
    QCOMPARE(t.remainingSeconds(), 55);
}

void TestRhythmAuraIntegration::forceResume_fromAwayOnlyPause() {
    // Bug A fix (2026-06-27): legacy setPaused(false) only clears the User
    // bit. When paused-by-Aura-only, User was never set so the call is a
    // no-op — the "继续 button does nothing" complaint. forceResume()
    // clears the entire mask regardless of source.
    PomodoroTimer t;
    t.setWorkMinutes(1);
    t.start();

    // Pure AuraAway pause — no User, no Idle.
    t.setPausedForReason(true, PomodoroTimer::PauseReason::AuraAway);
    QCOMPARE(t.paused(), true);
    QCOMPARE(int(t.pauseMask()), int(PomodoroTimer::PauseReason::AuraAway));

    // The legacy path — what setPaused(false) would do.
    t.setPaused(false);
    // Still paused — User bit wasn't set, so clearing it changes nothing.
    QCOMPARE(t.paused(),         true);
    QCOMPARE(int(t.pauseMask()), int(PomodoroTimer::PauseReason::AuraAway));

    // The new path — explicit user override.
    t.forceResume();
    QCOMPARE(t.paused(),         false);
    QCOMPARE(int(t.pauseMask()), 0);
    t.advance(5);
    QCOMPARE(t.remainingSeconds(), 55);
}

QTEST_MAIN(TestRhythmAuraIntegration)
#include "test_rhythm_aura_integration.moc"
