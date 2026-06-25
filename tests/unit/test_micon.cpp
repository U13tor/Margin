// tests/unit/test_micon.cpp
//
// M4-C3: verifies the MIcon atom (Image + MultiEffect tinting) loads a valid
// qrc source to Image.Ready, that an invalid source reports Error without
// crashing, and that the default color tracks Theme.fgPrimary. Mirrors the
// test_theme_tokens pattern: inline QML probe + QML property read-back from
// C++. Runs offscreen. Links the Primitives qml-module plugin + host.qrc so
// qrc:/icons/icon-bell.svg resolves.

#include <QColor>
#include <QObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QString>
#include <QTest>
#include <QUrl>
#include <memory>

class TestMIcon : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void validSourceLoadsToReady();
    void invalidSourceDoesNotCrash();
    void defaultColorIsFgPrimary();
    void customColorOverrides();

private:
    QObject* loadProbe(const QByteArray& qml);
    std::unique_ptr<QQmlEngine>    m_engine;
    std::unique_ptr<QQmlComponent> m_component;
    std::unique_ptr<QObject>       m_probe;
};

void TestMIcon::init() {
    m_engine = std::make_unique<QQmlEngine>();
}

void TestMIcon::cleanup() {
    m_probe.reset();
    m_component.reset();
    m_engine.reset();
}

QObject* TestMIcon::loadProbe(const QByteArray& qml) {
    m_component = std::make_unique<QQmlComponent>(m_engine.get());
    m_component->setData(qml, QUrl());
    m_probe.reset(m_component->create());
    return m_probe.get();
}

void TestMIcon::validSourceLoadsToReady() {
    // MIcon exposes imageStatus (Image.status mirror). Image.Ready = 1. SVG
    // loads via qrc synchronously in practice; we pump the loop defensively
    // for async image backends.
    QObject* probe = loadProbe(
        "import QtQuick\n"
        "import QtQuick.Effects\n"
        "import Margin.Ui.Primitives\n"
        "MIcon {\n"
        "  source: \"qrc:/icons/icon-bell.svg\"\n"
        "  size: 16\n"
        "}\n");
    QVERIFY2(probe, qPrintable(QStringLiteral("probe failed: %1")
                                   .arg(m_component->errorString())));

    for (int i = 0; i < 50; ++i) {
        if (probe->property("imageStatus").toInt() == 1) break;
        QTest::qWait(10);
    }
    QCOMPARE(probe->property("imageStatus").toInt(), 1);  // Image.Ready
}

void TestMIcon::invalidSourceDoesNotCrash() {
    // Invalid source → Image.status = 3 (Error). No exception, no crash;
    // MIcon itself stays alive.
    QObject* probe = loadProbe(
        "import QtQuick\n"
        "import QtQuick.Effects\n"
        "import Margin.Ui.Primitives\n"
        "MIcon {\n"
        "  source: \"qrc:/icons/__definitely_not_there__.svg\"\n"
        "  size: 16\n"
        "}\n");
    QVERIFY2(probe, qPrintable(QStringLiteral("probe failed: %1")
                                   .arg(m_component->errorString())));

    for (int i = 0; i < 50; ++i) {
        const int s = probe->property("imageStatus").toInt();
        if (s == 3) break;  // Error — done waiting
        QTest::qWait(10);
    }
    QCOMPARE(probe->property("imageStatus").toInt(), 3);  // Image.Error
}

void TestMIcon::defaultColorIsFgPrimary() {
    // Without an explicit `color:` the atom must default to Theme.fgPrimary
    // so buttons / status bars inherit primary foreground automatically.
    QObject* probe = loadProbe(
        "import QtQuick\n"
        "import QtQuick.Effects\n"
        "import Margin.Ui.Primitives\n"
        "MIcon {\n"
        "  source: \"qrc:/icons/icon-bell.svg\"\n"
        "  size: 16\n"
        "}\n");
    QVERIFY(probe);
    QCOMPARE(probe->property("color").value<QColor>(),
             QColor(QStringLiteral("#E4E4E7")));  // Theme.fgPrimary
}

void TestMIcon::customColorOverrides() {
    // Specifying `color:` overrides the default.
    QObject* probe = loadProbe(
        "import QtQuick\n"
        "import QtQuick.Effects\n"
        "import Margin.Ui.Primitives\n"
        "MIcon {\n"
        "  source: \"qrc:/icons/icon-bell.svg\"\n"
        "  size: 16\n"
        "  color: \"#FF0000\"\n"
        "}\n");
    QVERIFY(probe);
    QCOMPARE(probe->property("color").value<QColor>(),
             QColor(QStringLiteral("#FF0000")));
}

QTEST_MAIN(TestMIcon)
#include "test_micon.moc"
