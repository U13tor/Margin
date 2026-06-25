// RhythmPlugin entry point — wires RhythmPlugin into the C ABI.
// The MARGIN_PLUGIN_ENTRY macro expands to an extern "C" exported function
// returning a function-static RhythmPlugin instance.

#include "RhythmPlugin.h"

#include "Margin/PluginEntry.h"

MARGIN_PLUGIN_ENTRY(Margin::Plugins::Rhythm::RhythmPlugin)
