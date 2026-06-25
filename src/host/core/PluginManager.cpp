// PluginManager impl — see host/core/PluginManager.h.

#include "PluginManager.h"

#include "Margin/EventBus.h"
#include "Margin/HostServices.h"
#include "Margin/Logger.h"
#include "Margin/PluginContext.h"
#include "Margin/Result.h"
#include "Margin/Settings.h"
#include "Margin/TrayMenuContributor.h"
#include "Margin/TrayService.h"
#include "SystemTray.h"
#include "host/security/CryptoServicePool.h"
#include "host/services/HostServicesImpl.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLibrary>
#include <QObject>
#include <QSet>
#include <QString>
#include <QTranslator>
#include <QVersionNumber>

#include <exception>
#include <algorithm>
#include <utility>
#include <vector>

namespace Margin {

struct PluginManager::LoadedPlugin {
    std::string                            id;
    std::unique_ptr<HostServicesImpl>      hostWrapper;   // per-plugin facade
    std::unique_ptr<QLibrary>              library;
    PluginInterface*                       instance = nullptr;
    std::unique_ptr<QObject>               subscriberIdentity;
};

PluginManager::PluginManager(Logger& logger,
                             EventBus& eventBus,
                             Settings& settings,
                             TrayService& tray,
                             CryptoServicePool& cryptoPool)
    : m_logger(logger)
    , m_eventBus(eventBus)
    , m_settings(settings)
    , m_tray(tray)
    , m_cryptoPool(cryptoPool) {}

PluginManager::~PluginManager() {
    unloadAll();
}

void PluginManager::discoverInDir(const QString& dirPath,
                                  QHash<QString, DiscoveredPlugin>* byId) const {
    QDir dir(dirPath);
    if (!dir.exists()) return;

    QStringList filters;
#if defined(Q_OS_WIN)
    filters << QStringLiteral("*.dll");
#elif defined(Q_OS_MAC)
    filters << QStringLiteral("*.dylib");
#else
    filters << QStringLiteral("*.so");
#endif

    for (const QFileInfo& info : dir.entryInfoList(filters, QDir::Files)) {
        const QString baseName = info.completeBaseName();
        const QString manifestPath =
            dir.filePath(baseName + QStringLiteral(".manifest.json"));
        if (!QFileInfo::exists(manifestPath)) {
            m_logger.warn(QStringLiteral("plugin"),
                          QStringLiteral("Skipping %1: missing manifest %2")
                              .arg(info.fileName(), manifestPath));
            continue;
        }

        QFile f(manifestPath);
        if (!f.open(QIODevice::ReadOnly)) {
            m_logger.warn(QStringLiteral("plugin"),
                          QStringLiteral("Skipping %1: cannot open manifest")
                              .arg(info.fileName()));
            continue;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        if (!doc.isObject()) {
            m_logger.warn(QStringLiteral("plugin"),
                          QStringLiteral("Skipping %1: manifest not a JSON object")
                              .arg(info.fileName()));
            continue;
        }
        const QJsonObject obj = doc.object();
        const QString id = obj.value(QStringLiteral("id")).toString();
        if (id.isEmpty()) {
            m_logger.warn(QStringLiteral("plugin"),
                          QStringLiteral("Skipping %1: manifest missing id")
                              .arg(info.fileName()));
            continue;
        }
        const int priority = obj.value(QStringLiteral("priority")).toInt(100);

        // Pull encrypted_settings (§4.4 step 2) so PluginManager can pre-register
        // the set with Settings before any plugin onLoad — even a failed onLoad
        // leaves the declaration counted, which matches the audit guarantee.
        DiscoveredPlugin dp{ id, info.absoluteFilePath(), priority, {} };
        const QJsonValue encVal = obj.value(QStringLiteral("encrypted_settings"));
        if (encVal.isArray()) {
            const QJsonArray arr = encVal.toArray();
            for (const QJsonValue& v : arr) {
                if (v.isString()) dp.encryptedSettings.append(v.toString());
            }
        }

        // Later dirs override earlier (user-level wins per spec §3.3).
        byId->insert(id, std::move(dp));
    }
}

void PluginManager::loadAll(const QStringList& scanDirs) {
    QHash<QString, DiscoveredPlugin> byId;
    for (const QString& dir : scanDirs) {
        discoverInDir(dir, &byId);
    }
    std::vector<DiscoveredPlugin> ordered;
    ordered.reserve(static_cast<size_t>(byId.size()));
    for (auto it = byId.cbegin(); it != byId.cend(); ++it) {
        ordered.push_back(it.value());
    }
    sortByLoadOrder(ordered);

    // Pre-register encrypted keys with Settings BEFORE any onLoad. Per
    // docs/05-host-services.md §4.4 step 2 this runs at discovery time
    // so a later plugin's pairDevice(...) call lands as ciphertext.
    QSet<QString> encryptedKeys;
    for (const DiscoveredPlugin& dp : ordered) {
        for (const QString& k : dp.encryptedSettings) encryptedKeys.insert(k);
    }
    if (!encryptedKeys.isEmpty()) {
        m_settings.registerEncryptedKeys(encryptedKeys);
        m_logger.info(QStringLiteral("plugin"),
                      QStringLiteral("registered %1 encrypted settings key(s)")
                          .arg(encryptedKeys.size()));
    }

    for (const DiscoveredPlugin& dp : ordered) {
        loadOne(dp.path);
    }
    m_logger.info(QStringLiteral("plugin"),
                  QStringLiteral("loaded %1 plugin(s)").arg(m_loaded.size()));
}

void PluginManager::sortByLoadOrder(std::vector<DiscoveredPlugin>& plugins) {
    std::sort(plugins.begin(), plugins.end(),
              [](const DiscoveredPlugin& a, const DiscoveredPlugin& b) {
                  if (a.priority != b.priority) return a.priority < b.priority;
                  return a.id < b.id;
              });
}

bool PluginManager::loadOne(const QString& dllPath) {
    auto lib = std::make_unique<QLibrary>(dllPath);
    if (!lib->load()) {
        m_logger.warn(QStringLiteral("plugin"),
                      QStringLiteral("Failed to load %1: %2")
                          .arg(dllPath, lib->errorString()));
        return false;  // ~QLibrary no-op since load failed
    }

    using EntryFn = PluginInterface* (*)();
    auto entry = reinterpret_cast<EntryFn>(lib->resolve("margin_plugin_entry"));
    if (!entry) {
        m_logger.warn(QStringLiteral("plugin"),
                      QStringLiteral("%1: missing margin_plugin_entry symbol")
                          .arg(dllPath));
        return false;
    }
    PluginInterface* instance = entry();
    if (!instance) {
        m_logger.warn(QStringLiteral("plugin"),
                      QStringLiteral("%1: entry returned null").arg(dllPath));
        return false;
    }

    // Re-read manifest for ABI / version validation.
    const QFileInfo info(dllPath);
    const QString manifestPath =
        info.dir().filePath(info.completeBaseName() + QStringLiteral(".manifest.json"));
    QFile mf(manifestPath);
    if (!mf.open(QIODevice::ReadOnly)) {
        m_logger.warn(QStringLiteral("plugin"),
                      QStringLiteral("%1: cannot reopen manifest").arg(dllPath));
        return false;
    }
    const QJsonObject manifest = QJsonDocument::fromJson(mf.readAll()).object();

    // 1. abi_version strict-equal.
    const QString abiVersion = manifest.value(QStringLiteral("abi_version")).toString();
    if (abiVersion != QLatin1String(MARGIN_ABI_VERSION)) {
        m_logger.warn(QStringLiteral("plugin"),
                      QStringLiteral("%1: abi_version mismatch (manifest=%2 host=%3), rejecting")
                          .arg(QString::fromStdString(instance->id()),
                               abiVersion, QLatin1String(MARGIN_ABI_VERSION)));
        return false;
    }

    // 2. min_host_version <= host version.
    const QString minHost = manifest.value(QStringLiteral("min_host_version")).toString();
    const auto hostVer = QVersionNumber::fromString(QLatin1String(MARGIN_VERSION));
    const auto pluginMin = QVersionNumber::fromString(minHost);
    if (!pluginMin.isNull() && QVersionNumber::compare(hostVer, pluginMin) < 0) {
        m_logger.warn(QStringLiteral("plugin"),
                      QStringLiteral("%1: requires host >= %2 (have %3), rejecting")
                          .arg(QString::fromStdString(instance->id()),
                               minHost, QLatin1String(MARGIN_VERSION)));
        return false;
    }

    // 3. manifest id matches instance id().
    const QString manifestId = manifest.value(QStringLiteral("id")).toString();
    if (manifestId.toStdString() != instance->id()) {
        m_logger.warn(QStringLiteral("plugin"),
                      QStringLiteral("%1: manifest id '%2' != instance id() '%3'")
                          .arg(dllPath, manifestId,
                               QString::fromStdString(instance->id())));
        return false;
    }

    // 4. manifest version matches instance version().
    const QString manifestVer = manifest.value(QStringLiteral("version")).toString();
    if (manifestVer.toStdString() != instance->version()) {
        m_logger.warn(QStringLiteral("plugin"),
                      QStringLiteral("%1: manifest version '%2' != instance version() '%3'")
                          .arg(dllPath, manifestVer,
                               QString::fromStdString(instance->version())));
        return false;
    }

    // Build LoadedPlugin + per-plugin HostServices wrapper + context before
    // onLoad. Each plugin gets its own HostServicesImpl sharing the 4 live
    // services but holding a per-plugin CryptoService& (HKDF-derived for
    // this plugin id; see docs/05-host-services.md §8/§9).
    auto loaded = std::make_unique<LoadedPlugin>();
    loaded->id = instance->id();
    loaded->subscriberIdentity = std::make_unique<QObject>();

    CryptoServiceImpl& perPluginCrypto =
        m_cryptoPool.getOrCreate(QString::fromStdString(loaded->id));
    loaded->hostWrapper = std::make_unique<HostServicesImpl>(
        m_logger, m_eventBus, m_settings, m_tray, perPluginCrypto);
    loaded->hostWrapper->setLockService(m_platformBackend);
    loaded->hostWrapper->setQmlService(m_qml);
    loaded->hostWrapper->setDatabase(m_database);
    loaded->hostWrapper->setWindowMonitorService(m_windowMonitor);
    loaded->hostWrapper->setInputMonitorService(m_inputMonitor);

    PluginContext ctx;
    ctx.id = instance->id();
    ctx.version = instance->version();
    ctx.grantedPermissions = {};     // deny-by-default; auth popup is M1
    ctx.host = loaded->hostWrapper.get();
    ctx.subscriberIdentity = loaded->subscriberIdentity.get();

    const std::string id = loaded->id;  // copy before unique_ptr transfer

    // onLoad. 5s timeout enforcement deferred — see docs/12-deferred-items.md A12.
    try {
        const Result<void, std::string> result = instance->onLoad(ctx);
        if (!result.isOk()) {
            m_logger.warn(QStringLiteral("plugin"),
                          QStringLiteral("%1: onLoad failed: %2")
                              .arg(QString::fromStdString(id),
                                   QString::fromStdString(result.error())));
            return false;
        }
    } catch (const std::exception& e) {
        m_logger.error(QStringLiteral("plugin"),
                       QStringLiteral("%1: onLoad threw: %2")
                           .arg(QString::fromStdString(id),
                                QString::fromUtf8(e.what())));
        return false;
    }

    // Commit: transfer lib ownership last (so ~QLibrary above keeps the DLL
    // alive if any earlier step returned false).
    loaded->instance = instance;
    loaded->library = std::move(lib);

    // Forward tray contributions to SystemTray if wired.
    if (m_trayIntegration) {
        if (auto* tc = loaded->instance->asTrayMenu()) {
            m_trayIntegration->addPluginItems(QString::fromStdString(id),
                                              tc->contributeTrayItems());
        }
    }

    m_logger.info(QStringLiteral("plugin"),
                  QStringLiteral("loaded %1 v%2")
                      .arg(QString::fromStdString(id),
                           QString::fromStdString(instance->version())));
    m_loaded.push_back(std::move(loaded));

    // PR2 i18n: install translator for the currently active language. If
    // m_currentLanguage is "auto", translator is skipped — HostCore's
    // first applyLanguage() call will resolve "auto" → "en"/"zh_CN" and
    // call setLanguage(), which retroactively installs translators for
    // already-loaded plugins.
    if (m_currentLanguage != QLatin1String("auto")) {
        loadPluginTranslator(QString::fromStdString(id), m_currentLanguage);
    }
    return true;
}

void PluginManager::unloadAll() {
    // Reverse-load order: onUnload -> unsubscribeAll -> QLibrary::unload.
    while (!m_loaded.empty()) {
        auto loaded = std::move(m_loaded.back());
        m_loaded.pop_back();

        // Drop tray items before the plugin goes away so the menu doesn't
        // hold a click handler that routes to a dying plugin.
        if (m_trayIntegration) {
            m_trayIntegration->removePluginItems(QString::fromStdString(loaded->id));
        }

        // PR2 i18n: uninstall translator before the DLL goes away so QML
        // doesn't query a translator whose catalog is being unmapped.
        removePluginTranslator(QString::fromStdString(loaded->id));

        if (loaded->instance) {
            m_logger.info(QStringLiteral("plugin"),
                          QStringLiteral("unloading %1")
                              .arg(QString::fromStdString(loaded->id)));
            try {
                loaded->instance->onUnload();
            } catch (const std::exception& e) {
                m_logger.error(QStringLiteral("plugin"),
                               QStringLiteral("%1: onUnload threw: %2")
                                   .arg(QString::fromStdString(loaded->id),
                                        QString::fromUtf8(e.what())));
            }
            m_logger.info(QStringLiteral("plugin"),
                          QStringLiteral("unloaded %1")
                              .arg(QString::fromStdString(loaded->id)));
        }

        // Defensive cleanup of any subscriptions the plugin forgot.
        m_eventBus.unsubscribeAll(loaded->subscriberIdentity.get());

        // ~LoadedPlugin runs here. Member reverse-declaration order:
        //   subscriberIdentity → instance(raw) → library(unload DLL) →
        //   hostWrapper → id.
        // hostWrapper is destroyed AFTER library unloads the DLL, which is
        // safe because the plugin's onUnload already returned by this point
        // and PluginManager is the only owner of the wrapper.
    }
}

void PluginManager::setTrayIntegration(SystemTray* tray) {
    if (m_trayIntegration == tray) return;
    m_trayIntegration = tray;
    if (m_trayIntegration) {
        connect(m_trayIntegration, &SystemTray::pluginItemClicked,
                this, &PluginManager::onTrayItemClicked);
        // M4-C16: inject a contributor lookup so SystemTray can re-pull
        // contributeTrayItems() when a plugin asks for a menu refresh
        // (dynamic toggle labels + read-only previews). The lookup captures
        // `this` — PluginManager outlives SystemTray (HostCore teardown order).
        m_trayIntegration->setContributorLookup(
            [this](const QString& pluginId) -> TrayMenuContributor* {
                auto* iface = plugin(pluginId.toStdString());
                return iface ? iface->asTrayMenu() : nullptr;
            });
    }
}

void PluginManager::setPlatformBackend(LockService* backend) {
    m_platformBackend = backend;
    // Already-loaded wrappers get retrofitted; subsequent loadOne() picks
    // m_platformBackend up at HostServicesImpl construction. In practice
    // HostCore calls this before loadAll, so the loop is empty.
    for (auto& loaded : m_loaded) {
        if (loaded->hostWrapper) {
            loaded->hostWrapper->setLockService(m_platformBackend);
        }
    }
}

void PluginManager::setQmlService(QmlService* qml) {
    m_qml = qml;
    for (auto& loaded : m_loaded) {
        if (loaded->hostWrapper) {
            loaded->hostWrapper->setQmlService(m_qml);
        }
    }
}

void PluginManager::setDatabase(Database* db) {
    m_database = db;
    for (auto& loaded : m_loaded) {
        if (loaded->hostWrapper) {
            loaded->hostWrapper->setDatabase(m_database);
        }
    }
}

void PluginManager::setWindowMonitorService(WindowMonitorService* wm) {
    m_windowMonitor = wm;
    for (auto& loaded : m_loaded) {
        if (loaded->hostWrapper) {
            loaded->hostWrapper->setWindowMonitorService(m_windowMonitor);
        }
    }
}

void PluginManager::setInputMonitorService(InputMonitorService* im) {
    m_inputMonitor = im;
    for (auto& loaded : m_loaded) {
        if (loaded->hostWrapper) {
            loaded->hostWrapper->setInputMonitorService(m_inputMonitor);
        }
    }
}

void PluginManager::onTrayItemClicked(const QString& pluginId,
                                       const QString& itemId) {
    PluginInterface* p = plugin(pluginId.toStdString());
    if (!p) return;
    if (auto* tc = p->asTrayMenu()) {
        tc->onTrayItemClicked(itemId.toStdString());
    }
}

PluginInterface* PluginManager::plugin(const std::string& id) const {
    for (const auto& loaded : m_loaded) {
        if (loaded->id == id) return loaded->instance;
    }
    return nullptr;
}

void PluginManager::forEachPlugin(
        std::function<void(PluginInterface*)> fn) const {
    for (const auto& loaded : m_loaded) {
        if (loaded->instance) fn(loaded->instance);
    }
}

void PluginManager::setLanguage(const QString& localeCode) {
    // PR2 i18n: reload every plugin translator under the new locale. Host
    // has already installed its own host_<lang>.qm translator before
    // calling this — plugin translators stack on top so plugin qsTr()
    // lookups resolve before falling back to the host catalog.
    m_currentLanguage = localeCode;
    for (const auto& loaded : m_loaded) {
        const QString id = QString::fromStdString(loaded->id);
        removePluginTranslator(id);
        loadPluginTranslator(id, localeCode);
    }
}

void PluginManager::loadPluginTranslator(const QString& pluginId,
                                          const QString& localeCode) {
    // Catalog path matches cmake/plugin_i18n.cmake qrc prefix:
    //   :/<plugin_id>/i18n/<plugin_id>_<lang>.qm
    const QString qmPath = QStringLiteral(":%1/i18n/%2_%3.qm")
                               .arg(pluginId, pluginId, localeCode);
    auto t = std::make_unique<QTranslator>();
    if (!t->load(qmPath)) {
        // Missing catalog is not an error — plugin may ship only one
        // language, or be third-party without translations. Log at info
        // so it shows up next to plugin load without flooding the log.
        m_logger.info(QStringLiteral("plugin"),
                      QStringLiteral("%1: no i18n catalog at %2 (falling back to source strings)")
                          .arg(pluginId, qmPath));
        return;
    }
    qApp->installTranslator(t.get());
    m_pluginTranslators[pluginId.toStdString()] = std::move(t);
}

void PluginManager::removePluginTranslator(const QString& pluginId) {
    auto it = m_pluginTranslators.find(pluginId.toStdString());
    if (it == m_pluginTranslators.end()) return;
    // ~QTranslator on unique_ptr reset removes it from qApp. Be explicit
    // anyway so a future QTranslator subclass doesn't rely on dtor order.
    qApp->removeTranslator(it->second.get());
    m_pluginTranslators.erase(it);
}

} // namespace Margin
