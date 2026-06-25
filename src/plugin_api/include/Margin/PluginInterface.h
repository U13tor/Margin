// PluginInterface - plugin ABI core. Spec: docs/04-plugin-spec.md §2.
//
// 3 lifecycle hooks (onLoad/onConfigChange/onUnload) + 4 optional Contributor
// accessors (default nullptr, override as needed). Contributor types are
// defined in §7; the three landed ones are included above, OverlayContributor
// stays forward-declared to avoid pulling Qt Quick into plugin_api.

#pragma once

#include <Margin/DashboardTabContributor.h>
#include <Margin/PluginContext.h>
#include <Margin/Result.h>
#include <Margin/SettingsPageContributor.h>
#include <Margin/TrayMenuContributor.h>

#include <QJsonObject>

#include <string>

namespace Margin {

// Forward decl: OverlayContributor lands in M3-C4 (header exists but is kept
// out of PluginInterface.h's hot path). Pointer return type makes the forward
// decl sufficient — Host #includes OverlayContributor.h directly where it
// needs to call shouldShow()/overlayUrl()/dismiss().
class OverlayContributor;

class PluginInterface {
public:
    virtual ~PluginInterface() = default;

    /// Plugin id; must match manifest.json `id`.
    virtual std::string id() const = 0;

    /// Plugin version; must match manifest.json `version`.
    virtual std::string version() const = 0;

    /// Called once on load. Returning a failure puts the plugin in Unloaded.
    /// Subscribe to EventBus / init state / register Contributors here.
    virtual Result<void, std::string> onLoad(const PluginContext& ctx) = 0;

    /// Called when this plugin's settings subtree changes (user edited via
    /// settings center). The arg is the full snapshot of plugins.<id> taken
    /// from settings.json; same type as EventBus payloads (QJsonObject).
    virtual void onConfigChange(const QJsonObject& newConfig) = 0;

    /// Called once on unload. Must release all resources / drop all
    /// subscriptions. A 5-second timeout + force-unload is specified, but its
    /// enforcement is deferred — see docs/12-deferred-items.md A12.
    virtual void onUnload() = 0;

    // --- Contributor accessors (default nullptr, override as needed) ---
    //
    // These are the ONLY way for Host to obtain §7 optional-interface
    // pointers. The plugin does `return this;` inside its own TU (static_cast
    // is safe given known MI layout). Host side has zero dynamic_cast, which
    // keeps the boundary stable across DLLs. See docs/04 §2 "why not
    // dynamic_cast".

    virtual TrayMenuContributor*     asTrayMenu()     { return nullptr; }
    virtual SettingsPageContributor* asSettingsPage() { return nullptr; }
    virtual DashboardTabContributor* asDashboardTab() { return nullptr; }
    virtual OverlayContributor*      asOverlay()      { return nullptr; }
};

} // namespace Margin
