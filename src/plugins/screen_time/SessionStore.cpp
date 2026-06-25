// SessionStore impl — see SessionStore.h.

#include "SessionStore.h"

#include "Margin/Database.h"

#include <QDateTime>
#include <QDate>
#include <QStringList>
#include <QVariant>

namespace Margin::Plugins::ScreenTime {

SessionStore::SessionStore() = default;
SessionStore::~SessionStore() = default;

bool SessionStore::ensureSchema(Database& db) {
    // Each DDL statement goes through exec() individually. Qt's
    // QSqlDatabase::exec() does NOT support multiple semicolon-separated
    // statements in one call — it returns "Unable to execute multiple
    // statements at a time". Splitting also gives finer-grained
    // lastError() messages.
    //
    // Column types per docs/11-roadmap.md M2-C3 schema.
    static const QStringList kStatements = {
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS screen_time_app_session ("
            "    id                INTEGER PRIMARY KEY AUTOINCREMENT,"
            "    app_name          TEXT    NOT NULL,"
            "    window_title_enc  BLOB,"
            "    category          TEXT,"
            "    exe_path          TEXT,"
            "    started_at        INTEGER NOT NULL,"
            "    ended_at          INTEGER NOT NULL,"
            "    duration_ms       INTEGER NOT NULL,"
            "    is_idle_end       INTEGER NOT NULL DEFAULT 0,"
            "    day_local         INTEGER NOT NULL,"
            "    hour_local        INTEGER NOT NULL,"
            "    weekday_local     INTEGER NOT NULL"
            ")"),
        QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_session_started_at "
            "ON screen_time_app_session(started_at)"),
        QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_session_app "
            "ON screen_time_app_session(app_name, started_at)"),
        QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_session_day_app "
            "ON screen_time_app_session(day_local, app_name)"),
        QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_session_day_hour "
            "ON screen_time_app_session(day_local, hour_local)"),
        QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_session_day_cat "
            "ON screen_time_app_session(day_local, category)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS screen_time_activity_event ("
            "    id              INTEGER PRIMARY KEY AUTOINCREMENT,"
            "    event_type      TEXT NOT NULL,"
            "    occurred_at     INTEGER NOT NULL,"
            "    day_local       INTEGER NOT NULL,"
            "    hour_local      INTEGER NOT NULL,"
            "    weekday_local   INTEGER NOT NULL,"
            "    prev_idle_ms    INTEGER"
            ")"),
        QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_event_day_type "
            "ON screen_time_activity_event(day_local, event_type)"),
    };

    for (const QString& sql : kStatements) {
        if (!db.exec(sql)) return false;
    }

    // PR3 round-2 #2b: migrate pre-existing DBs by adding exe_path via
    // ALTER TABLE ADD COLUMN. CREATE TABLE IF NOT EXISTS is a no-op when
    // the table already exists, so the column would be missing on upgrades.
    // SQLite ALTER ADD COLUMN supports TEXT + nullable (no NOT NULL-without-
    // default). PRAGMA table_info gives us a column-existence check without
    // a try/SELECT-catch round-trip.
    const auto cols = db.query(
        QStringLiteral("PRAGMA table_info(screen_time_app_session)"));
    bool hasExePath = false;
    for (const QVariantMap& r : cols) {
        if (r.value(QStringLiteral("name")).toString() == QStringLiteral("exe_path")) {
            hasExePath = true;
            break;
        }
    }
    if (!hasExePath) {
        if (!db.exec(QStringLiteral(
            "ALTER TABLE screen_time_app_session ADD COLUMN exe_path TEXT"))) {
            return false;
        }
    }
    return true;
}

SessionStore::TimeBuckets SessionStore::computeTimeBuckets(qint64 epochMs) {
    const QDateTime dt = QDateTime::fromMSecsSinceEpoch(epochMs).toLocalTime();
    const QDate d = dt.date();
    // Qt: Mon=1..Sun=7. We shift to Mon=0..Sun=6 so the integer aligns
    // with "weekday index" in user-facing reports (Monday-first weeks are
    // the convention in Chinese / most European locales).
    const int qtWeekday = d.dayOfWeek();  // 1..7
    return TimeBuckets{
        d.year() * 10000 + d.month() * 100 + d.day(),  // YYYYMMDD
        dt.time().hour(),                                // 0..23
        qtWeekday - 1                                    // 0..6, Mon=0
    };
}

long long SessionStore::openSession(Database& db,
                                    const QString& appName,
                                    const QByteArray& windowTitleEnc,
                                    const QString& category,
                                    const QString& exePath,
                                    qint64 startedAt) {
    const TimeBuckets tb = computeTimeBuckets(startedAt);

    // INSERT with started_at = ended_at = duration_ms = startedAt as a
    // sentinel. closeSession writes the real ended_at / duration_ms.
    // is_idle_end defaults to 0 in DDL.
    const QString sql = QStringLiteral(
        "INSERT INTO screen_time_app_session "
        "  (app_name, window_title_enc, category, exe_path, started_at, "
        "   ended_at, duration_ms, is_idle_end, day_local, hour_local, "
        "   weekday_local) "
        "VALUES "
        "  (:app, :title, :cat, :exe, :started, :started, 0, 0, "
        "   :day, :hour, :wd)"
    );
    QVariantMap params;
    params.insert(QStringLiteral("app"),     appName);
    params.insert(QStringLiteral("title"),   windowTitleEnc);
    params.insert(QStringLiteral("cat"),     category);
    params.insert(QStringLiteral("exe"),     exePath);
    params.insert(QStringLiteral("started"), startedAt);
    params.insert(QStringLiteral("day"),     tb.dayLocal);
    params.insert(QStringLiteral("hour"),    tb.hourLocal);
    params.insert(QStringLiteral("wd"),      tb.weekdayLocal);

    if (!db.exec(sql, params)) return 0;

    // lastInsertId via a follow-up query — Database interface doesn't
    // expose the QSqlQuery directly, so we use SELECT last_insert_rowid().
    // SQLite guarantees this returns the rowid of the most recent
    // successful INSERT on this connection, which (single-connection,
    // serialized via QMutex in DatabaseImpl) is exactly ours.
    const auto rows = db.query(QStringLiteral("SELECT last_insert_rowid() AS id"));
    if (rows.isEmpty()) return 0;
    return rows.first().value(QStringLiteral("id")).toLongLong();
}

bool SessionStore::closeSession(Database& db,
                                long long rowId,
                                qint64 endedAt,
                                bool isIdleEnd) {
    if (rowId == 0) return true;  // no session to close

    // duration_ms is computed in SQL as (ended - started_at) — SQLite can
    // reference the row's own column, so no read-back round-trip is needed.
    const QString sql = QStringLiteral(
        "UPDATE screen_time_app_session "
        "SET ended_at = :ended, duration_ms = (:ended - started_at), "
        "    is_idle_end = :idle "
        "WHERE id = :id"
    );
    QVariantMap params;
    params.insert(QStringLiteral("ended"), endedAt);
    params.insert(QStringLiteral("idle"),  isIdleEnd ? 1 : 0);
    params.insert(QStringLiteral("id"),    rowId);
    return db.exec(sql, params);
}

bool SessionStore::insertPickup(Database& db,
                                qint64 occurredAt,
                                qint64 prevIdleMs) {
    const TimeBuckets tb = computeTimeBuckets(occurredAt);

    const QString sql = QStringLiteral(
        "INSERT INTO screen_time_activity_event "
        "  (event_type, occurred_at, day_local, hour_local, weekday_local, "
        "   prev_idle_ms) "
        "VALUES "
        "  ('pickup', :occ, :day, :hour, :wd, :prev)"
    );
    QVariantMap params;
    params.insert(QStringLiteral("occ"),  occurredAt);
    params.insert(QStringLiteral("day"),  tb.dayLocal);
    params.insert(QStringLiteral("hour"), tb.hourLocal);
    params.insert(QStringLiteral("wd"),   tb.weekdayLocal);
    params.insert(QStringLiteral("prev"), prevIdleMs);
    return db.exec(sql, params);
}

// ── Report queries ────────────────────────────────────────────────────
// All use GROUP BY on pre-denormalized columns so no runtime timezone
// math runs per row. LIMIT is bound via QVariant because the named
// param's value comes from the caller; SQLite's named-binding API
// requires a QVariant-typed bind.

QList<QVariantMap> SessionStore::topAppsByDay(Database& db, int dayLocal, int limit) {
    // PR3 round-2 #2b: include the most-recent non-empty exe_path so
    // MBarChart delegates can bind image://appicon/<exe_path>. MAX on a
    // TEXT column in SQLite returns the lexically-largest value, which is
    // arbitrary but stable for a given dataset — we just need *some*
    // non-empty path per app for icon lookup. Empty paths remain empty.
    const QString sql = QStringLiteral(
        "SELECT app_name AS app_name, "
        "       SUM(duration_ms) AS duration_ms, "
        "       MAX(category) AS category, "  // tie-break if an app spans categories (rare): alphabetical, skips NULL
        "       MAX(NULLIF(exe_path, '')) AS exe_path "
        "FROM screen_time_app_session "
        "WHERE day_local = :day "
        "GROUP BY app_name "
        "ORDER BY duration_ms DESC "
        "LIMIT :limit"
    );
    QVariantMap params;
    params.insert(QStringLiteral("day"),  dayLocal);
    params.insert(QStringLiteral("limit"), limit);
    return db.query(sql, params);
}

QList<QVariantMap> SessionStore::topAppsByRange(Database& db, int fromDayLocal, int toDayLocal, int limit) {
    const QString sql = QStringLiteral(
        "SELECT app_name AS app_name, "
        "       SUM(duration_ms) AS duration_ms, "
        "       MAX(category) AS category, "
        "       MAX(NULLIF(exe_path, '')) AS exe_path "
        "FROM screen_time_app_session "
        "WHERE day_local BETWEEN :from AND :to "
        "GROUP BY app_name "
        "ORDER BY duration_ms DESC "
        "LIMIT :limit"
    );
    QVariantMap params;
    params.insert(QStringLiteral("from"), fromDayLocal);
    params.insert(QStringLiteral("to"),   toDayLocal);
    params.insert(QStringLiteral("limit"), limit);
    return db.query(sql, params);
}

QList<QVariantMap> SessionStore::categoriesByDay(Database& db, int dayLocal) {
    const QString sql = QStringLiteral(
        "SELECT COALESCE(category, 'Uncategorized') AS category, "
        "       SUM(duration_ms) AS duration_ms "
        "FROM screen_time_app_session "
        "WHERE day_local = :day "
        "GROUP BY COALESCE(category, 'Uncategorized') "
        "ORDER BY duration_ms DESC"
    );
    QVariantMap params;
    params.insert(QStringLiteral("day"), dayLocal);
    return db.query(sql, params);
}

QList<QVariantMap> SessionStore::categoriesByRange(Database& db, int fromDayLocal, int toDayLocal) {
    const QString sql = QStringLiteral(
        "SELECT COALESCE(category, 'Uncategorized') AS category, "
        "       SUM(duration_ms) AS duration_ms "
        "FROM screen_time_app_session "
        "WHERE day_local BETWEEN :from AND :to "
        "GROUP BY COALESCE(category, 'Uncategorized') "
        "ORDER BY duration_ms DESC"
    );
    QVariantMap params;
    params.insert(QStringLiteral("from"), fromDayLocal);
    params.insert(QStringLiteral("to"),   toDayLocal);
    return db.query(sql, params);
}

QList<QVariantMap> SessionStore::dailyTotals(Database& db, int fromDayLocal, int toDayLocal) {
    const QString sql = QStringLiteral(
        "SELECT day_local AS day_local, "
        "       SUM(duration_ms) AS duration_ms "
        "FROM screen_time_app_session "
        "WHERE day_local BETWEEN :from AND :to "
        "GROUP BY day_local "
        "ORDER BY day_local ASC"
    );
    QVariantMap params;
    params.insert(QStringLiteral("from"), fromDayLocal);
    params.insert(QStringLiteral("to"),   toDayLocal);
    return db.query(sql, params);
}

int SessionStore::pickupCountByDay(Database& db, int dayLocal) {
    const QString sql = QStringLiteral(
        "SELECT COUNT(*) AS c "
        "FROM screen_time_activity_event "
        "WHERE day_local = :day AND event_type = 'pickup'"
    );
    QVariantMap params;
    params.insert(QStringLiteral("day"), dayLocal);
    const auto rows = db.query(sql, params);
    if (rows.isEmpty()) return 0;
    return rows.first().value(QStringLiteral("c")).toInt();
}

// ── Export / clear (M2-C7) ─────────────────────────────────────────
//
// Raw dump queries — no transforms here. Encryption stays on the blob;
// ScreenTimePlugin handles decryption via CryptoService because the key
// is per-plugin and SessionStore has no visibility into CryptoService.

QList<QVariantMap> SessionStore::allSessions(Database& db) {
    // window_title_enc comes back as a QByteArray from the BLOB column;
    // we surface it as-is and let the plugin decrypt.
    const QString sql = QStringLiteral(
        "SELECT id, app_name, window_title_enc, category, exe_path, "
        "       started_at, ended_at, duration_ms, is_idle_end, "
        "       day_local, hour_local, weekday_local "
        "FROM screen_time_app_session "
        "ORDER BY started_at ASC"
    );
    return db.query(sql);
}

QList<QVariantMap> SessionStore::allPickups(Database& db) {
    const QString sql = QStringLiteral(
        "SELECT id, event_type, occurred_at, day_local, hour_local, "
        "       weekday_local, prev_idle_ms "
        "FROM screen_time_activity_event "
        "ORDER BY occurred_at ASC"
    );
    return db.query(sql);
}

int SessionStore::sessionCount(Database& db) {
    const auto rows = db.query(QStringLiteral(
        "SELECT COUNT(*) AS c FROM screen_time_app_session"));
    if (rows.isEmpty()) return 0;
    return rows.first().value(QStringLiteral("c")).toInt();
}

bool SessionStore::clearAll(Database& db) {
    // Transaction so a mid-wipe failure rolls back — partial data loss
    // on "clear everything" is the worst possible UX.
    if (!db.transaction()) return false;
    if (!db.exec(QStringLiteral("DELETE FROM screen_time_app_session"))) {
        db.rollback();
        return false;
    }
    if (!db.exec(QStringLiteral("DELETE FROM screen_time_activity_event"))) {
        db.rollback();
        return false;
    }
    return db.commit();
}

} // namespace Margin::Plugins::ScreenTime
