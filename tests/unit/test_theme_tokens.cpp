// tests/unit/test_theme_tokens.cpp
//
// M4-C1: verifies the Theme design-token singleton exposes the full
// docs/06-ui-design.md §2 token set — the new type step (text2xl), the
// 8-multiple spacing scale, radii, motion (durations + easing), and the
// sans/mono font stacks — with the spec values. A spacing/radius/motion typo
// in Theme.qml would slip past the shell-load test (which only reads colours
// + version), so this guards the token contract directly.
//
// Mechanism: load an inline QML probe that imports Margin.Ui.Primitives and
// pulls Theme.* onto its own properties, then read them back from C++. The
// STATIC primitives qml-module plugin is linked + qt_import_qml_plugins'd so
// `import Margin.Ui.Primitives` resolves; runs offscreen (headless on CI).

#include <QColor>
#include <QObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QString>
#include <QTest>
#include <QUrl>
#include <QVariantList>

#include <memory>

class TestThemeTokens : public QObject {
    Q_OBJECT

    // One probe reading every token under test. Kept on the members so the
    // engine/component outlive each test method's property reads.
    QObject* loadProbe() {
        m_engine = std::make_unique<QQmlEngine>();
        m_component = std::make_unique<QQmlComponent>(m_engine.get());
        m_component->setData(QByteArrayLiteral(
            "import QtQuick\n"
            "import Margin.Ui.Primitives\n"
            "QtObject {\n"
            "  property color bgBase: Theme.bgBase\n"
            "  property int textXl: Theme.textXl\n"
            "  property int text2xl: Theme.text2xl\n"
            "  property int text3xl: Theme.text3xl\n"
            "  property int space1: Theme.space1\n"
            "  property int space4: Theme.space4\n"
            "  property int space12: Theme.space12\n"
            "  property int radiusSm: Theme.radiusSm\n"
            "  property int radiusMd: Theme.radiusMd\n"
            "  property int radiusLg: Theme.radiusLg\n"
            "  property int radiusFull: Theme.radiusFull\n"
            "  property int durationFast: Theme.durationFast\n"
            "  property int durationNormal: Theme.durationNormal\n"
            "  property int durationSlow: Theme.durationSlow\n"
            "  property var easeOut: Theme.easeOut\n"
            "  property var fontSans: Theme.fontSans\n"
            "  property var fontMono: Theme.fontMono\n"
            "  property bool bogusUndef: (typeof Theme.__nope__ === \"undefined\")\n"
            "}\n"),
            QUrl(QStringLiteral("qrc:/test/theme_probe.qml")));
        m_probe.reset(m_component->create());
        return m_probe.get();
    }

private slots:
    void init();  // load a fresh probe per test
    void typeScaleHasText2xl();
    void spacingTokensPresent();
    void radiusTokensPresent();
    void motionTokensPresent();
    void fontStacksPresent();
    void existingColourStillResolves();
    void unknownTokenIsUndefined();  // failure-path

private:
    std::unique_ptr<QQmlEngine>    m_engine;
    std::unique_ptr<QQmlComponent> m_component;
    std::unique_ptr<QObject>       m_probe;
};

void TestThemeTokens::init() {
    QObject* probe = loadProbe();
    QVERIFY2(probe, qPrintable(QStringLiteral("probe failed to create: %1")
                                   .arg(m_component->errorString())));
}

void TestThemeTokens::typeScaleHasText2xl() {
    // text2xl (24px) was the missing rung between textXl (20) and text3xl (32).
    QCOMPARE(m_probe->property("text2xl").toInt(), 24);
    QVERIFY(m_probe->property("textXl").toInt() < m_probe->property("text2xl").toInt());
    QVERIFY(m_probe->property("text2xl").toInt() < m_probe->property("text3xl").toInt());
}

void TestThemeTokens::spacingTokensPresent() {
    QCOMPARE(m_probe->property("space1").toInt(), 4);
    QCOMPARE(m_probe->property("space4").toInt(), 16);
    QCOMPARE(m_probe->property("space12").toInt(), 48);
}

void TestThemeTokens::radiusTokensPresent() {
    QCOMPARE(m_probe->property("radiusSm").toInt(), 4);
    QCOMPARE(m_probe->property("radiusMd").toInt(), 8);
    QCOMPARE(m_probe->property("radiusLg").toInt(), 12);
    QCOMPARE(m_probe->property("radiusFull").toInt(), 9999);
}

void TestThemeTokens::motionTokensPresent() {
    QCOMPARE(m_probe->property("durationFast").toInt(), 100);
    QCOMPARE(m_probe->property("durationNormal").toInt(), 200);
    QCOMPARE(m_probe->property("durationSlow").toInt(), 300);

    // easeOut is the bezierCurve-ready control-point list (spec 0.16,1,0.3,1).
    const QVariantList ease = m_probe->property("easeOut").toList();
    QCOMPARE(ease.size(), 6);
    QCOMPARE(ease.at(0).toDouble(), 0.16);
    QCOMPARE(ease.at(2).toDouble(), 0.30);
}

void TestThemeTokens::fontStacksPresent() {
    // fontSans/fontMono are primary-family strings (Qt 6.5 QML `font` value
    // type only exposes `family` singular, not the QFont `families` list —
    // see Theme.qml §2.2 comment). Glyph fallback for CJK chars happens at
    // the QFontDatabase level when Inter is missing a glyph.
    QCOMPARE(m_probe->property("fontSans").toString(), QStringLiteral("Inter"));
    QCOMPARE(m_probe->property("fontMono").toString(), QStringLiteral("JetBrains Mono"));
}

void TestThemeTokens::existingColourStillResolves() {
    // Guard against the token expansion accidentally breaking the colour layer.
    QCOMPARE(m_probe->property("bgBase").value<QColor>(), QColor(QStringLiteral("#0E0E10")));
}

void TestThemeTokens::unknownTokenIsUndefined() {
    // The singleton must NOT silently answer arbitrary token names — a missing
    // token reads back undefined, which is what lets the value asserts above
    // mean something.
    QCOMPARE(m_probe->property("bogusUndef").toBool(), true);
}

QTEST_MAIN(TestThemeTokens)
#include "test_theme_tokens.moc"
