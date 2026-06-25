// tests/unit/test_aura_recent_events_persist.cpp
//
// PR7: AuraLockerPlugin's Recent Activity trail survives process restart
// via the aura_recent_event SQLite table. Three cases:
//   (1) persistAcrossRestart — events appended in one lifetime are
//       reloaded from disk when a fresh plugin instance is constructed
//       against the same SQLite file
//   (2) overflowDropsOldestAt30 — beyond kMaxRecentEvents the DB itself
//       is capped, not just the in-memory cache (else restart would
//       resurrect dropped rows as ghosts)
//   (3) nullDatabaseGraceful — no DB attached → pure in-memory behavior,
//       no crashes, no SQL touches
//
// Test seam: AuraLockerPlugin declares `friend class TestAuraRecentEvents;`
// so this TU can drive the private ensureRecentEventSchema /
// loadRecentEventsFromDb / m_db directly without going through onLoad
// (which would require BluetoothProximityTracker + HostServices +
// Settings — a ~500-line integration harness for a ~30-line DB check).
//
// We use the same temp-file + DatabaseImpl compile-in pattern as
// test_session_store.cpp: DatabaseImpl sets WAL pragmas that SQLite
// disallows on ":memory:", so a temp file is the hermetic choice.

#include <QObject>
#include <QTemporaryDir>
#include <QTest>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

#include "host/services/DatabaseImpl.h"
#include "plugins/aura_locker/AuraLockerPlugin.h"

using Margin::DatabaseImpl;
using Margin::Plugins::Aura::AuraLockerPlugin;

class TestAuraRecentEvents : public QObject {
    Q_OBJECT

private slots:
    void persistAcrossRestart();
    void overflowDropsOldestAt30();
    void nullDatabaseGraceful();
};

// (1) Phase 1: open DB on a fresh temp file, construct a plugin, wire its
// m_db, ensure schema, append two events. Phase 2: brand-new DatabaseImpl
// + plugin on the same file. The plugin's recentEvents() must surface
// both rows with the newest at index 0.
void TestAuraRecentEvents::persistAcrossRestart() {
    QTemporaryDir tmp;
    QVERIFY2(tmp.isValid(), qPrintable(tmp.errorString()));
    const QString path = tmp.path() + QStringLiteral("/aura.db");

    // Phase 1 — write.
    {
        DatabaseImpl db1;
        QVERIFY2(db1.open(path), qPrintable(db1.lastError()));
        AuraLockerPlugin p;
        p.m_db = &db1;
        QVERIFY2(p.ensureRecentEventSchema(), qPrintable(db1.lastError()));
        p.appendEvent(QStringLiteral("lock"), QStringLiteral("Locked (away)"));
        p.appendEvent(QStringLiteral("back"), QStringLiteral("Unlocked (back)"));
        QCOMPARE(p.recentEvents().size(), 2);
        // newest first
        QCOMPARE(p.recentEvents().first().toMap().value(QStringLiteral("kind")).toString(),
                 QStringLiteral("back"));
    }
    // Phase 2 — reload.
    {
        DatabaseImpl db2;
        QVERIFY2(db2.open(path), qPrintable(db2.lastError()));
        AuraLockerPlugin p;
        p.m_db = &db2;
        QVERIFY2(p.ensureRecentEventSchema(), qPrintable(db2.lastError()));
        QVERIFY2(p.loadRecentEventsFromDb(), qPrintable(db2.lastError()));
        p.rebuildRecentEventsCache();
        QCOMPARE(p.recentEvents().size(), 2);
        // ASC load preserves "newest at tail of m_recentEvents", then
        // rebuildRecentEventsCache reverses → newest at index 0.
        QCOMPARE(p.recentEvents().first().toMap().value(QStringLiteral("kind")).toString(),
                 QStringLiteral("back"));
        QCOMPARE(p.recentEvents().last().toMap().value(QStringLiteral("kind")).toString(),
                 QStringLiteral("lock"));
        // The user-facing message is frozen verbatim — i18n renames in
        // source code do NOT mutate historical rows.
        QCOMPARE(p.recentEvents().first().toMap().value(QStringLiteral("message")).toString(),
                 QStringLiteral("Unlocked (back)"));
    }
}

// (2) Append 35 events. The in-memory cache, DB table, and QML cache must
// all cap at kMaxRecentEvents == 30. The 5 oldest (#0..4) must be gone
// from BOTH the cache and the table.
void TestAuraRecentEvents::overflowDropsOldestAt30() {
    QTemporaryDir tmp;
    QVERIFY2(tmp.isValid(), qPrintable(tmp.errorString()));
    const QString path = tmp.path() + QStringLiteral("/of.db");

    DatabaseImpl db;
    QVERIFY2(db.open(path), qPrintable(db.lastError()));
    AuraLockerPlugin p;
    p.m_db = &db;
    QVERIFY2(p.ensureRecentEventSchema(), qPrintable(db.lastError()));

    for (int i = 0; i < 35; ++i) {
        p.appendEvent(QStringLiteral("lock"),
                      QStringLiteral("Lock #%1").arg(i));
    }
    QCOMPARE(p.recentEvents().size(), 30);
    // newest first: #34 at index 0
    QCOMPARE(p.recentEvents().first().toMap().value(QStringLiteral("message")).toString(),
             QStringLiteral("Lock #34"));
    // oldest in cache: #5 (rows #0..4 dropped)
    QCOMPARE(p.recentEvents().last().toMap().value(QStringLiteral("message")).toString(),
             QStringLiteral("Lock #5"));

    // Direct DB probe — proves the cap is at the table layer, not just
    // the in-memory QList. Without this check a future refactor could
    // accidentally truncate the cache while leaving stale rows that
    // would re-surface on next restart.
    const auto rows = db.query(
        QStringLiteral("SELECT COUNT(*) AS c FROM aura_recent_event"));
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().value(QStringLiteral("c")).toInt(), 30);
}

// (3) Default-constructed plugin with no m_db assignment. appendEvent must
// behave as the pre-PR7 in-memory ring buffer. Pass condition is reaching
// the final QCOMPARE without any SQL touching a null m_db (which would
// SEGFAULT before we got here).
void TestAuraRecentEvents::nullDatabaseGraceful() {
    AuraLockerPlugin p;  // m_db intentionally left null

    QCOMPARE(p.recentEvents().size(), 0);
    p.appendEvent(QStringLiteral("lock"), QStringLiteral("test"));
    QCOMPARE(p.recentEvents().size(), 1);
    p.appendEvent(QStringLiteral("back"), QStringLiteral("test2"));
    QCOMPARE(p.recentEvents().size(), 2);
    QCOMPARE(p.recentEvents().first().toMap().value(QStringLiteral("kind")).toString(),
             QStringLiteral("back"));
}

QTEST_MAIN(TestAuraRecentEvents)
#include "test_aura_recent_events_persist.moc"
