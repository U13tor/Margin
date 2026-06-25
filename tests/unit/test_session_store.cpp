// tests/unit/test_session_store.cpp
//
// Validates SessionStore + computeTimeBuckets against an in-memory SQLite
// Database. Hermetic — doesn't touch the real margin.db.
//
// What we verify:
//   1. ensureSchema creates both tables + 6 indexes.
//   2. openSession returns a row id; the row exists with started_at set.
//   3. closeSession writes ended_at + duration_ms + is_idle_end correctly.
//   4. insertPickup adds a row to screen_time_activity_event.
//   5. computeTimeBuckets is right for a known timestamp + zone (we
//      can't change the system timezone in a unit test, so we verify
//      the YYYYMMDD/hour/weekday math against the system's local time
//      representation of that timestamp instead — that's exactly what
//      the function is supposed to compute).
//
// Encryption is not tested here — SessionStore takes already-encrypted
// bytes. CryptoService is exercised by test_crypto_service / test_aura_*
// tests.

#include <QObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>
#include <QVariant>

#include "host/services/DatabaseImpl.h"
#include "plugins/screen_time/SessionStore.h"

using Margin::DatabaseImpl;
using Margin::Database;
using Margin::Plugins::ScreenTime::SessionStore;

class TestSessionStore : public QObject {
    Q_OBJECT

private slots:
    void testEnsureSchemaCreatesTablesAndIndexes();
    void testOpenSessionReturnsRowId();
    void testCloseSessionUpdatesEndedAndDuration();
    void testCloseSessionZeroRowIdIsNoOp();
    void testInsertPickupAddsRow();
    void testComputeTimeBucketsKnown();
    void testComputeTimeBucketsMidnightBoundary();
    void testAllSessionsReturnsOldestFirst();
    void testClearAllWipesBothTables();
};

namespace {
// Open a fresh in-memory Database for each test. SQLite ":memory:" gives
// a private per-connection DB — perfect for hermetic tests.
struct MemoryDb {
    DatabaseImpl db;
    bool opened = false;
    MemoryDb() {
        // We use a temp file rather than :memory: because DatabaseImpl
        // sets WAL pragmas, which SQLite disallows on the in-memory db
        // (the pragma becomes a silent no-op, which is fine). Either
        // works; we pick a temp file so WAL behavior is real.
        tmpDir = std::make_unique<QTemporaryDir>();
        Q_ASSERT(tmpDir->isValid());
        opened = db.open(tmpDir->path() + QStringLiteral("/test.db"));
        Q_ASSERT(opened);
    }
    std::unique_ptr<QTemporaryDir> tmpDir;
};

int countIndexes(Database& db, const QString& tableName) {
    // SQLite's sqlite_master holds one row per CREATE INDEX.
    const auto rows = db.query(
        QStringLiteral("SELECT count(*) AS c FROM sqlite_master "
                       "WHERE type='index' AND tbl_name='%1'").arg(tableName));
    if (rows.isEmpty()) return -1;
    return rows.first().value(QStringLiteral("c")).toInt();
}

int tableExists(Database& db, const QString& tableName) {
    const auto rows = db.query(
        QStringLiteral("SELECT count(*) AS c FROM sqlite_master "
                       "WHERE type='table' AND name='%1'").arg(tableName));
    if (rows.isEmpty()) return 0;
    return rows.first().value(QStringLiteral("c")).toInt();
}
} // namespace

void TestSessionStore::testEnsureSchemaCreatesTablesAndIndexes() {
    MemoryDb mem;
    SessionStore store;
    QVERIFY(store.ensureSchema(mem.db));

    QCOMPARE(tableExists(mem.db, QStringLiteral("screen_time_app_session")), 1);
    QCOMPARE(tableExists(mem.db, QStringLiteral("screen_time_activity_event")), 1);

    // 5 indexes on app_session (started_at / app / day_app / day_hour / day_cat)
    QCOMPARE(countIndexes(mem.db, QStringLiteral("screen_time_app_session")), 5);
    // 1 index on activity_event (day_type)
    QCOMPARE(countIndexes(mem.db, QStringLiteral("screen_time_activity_event")), 1);

    // Idempotent — re-running doesn't error out.
    QVERIFY(store.ensureSchema(mem.db));
}

void TestSessionStore::testOpenSessionReturnsRowId() {
    MemoryDb mem;
    SessionStore store;
    QVERIFY(store.ensureSchema(mem.db));

    const qint64 started = 1'718'700'000'000LL;  // 2024-06-18 ~13:00 UTC
    const long long id = store.openSession(
        mem.db, QStringLiteral("Code.exe"),
        QByteArray::fromRawData("\x01\x02\x03", 3),  // fake ciphertext
        QString(), QStringLiteral("C:/Program Files/Code/Code.exe"), started);

    QVERIFY2(id > 0, "openSession must return a positive row id");

    const auto rows = mem.db.query(
        QStringLiteral("SELECT app_name, started_at, ended_at, duration_ms, "
                       "       is_idle_end, day_local, hour_local, weekday_local "
                       "FROM screen_time_app_session WHERE id = :id"),
        {{"id", QVariant::fromValue(id)}});
    QCOMPARE(rows.size(), 1);
    const QVariantMap& row = rows.first();
    QCOMPARE(row.value(QStringLiteral("app_name")).toString(), QStringLiteral("Code.exe"));
    QCOMPARE(row.value(QStringLiteral("started_at")).toLongLong(), started);
    // ended_at initialized to started_at as a sentinel until closeSession.
    QCOMPARE(row.value(QStringLiteral("ended_at")).toLongLong(), started);
    QCOMPARE(row.value(QStringLiteral("duration_ms")).toLongLong(), 0);
    QCOMPARE(row.value(QStringLiteral("is_idle_end")).toInt(), 0);
    // day_local/hour_local/weekday_local are computed from local time —
    // verify they match what computeTimeBuckets returns for the same ms.
    const auto tb = SessionStore::computeTimeBuckets(started);
    QCOMPARE(row.value(QStringLiteral("day_local")).toInt(), tb.dayLocal);
    QCOMPARE(row.value(QStringLiteral("hour_local")).toInt(), tb.hourLocal);
    QCOMPARE(row.value(QStringLiteral("weekday_local")).toInt(), tb.weekdayLocal);
}

void TestSessionStore::testCloseSessionUpdatesEndedAndDuration() {
    MemoryDb mem;
    SessionStore store;
    QVERIFY(store.ensureSchema(mem.db));

    const qint64 started = 1'718'700'000'000LL;
    const qint64 ended   = started + 5 * 60 * 1000;  // 5 minutes later
    const long long id = store.openSession(
        mem.db, QStringLiteral("chrome.exe"),
        QByteArray::fromRawData("\xAA", 1),
        QString(), QStringLiteral("C:/Program Files/Chrome/chrome.exe"), started);
    QVERIFY(id > 0);

    QVERIFY(store.closeSession(mem.db, id, ended, /*isIdleEnd=*/true));

    const auto rows = mem.db.query(
        QStringLiteral("SELECT ended_at, duration_ms, is_idle_end "
                       "FROM screen_time_app_session WHERE id = :id"),
        {{"id", QVariant::fromValue(id)}});
    QCOMPARE(rows.size(), 1);
    const QVariantMap& row = rows.first();
    QCOMPARE(row.value(QStringLiteral("ended_at")).toLongLong(), ended);
    QCOMPARE(row.value(QStringLiteral("duration_ms")).toLongLong(),
             ended - started);
    QCOMPARE(row.value(QStringLiteral("is_idle_end")).toInt(), 1);
}

void TestSessionStore::testCloseSessionZeroRowIdIsNoOp() {
    MemoryDb mem;
    SessionStore store;
    QVERIFY(store.ensureSchema(mem.db));
    // No row opened — rowId=0 must be a no-op, returning true.
    QVERIFY(store.closeSession(mem.db, /*rowId=*/0,
                               QDateTime::currentMSecsSinceEpoch(),
                               /*isIdleEnd=*/false));
}

void TestSessionStore::testInsertPickupAddsRow() {
    MemoryDb mem;
    SessionStore store;
    QVERIFY(store.ensureSchema(mem.db));

    const qint64 now = 1'718'700'000'000LL;
    QVERIFY(store.insertPickup(mem.db, now, /*prevIdleMs=*/42000));

    const auto rows = mem.db.query(
        QStringLiteral("SELECT event_type, occurred_at, prev_idle_ms "
                       "FROM screen_time_activity_event "
                       "WHERE occurred_at = :occ"),
        {{"occ", QVariant::fromValue(now)}});
    QCOMPARE(rows.size(), 1);
    const QVariantMap& row = rows.first();
    QCOMPARE(row.value(QStringLiteral("event_type")).toString(), QStringLiteral("pickup"));
    QCOMPARE(row.value(QStringLiteral("prev_idle_ms")).toLongLong(), 42000);
}

void TestSessionStore::testComputeTimeBucketsKnown() {
    // Pick a timestamp we can reason about. QDateTime::fromMSecsSinceEpoch
    // gives local time, so the answer is whatever the system timezone says
    // for this instant. We verify the math is internally consistent: the
    // day_local / hour_local / weekday_local returned for a fixed ms match
    // what QDateTime::fromMSecsSinceEpoch(ms).toLocalTime() reports.
    const qint64 ts = 1'718'700'000'000LL;
    const auto tb = SessionStore::computeTimeBuckets(ts);

    const QDateTime dt = QDateTime::fromMSecsSinceEpoch(ts).toLocalTime();
    const QDate d = dt.date();
    QCOMPARE(tb.dayLocal, d.year() * 10000 + d.month() * 100 + d.day());
    QCOMPARE(tb.hourLocal, dt.time().hour());
    QCOMPARE(tb.weekdayLocal, d.dayOfWeek() - 1);  // Mon=0..Sun=6
}

void TestSessionStore::testComputeTimeBucketsMidnightBoundary() {
    // A timestamp exactly at 00:00:00 local — should produce hour_local=0
    // and a day_local matching that date.
    QDateTime midnight(QDate(2024, 6, 18), QTime(0, 0, 0));
    const qint64 ts = midnight.toMSecsSinceEpoch();
    const auto tb = SessionStore::computeTimeBuckets(ts);

    QCOMPARE(tb.dayLocal,  20240618);
    QCOMPARE(tb.hourLocal, 0);
    // 2024-06-18 was a Tuesday — ISO weekday 2, our convention Mon=0 → 1.
    QCOMPARE(tb.weekdayLocal, 1);
}

void TestSessionStore::testAllSessionsReturnsOldestFirst() {
    MemoryDb mem;
    SessionStore store;
    QVERIFY(store.ensureSchema(mem.db));

    // Insert 3 sessions with increasing started_at. allSessions must
    // return them oldest-first so export JSON reads chronologically.
    const qint64 t0 = 1'718'700'000'000LL;
    store.openSession(mem.db, QStringLiteral("c.exe"),
                      QByteArray("\x01", 1), QStringLiteral("Dev"),
                      QStringLiteral("C:/dev/c.exe"), t0);
    store.openSession(mem.db, QStringLiteral("b.exe"),
                      QByteArray("\x02", 1), QString(),
                      QStringLiteral("C:/dev/b.exe"), t0 + 1000);
    store.openSession(mem.db, QStringLiteral("a.exe"),
                      QByteArray("\x03", 1), QString(),
                      QString(), t0 + 2000);  // empty exePath is allowed

    QCOMPARE(store.sessionCount(mem.db), 3);

    const auto rows = store.allSessions(mem.db);
    QCOMPARE(rows.size(), 3);
    QCOMPARE(rows[0].value(QStringLiteral("app_name")).toString(), QStringLiteral("c.exe"));
    QCOMPARE(rows[1].value(QStringLiteral("app_name")).toString(), QStringLiteral("b.exe"));
    QCOMPARE(rows[2].value(QStringLiteral("app_name")).toString(), QStringLiteral("a.exe"));

    // window_title_enc surfaces as raw bytes — caller (plugin) decrypts.
    QCOMPARE(rows[0].value(QStringLiteral("window_title_enc")).toByteArray(),
             QByteArray("\x01", 1));
    QCOMPARE(rows[0].value(QStringLiteral("category")).toString(), QStringLiteral("Dev"));
}

void TestSessionStore::testClearAllWipesBothTables() {
    MemoryDb mem;
    SessionStore store;
    QVERIFY(store.ensureSchema(mem.db));

    // Seed both tables.
    store.openSession(mem.db, QStringLiteral("a.exe"),
                      QByteArray("\x01", 1), QString(),
                      QStringLiteral("C:/dev/a.exe"), 1'718'700'000'000LL);
    store.openSession(mem.db, QStringLiteral("b.exe"),
                      QByteArray("\x02", 1), QString(),
                      QString(), 1'718'700'001'000LL);
    store.insertPickup(mem.db, 1'718'700'002'000LL, 30000);

    QCOMPARE(store.sessionCount(mem.db), 2);
    QVERIFY(store.clearAll(mem.db));
    QCOMPARE(store.sessionCount(mem.db), 0);
    // activity_event also cleared — verify via pickupCountByDay which
    // returns 0 when the table is empty.
    QCOMPARE(store.pickupCountByDay(mem.db, /*dayLocal=*/20240618), 0);

    // Clearing an already-empty DB is safe (idempotent DELETE).
    QVERIFY(store.clearAll(mem.db));
}

QTEST_MAIN(TestSessionStore)
#include "test_session_store.moc"
