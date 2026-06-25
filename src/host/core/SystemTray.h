// SystemTray — QSystemTrayIcon wrapper, implements TrayService.
// Spec: docs/05 §6 + docs/08 (Qt cross-platform, no platform-specific code).
// Plugin-contributed menu items land via TrayMenuContributor (M0-C6):
// PluginManager calls addPluginItems() after load and connects to
// pluginItemClicked to route clicks back through onTrayItemClicked().

#pragma once

#include "Margin/TrayMenuContributor.h"
#include "Margin/TrayService.h"

#include <QHash>
#include <QIcon>
#include <QObject>
#include <QString>

#include <functional>
#include <memory>

class QMenu;
class QSystemTrayIcon;
class QAction;
class QDialog;

namespace Margin {

/// Lookup callback: given a pluginId, return its TrayMenuContributor (or
/// nullptr if the plugin exposes none). Injected by PluginManager so
/// SystemTray can re-pull items on `refreshPluginMenu` without depending on
/// plugin internals. M4-C16.
using TrayContributorLookup =
    std::function<TrayMenuContributor*(const QString& pluginId)>;

class SystemTray : public QObject, public TrayService {
    Q_OBJECT

public:
    SystemTray();
    ~SystemTray() override;

    SystemTray(const SystemTray&) = delete;
    SystemTray& operator=(const SystemTray&) = delete;

    // TrayService
    void showToast(const QString& title,
                   const QString& message,
                   int timeoutMs = 5000) override;
    void setIconState(IconState state) override;
    void setTooltip(const QString& text) override;
    void refreshPluginMenu(const QString& pluginId) override;

    // HostCore owns visibility lifecycle.
    void show();
    void hide();

    // Plugin menu integration. Called by PluginManager after each plugin
    // loads. Items are appended above the Quit action with a separator.
    // pluginItemClicked(pluginId, itemId) fires on click.
    void addPluginItems(const QString& pluginId,
                        const QList<TrayMenuContributor::TrayItem>& items);

    /// Remove all menu items contributed by `pluginId`. Called by
    /// PluginManager when unloading a plugin (forward path; not used in
    /// M0-C6 since Hello lives until shutdown).
    void removePluginItems(const QString& pluginId);

    /// Inject the contributor lookup callback used by refreshPluginMenu.
    /// Called once by PluginManager during bootstrap.
    void setContributorLookup(TrayContributorLookup lookup);

    /// Re-evaluate every menu label under the currently-installed translator.
    /// Called by HostCore::applyLanguage after installTranslator so host
    /// actions (Open Dashboard / Settings / About / Quit) flip language
    /// alongside the QML UI. Also re-pulls each plugin's tray items so
    /// labels built with QCoreApplication::translate() in plugin code
    /// refresh — otherwise plugin-contributed labels stay stale.
    void retranslate();

    /// Test-only accessor: exposes the internal QMenu for structural
    /// assertions (M4-C16). Not for production use.
    QMenu* menuForTesting() const { return m_menu.get(); }

signals:
    void pluginItemClicked(const QString& pluginId, const QString& itemId);
    // Emitted on the "Open Dashboard" menu item or a left-click/double-click on
    // the tray icon (docs/06 §3.2 "Layer 0 → Layer 1"). HostCore shows the panel.
    void openDashboardRequested();
    // Emitted on the "Settings..." menu item AND the "About" menu item (both
    // route to the SettingsWindow — About opens the About page). The pageId
    // arg lets the receiver tell SettingsWindow which sidebar entry to focus:
    //   empty string → default (General) — used by tray Settings and Ctrl+,
    //   "about"      → About page       — used by tray About,
    //   "aura"/"rhythm"/"screen_time"/... → plugin page — used by Tab Settings.
    // Same signal/one signature avoids the alternative of having two separate
    // signals that both end up calling QML openSettings().
    void openSettingsRequested(const QString& pageId = {});

private:
    void buildMenu();
    void loadIcons();
    void applyMenuStyle();
    void rebuildMenu();
    // PR5: render a stroke-only SVG (e.g. tray.svg) to a 32x32 transparent
    // QPixmap, then wrap in QIcon. Prevents Windows shell from monochroming
    // the SVG's brand-purple stroke when it rasterizes QIcon(svgPath).
    QIcon renderSvgIcon(const QString& svgPath);

    struct PluginAction {
        QString pluginId;
        QString itemId;
    };

    std::unique_ptr<QSystemTrayIcon>     m_tray;
    std::unique_ptr<QMenu>               m_menu;
    QIcon                                m_iconNormal;
    QIcon                                m_iconLocked;
    QIcon                                m_iconStretching;
    QIcon                                m_iconOpen;
    QIcon                                m_iconSettings;
    QIcon                                m_iconAbout;
    QIcon                                m_iconQuit;
    // Plugin contributions, kept in insertion order for stable rebuilds.
    QList<QPair<QString, QList<TrayMenuContributor::TrayItem>>> m_pluginItems;
    QHash<QAction*, PluginAction>        m_actionMap;
    TrayContributorLookup                m_lookup;
};

} // namespace Margin
