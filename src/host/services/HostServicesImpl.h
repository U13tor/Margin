// HostServicesImpl — concrete HostServices facade. Per-plugin instances are
// constructed by PluginManager (one wrapper per loaded plugin) so that
// crypto() can return a per-plugin CryptoService& derived via HKDF. The
// four live services (logger/eventBus/settings/tray) are shared references
// to HostCore-owned singletons.
//
// Spec: docs/05-host-services.md §1.2 + §9 ("host=带独立 Crypto 的服务包装器").

#pragma once

#include "Margin/HostServices.h"
#include "Margin/LockService.h"

namespace Margin {

class HostServicesImpl : public HostServices {
public:
    HostServicesImpl(Logger& logger,
                     EventBus& eventBus,
                     Settings& settings,
                     TrayService& tray,
                     CryptoService& crypto);

    Logger&       logger()   override { return m_logger; }
    EventBus&     eventBus() override { return m_eventBus; }
    Settings&     settings() override { return m_settings; }
    TrayService&  tray()     override { return m_tray; }

    CryptoService* crypto()        override { return &m_crypto; }
    QmlService*    qml()           override { return m_qml; }
    Database*      database()      override { return m_database; }
    WindowMonitorService* windowMonitor() override { return m_windowMonitor; }
    InputMonitorService*  inputMonitor()  override { return m_inputMonitor; }

    // LockService is platform-dependent. PluginManager injects the host's
    // PlatformBackend (as LockService*) via setLockService after HostCore
    // bootstrap; nullptr on platforms without a backend (macOS until the
    // CoreBluetooth deferral §A15-equivalent lifts).
    LockService*   lock()     override { return m_lock; }
    void setLockService(LockService* lock) { m_lock = lock; }

    // QmlService is shared across all plugin wrappers — HostCore owns one
    // QmlServiceImpl wrapping the dashboard QQmlApplicationEngine. Injected
    // after the engine is created but before PluginManager loads plugins.
    void setQmlService(QmlService* qml) { m_qml = qml; }

    // Database (M2-C2) is shared across all plugin wrappers — HostCore owns
    // one DatabaseImpl wrapping margin.db. Injected after open() succeeds
    // but before PluginManager loads plugins. Stays nullptr if open() failed
    // at bootstrap (disk full / permissions) — plugins MUST null-check.
    void setDatabase(Database* db) { m_database = db; }

    // WindowMonitorService (M2-C1) is shared across all plugin wrappers.
    // HostCore owns one ActiveWindowTrackerWin via PlatformBackend.
    // nullptr on macOS until §A19 lifts.
    void setWindowMonitorService(WindowMonitorService* wm) { m_windowMonitor = wm; }

    // InputMonitorService (M2-C4) is shared across all plugin wrappers.
    // HostCore owns one IdleDetectorWin via PlatformBackend. Same macOS
    // deferral story as windowMonitor().
    void setInputMonitorService(InputMonitorService* im) { m_inputMonitor = im; }

private:
    Logger&         m_logger;
    EventBus&       m_eventBus;
    Settings&       m_settings;
    TrayService&    m_tray;
    CryptoService&  m_crypto;
    LockService*    m_lock = nullptr;
    QmlService*     m_qml  = nullptr;
    Database*       m_database = nullptr;
    WindowMonitorService* m_windowMonitor = nullptr;
    InputMonitorService*  m_inputMonitor  = nullptr;
};

} // namespace Margin
