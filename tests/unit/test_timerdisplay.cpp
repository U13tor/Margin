// tests/unit/test_timerdisplay.cpp
//
// M4-C7: verifies the TimerDisplay composite's formatting logic (MmSs /
// HhMmSs), negative-seconds clamping (prevents "-00:01" flashes during
// tick/state-transition races), explicit text override, label propagation,
// and the Format enum default. Mirrors test_mbutton pattern. Links both
// primitives + composite qml-module plugins (TimerDisplay reads Theme
// tokens from Primitives, and the composite URI itself).

#include <QObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QString>
#include <QTest>
#include <QUrl>
#include <memory>

class TestTimerDisplay : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void zeroSecondsMmSsDisplays();
    void positiveSecondsMmSsDisplays();
    void negativeSecondsClampsToZero();
    void hhMmSsFormatDisplays();
    void textOverrideBeatsSeconds();
    void labelPropagates();
    void defaultFormatIsMmSs();

private:
    QObject* loadProbe(const QByteArray& qml);
    std::unique_ptr<QQmlEngine>    m_engine;
    std::unique_ptr<QQmlComponent> m_component;
    std::unique_ptr<QObject>       m_probe;
};

void TestTimerDisplay::init() {
    m_engine = std::make_unique<QQmlEngine>();
}

void TestTimerDisplay::cleanup() {
    m_probe.reset();
    m_component.reset();
    m_engine.reset();
}

QObject* TestTimerDisplay::loadProbe(const QByteArray& qml) {
    m_component = std::make_unique<QQmlComponent>(m_engine.get());
    m_component->setData(qml, QUrl());
    m_probe.reset(m_component->create());
    return m_probe.get();
}

void TestTimerDisplay::zeroSecondsMmSsDisplays() {
    QObject* probe = loadProbe(
        "import QtQuick\n"
        "import Margin.Ui.Composite\n"
        "TimerDisplay { seconds: 0 }\n");
    QVERIFY2(probe, qPrintable(QStringLiteral("probe failed: %1")
                                   .arg(m_component->errorString())));
    QCOMPARE(probe->property("_derivedText").toString(), QStringLiteral("00:00"));
    QCOMPARE(probe->property("_displayText").toString(), QStringLiteral("00:00"));
}

void TestTimerDisplay::positiveSecondsMmSsDisplays() {
    // 1500 s = 25 min — the default PomodoroTimer work interval.
    QObject* probe = loadProbe(
        "import QtQuick\n"
        "import Margin.Ui.Composite\n"
        "TimerDisplay { seconds: 1500 }\n");
    QVERIFY(probe);
    QCOMPARE(probe->property("_derivedText").toString(), QStringLiteral("25:00"));
    QCOMPARE(probe->property("_displayText").toString(), QStringLiteral("25:00"));
}

void TestTimerDisplay::negativeSecondsClampsToZero() {
    // PomodoroTimer::tick / state-transition races can briefly push
    // remainingSeconds below zero; TimerDisplay must render 00:00, not
    // "-00:10" or "NaN:NaN".
    QObject* probe = loadProbe(
        "import QtQuick\n"
        "import Margin.Ui.Composite\n"
        "TimerDisplay { seconds: -10 }\n");
    QVERIFY(probe);
    QCOMPARE(probe->property("_derivedText").toString(), QStringLiteral("00:00"));
    QCOMPARE(probe->property("_displayText").toString(), QStringLiteral("00:00"));
}

void TestTimerDisplay::hhMmSsFormatDisplays() {
    // 8100 s = 2 h 15 m — Screen Time "今日专注" total scope.
    QObject* probe = loadProbe(
        "import QtQuick\n"
        "import Margin.Ui.Composite\n"
        "TimerDisplay {\n"
        "  seconds: 8100\n"
        "  format: TimerDisplay.Format.HhMmSs\n"
        "}\n");
    QVERIFY(probe);
    QCOMPARE(probe->property("_derivedText").toString(), QStringLiteral("02:15:00"));
    QCOMPARE(probe->property("_displayText").toString(), QStringLiteral("02:15:00"));
}

void TestTimerDisplay::textOverrideBeatsSeconds() {
    // Non-empty text must win over seconds + format. Lets callers reuse
    // TimerDisplay's visual treatment for non-duration strings (clock time,
    // custom compact format) without disabling the seconds pipeline.
    QObject* probe = loadProbe(
        "import QtQuick\n"
        "import Margin.Ui.Composite\n"
        "TimerDisplay {\n"
        "  seconds: 60\n"
        "  text: \"custom\"\n"
        "}\n");
    QVERIFY(probe);
    QCOMPARE(probe->property("_derivedText").toString(), QStringLiteral("01:00"));
    QCOMPARE(probe->property("_displayText").toString(), QStringLiteral("custom"));
}

void TestTimerDisplay::labelPropagates() {
    QObject* probe = loadProbe(
        "import QtQuick\n"
        "import Margin.Ui.Composite\n"
        "TimerDisplay { seconds: 0; label: \"下次休息\" }\n");
    QVERIFY(probe);
    QCOMPARE(probe->property("label").toString(), QStringLiteral("下次休息"));
    QCOMPARE(probe->property("_hasLabel").toBool(), true);
}

void TestTimerDisplay::defaultFormatIsMmSs() {
    // format is `int` (QML enum values are ints); MmSs = 0 in the
    // Format enum. Default must match so callers who omit `format` get
    // mm:ss rendering (RhythmTab's countdown contract).
    QObject* probe = loadProbe(
        "import QtQuick\n"
        "import Margin.Ui.Composite\n"
        "TimerDisplay { seconds: 0 }\n");
    QVERIFY(probe);
    QCOMPARE(probe->property("format").toInt(), 0);  // TimerDisplay.Format.MmSs
}

QTEST_MAIN(TestTimerDisplay)
#include "test_timerdisplay.moc"
