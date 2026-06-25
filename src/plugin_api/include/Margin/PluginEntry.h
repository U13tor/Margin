// PluginEntry - C entry-point macro for one-line PluginInterface declaration.
// Spec: docs/04-plugin-spec.md §2 "C entry function".
//
// Usage (at the bottom of your plugin's PluginImpl.cpp):
//
//     #include <Margin/PluginEntry.h>
//     #include "MyPlugin.h"
//
//     MARGIN_PLUGIN_ENTRY(MyNamespace::MyPlugin)
//
// Expands to an extern "C" exported function `margin_plugin_entry` that
// returns a pointer to a function-static instance. The manifest.json
// `entry_point` field must match this name (default "margin_plugin_entry";
// for a custom name, write the extern "C" block by hand).
//
// Why not Q_PLUGIN_METADATA: Margin2 rejects Qt's meta-object system across
// DLL boundaries, using a C ABI entry + QLibrary::resolve instead. See
// docs/04 §2 "why not dynamic_cast" and docs/13-lessons-learned.md anti-
// pattern 3.

#pragma once

#include <Margin/PluginInterface.h>
#include <QtPlugin>  // Q_DECL_EXPORT

#define MARGIN_PLUGIN_ENTRY(ClassName)                                          \
    extern "C" Q_DECL_EXPORT ::Margin::PluginInterface* margin_plugin_entry() { \
        static ClassName instance;                                              \
        return &instance;                                                       \
    }
