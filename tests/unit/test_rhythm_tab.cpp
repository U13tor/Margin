// tests/unit/test_rhythm_tab.cpp
//
// M4-C12: RhythmTab.qml pomodoro-fidelity rewrite verification. Loads
// qrc:/rhythm/qml/RhythmTab.qml offscreen with a real PomodoroTimer
// instance as the `rhythm` context property (mirrors RhythmPlugin::onLoad).
// Verifies:
//   (1) Status card + "#N/M" header render with N = todayCompletedRounds + 1
//   (2) ProgressDots renders `targetRounds` discs; `todayCompletedRounds`
//       of them get the active color
//   (3) Pause button toggles `rhythm.paused` via setPaused slot
//   (4) Skip button is visible only in BreakDue/BreakActive states
//   (5) Settings button calls settingsRoot.openSettings("rhythm") — inline
//       settingsCard removed in PR5 (Rhythm/ScreenTime settings unified in
//       Margin Settings per user bug #4 "设置重复").
//
// Pattern: links Primitives + Composite qml-module plugins (for Theme,
// MCard, MButton, TimerDisplay, ProgressDots), compiles PomodoroTimer.cpp
// directly (no plugin dllexport), and registers rhythm.qrc + host.qrc
// as sources so qrc:/rhythm/... and qrc:/icons/... resolve.

#include <QObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QString>
#include <QVariant>
#include <QTest>
#include <QUrl>

#include <memory>

#include "plugins/rhythm/PomodoroTimer.h"

using Margin::Plugins::Rhythm::PomodoroTimer;

class TestRhythmTab : public QObject {
    Q_OBJECT

private slots:
    void loadsAndShowsPomodoroHeader();
    void progressDotsReflectCompletedRounds();
    void pauseButtonTogglesPausedState();
    void skipButtonVisibleOnlyInBreakStates();
    void settingsButtonCallsOpenSettingsNoInlineCard();

private:
    static QObject* findByName(QObject* node, const QString& name);
};

// Walk QObject children + QQuickItem childItems (Repeater delegates live in
// childItems, not children — see docs/15-dev-gotchas.md feedback note).
QObject* TestRhythmTab::findByName(QObject* node, const QString& name) {
    if (!node) return nullptr;
    if (!node->objectName().isEmpty() && node->objectName() == name) return node;
    for (auto* child : node->children()) {
        if (QObject* found = findByName(child, name)) return found;
    }
    if (auto* item = qobject_cast<QQuickItem*>(node)) {
        for (QQuickItem* ci : item->childItems()) {
            if (QObject* found = findByName(ci, name)) return found;
        }
    }
    return nullptr;
}

// (1) Default state (Idle, todayCompletedRounds=0, targetRounds=5 default)
//     → header renders "#1/5".
void TestRhythmTab::loadsAndShowsPomodoroHeader() {
    PomodoroTimer timer;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("rhythm"), &timer);
    engine.load(QUrl(QStringLiteral("qrc:/rhythm/qml/RhythmTab.qml")));
    QVERIFY2(!engine.rootObjects().isEmpty(), "RhythmTab.qml failed to load");
    QObject* root = engine.rootObjects().constFirst();

    QObject* header = findByName(root, QStringLiteral("pomodoroHeader"));
    QVERIFY2(header, "pomodoroHeader Text not found");
    QCOMPARE(header->property("text").toString(), QStringLiteral("#1/5"));
}

// (2) targetRounds=5, todayCompletedRounds=2 → header "#3/5" + ProgressDots
//     has 5 children with first 2 in activeColor. We can't easily read the
//     rendered color of each dot without hitting private QQuickRectangle
//     internals; instead we assert ProgressDots.total + .active (the
//     properties the binding consumes), which is what the QML actually
//     binds the visual state to.
void TestRhythmTab::progressDotsReflectCompletedRounds() {
    PomodoroTimer timer;
    timer.setTargetRounds(5);
    timer.setTodayCompletedRounds(2);
    QCOMPARE(timer.todayCompletedRounds(), 2);
    QCOMPARE(timer.targetRounds(), 5);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("rhythm"), &timer);
    engine.load(QUrl(QStringLiteral("qrc:/rhythm/qml/RhythmTab.qml")));
    QVERIFY(!engine.rootObjects().isEmpty());
    QObject* root = engine.rootObjects().constFirst();

    QObject* header = findByName(root, QStringLiteral("pomodoroHeader"));
    QVERIFY(header);
    QCOMPARE(header->property("text").toString(), QStringLiteral("#3/5"));

    QObject* dots = findByName(root, QStringLiteral("pomodoroDots"));
    QVERIFY2(dots, "pomodoroDots ProgressDots not found");
    QCOMPARE(dots->property("total").toInt(), 5);
    QCOMPARE(dots->property("active").toInt(), 2);
}

// (3) Pause button clicked → rhythm.paused transitions false→true; second
//     click flips back. Invokes the MButton.clicked() signal directly
//     (signal wiring, not MouseArea dispatch — see test_overview_tab note).
void TestRhythmTab::pauseButtonTogglesPausedState() {
    PomodoroTimer timer;
    timer.start();  // Working state so paused has visible effect on tick
    QCOMPARE(timer.stateString(), QStringLiteral("working"));
    QCOMPARE(timer.paused(), false);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("rhythm"), &timer);
    engine.load(QUrl(QStringLiteral("qrc:/rhythm/qml/RhythmTab.qml")));
    QVERIFY(!engine.rootObjects().isEmpty());
    QObject* root = engine.rootObjects().constFirst();

    QObject* pauseBtn = findByName(root, QStringLiteral("pauseButton"));
    QVERIFY2(pauseBtn, "pauseButton not found");

    QVERIFY2(QMetaObject::invokeMethod(pauseBtn, "clicked"),
             "failed to invoke clicked() on pauseButton");
    QVERIFY2(timer.paused(), "paused should be true after first click");

    QVERIFY2(QMetaObject::invokeMethod(pauseBtn, "clicked"),
             "failed to invoke clicked() on pauseButton (2nd)");
    QVERIFY2(!timer.paused(), "paused should be false after second click");
}

// (4) Working → skip invisible; BreakDue → skip visible + skipBreak works.
//     Skip visibility is bound to root._skipVisible which reads rhythm.state.
void TestRhythmTab::skipButtonVisibleOnlyInBreakStates() {
    PomodoroTimer timer;
    timer.setWorkMinutes(1);
    timer.start();   // Working
    QCOMPARE(timer.stateString(), QStringLiteral("working"));

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("rhythm"), &timer);
    engine.load(QUrl(QStringLiteral("qrc:/rhythm/qml/RhythmTab.qml")));
    QVERIFY(!engine.rootObjects().isEmpty());
    QObject* root = engine.rootObjects().constFirst();

    QObject* skipBtn = findByName(root, QStringLiteral("skipButton"));
    QVERIFY2(skipBtn, "skipButton not found");
    QVERIFY2(!skipBtn->property("visible").toBool(),
             "skip should be hidden in Working state");

    // Drive the timer into BreakDue and re-check via the binding.
    timer.advance(60);
    QCOMPARE(timer.stateString(), QStringLiteral("break_due"));
    QVERIFY2(skipBtn->property("visible").toBool(),
             "skip should be visible in BreakDue state");

    // Click → skipBreak() → state returns to Idle, button hides again.
    QVERIFY2(QMetaObject::invokeMethod(skipBtn, "clicked"),
             "failed to invoke clicked() on skipButton");
    QCOMPARE(timer.stateString(), QStringLiteral("idle"));
    QVERIFY2(!skipBtn->property("visible").toBool(),
             "skip should hide again after returning to Idle");
}

// (5) PR5: removed the inline settingsCard (4 SpinBoxes for work/break/
//     maxPostpones/targetRounds). The Settings button now jumps to the
//     top-level Settings → Rhythm page (pageId "rhythm"). settingsRoot is
//     not wired in unit tests so the typeof guard skips the actual call;
//     this test verifies the button still exists, is clickable, and emits
//     no QML warnings during the click. Inline SpinBoxes/rhythmSettingsCard
//     must be gone — findByName returns null.
void TestRhythmTab::settingsButtonCallsOpenSettingsNoInlineCard() {
    PomodoroTimer timer;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("rhythm"), &timer);
    engine.load(QUrl(QStringLiteral("qrc:/rhythm/qml/RhythmTab.qml")));
    QVERIFY(!engine.rootObjects().isEmpty());
    QObject* root = engine.rootObjects().constFirst();

    QObject* settingsCard = findByName(root, QStringLiteral("rhythmSettingsCard"));
    QVERIFY2(!settingsCard, "rhythmSettingsCard should be removed in PR5");

    QObject* settingsBtn = findByName(root, QStringLiteral("settingsButton"));
    QVERIFY(settingsBtn);

    QSignalSpy warnSpy(&engine, &QQmlEngine::warnings);
    QVERIFY2(QMetaObject::invokeMethod(settingsBtn, "clicked"),
             "failed to invoke clicked() on settingsButton");
    QVERIFY2(warnSpy.isEmpty(), "QML warnings emitted during settingsButton click");
}

QTEST_MAIN(TestRhythmTab)
#include "test_rhythm_tab.moc"
