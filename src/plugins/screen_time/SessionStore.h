// SessionStore — SQLite persistence for screen_time plugin.
// Spec: docs/11-roadmap.md M2-C3 schema (Apple Screen Time / 小米 usage
// tracking parity).
//
// Two tables, both prefixed `screen_time_` per docs/05 §5.2:
//
//   screen_time_app_session
//     One row per foreground-window interval. Opened on
//     activeWindowChanged; closed on the next switch or on idle. The
//     window_title is AES-256-GCM ciphertext under the plugin's per-plugin
//     HKDF key (caller encrypts before passing). day_local / hour_local /
//     weekday_local are denormalized time-bucket columns precomputed at
//     insert so day/week/hour-heatmap GROUP BY queries don't need to do
//     timezone math at query time.
//
//   screen_time_activity_event
//     One row per discrete pickup/resume event (idle→active edge).
//     Separated from session because pickups are point events, not
//     intervals; prev_idle_ms carries the just-ended idle duration so
//     "you zoned out for 15 minutes" can be shown next to the pickup.
//
// Pure C++ class — ScreenTimePlugin owns one. Not a QObject: the plugin's
// Q_PROPERTY surface wraps the data this class writes; signals are
// emitted from ScreenTimePlugin::onActiveWindowChanged etc. directly.

#pragma once

#include <QByteArray>
#include <QDate>
#include <QList>
#include <QString>
#include <QVariantMap>

namespace Margin {
class Database;
}

namespace Margin::Plugins::ScreenTime {

class SessionStore {
public:
    SessionStore();
    ~SessionStore();

    SessionStore(const SessionStore&) = delete;
    SessionStore& operator=(const SessionStore&) = delete;

    /// CREATE TABLE + indexes. Idempotent (CREATE ... IF NOT EXISTS).
    /// Safe to call every onLoad.
    bool ensureSchema(Database& db);

    /// Open a new app_session row with `started_at` (epoch ms UTC) +
    /// precomputed time-bucket columns. windowTitleEnc is already-encrypted
    /// ciphertext from CryptoService::encryptString — this layer is
    /// storage-only, knows nothing about keys. exePath is the full
    /// executable path (PR3 round-2 #2b) — surfaced via topAppsByDay so
    /// MBarChart can bind image://appicon/<exePath> delegates. Empty
    /// string is allowed (older rows + Mac stub). Returns the new row id;
    /// 0 on failure (caller checks db->lastError()).
    long long openSession(Database& db,
                          const QString& appName,
                          const QByteArray& windowTitleEnc,
                          const QString& category,
                          const QString& exePath,
                          qint64 startedAt);

    /// Close a session row: set ended_at / duration_ms / is_idle_end.
    /// Safe to call with rowId=0 (no-op).
    bool closeSession(Database& db,
                      long long rowId,
                      qint64 endedAt,
                      bool isIdleEnd);

    /// Insert a pickup event (idle→active edge). prevIdleMs is the
    /// duration of the just-ended idle window; 0 if there was no idle.
    bool insertPickup(Database& db,
                      qint64 occurredAt,
                      qint64 prevIdleMs);

    // ── Report queries (M2-C6) ──────────────────────────────────────
    // Each returns rows as QVariantMaps so the plugin can expose them
    // directly as Q_PROPERTY QVariantList. All queries are read-only
    // (SELECT) — caller does not need to wrap in a transaction.

    /// Top N apps by total duration in a single day.
    /// Each row: {app_name, duration_ms, category, exe_path}.
    /// exe_path is the most-recent non-empty path for the app (MAX(EXPR))
    /// — nullable for older rows + Mac stub. PR3 round-2 #2b.
    QList<QVariantMap> topAppsByDay(Database& db, int dayLocal, int limit);

    /// Top N apps by total duration in a date range [from, to] inclusive.
    /// Each row: {app_name, duration_ms, category, exe_path}.
    QList<QVariantMap> topAppsByRange(Database& db, int fromDayLocal, int toDayLocal, int limit);

    /// Per-category totals for a single day.
    /// Each row: {category, duration_ms}.
    QList<QVariantMap> categoriesByDay(Database& db, int dayLocal);

    /// Per-category totals for a date range [from, to] inclusive.
    /// Each row: {category, duration_ms}.
    QList<QVariantMap> categoriesByRange(Database& db, int fromDayLocal, int toDayLocal);

    /// Total duration per day over a [from, to] inclusive range.
    /// Used by week + month views. Each row: {day_local, duration_ms}.
    QList<QVariantMap> dailyTotals(Database& db, int fromDayLocal, int toDayLocal);

    /// Count of pickup events in a single day.
    int pickupCountByDay(Database& db, int dayLocal);

    // ── Export / clear (M2-C7) ───────────────────────────────────────
    // allSessions / allPickups return encrypted blobs as-is — the caller
    // (ScreenTimePlugin) owns CryptoService and decrypts per row. This
    // keeps the crypto boundary at the plugin layer where the per-plugin
    // HKDF key lives; SessionStore stays storage-only.

    /// Every session row, ordered oldest-first. Each row mirrors the
    /// schema: {id, app_name, window_title_enc (QByteArray),
    /// category, started_at, ended_at, duration_ms, is_idle_end,
    /// day_local, hour_local, weekday_local, exe_path}.
    QList<QVariantMap> allSessions(Database& db);

    /// Every pickup event, ordered oldest-first. Each row:
    /// {id, event_type, occurred_at, day_local, hour_local,
    ///  weekday_local, prev_idle_ms}.
    QList<QVariantMap> allPickups(Database& db);

    /// Total session count — used by the "clear N sessions" confirm
    /// dialog in ExportClearDialog.qml.
    int sessionCount(Database& db);

    /// DELETE FROM both tables. Wrap in a transaction so a partial wipe
    /// can't happen if SQLite fails mid-way.
    bool clearAll(Database& db);

    /// ── Time-bucket helper ──────────────────────────────────────────
    /// Decompose epoch-ms UTC into local-time-zone buckets:
    ///   day_local     — YYYYMMDD (e.g. 20260619)
    ///   hour_local    — 0..23
    ///   weekday_local — 0..6 with Monday=0, Sunday=6 (ISO weekday - 1)
    /// Public + static so unit tests can assert math without a Database.
    struct TimeBuckets {
        int dayLocal;
        int hourLocal;
        int weekdayLocal;
    };
    static TimeBuckets computeTimeBuckets(qint64 epochMs);

    /// Convert a QDate to its YYYYMMDD int — convenience for callers
    /// driving queries with day arithmetic.
    static int dayLocalFromDate(const QDate& d) {
        return d.year() * 10000 + d.month() * 100 + d.day();
    }
};

} // namespace Margin::Plugins::ScreenTime
