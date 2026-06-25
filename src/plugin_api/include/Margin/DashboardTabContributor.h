// DashboardTabContributor — host-pull interface for plugins to contribute a
// tab to the Layer 1 main panel (docs/06 §3.1). Spec: docs/04-plugin-spec.md
// §7.
//
// Plugins override PluginInterface::asDashboardTab() to return `this` (after
// multiply-inheriting DashboardTabContributor). Host calls tabInfo() once on
// load to register the tab with DashboardTabRegistry, which exposes all tabs
// (host + plugin) to QML as the `dashboardTabs` context property. Display
// order is determined by TabInfo::order (ascending), not by load order —
// M1+ plugins slot in deterministically without host-side hardcoding.

#pragma once

#include <QUrl>

#include <string>

namespace Margin {

class DashboardTabContributor {
public:
    struct TabInfo {
        std::string  id;           // "aura", "screen_time", "rhythm"
        std::string  title;        // "蓝牙锁屏" / "应用时长" / "健康节律"
        QUrl         icon;         // qrc:/icons/<x>.svg
        QUrl         content_qml;  // qrc:/ui/<x>Tab.qml
        int          order = 0;    // ascending; Overview=10, Aura=20,
                                   // Screen Time=30, Rhythm=40
    };

    virtual ~DashboardTabContributor() = default;

    /// Called by Host on plugin load. Returns the tab descriptor used to
    /// populate the main panel's TabBar and StackLayout. Stable across calls
    /// — Host caches the result for the plugin's lifetime.
    virtual TabInfo tabInfo() const = 0;
};

} // namespace Margin
