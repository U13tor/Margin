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
        [&t](const QJsonObject&) { t.setPaused(true); }, this);

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
    t.setPaused(true);

    QTimer resumeTimer;
    resumeTimer.setSingleShot(true);
    resumeTimer.setInterval(50);  // stands in for resume_delay_sec
    QObject::connect(&resumeTimer, &QTimer::timeout, &t,
                     [&t]() { t.setPaused(false); });

    m_bus->subscribe(QStringLiteral("margin.aura.away"),
        [&](const QJsonObject&) { resumeTimer.stop(); t.setPaused(true); }, this);
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
    t.setPaused(true);

    QTimer resumeTimer;
    resumeTimer.setSingleShot(true);
    resumeTimer.setInterval(200);
    QObject::connect(&resumeTimer, &QTimer::timeout, &t,
                     [&t]() { t.setPaused(false); });

    m_bus->subscribe(QStringLiteral("margin.aura.away"),
        [&](const QJsonObject&) { resumeTimer.stop(); t.setPaused(true); }, this);
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
        [&t](const QJsonObject&) { t.setPaused(true); }, this);

    QJsonObject junk;
    junk["device"]    = "iPhone XYZ";
    junk["rssi"]      = -85;
    junk["timestamp"] = 12345;
    m_bus->publish(QStringLiteral("margin.aura.away"), junk);
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    QCoreApplication::processEvents(QEventLoop::AllEvents);

    QCOMPARE(t.paused(), true);
}

QTEST_MAIN(TestRhythmAuraIntegration)
#include "test_rhythm_aura_integration.moc"
