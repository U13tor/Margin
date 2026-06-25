// tests/unit/test_mlistitem.cpp
//
// M4-C6: verifies the MListItem atom's title/subtitle/iconSource properties,
// the derived _hasIcon / _hasSubtitle flags, default trailing slot population,
// and the default spacing. Mirrors test_mbutton pattern.

#include <QObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QString>
#include <QTest>
#include <QUrl>
#include <QVariant>
#include <memory>

class TestMListItem : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void titleAndSubtitleRender();
    void emptyIconSourceHidesSlot();
    void setIconSourceShowsSlot();
    void defaultSlotPopulatesTrailing();
    void allSlotsTogetherRender();
    void defaultsSpacingIsSpace2();

private:
    QObject* loadProbe(const QByteArray& qml);
    std::unique_ptr<QQmlEngine>    m_engine;
    std::unique_ptr<QQmlComponent> m_component;
    std::unique_ptr<QObject>       m_probe;
};

void TestMListItem::init() {
    m_engine = std::make_unique<QQmlEngine>();
}

void TestMListItem::cleanup() {
    m_probe.reset();
    m_component.reset();
    m_engine.reset();
}

QObject* TestMListItem::loadProbe(const QByteArray& qml) {
    m_component = std::make_unique<QQmlComponent>(m_engine.get());
    m_component->setData(qml, QUrl());
    m_probe.reset(m_component->create());
    return m_probe.get();
}

void TestMListItem::titleAndSubtitleRender() {
    QObject* probe = loadProbe(
        "import QtQuick\n"
        "import Margin.Ui.Primitives\n"
        "MListItem {\n"
        "  title: \"iPhone X\"\n"
        "  subtitle: \"RSSI: -52 dBm\"\n"
        "}\n");
    QVERIFY2(probe, qPrintable(QStringLiteral("probe failed: %1")
                                   .arg(m_component->errorString())));
    QCOMPARE(probe->property("title").toString(), QStringLiteral("iPhone X"));
    QCOMPARE(probe->property("subtitle").toString(), QStringLiteral("RSSI: -52 dBm"));
    QCOMPARE(probe->property("_hasSubtitle").toBool(), true);
}

void TestMListItem::emptyIconSourceHidesSlot() {
    QObject* probe = loadProbe(
        "import QtQuick\n"
        "import Margin.Ui.Primitives\n"
        "MListItem { title: \"x\" }\n");
    QVERIFY(probe);
    QCOMPARE(probe->property("iconSource").toString(), QStringLiteral(""));
    QCOMPARE(probe->property("_hasIcon").toBool(), false);
}

void TestMListItem::setIconSourceShowsSlot() {
    QObject* probe = loadProbe(
        "import QtQuick\n"
        "import Margin.Ui.Primitives\n"
        "MListItem {\n"
        "  title: \"x\"\n"
        "  iconSource: \"qrc:/icons/icon-bluetooth.svg\"\n"
        "}\n");
    QVERIFY(probe);
    QCOMPARE(probe->property("_hasIcon").toBool(), true);
}

void TestMListItem::defaultSlotPopulatesTrailing() {
    // Default trailing alias must route caller Items into the right-side slot.
    QObject* probe = loadProbe(
        "import QtQuick\n"
        "import Margin.Ui.Primitives\n"
        "MListItem {\n"
        "  title: \"x\"\n"
        "  Item { objectName: \"act\"; width: 32; height: 32 }\n"
        "}\n");
    QVERIFY(probe);
    auto* act = probe->findChild<QObject*>(QStringLiteral("act"));
    QVERIFY2(act, "default trailing slot did not populate");
    QCOMPARE(act->property("width").toInt(), 32);
}

void TestMListItem::allSlotsTogetherRender() {
    // Full probe: title + subtitle + iconSource + trailing Item. Must load
    // without error — catches regressions in RowLayout / Column layout /
    // default-slot interaction.
    QObject* probe = loadProbe(
        "import QtQuick\n"
        "import Margin.Ui.Primitives\n"
        "MListItem {\n"
        "  width: 300; height: 48\n"
        "  title: \"iPhone X\"\n"
        "  subtitle: \"AA:BB:CC · -72 dBm\"\n"
        "  iconSource: \"qrc:/icons/icon-bluetooth.svg\"\n"
        "  Item { objectName: \"act\"; width: 24; height: 24 }\n"
        "}\n");
    QVERIFY(probe);
    QVERIFY(probe->property("_hasIcon").toBool());
    QVERIFY(probe->property("_hasSubtitle").toBool());
    QVERIFY(probe->findChild<QObject*>(QStringLiteral("act")) != nullptr);
}

void TestMListItem::defaultsSpacingIsSpace2() {
    QObject* probe = loadProbe(
        "import QtQuick\n"
        "import Margin.Ui.Primitives\n"
        "MListItem { title: \"x\" }\n");
    QVERIFY(probe);
    QCOMPARE(probe->property("spacing").toInt(), 8);  // Theme.space2
}

QTEST_MAIN(TestMListItem)
#include "test_mlistitem.moc"
