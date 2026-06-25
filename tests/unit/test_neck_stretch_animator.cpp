// test_neck_stretch_animator — verifies the 8-frame stretch animation
// resource bundle + NeckStretchAnimator QML surface (M4-C13b).
//
// Coverage:
//   - All 8 qrc:/rhythm/icons/neck-stretch-{N}.png resources load to a
//     non-null QImage. Note: frames are mixed-format on disk (some PNG,
//     some JPEG-encoded) — the test reads raw qrc bytes and uses
//     QImage::fromData so format detection follows content, not suffix.
//     QML Image uses the same content-sniffing path at runtime via
//     QQuickImageProvider, so production behavior matches the test.
//   - NeckStretchAnimator.qml instantiates and exposes currentStep,
//     stepNames, currentName, currentImage properties.
//   - For each currentStep in 1..8, currentName matches the expected
//     Chinese pose name and currentImage resolves to the right qrc URL.
//
// Mirrors test_rhythm_tab offscreen pattern — links rhythm.qrc + host.qrc
// so qrc:/rhythm/... URLs resolve, instantiates NeckStretchAnimator via
// QQmlComponent.

#include <QFile>
#include <QImage>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QString>
#include <QTest>

class TestNeckStretchAnimator : public QObject {
    Q_OBJECT

private slots:
    void eightFramesAreLoadable_data();
    void eightFramesAreLoadable();
    void animatorInstantiates();
    void animatorExposesExpectedCurrentName();
    void animatorExposesExpectedCurrentImage();
};

void TestNeckStretchAnimator::eightFramesAreLoadable_data() {
    QTest::addColumn<QString>("path");
    for (int i = 1; i <= 8; ++i) {
        QTest::newRow(QString("frame-%1").arg(i).toUtf8().constData())
            << QStringLiteral("qrc:/rhythm/icons/neck-stretch-%1.png").arg(i);
    }
}

void TestNeckStretchAnimator::eightFramesAreLoadable() {
    QFETCH(const QString, path);
    // Read raw bytes from qrc, then fromData() — content sniffing. Several
    // frames have .png extension but are JPEG-encoded (asset export quirk);
    // QImage(path) uses suffix to pick a decoder and fails on the mismatch,
    // but QML Image at runtime sniffs bytes via QQuickImageProvider. This
    // test uses the same sniffing path to mirror production.
    // Note: QFile wants the ":/..." resource path, not the "qrc:/..." URL
    // (qrc scheme is QML-only). Strip the scheme here.
    const QString qrcPath = path.startsWith(QStringLiteral("qrc:"))
        ? path.mid(3)  // "qrc:" is 4 chars, keep everything after the colon
        : path;
    QFile f(qrcPath);
    QVERIFY2(f.open(QIODevice::ReadOnly),
             qPrintable(QStringLiteral("Cannot open qrc resource: %1").arg(path)));
    const QByteArray bytes = f.readAll();
    QVERIFY(!bytes.isEmpty());
    const QImage img = QImage::fromData(bytes);
    QVERIFY2(!img.isNull(),
             qPrintable(QStringLiteral("Neck stretch frame failed to load: %1").arg(path)));
    QCOMPARE(img.width(), img.height());  // all frames are square
}

void TestNeckStretchAnimator::animatorInstantiates() {
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/rhythm/qml/NeckStretchAnimator.qml")));
    QVERIFY2(!component.isError(), qPrintable(component.errorString()));
    QScopedPointer<QObject> obj(component.create());
    QVERIFY(obj);
}

void TestNeckStretchAnimator::animatorExposesExpectedCurrentName() {
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/rhythm/qml/NeckStretchAnimator.qml")));
    QVERIFY2(!component.isError(), qPrintable(component.errorString()));
    QScopedPointer<QObject> obj(component.create());
    QVERIFY(obj);

    const QStringList expectedNames = {
        QStringLiteral("双掌擦颈"), QStringLiteral("左顾右盼"),
        QStringLiteral("前后点头"), QStringLiteral("青龙摆尾"),
        QStringLiteral("旋肩舒颈"), QStringLiteral("头手相抗"),
        QStringLiteral("颈项争力"), QStringLiteral("仰头望掌"),
    };
    for (int step = 1; step <= 8; ++step) {
        obj->setProperty("currentStep", step);
        QCOMPARE(obj->property("currentStep").toInt(), step);
        QCOMPARE(obj->property("currentName").toString(), expectedNames.at(step - 1));
    }
}

void TestNeckStretchAnimator::animatorExposesExpectedCurrentImage() {
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/rhythm/qml/NeckStretchAnimator.qml")));
    QVERIFY2(!component.isError(), qPrintable(component.errorString()));
    QScopedPointer<QObject> obj(component.create());
    QVERIFY(obj);

    for (int step = 1; step <= 8; ++step) {
        obj->setProperty("currentStep", step);
        QCOMPARE(obj->property("currentImage").toString(),
                 QStringLiteral("qrc:/rhythm/icons/neck-stretch-%1.png").arg(step));
    }
}

QTEST_MAIN(TestNeckStretchAnimator)
#include "test_neck_stretch_animator.moc"
