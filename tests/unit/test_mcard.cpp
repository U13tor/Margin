// tests/unit/test_mcard.cpp
//
// M4-C6: verifies the MCard atom's defaults (padding / radius / elevation) +
// override behaviour + default children slot population. Mirrors test_mbutton
// pattern.

#include <QObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QString>
#include <QTest>
#include <QUrl>
#include <QVariant>
#include <memory>

class TestMCard : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void defaultsUseSpace3AndRadiusSm();
    void customPaddingOverrides();
    void customRadiusOverrides();
    void childrenSlotPopulates();

private:
    QObject* loadProbe(const QByteArray& qml);
    std::unique_ptr<QQmlEngine>    m_engine;
    std::unique_ptr<QQmlComponent> m_component;
    std::unique_ptr<QObject>       m_probe;
};

void TestMCard::init() {
    m_engine = std::make_unique<QQmlEngine>();
}

void TestMCard::cleanup() {
    m_probe.reset();
    m_component.reset();
    m_engine.reset();
}

QObject* TestMCard::loadProbe(const QByteArray& qml) {
    m_component = std::make_unique<QQmlComponent>(m_engine.get());
    m_component->setData(qml, QUrl());
    m_probe.reset(m_component->create());
    return m_probe.get();
}

void TestMCard::defaultsUseSpace3AndRadiusSm() {
    QObject* probe = loadProbe(
        "import QtQuick\n"
        "import Margin.Ui.Primitives\n"
        "MCard { width: 100; height: 100 }\n");
    QVERIFY2(probe, qPrintable(QStringLiteral("probe failed: %1")
                                   .arg(m_component->errorString())));
    QCOMPARE(probe->property("padding").toInt(), 12);    // Theme.space3
    QCOMPARE(probe->property("radius").toInt(), 4);      // Theme.radiusSm
    QCOMPARE(probe->property("elevation").toInt(), 0);
}

void TestMCard::customPaddingOverrides() {
    QObject* probe = loadProbe(
        "import QtQuick\n"
        "import Margin.Ui.Primitives\n"
        "MCard { width: 100; height: 100; padding: 24 }\n");
    QVERIFY(probe);
    QCOMPARE(probe->property("padding").toInt(), 24);
}

void TestMCard::customRadiusOverrides() {
    QObject* probe = loadProbe(
        "import QtQuick\n"
        "import Margin.Ui.Primitives\n"
        "MCard { width: 100; height: 100; radius: 12 }\n");
    QVERIFY(probe);
    QCOMPARE(probe->property("radius").toInt(), 12);
}

void TestMCard::childrenSlotPopulates() {
    // Default children alias must route caller's Items into the padded
    // content slot — findChild reaches them by objectName.
    QObject* probe = loadProbe(
        "import QtQuick\n"
        "import Margin.Ui.Primitives\n"
        "MCard {\n"
        "  width: 100; height: 100\n"
        "  Text { objectName: \"kid\"; text: \"hello\" }\n"
        "}\n");
    QVERIFY(probe);
    auto* kid = probe->findChild<QObject*>(QStringLiteral("kid"));
    QVERIFY2(kid, "default children slot did not populate");
    QCOMPARE(kid->property("text").toString(), QStringLiteral("hello"));
}

QTEST_MAIN(TestMCard)
#include "test_mcard.moc"
