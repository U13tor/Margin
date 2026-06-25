// DatabaseImpl impl — see host/services/DatabaseImpl.h.

#include "DatabaseImpl.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QString>
#include <QUuid>
#include <QVariant>
#include <QVariantMap>

#include <QStringList>

namespace Margin {

namespace {

// Tag prefix for the per-instance connection name. QSqlDatabase::addDatabase
// requires a UNIQUE name across the application, so we suffix with a UUID.
constexpr auto kConnectionPrefix = "margin_db_";

// Tag prefix for named bind params. SQL writes `:name`; the QVariantMap
// keys come without the colon. exec()/query() prepend it.
constexpr auto kParamPrefix = ":";

} // namespace

DatabaseImpl::DatabaseImpl()
    : m_connectionName(QString::fromLatin1(kConnectionPrefix) +
                       QUuid::createUuid().toString(QUuid::Id128)) {}

DatabaseImpl::~DatabaseImpl() {
    close();
}

bool DatabaseImpl::open(const QString& path) {
    QMutexLocker lock(&m_mutex);
    if (m_opened) return true;

    QSqlDatabase db = QSqlDatabase::addDatabase(
        QStringLiteral("QSQLITE"), m_connectionName);
    db.setDatabaseName(path);
    if (!db.open()) {
        captureError(QStringLiteral("QSqlDatabase::open failed"));
        // Drop the half-registered connection so a retry isn't blocked by
        // "duplicate connection name".
        QSqlDatabase::removeDatabase(m_connectionName);
        return false;
    }

    m_opened = true;
    if (!applyPragmas()) {
        // Pragma failure is fatal — close + remove so the next open() can
        // retry cleanly.
        db.close();
        QSqlDatabase::removeDatabase(m_connectionName);
        m_opened = false;
        return false;
    }

    m_lastError.clear();
    return true;
}

void DatabaseImpl::close() {
    QMutexLocker lock(&m_mutex);
    if (!m_opened) return;

    {
        QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
        if (db.isOpen()) db.close();
    }
    // removeDatabase must run after every QSqlDatabase handle for this
    // connection goes out of scope, else Qt logs "connection still in use".
    QSqlDatabase::removeDatabase(m_connectionName);
    m_opened = false;
    m_lastError.clear();
}

bool DatabaseImpl::applyPragmas() {
    // m_mutex held by caller.
    static const QStringList kPragmas = {
        QStringLiteral("PRAGMA journal_mode=WAL"),
        QStringLiteral("PRAGMA synchronous=NORMAL"),
        QStringLiteral("PRAGMA foreign_keys=ON"),
        QStringLiteral("PRAGMA busy_timeout=5000"),
    };
    QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
    for (const QString& sql : kPragmas) {
        QSqlQuery q(db);
        if (!q.exec(sql)) {
            captureError(QStringLiteral("pragma '%1' failed").arg(sql));
            return false;
        }
    }
    return true;
}

void DatabaseImpl::captureError(const QString& fallback) {
    // m_mutex held by caller.
    QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
    const QSqlError err = db.lastError();
    if (err.isValid() && !err.text().isEmpty()) {
        m_lastError = err.text();
    } else {
        m_lastError = fallback;
    }
}

bool DatabaseImpl::exec(const QString& sql, const QVariantMap& params) {
    QMutexLocker lock(&m_mutex);
    if (!m_opened) {
        m_lastError = QStringLiteral("database not open");
        return false;
    }
    QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
    QSqlQuery q(db);
    if (!q.prepare(sql)) {
        m_lastError = q.lastError().text();
        return false;
    }
    for (auto it = params.cbegin(); it != params.cend(); ++it) {
        q.bindValue(QString::fromLatin1(kParamPrefix) + it.key(), it.value());
    }
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    m_lastError.clear();
    return true;
}

QList<QVariantMap> DatabaseImpl::query(const QString& sql,
                                        const QVariantMap& params) {
    QMutexLocker lock(&m_mutex);
    QList<QVariantMap> rows;
    if (!m_opened) {
        m_lastError = QStringLiteral("database not open");
        return rows;
    }
    QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
    QSqlQuery q(db);
    if (!q.prepare(sql)) {
        m_lastError = q.lastError().text();
        return rows;
    }
    for (auto it = params.cbegin(); it != params.cend(); ++it) {
        q.bindValue(QString::fromLatin1(kParamPrefix) + it.key(), it.value());
    }
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return rows;
    }
    const QSqlRecord rec = q.record();
    const int n = rec.count();
    while (q.next()) {
        QVariantMap row;
        for (int i = 0; i < n; ++i) {
            row.insert(rec.fieldName(i), q.value(i));
        }
        rows.append(row);
    }
    m_lastError.clear();
    return rows;
}

bool DatabaseImpl::transaction() {
    QMutexLocker lock(&m_mutex);
    if (!m_opened) {
        m_lastError = QStringLiteral("database not open");
        return false;
    }
    QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
    if (!db.transaction()) {
        m_lastError = db.lastError().text();
        return false;
    }
    m_lastError.clear();
    return true;
}

bool DatabaseImpl::commit() {
    QMutexLocker lock(&m_mutex);
    if (!m_opened) {
        m_lastError = QStringLiteral("database not open");
        return false;
    }
    QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
    if (!db.commit()) {
        m_lastError = db.lastError().text();
        return false;
    }
    m_lastError.clear();
    return true;
}

bool DatabaseImpl::rollback() {
    QMutexLocker lock(&m_mutex);
    if (!m_opened) {
        m_lastError = QStringLiteral("database not open");
        return false;
    }
    QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
    if (!db.rollback()) {
        m_lastError = db.lastError().text();
        return false;
    }
    m_lastError.clear();
    return true;
}

QString DatabaseImpl::lastError() const {
    QMutexLocker lock(&m_mutex);
    return m_lastError;
}

} // namespace Margin
