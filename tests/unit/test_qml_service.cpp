// tests/unit/test_qml_service.cpp
//
// M1-C6a: QmlServiceImpl minimal impl. Verifies:
//   - registerContextProperty exposes a QObject under a given name on the
//     engine's root context (looked up via contextForProperty or by
//     evaluating a QML expression that binds to the property).
//   - engine() returns the borrowed QQmlEngine pointer.
//   - registerType is callable without error (no-op stub for now).
//
// Does NOT load DashboardWindow.qml — uses a fresh QQmlEngine to keep the
// test hermetic.

#include <QObject>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QString>
#include <QTest>

#include "host/services/QmlServiceImpl.h"
#include "Margin/QmlService.h"

class TestQmlService : public QObject {
    Q_OBJECT

private slots:
    void registerContextProperty_makesObjectVisible();
    void engine_returnsBorrowedPointer();
    void registerType_isNoOp();
};

namespace {
class FakeStateObject : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString greeting READ greeting CONSTANT)
public:
    QString greeting() const { return QStringLiteral("hello from C++"); }
};
} // namespace

void TestQmlService::registerContextProperty_makesObjectVisible() {
    QQmlEngine engine;
    Margin::QmlServiceImpl svc(&engine);

    FakeStateObject obj;
    svc.registerContextProperty(QStringLiteral("auraState"), &obj);

    // Same pointer reachable via root context.
    QCOMPARE(engine.rootContext()->contextProperty(QStringLiteral("auraState")),
             QVariant::fromValue<QObject*>(&obj));

    // And resolvable from a QML expression.
    QQmlComponent comp(&engine);
    comp.setData("import QtQml 2.15; QtObject { property string g: auraState.greeting }",
                 QUrl());
    QVERIFY(!comp.isError());
    QObject* root = comp.create();
    QVERIFY(root);
    QCOMPARE(root->property("g").toString(), QStringLiteral("hello from C++"));
    delete root;
}

void TestQmlService::engine_returnsBorrowedPointer() {
    QQmlEngine engine;
    Margin::QmlServiceImpl svc(&engine);
    QCOMPARE(svc.engine(), &engine);
}

void TestQmlService::registerType_isNoOp() {
    QQmlEngine engine;
    Margin::QmlServiceImpl svc(&engine);
    // Stub — just verify it doesn't throw or abort.
    svc.registerType("Margin.Tests", 1, 0, "Fake");
}

QTEST_MAIN(TestQmlService)
#include "test_qml_service.moc"
