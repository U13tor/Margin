// src/paths/Paths.h
//
// Unified access to all runtime directories (config / data / logs / cache / plugins).
// Code MUST go through Margin::Paths instead of calling QStandardPaths directly.
//
// Spec: docs/02-source-layout.md §3 + §4.

#pragma once

#include <QString>

namespace Margin {

class Paths {
public:
    static QString config();
    static QString data();
    static QString logs();
    static QString cache();
    static QString userPlugins();
    static QString officialPlugins();

    static void ensureDirs();

    // One-shot migration from the legacy layout (where Paths::data() pointed
    // at %LOCALAPPDATA%\Margin\ — the NSIS InstallDir, so uninstall wiped
    // margin.db / keyring / user plugins). Called from HostCore::bootstrap
    // after ensureDirs(). No-op on macOS / Linux (paths never overlapped the
    // install prefix there). Idempotent: each item is skipped if dst exists.
    static void migrateFromLegacyLayout();

    // Test seam: migrates one item (file or directory tree) from src to dst.
    // Returns true if (a) src doesn't exist (nothing to do, success),
    // (b) dst already exists (idempotent skip, success), or (c) copy
    // succeeded. Returns false on copy error; partial copy may leave
    // whatever got copied in place for diagnostic purposes.
    static bool migrateItem(const QString& src, const QString& dst);

    static QString dbFile();
    static QString mainLog();
    static QString permissionsLog();
};

} // namespace Margin
