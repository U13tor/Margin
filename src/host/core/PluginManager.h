// PluginManager — discovers, loads, validates and unloads plugin DLLs.
// Spec: docs/04-plugin-spec.md §2-3 + docs/05-host-services.md §1.
//
// Scans one or more dirs in order; later dirs override earlier ones by id
// (user-level wins over official per docs/02-source-layout.md §3.3).
// Each candidate must ship `<basename>.manifest.json` next to the DLL.
//
// ABI validation per docs/04 §2: abi_version strict-equal, min_host_version
// <= MARGIN_VERSION, manifest id/version match instance id()/version().
//
// Tray integration: after a successful load, if the plugin exposes
// TrayMenuContributor (asTrayMenu() != nullptr), PluginManager forwards
// the contributed items to SystemTray and routes click events back.
//
// Lifetime: HostCore owns one PluginManager; unloadAll is called before
// EventBus::reset so plugins can still publish a goodbye event.

#pragma once

#include "Margin/PluginInterface.h"

#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <functional>

class QTranslator;

namespace Margin {

class Logger;
class EventBus;
class Settings;
class TrayService;
class CryptoServicePool;
class SystemTray;
class LockService;
class QmlService;
class Database;
class WindowMonitorService;
class InputMonitorService;

class PluginManager : public QObject {
    Q_OBJECT

public:
    // Each plugin gets its own HostServicesImpl wrapper on load, sharing
    // the four live services but holding a per-plugin CryptoService&
    // (see docs/05-host-services.md §9 "host=带独立 Crypto 的服务包装器").
    PluginManager(Logger& logger,
                  EventBus& eventBus,
                  Settings& settings,
                  TrayService& tray,
                  CryptoServicePool& cryptoPool);
    ~PluginManager() override;

    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    /// Scan dirs in order; later dirs override earlier ones by id.
    /// Loads all discovered plugins; failures are logged and skipped.
    void loadAll(const QStringList& scanDirs);

    /// Unload all plugins in reverse-load order:
    ///   onUnload -> eventBus.unsubscribeAll -> QLibrary::unload
    void unloadAll();

    /// Lookup loaded plugin by id; nullptr if not loaded.
    PluginInterface* plugin(const std::string& id) const;

    /// Invoke fn for each loaded plugin instance (skip nulls). Used by Host
    /// to scan for optional Contributor interfaces (e.g. DashboardTab) —
    /// hides the LoadedPlugin struct from callers.
    void forEachPlugin(std::function<void(PluginInterface*)> fn) const;

    /// Wire up tray-menu contribution. Optional; safe to leave unset (no
    /// tray contributions are forwarded). Click events are routed back to
    /// the contributing plugin via TrayMenuContributor::onTrayItemClicked.
    void setTrayIntegration(SystemTray* tray);

    /// Inject the host's PlatformBackend (as LockService*) so each plugin's
    /// HostServicesImpl.lock() resolves. Optional; nullptr on platforms
    /// without a backend (macOS until PlatformBackendMac lands).
    void setPlatformBackend(LockService* backend);

    /// Inject the host's QmlService (wraps the dashboard QQmlApplicationEngine).
    /// Plugins call host->qml()->registerContextProperty at onLoad to expose
    /// their QObject state to QML. Must be called before loadAll().
    void setQmlService(QmlService* qml);

    /// Inject the host's Database (M2-C2). HostCore owns one DatabaseImpl
    /// wrapping margin.db; null if open() failed at bootstrap. Plugins
    /// null-check host->database() before use.
    void setDatabase(Database* db);

    /// Inject the host's WindowMonitorService (M2-C1). Pulled from
    /// PlatformBackend::windowMonitor(); null on platforms without a
    /// backend. Plugins connect its activeWindowChanged signal.
    void setWindowMonitorService(WindowMonitorService* wm);

    /// Inject the host's InputMonitorService (M2-C4). Pulled from
    /// PlatformBackend::inputMonitor(); null on platforms without a
    /// backend. Plugins connect its userIdleStateChanged signal.
    void setInputMonitorService(InputMonitorService* im);

    /// Set the active locale for all loaded plugins (PR2 i18n autonomy).
    /// Removes the old translator and loads + installs
    ///   :/<plugin_id>/i18n/<plugin_id>_<lang>.qm
    /// for each plugin whose catalog exists. Plugins without a bundled
    /// catalog silently fall back to source strings. Called by
    /// HostCore::applyLanguage after the host translator is installed
    /// and before m_engine->retranslate().
    void setLanguage(const QString& localeCode);

    /// A plugin discovered on disk, before load. Public so the load-order
    /// policy (sortByLoadOrder) is unit-testable without the filesystem.
    struct DiscoveredPlugin {
        QString      id;
        QString      path;                 // absolute path to the DLL
        int          priority = 100;       // manifest "priority"; lower loads first
        QStringList  encryptedSettings;    // manifest `encrypted_settings` array
    };

    /// Stable load order per docs/04-plugin-spec.md §加载顺序:
    /// priority ascending, then id lexicographically ascending.
    static void sortByLoadOrder(std::vector<DiscoveredPlugin>& plugins);

private:
    struct LoadedPlugin;

    void discoverInDir(const QString& dirPath,
                       QHash<QString, DiscoveredPlugin>* byId) const;
    bool loadOne(const QString& dllPath);
    void onTrayItemClicked(const QString& pluginId, const QString& itemId);

    // PR2 i18n: per-plugin QTranslator lifecycle. loadPluginTranslator
    // resolves :/<id>/i18n/<id>_<lang>.qm; missing catalog is not an error.
    // removePluginTranslator uninstalls + drops the entry; safe to call
    // when no translator exists for the id.
    void loadPluginTranslator(const QString& pluginId, const QString& localeCode);
    void removePluginTranslator(const QString& pluginId);

    Logger&            m_logger;
    EventBus&          m_eventBus;
    Settings&          m_settings;
    TrayService&       m_tray;
    CryptoServicePool& m_cryptoPool;
    SystemTray*        m_trayIntegration = nullptr;
    LockService*       m_platformBackend = nullptr;
    QmlService*        m_qml             = nullptr;
    Database*          m_database        = nullptr;
    WindowMonitorService* m_windowMonitor = nullptr;
    InputMonitorService*  m_inputMonitor  = nullptr;
    std::vector<std::unique_ptr<LoadedPlugin>> m_loaded;

    // PR2: pluginId → installed QTranslator. unique_ptr removes + destroys
    // the translator in one step on unload / language switch. QHash doesn't
    // accept non-copyable value types, hence std::unordered_map keyed on
    // std::string (pluginId type in LoadedPlugin is std::string anyway).
    std::unordered_map<std::string, std::unique_ptr<QTranslator>> m_pluginTranslators;
    // Defaults to "auto" until the first setLanguage call. Resolved locale
    // is what gets passed back in for any plugin loaded later.
    QString m_currentLanguage = QStringLiteral("auto");
};

} // namespace Margin
