// Database — SQLite-backed persistence exposed to plugins.
// Spec: docs/05-host-services.md §5.
//
// Impl lives in host/services/DatabaseImpl (host-internal). Plugins obtain
// the Database via HostServices::database() — returns nullptr until M2-C2
// wires HostCore to instantiate DatabaseImpl, or if open() failed at host
// bootstrap (disk full / permissions). Plugins MUST null-check before use.
//
// Threading: all methods are serialized via internal mutex, but callers
// should still confine use to the Qt main thread — Qt's QSqlDatabase has
// thread-affinity rules and M2 plugin signals (WindowMonitorService /
// InputMonitorService) deliver on the main thread anyway.
//
// Schema ownership: each plugin owns its tables with a per-plugin prefix
// (e.g. screen_time_*, aura_*, rhythm_*) per docs/05 §5.2. Host does not
// enforce prefixing — plugins self-police via their CREATE TABLE names.

#pragma once

#include <QList>
#include <QString>
#include <QVariantMap>

namespace Margin {

class Database {
public:
    virtual ~Database() = default;

    /// Open or create the SQLite file at `path`. Idempotent: returns true
    /// if already open with the same path. Returns false on failure
    /// (lastError() describes why).
    virtual bool open(const QString& path) = 0;

    /// Close + drop the underlying connection. Safe to call when closed.
    virtual void close() = 0;

    /// Execute a non-SELECT statement with named bind parameters
    /// (`:name` syntax in SQL, key in params without the colon).
    /// Returns false on error.
    virtual bool exec(const QString& sql,
                      const QVariantMap& params = {}) = 0;

    /// Run a SELECT, return rows as a list of {column_name: value} maps.
    /// Empty list on error or no rows — callers distinguish via
    /// lastError().isEmpty().
    virtual QList<QVariantMap> query(const QString& sql,
                                     const QVariantMap& params = {}) = 0;

    /// Transaction control. SQLite default isolation. Nested transactions
    /// not supported — calling transaction() twice returns true (no-op).
    virtual bool transaction() = 0;
    virtual bool commit() = 0;
    virtual bool rollback() = 0;

    /// Human-readable error from the most recent failed call. Empty when
    /// the last operation succeeded.
    virtual QString lastError() const = 0;
};

} // namespace Margin
