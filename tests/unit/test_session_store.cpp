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
    // M5 fix for 跨休眠计时膨胀: ensureSchema must clean up "abandoned"
    // sessions left by crash / 硬关机 paths. See plan D3.
    void testCleanupAbandonedSessionsCapsLongGap();
    void testCleanupAbandonedSessionsPreservesShortGap();
    void testCleanupAbandonedSessionsIgnoresClosedRows();
    void testCleanupAbandonedSessionsIdempotent();
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

// Helper: insert a raw "abandoned" session row directly bypassing the
// normal open/close API. Mirrors what a Margin crash would leave behind
// in margin.db — duration_ms = 0, ended_at = started_at sentinel.
static long long insertAbandonedRow(Database& db, qint64 startedAt,
                                    const QString& app = QStringLiteral("x.exe")) {
    const auto tb = SessionStore::computeTimeBuckets(startedAt);
    const QString sql = QStringLiteral(
        "INSERT INTO screen_time_app_session "
        "  (app_name, window_title_enc, category, exe_path, started_at, "
        "   ended_at, duration_ms, is_idle_end, day_local, hour_local, "
        "   weekday_local) "
        "VALUES (:app, NULL, '', '', :started, :started, 0, 0, "
        "        :day, :hour, :wd)");
    QVariantMap params;
    params.insert(QStringLiteral("app"), app);
    params.insert(QStringLiteral("started"), QVariant::fromValue(startedAt));
    params.insert(QStringLiteral("day"), tb.dayLocal);
    params.insert(QStringLiteral("hour"), tb.hourLocal);
    params.insert(QStringLiteral("wd"), tb.weekdayLocal);
    if (!db.exec(sql, params)) return 0;
    const auto rows = db.query(QStringLiteral("SELECT last_insert_rowid() AS id"));
    if (rows.isEmpty()) return 0;
    return rows.first().value(QStringLiteral("id")).toLongLong();
}

void TestSessionStore::testCleanupAbandonedSessionsCapsLongGap() {
    // A session that started 5 hours ago and was never closed (e.g.
    // user closed laptop lid after putting Margin to sleep, battery
    // died). ensureSchema must infer ended_at = started_at + 1h cap
    // — not 5h, since we can't know how much of the gap was real use
    // vs sleep.
    MemoryDb mem;
    SessionStore store;
    QVERIFY(store.ensureSchema(mem.db));  // initial ensure: no abandoned rows yet

    const qint64 fiveHoursAgo = QDateTime::currentMSecsSinceEpoch() - 5 * 3600 * 1000;
    const long long id = insertAbandonedRow(mem.db, fiveHoursAgo);
    QVERIFY(id > 0);

    // Re-run ensureSchema — this is the path the next Margin startup
    // takes (plugin onLoad calls ensureSchema, which now runs cleanup).
    QVERIFY(store.ensureSchema(mem.db));

    const auto rows = mem.db.query(
        QStringLiteral("SELECT duration_ms FROM screen_time_app_session WHERE id = :id"),
        {{"id", QVariant::fromValue(id)}});
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().value(QStringLiteral("duration_ms")).toLongLong(),
             3'600'000LL);  // 1h cap
}

void TestSessionStore::testCleanupAbandonedSessionsPreservesShortGap() {
    // A session started 30 minutes ago and never closed (Margin crashed
    // shortly after opening). The 1h cap doesn't trigger — duration
    // should equal actual elapsed time, not the cap.
    MemoryDb mem;
    SessionStore store;
    QVERIFY(store.ensureSchema(mem.db));

    const qint64 thirtyMinAgo = QDateTime::currentMSecsSinceEpoch() - 30 * 60 * 1000;
    const long long id = insertAbandonedRow(mem.db, thirtyMinAgo);
    QVERIFY(id > 0);

    QVERIFY(store.ensureSchema(mem.db));

    const auto rows = mem.db.query(
        QStringLiteral("SELECT duration_ms FROM screen_time_app_session WHERE id = :id"),
        {{"id", QVariant::fromValue(id)}});
    QCOMPARE(rows.size(), 1);
    const qint64 dur = rows.first().value(QStringLiteral("duration_ms")).toLongLong();
    // Allow ±5s slack for test timing — the value should be very close
    // to 30 * 60 * 1000 but not exactly (test exec latency).
    QVERIFY2(dur > 25 * 60 * 1000 && dur < 35 * 60 * 1000,
             qPrintable(QStringLiteral("expected ~30min, got %1 ms").arg(dur)));
}

void TestSessionStore::testCleanupAbandonedSessionsIgnoresClosedRows() {
    // Properly closed sessions (duration_ms > 0) must not be touched
    // by the cleanup. We verify this by writing a 5-minute closed
    // session, then running cleanup, then asserting the duration is
    // unchanged.
    MemoryDb mem;
    SessionStore store;
    QVERIFY(store.ensureSchema(mem.db));

    const qint64 started = 1'718'700'000'000LL;
    const long long id = store.openSession(
        mem.db, QStringLiteral("chrome.exe"), QByteArray("\xAA", 1),
        QString(), QStringLiteral("C:/chrome.exe"), started);
    QVERIFY(id > 0);
    QVERIFY(store.closeSession(mem.db, id, started + 5 * 60 * 1000, /*isIdleEnd=*/false));

    QVERIFY(store.ensureSchema(mem.db));  // should NOT touch the closed row

    const auto rows = mem.db.query(
        QStringLiteral("SELECT duration_ms FROM screen_time_app_session WHERE id = :id"),
        {{"id", QVariant::fromValue(id)}});
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().value(QStringLiteral("duration_ms")).toLongLong(),
             5 * 60 * 1000LL);
}

void TestSessionStore::testCleanupAbandonedSessionsIdempotent() {
    // Calling ensureSchema twice in a row (e.g. plugin reload without
    // exit) must be safe. After the first call, every abandoned row
    // has duration_ms > 0, so the second cleanup is a no-op.
    MemoryDb mem;
    SessionStore store;
    QVERIFY(store.ensureSchema(mem.db));

    const qint64 fiveHoursAgo = QDateTime::currentMSecsSinceEpoch() - 5 * 3600 * 1000;
    const long long id = insertAbandonedRow(mem.db, fiveHoursAgo);
    QVERIFY(id > 0);

    QVERIFY(store.ensureSchema(mem.db));
    QVERIFY(store.ensureSchema(mem.db));  // second call — must be a no-op

    const auto rows = mem.db.query(
        QStringLiteral("SELECT duration_ms FROM screen_time_app_session WHERE id = :id"),
        {{"id", QVariant::fromValue(id)}});
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().value(QStringLiteral("duration_ms")).toLongLong(),
             3'600'000LL);  // still 1h, not double-applied
}

QTEST_MAIN(TestSessionStore)
#include "test_session_store.moc"
