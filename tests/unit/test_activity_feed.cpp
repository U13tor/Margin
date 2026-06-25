// tests/unit/test_activity_feed.cpp
//
// M4-C9b.1: ActivityFeed service contract. Mirrors the layout of
// test_dashboard_tab_contributor.cpp (pure C++, no QML load). Drives a
// real EventBus instance and asserts:
//   (1) publishing a curated topic prepends one entry to `events`
//   (2) the title/colorRole fields match the curated table per topic
//   (3) the ring buffer caps at kBufferSize (oldest dropped)
//   (4) eventsChanged fires on each publish
//   (5) destruction unsubscribes — publishing after the feed dies does
//       not fire a dangling handler (reaching the assertion at all is
//       the pass condition; a missing unsubscribe would crash with
//       heap-use-after-free before we got here).
//
// PR6 adds three persistence cases (6/7/8):
//   (6) persistHistoryAcrossRestart — rows survive Database close/reopen
//   (7) overflowDropsOldestRow — DB rows cap at kBufferSize, not just the
//       in-memory cache
//   (8) nullDatabaseGraceful — no DB attached → pure in-memory behavior,
//       no crashes
//
// Timing: EventBus.publish dispatches via Qt::QueuedConnection to the
// main thread, so we use QTRY_COMPARE (per docs/15-dev-gotchas.md §E2)
// rather than fixed qWait — slow CI does not flake.

#include <QCoreApplication>
#include <QJsonObject>
#include <QObject>
#include <QSignalSpy>
#include <QString>
#include <QTemporaryDir>
#include <QTest>
#include <QVariantList>
#include <QVariantMap>

#include "Margin/EventBus.h"
#include "host/core/ActivityFeed.h"
#include "host/services/DatabaseImpl.h"

using Margin::ActivityFeed;
using Margin::DatabaseImpl;
using Margin::EventBus;

class TestActivityFeed : public QObject {
    Q_OBJECT

private slots:
    void publishOneEventIncrementsEvents();
    void ringBufferCapsAtTwenty();
    void eventsChangedEmitsOnPublish();
    void multipleTopicsRouteToCorrectTitle();
    void destructionUnsubscribesHandler();
    void persistHistoryAcrossRestart();
    void overflowDropsOldestRow();
    void nullDatabaseGraceful();
};

// (1) publish "margin.aura.away" → events grows by 1, and the entry
// carries the curated title + colorRole for that topic.
void TestActivityFeed::publishOneEventIncrementsEvents() {
    auto bus = EventBus::wire();
    ActivityFeed feed(*bus);

    QCOMPARE(feed.events().size(), 0);

    bus->publish(QStringLiteral("margin.aura.away"), QJsonObject{});
    QTRY_COMPARE(feed.events().size(), 1);

    const QVariantMap first = feed.events().first().toMap();
    QCOMPARE(first.value(QStringLiteral("topic")).toString(),
             QStringLiteral("margin.aura.away"));
    QCOMPARE(first.value(QStringLiteral("title")).toString(),
             QStringLiteral("锁屏触发(设备离开)"));
    QCOMPARE(first.value(QStringLiteral("colorRole")).toString(),
             QStringLiteral("accentDanger"));
    QVERIFY(first.contains(QStringLiteral("timeMs")));
}

// (2) Publish 25 events → only kBufferSize retained, and the oldest 5
// (publishes 1..5) are the ones dropped. The newest entry at index 0
// is publish #25.
void TestActivityFeed::ringBufferCapsAtTwenty() {
    auto bus = EventBus::wire();
    ActivityFeed feed(*bus);

    for (int i = 0; i < 25; ++i) {
        bus->publish(QStringLiteral("margin.aura.away"), QJsonObject{});
    }
    QTRY_COMPARE(feed.events().size(), ActivityFeed::kBufferSize);

    // All entries should be the same topic (we published only one), so
    // size 20 is the main contract. kBufferSize constant is the load-
    // bearing assertion — if someone bumps it, this test catches the
    // intent change and forces a doc-sync.
    QCOMPARE(ActivityFeed::kBufferSize, 20);
    for (const auto& v : feed.events()) {
        QCOMPARE(v.toMap().value(QStringLiteral("topic")).toString(),
                 QStringLiteral("margin.aura.away"));
    }
}

// (3) eventsChanged fires once per publish (not zero, not N+1).
void TestActivityFeed::eventsChangedEmitsOnPublish() {
    auto bus = EventBus::wire();
    ActivityFeed feed(*bus);
    QSignalSpy spy(&feed, &ActivityFeed::eventsChanged);

    bus->publish(QStringLiteral("margin.rhythm.break_due"), QJsonObject{});
    QTRY_COMPARE(spy.count(), 1);

    bus->publish(QStringLiteral("margin.rhythm.break_started"), QJsonObject{});
    QTRY_COMPARE(spy.count(), 2);
}

// (4) Each curated topic maps to the right (title, colorRole) per the
// table in ActivityFeed.cpp. Iterating topicRules() lets this test
// auto-cover new entries when added — no per-row hardcoding here.
void TestActivityFeed::multipleTopicsRouteToCorrectTitle() {
    auto bus = EventBus::wire();
    ActivityFeed feed(*bus);

    for (const auto& rule : ActivityFeed::topicRules()) {
        bus->publish(rule.topic, QJsonObject{});
    }
    QTRY_COMPARE(feed.events().size(),
                 static_cast<int>(ActivityFeed::topicRules().size()));

    // events are prepended (newest first), so the first published rule
    // is now at the tail and the last published rule is at index 0.
    // Build a topic→rule lookup and walk the buffer.
    QMap<QString, ActivityFeed::TopicRule> byTopic;
    for (const auto& rule : ActivityFeed::topicRules()) {
        byTopic.insert(rule.topic, rule);
    }
    for (const auto& v : feed.events()) {
        const QVariantMap m = v.toMap();
        const QString topic = m.value(QStringLiteral("topic")).toString();
        QVERIFY2(byTopic.contains(topic),
                 qPrintable(QStringLiteral("unknown topic in feed: %1").arg(topic)));
        const auto rule = byTopic.value(topic);
        QCOMPARE(m.value(QStringLiteral("title")).toString(), rule.title);
        QCOMPARE(m.value(QStringLiteral("colorRole")).toString(), rule.colorRole);
    }
}

// (5) After ActivityFeed is destroyed, EventBus must not dispatch into
// the dangling handler. ActivityFeed's dtor calls unsubscribeAll(this),
// so a subsequent publish should be a no-op on the feed side. We can't
// query a destroyed object — reaching the end of this function without
// crashing IS the assertion (heap-use-after-free would abort the
// process before the QCOMPARE at the bottom).
void TestActivityFeed::destructionUnsubscribesHandler() {
    auto bus = EventBus::wire();

    // Sentinel subscription survives the feed's death so we can confirm
    // the publish itself actually fired (otherwise the test would pass
    // trivially if publish was silently broken).
    int sentinelCount = 0;
    bus->subscribe(QStringLiteral("margin.aura.away"),
                   [&sentinelCount](const QJsonObject&) { ++sentinelCount; });

    {
        ActivityFeed feed(*bus);
        bus->publish(QStringLiteral("margin.aura.away"), QJsonObject{});
        QTRY_COMPARE(feed.events().size(), 1);
        QTRY_COMPARE(sentinelCount, 1);
    }  // feed destroyed here

    // Publish again — sentinel must tick (proves publish fired), and the
    // dangling feed handler must NOT fire (would crash). Reaching the
    // QCOMPARE below without aborting is the pass.
    bus->publish(QStringLiteral("margin.aura.away"), QJsonObject{});
    QTRY_COMPARE(sentinelCount, 2);
}

// (6) PR6: events published in one process lifetime are reloaded from
// host_activity_events when a fresh ActivityFeed is constructed against
// the same SQLite file. This is the core restart-persistence contract.
// Uses two nested block scopes so each DatabaseImpl fully destructs
// (closes its connection) before the next opens — DatabaseImpl uses a
// per-instance UUID connection name, but keeping scopes tight avoids any
// file-lock bleed on Windows.
void TestActivityFeed::persistHistoryAcrossRestart() {
    QTemporaryDir tmp;
    QVERIFY2(tmp.isValid(), qPrintable(tmp.errorString()));
    const QString path = tmp.path() + QStringLiteral("/feed.db");
    auto bus = EventBus::wire();

    // Phase 1: open DB, attach, publish three events across distinct
    // topics, verify they landed in-memory.
    {
        DatabaseImpl db1;
        QVERIFY2(db1.open(path), qPrintable(db1.lastError()));
        ActivityFeed feed(*bus);
        feed.attachDatabase(db1);
        bus->publish(QStringLiteral("margin.aura.away"), QJsonObject{});
        bus->publish(QStringLiteral("margin.aura.back"), QJsonObject{});
        bus->publish(QStringLiteral("margin.rhythm.break_due"), QJsonObject{});
        QTRY_COMPARE(feed.events().size(), 3);
    }
    // Phase 2: brand-new DatabaseImpl + ActivityFeed on same path. Before
    // attachDatabase the feed is empty; after, all three rows are loaded
    // with the newest (break_due) at index 0.
    {
        DatabaseImpl db2;
        QVERIFY2(db2.open(path), qPrintable(db2.lastError()));
        ActivityFeed feed2(*bus);
        QCOMPARE(feed2.events().size(), 0);
        feed2.attachDatabase(db2);
        QCOMPARE(feed2.events().size(), 3);
        QCOMPARE(feed2.events().first().toMap().value(QStringLiteral("topic")).toString(),
                 QStringLiteral("margin.rhythm.break_due"));
        // title/colorRole derived from topicRules() on load — proves the
        // schema stores topic only and the render metadata is reproducible.
        const QVariantMap first = feed2.events().first().toMap();
        QCOMPARE(first.value(QStringLiteral("title")).toString(),
                 QStringLiteral("休息时间到"));
        QCOMPARE(first.value(QStringLiteral("colorRole")).toString(),
                 QStringLiteral("accentBrand"));
    }
}

// (7) PR6: publishing beyond kBufferSize caps the DB at kBufferSize rows,
// not just the in-memory cache. Without this check, a future refactor
// could accidentally truncate the cache while leaving stale rows in the
// table — which would then re-surface on next restart as ghosts.
void TestActivityFeed::overflowDropsOldestRow() {
    QTemporaryDir tmp;
    QVERIFY2(tmp.isValid(), qPrintable(tmp.errorString()));
    const QString path = tmp.path() + QStringLiteral("/of.db");
    auto bus = EventBus::wire();

    DatabaseImpl db;
    QVERIFY2(db.open(path), qPrintable(db.lastError()));
    ActivityFeed feed(*bus);
    feed.attachDatabase(db);

    for (int i = 0; i < 25; ++i) {
        bus->publish(QStringLiteral("margin.aura.away"), QJsonObject{});
        QTRY_COMPARE(feed.events().size(), qMin(i + 1, ActivityFeed::kBufferSize));
    }
    QCOMPARE(feed.events().size(), ActivityFeed::kBufferSize);

    // Direct DB probe — the table itself must be capped, not just the
    // in-memory mirror. Otherwise restart would resurrect dropped rows.
    const auto rows = db.query(
        QStringLiteral("SELECT COUNT(*) AS c FROM host_activity_events"));
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().value(QStringLiteral("c")).toInt(),
             ActivityFeed::kBufferSize);

    // Drain Qt::QueuedConnection lambdas for publishes 21-25. The ring
    // buffer cap made QTRY_COMPARE return without waiting for those
    // dispatches (in-memory size was already 20), so they sit in qApp's
    // post queue. Without this drain the NEXT test's QTRY_COMPARE would
    // fire them against this test's destroyed `feed` — use-after-free.
    QCoreApplication::sendPostedEvents();
}

// (8) PR6: when no Database is attached (HostCore's m_database was null
// because open() failed at bootstrap, or unit tests that don't bother),
// ActivityFeed must behave exactly like the pre-PR6 in-memory feed. The
// pass condition is reaching the end of the function without any SQL
// being executed (which would NPE on m_db).
void TestActivityFeed::nullDatabaseGraceful() {
    auto bus = EventBus::wire();
    ActivityFeed feed(*bus);  // attachDatabase() intentionally never called

    QCOMPARE(feed.events().size(), 0);
    bus->publish(QStringLiteral("margin.aura.away"), QJsonObject{});
    QTRY_COMPARE(feed.events().size(), 1);
    bus->publish(QStringLiteral("margin.rhythm.break_due"), QJsonObject{});
    QTRY_COMPARE(feed.events().size(), 2);
}

QTEST_MAIN(TestActivityFeed)
#include "test_activity_feed.moc"
