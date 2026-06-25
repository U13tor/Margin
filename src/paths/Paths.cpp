// src/paths/Paths.cpp
//
// Wraps QStandardPaths so the rest of the codebase never hardcodes %APPDATA%
// or ~/Library/.... See docs/02-source-layout.md §3 for platform mappings.

#include "Paths.h"

#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>

namespace Margin {

QString Paths::config() {
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
}

QString Paths::data() {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

QString Paths::logs() {
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
           + QLatin1String("/logs");
}

QString Paths::cache() {
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
}

QString Paths::userPlugins() {
    return data() + QLatin1String("/plugins");
}

QString Paths::officialPlugins() {
    return QCoreApplication::applicationDirPath() + QLatin1String("/plugins");
}

void Paths::ensureDirs() {
    const QStringList dirs = {config(), data(), logs(), cache(), userPlugins()};
    for (const QString& p : dirs) {
        QDir().mkpath(p);
    }
}

QString Paths::dbFile() {
    return data() + QLatin1String("/margin.db");
}

QString Paths::mainLog() {
    return logs() + QLatin1String("/margin.log");
}

QString Paths::permissionsLog() {
    return logs() + QLatin1String("/permissions.log");
}

} // namespace Margin
