// tests/unit/test_mbutton.cpp
//
// M4-C4: verifies the MButton atom's variant color matrix (Primary /
// Secondary / Ghost), the icon slot wiring (iconSource → MIcon child), and
// the clicked() signal contract. Mirrors test_micon pattern. Runs offscreen.
// Links the Primitives qml-module plugin + host.qrc so qrc:/icons/icon-play.svg
// resolves.

#include <QColor>
#include <QObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QSignalSpy>
#include <QString>
#include <QTest>
#include <QUrl>
#include <memory>

class TestMButton : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void primaryVariantUsesBrandBackground();
    void secondaryVariantUsesTransparentBackground();
    void ghostVariantUsesTransparentBackground();
    void iconSlotLoadsToReady();
    void disabledStateUsesMutedForeground();
    void clickEmitsClickedSignal();

private:
    QObject* loadProbe(const QByteArray& qml);
    std::unique_ptr<QQmlEngine>    m_engine;
    std::unique_ptr<QQmlComponent> m_component;
    std::unique_ptr<QObject>       m_probe;
};

void TestMButton::init() {
    m_engine = std::make_unique<QQmlEngine>();
}

void TestMButton::cleanup() {
    m_probe.reset();
    m_component.reset();
    m_engine.reset();
}

QObject* TestMButton::loadProbe(const QByteArray& qml) {
    m_component = std::make_unique<QQmlComponent>(m_engine.get());
    m_component->setData(qml, QUrl());
    m_probe.reset(m_component->create());
    return m_probe.get();
}

void TestMButton::primaryVariantUsesBrandBackground() {
    QObject* probe = loadProbe(
        "import QtQuick\n"
        "import Margin.Ui.Primitives\n"
        "MButton {\n"
        "  variant: MButton.Variant.Primary\n"
        "  text: \"Apply\"\n"
        "}\n");
    QVERIFY2(probe, qPrintable(QStringLiteral("probe failed: %1")
                                   .arg(m_component->errorString())));
    QCOMPARE(probe->property("_bg").value<QColor>(),
             QColor(QStringLiteral("#7C3AED")));  // Theme.accentBrand
    QCOMPARE(probe->property("_fg").value<QColor>(),
             QColor(QStringLiteral("#E4E4E7")));  // Theme.fgPrimary
    QCOMPARE(probe->property("_hasBorder").toBool(), false);
}

void TestMButton::secondaryVariantUsesTransparentBackground() {
    QObject* probe = loadProbe(
        "import QtQuick\n"
        "import Margin.Ui.Primitives\n"
        "MButton {\n"
        "  variant: MButton.Variant.Secondary\n"
        "  text: \"Cancel\"\n"
        "}\n");
    QVERIFY(probe);
    QCOMPARE(probe->property("_bg").value<QColor>(),
             QColor(QStringLiteral("#00000000")));  // transparent
    QCOMPARE(probe->property("_hasBorder").toBool(), true);
}

void TestMButton::ghostVariantUsesTransparentBackground() {
    QObject* probe = loadProbe(
        "import QtQuick\n"
        "import Margin.Ui.Primitives\n"
        "MButton {\n"
        "  variant: MButton.Variant.Ghost\n"
        "  text: \"Skip\"\n"
        "}\n");
    QVERIFY(probe);
    QCOMPARE(probe->property("_bg").value<QColor>(),
             QColor(QStringLiteral("#00000000")));
    QCOMPARE(probe->property("_fg").value<QColor>(),
             QColor(QStringLiteral("#A1A1AA")));  // Theme.fgSecondary
    QCOMPARE(probe->property("_hasBorder").toBool(), false);
}

void TestMButton::iconSlotLoadsToReady() {
    // When iconSource is set, MButton instantiates an internal MIcon; the
    // icon must reach Image.Ready. We can't reach the MIcon instance
    // directly from outside, but we can confirm the MButton instantiates
    // without errors and the iconSource property sticks. The MIcon load
    // itself is covered by test_micon.
    QObject* probe = loadProbe(
        "import QtQuick\n"
        "import Margin.Ui.Primitives\n"
        "MButton {\n"
        "  variant: MButton.Variant.Primary\n"
        "  text: \"Play\"\n"
        "  iconSource: \"qrc:/icons/icon-play.svg\"\n"
        "}\n");
    QVERIFY(probe);
    QCOMPARE(probe->property("iconSource").toString(),
             QStringLiteral("qrc:/icons/icon-play.svg"));
}

void TestMButton::disabledStateUsesMutedForeground() {
    // enabled=false must mute foreground + suppress border so a disabled
    // Primary doesn't read as "active but unclickable".
    QObject* probe = loadProbe(
        "import QtQuick\n"
        "import Margin.Ui.Primitives\n"
        "MButton {\n"
        "  variant: MButton.Variant.Primary\n"
        "  text: \"Apply\"\n"
        "  enabled: false\n"
        "}\n");
    QVERIFY(probe);
    QCOMPARE(probe->property("_fg").value<QColor>(),
             QColor(QStringLiteral("#71717A")));  // Theme.fgMuted
    QCOMPARE(probe->property("_hasBorder").toBool(), false);
}

void TestMButton::clickEmitsClickedSignal() {
    // clicked() must fire when the MouseArea reports a click. We don't
    // synthesize a real mouse event (offscreen + no window); we verify the
    // signal exists, is connectable, and fires when manually invoked — this
    // catches the "signal not declared" regression.
    QObject* probe = loadProbe(
        "import QtQuick\n"
        "import Margin.Ui.Primitives\n"
        "MButton {\n"
        "  variant: MButton.Variant.Primary\n"
        "  text: \"Apply\"\n"
        "}\n");
    QVERIFY(probe);
    QSignalSpy spy(probe, SIGNAL(clicked()));
    QVERIFY(spy.isValid());
    QMetaObject::invokeMethod(probe, "clicked");
    QCOMPARE(spy.count(), 1);
}

QTEST_MAIN(TestMButton)
#include "test_mbutton.moc"
