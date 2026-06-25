// ScreenTimePlugin entry point — wires ScreenTimePlugin into the C ABI.
// The MARGIN_PLUGIN_ENTRY macro expands to an extern "C" exported function
// returning a function-static ScreenTimePlugin instance.

#include "ScreenTimePlugin.h"

#include "Margin/PluginEntry.h"

MARGIN_PLUGIN_ENTRY(Margin::Plugins::ScreenTime::ScreenTimePlugin)
