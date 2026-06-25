// HostCore — singleton orchestrator. Owns Logger + Settings + Crypto pool,
// sequences their startup/shutdown. main.cpp owns QApplication; HostCore
// owns services. Spec: docs/05 §9.

#pragma once

#include "Margin/EventBus.h"
#include "host/core/ActivityFeed.h"
#include "host/core/DashboardTabRegistry.h"
#include "host/core/HostGeneralSettings.h"
#include "host/core/OverlayRegistry.h"
#include "host/core/PluginManager.h"
#include "host/core/SettingsRegistry.h"
#include "host/core/SystemTray.h"
#include "host/platform/PlatformBackend.h"
#include "host/security/CryptoServicePool.h"
#include "host/services/DatabaseImpl.h"
#include "host/services/Logger.h"
#include "host/services/Settings.h"

#include <memory>

class QQmlApplicationEngine;
class QTimer;
class QTranslator;
class QWindow;

namespace Margin {

class QmlServiceImpl;

class HostCore {
public:
    static HostCore& instance();

    bool bootstrap();
    void shutdown();

    Logger&         logger()   { return *m_logger; }
    Settings&       settings() { return *m_settings; }
    EventBus&       eventBus() { return *m_eventBus; }
    PluginManager&  plugins()  { return *m_plugins; }

    /// Show the Settings window (Layer 2). Idempotent — if already visible,
    /// just raises + activates. Wired to SystemTray::openSettingsRequested
    /// (M5-C3) and the QML Ctrl+, shortcut via the same tray-signal pattern.
    void openSettings();

private:
    HostCore() = default;
    // Defined in the .cpp: m_engine holds a forward-declared QQmlApplicationEngine.
    ~HostCore();
    Q_DISABLE_COPY(HostCore)

    void applyLogLevel();
    // PR6 round-2 #7: install/swap the QTranslator for the current
    // general.language value ("auto" → QLocale::system().name(); "zh_CN"
    // / "en" → that catalog). Reload is what makes Settings → Language
    // flip the whole UI live without restart. The QML engine is
    // retranslated after the swap so bindings re-evaluate qsTr() calls.
    void applyLanguage();

    // PR3 i18n: C++ hard-coded tab/page titles use QCoreApplication::translate
    // so they flip with the installed catalog. On language change, the host
    // calls reemitRegistries() to clear+re-add every entry — the fresh
    // translate() calls pick up the new catalog and the registry's
    // tabsChanged/pagesChanged signal fires, making the QML ListView rebuild
    // with the new titles. Same idea as QQmlApplicationEngine::retranslate()
    // but for the C++ side that the QML engine cannot reach.
    void registerHostTabs();
    void registerHostPages();
    void registerPluginTabsAndPages();
    void reemitRegistries();

    std::unique_ptr<LoggerImpl>           m_logger;
    std::unique_ptr<SettingsImpl>         m_settings;
    std::unique_ptr<EventBus>             m_eventBus;
    std::unique_ptr<SystemTray>           m_tray;
    std::unique_ptr<PlatformBackend>      m_platformBackend;
    std::unique_ptr<DashboardTabRegistry> m_tabRegistry;
    std::unique_ptr<ActivityFeed>         m_activityFeed;
    std::unique_ptr<OverlayRegistry>      m_overlayRegistry;
    std::unique_ptr<SettingsRegistry>     m_settingsRegistry;
    std::unique_ptr<HostGeneralSettings>  m_generalSettings;  // M5-C4d: QML bridge
    std::unique_ptr<QTimer>               m_overlayPollTimer;
    QWindow*                              m_overlayWindow = nullptr;  // raw: engine owns the QObject
    QWindow*                              m_settingsWindow = nullptr; // raw: engine owns the QObject
    std::unique_ptr<QQmlApplicationEngine> m_engine;
    std::unique_ptr<QmlServiceImpl>        m_qmlService;
    std::unique_ptr<QTranslator>            m_translator;  // PR6: i18n catalog
    std::unique_ptr<CryptoServicePool>    m_cryptoPool;
    std::unique_ptr<DatabaseImpl>         m_database;
    std::unique_ptr<PluginManager>        m_plugins;

    bool m_shutdownDone = false;
};

} // namespace Margin
