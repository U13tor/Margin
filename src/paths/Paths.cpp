// src/paths/Paths.cpp
//
// Wraps QStandardPaths so the rest of the codebase never hardcodes %APPDATA%
// or ~/Library/.... See docs/02-source-layout.md §3 for platform mappings.

#include "Paths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

#include <cstdio>

namespace Margin {

namespace {
// Recursive directory copy. Returns true on full success. Partial copy
// (some files failed) returns false but leaves whatever got copied for
// diagnostic purposes. Idempotent per-file: skips when target exists with
// the same size as source.
bool copyDirRecursive(const QString& src, const QString& dst) {
    QDir srcDir(src);
    if (!srcDir.exists()) return false;
    QDir().mkpath(dst);
    const QFileInfoList entries =
        srcDir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
    for (const QFileInfo& entry : entries) {
        const QString srcPath = entry.absoluteFilePath();
        const QString dstPath = dst + QLatin1Char('/') + entry.fileName();
        if (entry.isDir()) {
            if (!copyDirRecursive(srcPath, dstPath)) return false;
        } else {
            if (QFileInfo::exists(dstPath) && QFileInfo(dstPath).size() == entry.size()) {
                continue;  // already migrated
            }
            if (!QFile::copy(srcPath, dstPath)) return false;
        }
    }
    return true;
}
} // namespace

QString Paths::config() {
    // %APPDATA%\Margin (Win Roaming) | ~/Library/Preferences/Margin (macOS).
    // AppConfigLocation:settings.json lives directly under here (no /config
    // subdir; main.cpp:31 sets org name to empty so Qt does not double-nest).
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
}

QString Paths::data() {
    // Cross-platform "user data" root that must survive NSIS uninstall on
    // Windows. GenericDataLocation on Windows in Qt 6.7 actually resolves to
    // %LOCALAPPDATA% (Local, = NSIS InstallDir $INSTDIR), not %APPDATA%
    // (Roaming) as the Qt docs suggest — verified empirically. Force Roaming
    // via %APPDATA% on Win; macOS/Linux keep using GenericDataLocation (where
    // it correctly resolves to ~/Library/Application Support etc).
#if defined(Q_OS_WIN)
    const QByteArray appdata = qgetenv("APPDATA");
    if (appdata.isEmpty()) {
        // Fallback: GenericDataLocation if APPDATA is unset (service accounts,
        // unusual embedded hosts). Will land in Local but at least functional.
        return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
               + QLatin1String("/Margin/data");
    }
    return QString::fromLocal8Bit(appdata) + QLatin1String("/Margin/data");
#else
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
           + QLatin1String("/Margin/data");
#endif
}

QString Paths::logs() {
    // Logs are machine-local, recreatable, and tied to the install — Local
    // is the right home. NSIS uninstall clearing them is acceptable.
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
           + QLatin1String("/logs");
}

QString Paths::cache() {
    // Cache is machine-local and recreatable. Same reasoning as logs().
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
}

QString Paths::userPlugins() {
    // Third-party plugins installed by the user. Must survive NSIS uninstall
    // (user paid attention to picking them), so follow data() to Roaming,
    // sibling to data()/.
#if defined(Q_OS_WIN)
    const QByteArray appdata = qgetenv("APPDATA");
    if (!appdata.isEmpty()) {
        return QString::fromLocal8Bit(appdata) + QLatin1String("/Margin/plugins");
    }
#endif
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
           + QLatin1String("/Margin/plugins");
}

QString Paths::officialPlugins() {
    // Bundled with the .exe; reinstalls replace them. applicationDirPath()
    // = INSTDIR/plugins on Windows, .app/Contents/plugins on macOS.
    return QCoreApplication::applicationDirPath() + QLatin1String("/plugins");
}

void Paths::ensureDirs() {
    const QStringList dirs = {config(), data(), logs(), cache(), userPlugins()};
    for (const QString& p : dirs) {
        QDir().mkpath(p);
    }
}

bool Paths::migrateItem(const QString& src, const QString& dst) {
    if (!QFileInfo::exists(src)) return true;       // nothing to migrate

    const QFileInfo srcInfo(src);
    const QFileInfo dstInfo(dst);
    // File idempotency: if dst already exists as a file, never overwrite
    // (caller may have customized it since the last migration). For dirs,
    // the dst may have been pre-created empty by ensureDirs(); let
    // copyDirRecursive do per-file idempotency instead of bailing out.
    if (srcInfo.isFile() && dstInfo.exists()) return true;

    QDir().mkpath(dstInfo.absolutePath());
    const bool isDir = srcInfo.isDir();
    const bool ok = isDir ? copyDirRecursive(src, dst) : QFile::copy(src, dst);
    if (!ok) {
        fprintf(stderr,
                "[WARN] Paths::migrateItem: failed %s %s -> %s\n",
                isDir ? "dir" : "file",
                qPrintable(src),
                qPrintable(dst));
    } else {
        fprintf(stderr,
                "[INFO] Paths::migrateItem: %s migrated %s -> %s\n",
                isDir ? "dir" : "file",
                qPrintable(src),
                qPrintable(dst));
    }
    return ok;
}

void Paths::migrateFromLegacyLayout() {
#if defined(Q_OS_WIN)
    // Legacy Paths::data() (= AppDataLocation) used to land at
    // %LOCALAPPDATA%\Margin\, which is exactly the NSIS InstallDir. Any
    // margin.db / keyring/ / user plugins/ written there get wiped on
    // uninstall (RMDir /r "$INSTDIR"). Migrate them to the new Roaming
    // layout before any consumer reads Paths::data()/dbFile().
    //
    // Source file/dir kept in place after copy — the next NSIS uninstall
    // cleans INSTDIR naturally, and a downgrade reinstall can still find
    // the legacy data. We do NOT migrate settings.json because it has
    // always lived under Paths::config() (= Roaming), unaffected.
    const QString legacyRoot =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (legacyRoot.isEmpty()) return;

    migrateItem(legacyRoot + QLatin1String("/margin.db"),
                data() + QLatin1String("/margin.db"));
    migrateItem(legacyRoot + QLatin1String("/keyring"),
                data() + QLatin1String("/keyring"));
    // Note: legacy plugins/ is intentionally NOT migrated. The legacy layout
    // had userPlugins() overlap officialPlugins() (= INSTDIR/plugins), so a
    // blind copy would duplicate bundled plugins into userPlugins() and cause
    // PluginLoader to register each official plugin twice. Official plugins
    // reload from INSTDIR/plugins on every launch; third-party plugins (the
    // only thing userPlugins() is meant for) cannot be distinguished from
    // bundled ones without a manifest registry, so we accept losing them on
    // upgrade and let users reinstall. Margin has no shipped third-party
    // plugins today, so this is a no-op in practice.
#else
    // macOS / Linux: Paths::data() never overlapped the .app bundle / install
    // prefix, so there is nothing to migrate. If a future refactor changes
    // the macOS layout, add a platform branch here.
#endif
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
