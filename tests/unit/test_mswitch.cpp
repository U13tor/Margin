// tests/unit/test_mswitch.cpp
//
// M4-C5: verifies the MSwitch atom's visual state contract (track + thumb
// colors derived from checked/enabled), the checked property round-trip, and
// the toggled(bool) signal contract. Mirrors test_mbutton pattern. Runs
// offscreen. Links the Primitives qml-module plugin + host.qrc.

#include <QColor>
#include <QObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QSignalSpy>
#include <QString>
#include <QTest>
#include <QUrl>
#include <QVariant>
#include <memory>

class TestMSwitch : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void checkedDefaultsFalse();
    void setCheckedTrueUpdatesTrackColor();
    void disabledTrackColorIsBgHover();
    void thumbColorMutedWhenDisabled();
    void checkedPropertyRoundTrip();
    void toggledSignalIsConnectable();

private:
    QObject* loadProbe(const QByteArray& qml);
    std::unique_ptr<QQmlEngine>    m_engine;
    std::unique_ptr<QQmlComponent> m_component;
    std::unique_ptr<QObject>       m_probe;
};

void TestMSwitch::init() {
    m_engine = std::make_unique<QQmlEngine>();
}

void TestMSwitch::cleanup() {
    m_probe.reset();
    m_component.reset();
    m_engine.reset();
}

QObject* TestMSwitch::loadProbe(const QByteArray& qml) {
    m_component = std::make_unique<QQmlComponent>(m_engine.get());
    m_component->setData(qml, QUrl());
    m_probe.reset(m_component->create());
    return m_probe.get();
}

void TestMSwitch::checkedDefaultsFalse() {
    QObject* probe = loadProbe(
        "import QtQuick\n"
        "import Margin.Ui.Primitives\n"
        "MSwitch {}\n");
    QVERIFY2(probe, qPrintable(QStringLiteral("probe failed: %1")
                                   .arg(m_component->errorString())));
    QCOMPARE(probe->property("checked").toBool(), false);
    QCOMPARE(probe->property("_trackColor").value<QColor>(),
             QColor(QStringLiteral("#3F3F46")));  // Theme.borderStrong
}

void TestMSwitch::setCheckedTrueUpdatesTrackColor() {
    QObject* probe = loadProbe(
        "import QtQuick\n"
        "import Margin.Ui.Primitives\n"
        "MSwitch { checked: true }\n");
    QVERIFY(probe);
    QCOMPARE(probe->property("checked").toBool(), true);
    QCOMPARE(probe->property("_trackColor").value<QColor>(),
             QColor(QStringLiteral("#7C3AED")));  // Theme.accentBrand
    QCOMPARE(probe->property("_thumbColor").value<QColor>(),
             QColor(QStringLiteral("#E4E4E7")));  // Theme.fgPrimary
}

void TestMSwitch::disabledTrackColorIsBgHover() {
    QObject* probe = loadProbe(
        "import QtQuick\n"
        "import Margin.Ui.Primitives\n"
        "MSwitch { enabled: false }\n");
    QVERIFY(probe);
    QCOMPARE(probe->property("_trackColor").value<QColor>(),
             QColor(QStringLiteral("#1F1F23")));  // Theme.bgHover
}

void TestMSwitch::thumbColorMutedWhenDisabled() {
    QObject* probe = loadProbe(
        "import QtQuick\n"
        "import Margin.Ui.Primitives\n"
        "MSwitch { enabled: false }\n");
    QVERIFY(probe);
    QCOMPARE(probe->property("_thumbColor").value<QColor>(),
             QColor(QStringLiteral("#71717A")));  // Theme.fgMuted
}

void TestMSwitch::checkedPropertyRoundTrip() {
    // Read-write property: external assignment must stick.
    QObject* probe = loadProbe(
        "import QtQuick\n"
        "import Margin.Ui.Primitives\n"
        "MSwitch {}\n");
    QVERIFY(probe);
    QVERIFY(probe->setProperty("checked", true));
    QCOMPARE(probe->property("checked").toBool(), true);
}

void TestMSwitch::toggledSignalIsConnectable() {
    // Signal exists + is connectable + fires when invoked (catches the
    // "signal not declared" regression; offscreen can't synthesize a real
    // MouseArea click — see test_mbutton.cpp:140 note for the same pattern).
    QObject* probe = loadProbe(
        "import QtQuick\n"
        "import Margin.Ui.Primitives\n"
        "MSwitch {}\n");
    QVERIFY(probe);
    QSignalSpy spy(probe, SIGNAL(toggled(bool)));
    QVERIFY(spy.isValid());
    QMetaObject::invokeMethod(probe, "toggled", Qt::AutoConnection,
                              Q_ARG(bool, true));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toBool(), true);
}

QTEST_MAIN(TestMSwitch)
#include "test_mswitch.moc"
