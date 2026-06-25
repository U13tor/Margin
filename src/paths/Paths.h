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

    static QString dbFile();
    static QString mainLog();
    static QString permissionsLog();
};

} // namespace Margin
