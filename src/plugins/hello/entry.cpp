// Hello plugin entry point — wires HelloPlugin into the C ABI.
// The MARGIN_PLUGIN_ENTRY macro expands to an extern "C" exported function
// returning a function-static HelloPlugin instance.

#include "HelloPlugin.h"

#include "Margin/PluginEntry.h"

MARGIN_PLUGIN_ENTRY(Margin::Plugins::Hello::HelloPlugin)
