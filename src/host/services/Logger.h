// Logger impl — file sink with daily rotation, 7-day retention, 5MB cap,
// min-level filter, stdout mirror. Spec: docs/05 §2 + docs/02 §3.5.

#pragma once

#include "Margin/Logger.h"

#include <QFile>
#include <QMutex>
#include <QString>

namespace Margin {

class LoggerImpl : public Logger {
public:
    explicit LoggerImpl(const QString& logsDir);
    ~LoggerImpl() override;

    void log(Level level, const QString& tag, const QString& message) override;

    void setMinimumLevel(Level level) { m_min = level; }

    /// Current log file path (set in ctor after rotation prune). M5-C4d:
    /// surfaced so the Settings → About page can show users where the log
    /// lives without having to guess based on Paths::logs().
    QString path() const { return m_path; }

private:
    void rotateIfNeeded();
    void pruneOldLogs();
    QString formatLine(Level level, const QString& tag, const QString& message);
    static const char* levelLabel(Level level);

    static constexpr qint64 kMaxSize = 5 * 1024 * 1024;

    QMutex  m_mutex;
    QFile   m_file;
    QString m_path;
    QString m_dir;
    Level   m_min = Level::Info;
};

} // namespace Margin
