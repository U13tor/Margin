// AuraLockerPlugin entry point — wires AuraLockerPlugin into the C ABI.
// The MARGIN_PLUGIN_ENTRY macro expands to an extern "C" exported function
// returning a function-static AuraLockerPlugin instance.

#include "AuraLockerPlugin.h"

#include "Margin/PluginEntry.h"

MARGIN_PLUGIN_ENTRY(Margin::Plugins::Aura::AuraLockerPlugin)
