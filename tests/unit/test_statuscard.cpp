// tests/unit/test_statuscard.cpp
//
// M4-C7: verifies the StatusCard composite's API contract — title/value/
// subtitle property propagation, optional iconSource hide/show via the
// derived _hasIcon / _hasSubtitle flags, default extras slot population,
// and clicked() signal declaration. Mirrors test_mcard pattern but links
// both primitives + composite qml-module plugins (StatusCard composes
// MCard + MIcon atoms, so the Primitives URI must also resolve).

#include <QObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QSignalSpy>
#include <QString>
#include <QTest>
#include <QUrl>
#include <memory>

class TestStatusCard : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void defaultsRenderEmptyStrings();
    void titleValueSubtitlePropagate();
    void emptyIconSourceHidesSlot();
    void setIconSourceShowsSlot();
    void defaultSlotPopulatesExtras();
    void clickedSignalFires();

private:
    QObject* loadProbe(const QByteArray& qml);
    std::unique_ptr<QQmlEngine>    m_engine;
    std::unique_ptr<QQmlComponent> m_component;
    std::unique_ptr<QObject>       m_probe;
};

void TestStatusCard::init() {
    m_engine = std::make_unique<QQmlEngine>();
}

void TestStatusCard::cleanup() {
    m_probe.reset();
    m_component.reset();
    m_engine.reset();
}

QObject* TestStatusCard::loadProbe(const QByteArray& qml) {
    m_component = std::make_unique<QQmlComponent>(m_engine.get());
    m_component->setData(qml, QUrl());
    m_probe.reset(m_component->create());
    return m_probe.get();
}

void TestStatusCard::defaultsRenderEmptyStrings() {
    QObject* probe = loadProbe(
        "import QtQuick\n"
        "import Margin.Ui.Composite\n"
        "StatusCard { width: 160; height: 120 }\n");
    QVERIFY2(probe, qPrintable(QStringLiteral("probe failed: %1")
                                   .arg(m_component->errorString())));
    QCOMPARE(probe->property("title").toString(), QStringLiteral(""));
    QCOMPARE(probe->property("value").toString(), QStringLiteral(""));
    QCOMPARE(probe->property("subtitle").toString(), QStringLiteral(""));
    QCOMPARE(probe->property("iconSource").toString(), QStringLiteral(""));
    QCOMPARE(probe->property("_hasIcon").toBool(), false);
    QCOMPARE(probe->property("_hasSubtitle").toBool(), false);
}

void TestStatusCard::titleValueSubtitlePropagate() {
    QObject* probe = loadProbe(
        "import QtQuick\n"
        "import Margin.Ui.Composite\n"
        "StatusCard {\n"
        "  width: 160; height: 120\n"
        "  title: \"今日专注\"\n"
        "  value: \"2h 15m\"\n"
        "  subtitle: \"生产力 65%\"\n"
        "}\n");
    QVERIFY(probe);
    QCOMPARE(probe->property("title").toString(), QStringLiteral("今日专注"));
    QCOMPARE(probe->property("value").toString(), QStringLiteral("2h 15m"));
    QCOMPARE(probe->property("subtitle").toString(), QStringLiteral("生产力 65%"));
    QCOMPARE(probe->property("_hasSubtitle").toBool(), true);
}

void TestStatusCard::emptyIconSourceHidesSlot() {
    QObject* probe = loadProbe(
        "import QtQuick\n"
        "import Margin.Ui.Composite\n"
        "StatusCard { width: 160; height: 120; title: \"x\" }\n");
    QVERIFY(probe);
    QCOMPARE(probe->property("iconSource").toString(), QStringLiteral(""));
    QCOMPARE(probe->property("_hasIcon").toBool(), false);
}

void TestStatusCard::setIconSourceShowsSlot() {
    QObject* probe = loadProbe(
        "import QtQuick\n"
        "import Margin.Ui.Composite\n"
        "StatusCard {\n"
        "  width: 160; height: 120\n"
        "  title: \"x\"\n"
        "  iconSource: \"qrc:/icons/icon-pulse.svg\"\n"
        "}\n");
    QVERIFY(probe);
    QCOMPARE(probe->property("_hasIcon").toBool(), true);
}

void TestStatusCard::defaultSlotPopulatesExtras() {
    // Default extras alias must route caller's Items into the slot below
    // subtitle — findChild reaches them by objectName (same pattern as
    // test_mcard's children-slot test).
    QObject* probe = loadProbe(
        "import QtQuick\n"
        "import Margin.Ui.Composite\n"
        "StatusCard {\n"
        "  width: 160; height: 120\n"
        "  title: \"x\"; value: \"y\"; subtitle: \"z\"\n"
        "  Item { objectName: \"extra\"; width: 50; height: 12 }\n"
        "}\n");
    QVERIFY(probe);
    auto* extra = probe->findChild<QObject*>(QStringLiteral("extra"));
    QVERIFY2(extra, "default extras slot did not populate");
    QCOMPARE(extra->property("width").toInt(), 50);
}

void TestStatusCard::clickedSignalFires() {
    // Signal must be declared, connectable, and invokable. Doesn't verify
    // real mouse dispatch (offscreen can't synthesize real clicks without a
    // window) — just the signal contract. The MouseArea wiring inside
    // StatusCard.qml is visually inspected + covered by manual L4 smoke.
    QObject* probe = loadProbe(
        "import QtQuick\n"
        "import Margin.Ui.Composite\n"
        "StatusCard { width: 160; height: 120 }\n");
    QVERIFY(probe);
    QSignalSpy spy(probe, SIGNAL(clicked()));
    QVERIFY(spy.isValid());
    QMetaObject::invokeMethod(probe, "clicked");
    QCOMPARE(spy.count(), 1);
}

QTEST_MAIN(TestStatusCard)
#include "test_statuscard.moc"
