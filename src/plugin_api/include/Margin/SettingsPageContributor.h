// SettingsPageContributor — host-pull interface for plugins to contribute a
// page to the Layer 2 Settings window (docs/06 §4.6). Spec: docs/04-plugin-
// spec.md §7.
//
// Plugins override PluginInterface::asSettingsPage() to return `this` (after
// multiply-inheriting SettingsPageContributor). Host calls pageInfo() once on
// load to register the page with SettingsRegistry, which exposes all pages
// (host + plugin) to QML as the `settingsRegistry.pages` context property.
// Display order within the "plugins" section is determined by PageInfo::order
// (ascending).
//
// The `section` field on SettingsRegistry::Entry is Host-determined — plugin
// pages always land in the "plugins" section, so PageInfo does not expose it.
// Host pages (General / Appearance / About) use the "host" / "about" sections.

#pragma once

#include <QUrl>

#include <string>

namespace Margin {

class SettingsPageContributor {
public:
    struct PageInfo {
        std::string  id;           // "aura", "rhythm", ...
        std::string  title;        // sidebar label
        QUrl         icon;         // qrc:/icons/<x>.svg (optional)
        QUrl         content_qml;  // qrc:/<prefix>/qml/<X>SettingsPage.qml
        int          order = 0;    // ascending within "plugins" section
    };

    virtual ~SettingsPageContributor() = default;

    /// Called by Host on plugin load. Returns the page descriptor used to
    /// populate the Settings window's sidebar entry and content Loader.
    /// Stable across calls — Host caches the result for the plugin's
    /// lifetime.
    virtual PageInfo pageInfo() const = 0;
};

} // namespace Margin
