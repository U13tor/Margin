// DatabaseImpl — concrete QSqlDatabase wrapper. Single named connection,
// SQLite + WAL + synchronous=NORMAL + foreign_keys=ON + busy_timeout=5000
// per docs/05-host-services.md §5.3. Mutex-serialised for thread safety;
// see Database.h for the main-thread-affinity caveat.
//
// Spec: docs/05-host-services.md §5 + docs/02-source-layout.md §1.1
// (host/services/DatabaseImpl).

#pragma once

#include "Margin/Database.h"

#include <QMutex>
#include <QString>

namespace Margin {

class DatabaseImpl : public Database {
public:
    /// Connection name is generated per-instance so multiple DatabaseImpl
    /// objects (e.g. in tests) don't collide on QSqlDatabase's global
    /// connection registry.
    DatabaseImpl();
    ~DatabaseImpl() override;

    DatabaseImpl(const DatabaseImpl&) = delete;
    DatabaseImpl& operator=(const DatabaseImpl&) = delete;

    bool open(const QString& path) override;
    void close() override;
    bool exec(const QString& sql, const QVariantMap& params = {}) override;
    QList<QVariantMap> query(const QString& sql,
                             const QVariantMap& params = {}) override;
    bool transaction() override;
    bool commit() override;
    bool rollback() override;
    QString lastError() const override;

private:
    // Apply WAL + synchronous + foreign_keys + busy_timeout pragmas. Called
    // once under m_mutex after the underlying QSqlDatabase opens.
    bool applyPragmas();

    // Stash the QSqlError text into m_lastError. Helper for open()/applyPragmas().
    void captureError(const QString& fallback);

    // Serialises every method. QSqlDatabase is not internally thread-safe
    // even though SQLite itself supports concurrent reads under WAL — Qt
    // docs require all use from the connection's owning thread, which we
    // approximate by gating every call here.
    mutable QMutex m_mutex;
    QString        m_connectionName;
    QString        m_lastError;
    bool           m_opened = false;
};

} // namespace Margin
