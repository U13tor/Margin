// tests/unit/test_mslider.cpp
//
// M4-C5: verifies the MSlider atom's range/value contract (from / to / value
// defaults + clamping + stepSize snapping via the internal _setValueFromFraction
// entry point) and the moved(real) signal. Mirrors test_mbutton pattern.
//
// Drag is exercised through _setValueFromFraction — the same function the
// MouseArea onPressed/onPositionChanged call — so offscreen tests don't need
// to synthesize mouse events.

#include <QObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QSignalSpy>
#include <QString>
#include <QTest>
#include <QUrl>
#include <QVariant>
#include <memory>

class TestMSlider : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void defaultsFromZeroToOne();
    void setValueWithinRange();
    void stepSizeSnapsOnUpdate();
    void movedSignalFiresOnUpdate();
    void outOfRangeClamps();
    void disabledIgnoresUpdate();

private:
    QObject* loadProbe(const QByteArray& qml);
    std::unique_ptr<QQmlEngine>    m_engine;
    std::unique_ptr<QQmlComponent> m_component;
    std::unique_ptr<QObject>       m_probe;
};

void TestMSlider::init() {
    m_engine = std::make_unique<QQmlEngine>();
}

void TestMSlider::cleanup() {
    m_probe.reset();
    m_component.reset();
    m_engine.reset();
}

QObject* TestMSlider::loadProbe(const QByteArray& qml) {
    m_component = std::make_unique<QQmlComponent>(m_engine.get());
    m_component->setData(qml, QUrl());
    m_probe.reset(m_component->create());
    return m_probe.get();
}

void TestMSlider::defaultsFromZeroToOne() {
    QObject* probe = loadProbe(
        "import QtQuick\n"
        "import Margin.Ui.Primitives\n"
        "MSlider {}\n");
    QVERIFY2(probe, qPrintable(QStringLiteral("probe failed: %1")
                                   .arg(m_component->errorString())));
    QCOMPARE(probe->property("from").toReal(), 0.0);
    QCOMPARE(probe->property("to").toReal(), 1.0);
    QCOMPARE(probe->property("value").toReal(), 0.0);
    QCOMPARE(probe->property("stepSize").toReal(), 0.0);
    QCOMPARE(probe->property("_clampedFraction").toReal(), 0.0);
}

void TestMSlider::setValueWithinRange() {
    QObject* probe = loadProbe(
        "import QtQuick\n"
        "import Margin.Ui.Primitives\n"
        "MSlider { from: 0.0; to: 10.0; value: 5.0 }\n");
    QVERIFY(probe);
    QCOMPARE(probe->property("value").toReal(), 5.0);
    QCOMPARE(probe->property("_clampedFraction").toReal(), 0.5);
}

void TestMSlider::stepSizeSnapsOnUpdate() {
    // _setValueFromFraction routes both click + drag; stepSize rounds to
    // nearest multiple of stepSize (offset from `from`).
    QObject* probe = loadProbe(
        "import QtQuick\n"
        "import Margin.Ui.Primitives\n"
        "MSlider { from: 0.0; to: 10.0; stepSize: 2.0 }\n");
    QVERIFY(probe);
    // Fraction 0.55 → raw value 5.5 → snap to nearest 2-mult → 6.
    // QML JS functions expose QVariant to the meta-system (no `real` annotation
    // in QML 6.5 — qmlcachegen rejects `function(real f)`), so the test passes
    // a QVariant-wrapped real here. Inside the atom, `f` is used numerically.
    QMetaObject::invokeMethod(probe, "_setValueFromFraction",
                              Qt::AutoConnection,
                              Q_ARG(QVariant, QVariant(0.55)));
    QCOMPARE(probe->property("value").toReal(), 6.0);
}

void TestMSlider::movedSignalFiresOnUpdate() {
    QObject* probe = loadProbe(
        "import QtQuick\n"
        "import Margin.Ui.Primitives\n"
        "MSlider { from: 0.0; to: 10.0 }\n");
    QVERIFY(probe);
    QSignalSpy spy(probe, SIGNAL(moved(qreal)));
    QVERIFY(spy.isValid());
    QMetaObject::invokeMethod(probe, "_setValueFromFraction",
                              Qt::AutoConnection,
                              Q_ARG(QVariant, QVariant(0.5)));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toReal(), 5.0);
}

void TestMSlider::outOfRangeClamps() {
    // Caller writes out-of-range value → _clampedFraction clamps to [0,1].
    // The atom must NOT crash; the visual stays at the rail end.
    QObject* probe = loadProbe(
        "import QtQuick\n"
        "import Margin.Ui.Primitives\n"
        "MSlider { from: 0.0; to: 10.0; value: -5.0 }\n");
    QVERIFY(probe);
    QCOMPARE(probe->property("_clampedFraction").toReal(), 0.0);

    QVERIFY(probe->setProperty("value", 99.0));
    QCOMPARE(probe->property("_clampedFraction").toReal(), 1.0);
}

void TestMSlider::disabledIgnoresUpdate() {
    // enabled=false → _setValueFromFraction must early-return without
    // mutating value or emitting moved.
    QObject* probe = loadProbe(
        "import QtQuick\n"
        "import Margin.Ui.Primitives\n"
        "MSlider { from: 0.0; to: 10.0; value: 3.0; enabled: false }\n");
    QVERIFY(probe);
    QSignalSpy spy(probe, SIGNAL(moved(qreal)));
    QMetaObject::invokeMethod(probe, "_setValueFromFraction",
                              Qt::AutoConnection,
                              Q_ARG(QVariant, QVariant(0.8)));
    QCOMPARE(probe->property("value").toReal(), 3.0);
    QCOMPARE(spy.count(), 0);
}

QTEST_MAIN(TestMSlider)
#include "test_mslider.moc"
