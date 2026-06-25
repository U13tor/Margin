// tests/unit/test_event_bus.cpp
//
// Exercises EventBus publish/subscribe + both wildcards + unsubscribeAll +
// topic validation. Per docs/09-testing.md §3.1 we use QTRY_COMPARE (spins
// the event loop until the condition holds or 5s elapses) rather than
// fixed QTest::qWait — slow CI does not flake.

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTest>

#include "Margin/EventBus.h"

using Margin::EventBus;

class TestEventBus : public QObject {
    Q_OBJECT

private slots:
    void testPublishSubscribe();
    void testWildcardStar();
    void testWildcardHash();
    void testUnsubscribeAll();
    void testTopicValidation();
};

void TestEventBus::testPublishSubscribe() {
    auto bus = EventBus::wire();
    int received = 0;
    bus->subscribe(QStringLiteral("margin.hello.ping"),
                   [&received](const QJsonObject&) { ++received; });
    bus->publish(QStringLiteral("margin.hello.ping"), QJsonObject{});
    QTRY_COMPARE(received, 1);
}

void TestEventBus::testWildcardStar() {
    auto bus = EventBus::wire();
    int received = 0;
    bus->subscribe(QStringLiteral("margin.hello.*"),
                   [&received](const QJsonObject&) { ++received; });
    bus->publish(QStringLiteral("margin.hello.ping"), QJsonObject{});
    QTRY_COMPARE(received, 1);
    // '*' must NOT span multiple segments.
    bus->publish(QStringLiteral("margin.hello.ping.deep"), QJsonObject{});
    QTest::qWait(50);
    QCOMPARE(received, 1);
}

void TestEventBus::testWildcardHash() {
    auto bus = EventBus::wire();
    int received = 0;
    bus->subscribe(QStringLiteral("margin.host.#"),
                   [&received](const QJsonObject&) { ++received; });
    bus->publish(QStringLiteral("margin.host.shutdown"), QJsonObject{});
    bus->publish(QStringLiteral("margin.host.config_changed"), QJsonObject{});
    QTRY_COMPARE(received, 2);
    // '#' does NOT match a bare prefix without a trailing segment.
    bus->publish(QStringLiteral("margin"), QJsonObject{});
    QTest::qWait(50);
    QCOMPARE(received, 2);
}

void TestEventBus::testUnsubscribeAll() {
    auto bus = EventBus::wire();
    int received = 0;
    QObject subscriber;
    bus->subscribe(QStringLiteral("margin.a.x"),
                   [&received](const QJsonObject&) { ++received; },
                   &subscriber);
    bus->subscribe(QStringLiteral("margin.a.y"),
                   [&received](const QJsonObject&) { ++received; },
                   &subscriber);
    bus->unsubscribeAll(&subscriber);
    bus->publish(QStringLiteral("margin.a.x"), QJsonObject{});
    bus->publish(QStringLiteral("margin.a.y"), QJsonObject{});
    QTest::qWait(50);
    QCOMPARE(received, 0);
    // unsubscribeAll(nullptr) is a documented no-op.
    bus->unsubscribeAll(nullptr);
}

void TestEventBus::testTopicValidation() {
    auto bus = EventBus::wire();
    int received = 0;
    // '#' must be the last filter segment — 'margin.#.bad' is malformed.
    bus->subscribe(QStringLiteral("margin.#.bad"),
                   [&received](const QJsonObject&) { ++received; });
    // publish topic must start with 'margin.'.
    bus->publish(QStringLiteral("invalid.topic"), QJsonObject{});
    QTest::qWait(50);
    QCOMPARE(received, 0);
}

QTEST_MAIN(TestEventBus)
#include "test_event_bus.moc"
