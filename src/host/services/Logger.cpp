// Logger impl — see Logger.h for spec.

#include "Logger.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QMutexLocker>
#include <QStringLiteral>
#include <QTextStream>

#include <cstdio>

namespace Margin {

LoggerImpl::LoggerImpl(const QString& logsDir)
    : m_dir(logsDir),
      m_path(logsDir + QLatin1String("/margin.log")),
      m_min(Level::Info) {
    pruneOldLogs();
    m_file.setFileName(m_path);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        // No logger exists yet — surface on stderr (feedback_windows_build.md #7).
        fprintf(stderr, "[FATAL] Logger: cannot open %s\n", qPrintable(m_path));
    }
}

LoggerImpl::~LoggerImpl() {
    QMutexLocker lock(&m_mutex);
    if (m_file.isOpen()) m_file.close();
}

void LoggerImpl::log(Level level, const QString& tag, const QString& message) {
    if (static_cast<int>(level) < static_cast<int>(m_min)) return;

    QMutexLocker lock(&m_mutex);

    if (m_file.isOpen() && m_file.size() > kMaxSize) rotateIfNeeded();
    if (!m_file.isOpen()) return;

    const QString line = formatLine(level, tag, message);
    {
        QTextStream out(&m_file);
        out << line << '\n';
    }
    m_file.flush();

    fprintf(stdout, "%s\n", qPrintable(line));
    fflush(stdout);
}

void LoggerImpl::rotateIfNeeded() {
    if (m_file.isOpen()) m_file.close();

    const QString dateStr = QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));
    QString rotated = m_dir + QLatin1String("/margin-") + dateStr + QLatin1String(".log");
    if (QFileInfo::exists(rotated)) {
        rotated = m_dir + QLatin1String("/margin-") + dateStr + QLatin1Char('-')
                  + QTime::currentTime().toString(QStringLiteral("HHmmss")) + QLatin1String(".log");
    }

    if (!QFile::rename(m_path, rotated)) {
        // Rename failed (cross-device / permission) — fall back to truncate.
        m_file.setFileName(m_path);
        if (m_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            fprintf(stderr, "[WARN] Logger: rename failed, truncated %s\n",
                    qPrintable(m_path));
        }
        return;
    }

    m_file.setFileName(m_path);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        fprintf(stderr, "[FATAL] Logger: cannot reopen %s after rotate\n",
                qPrintable(m_path));
        return;
    }

    const QString marker = formatLine(Level::Warn, QStringLiteral("host"),
        QStringLiteral("Log rotated (previous file exceeded 5MB cap)"));
    QTextStream out(&m_file);
    out << marker << '\n';
    m_file.flush();
}

void LoggerImpl::pruneOldLogs() {
    QDir dir(m_dir);
    const QStringList files = dir.entryList({QStringLiteral("margin-*.log")}, QDir::Files);
    const QDateTime cutoff = QDateTime::currentDateTime().addDays(-7);
    for (const QString& name : files) {
        const QString full = dir.absoluteFilePath(name);
        if (QFileInfo(full).lastModified() < cutoff) QFile::remove(full);
    }
}

QString LoggerImpl::formatLine(Level level, const QString& tag, const QString& message) {
    return QStringLiteral("[%1] [%2] [%3] %4")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
             QLatin1String(levelLabel(level)),
             tag,
             message);
}

const char* LoggerImpl::levelLabel(Level level) {
    switch (level) {
        case Level::Debug: return "DEBUG";
        case Level::Info:  return "INFO";
        case Level::Warn:  return "WARN";
        case Level::Error: return "ERROR";
        case Level::Fatal: return "FATAL";
    }
    return "?";
}

} // namespace Margin
