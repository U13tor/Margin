// TrayMenuContributor — host-pull interface for plugins to contribute tray
// menu items. Spec: docs/04-plugin-spec.md §7.
//
// Plugins override PluginInterface::asTrayMenu() to return `this` (after
// multiply-inheriting TrayMenuContributor). Host calls contributeTrayItems
// on load and onTrayItemClicked when the user picks one.

#pragma once

#include <QList>
#include <QString>

#include <string>

namespace Margin {

class TrayMenuContributor {
public:
    struct TrayItem {
        std::string  id;          // routing key for click events
        std::string  label;       // display text
        std::string  icon_url;    // optional SVG URL; empty = no icon
        bool         checkable = false;
        bool         checked   = false;
        bool         enabled   = true;
        // M4-C16: info-only line (e.g. "Today's Focus: 2h 15m"). Host renders
        // read-only items as disabled labels in the preview section, never
        // routes clicks. See docs/04-plugin-spec.md §7 + docs/06 §4.8.
        bool         read_only = false;
    };

    virtual ~TrayMenuContributor() = default;

    /// Called by Host on plugin load (and any time the host rebuilds the
    /// menu). Return the items this plugin wants in the tray menu.
    virtual QList<TrayItem> contributeTrayItems() = 0;

    /// Called by Host when the user clicks an item whose id matches one
    /// returned by contributeTrayItems().
    virtual void onTrayItemClicked(const std::string& id) = 0;
};

} // namespace Margin
