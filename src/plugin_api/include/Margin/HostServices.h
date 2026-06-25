// HostServices - service-bus facade injected into each plugin via PluginContext.
// Spec: docs/05-host-services.md §1.2. Impl: host/services/HostServicesImpl.cpp (M0-C5b).
//
// Five services are live (logger/eventBus/settings/tray, plus crypto since
// M0-C8, lock since M1-C4 on Win). The remaining four
// (qml/database/windowMonitor/inputMonitor) return nullptr until their owning
// Milestone (C10/M2-C2/M2-C1/M2-C4) lands, so plugin authors must null-check
// those accessors before using them. lock() is non-null on Win, nullptr on
// macOS until PlatformBackendMac lands.

#pragma once

#include <Margin/EventBus.h>
#include <Margin/Logger.h>
#include <Margin/Settings.h>
#include <Margin/TrayService.h>

namespace Margin {

class CryptoService;          // M0-C8
class QmlService;             // M0-C10
class Database;               // M2-C2
class LockService;            // M1-C4
class WindowMonitorService;   // M2-C1
class InputMonitorService;    // M2-C4

class HostServices {
public:
    virtual ~HostServices() = default;

    // Live services (return reference, Host guarantees lifetime)
    virtual Logger&       logger()   = 0;
    virtual EventBus&     eventBus() = 0;
    virtual Settings&     settings() = 0;
    virtual TrayService&  tray()     = 0;

    // Service pointers (nullable; check before use):
    //   crypto()        — non-null since M0-C8
    //   lock()          — non-null on Win since M1-C4; nullptr on macOS until
    //                     PlatformBackendMac ships alongside §A15
    //   qml()           — M0-C10 (nullptr, impl deferred per 12 §A3a)
    //   database()      — M2-C2 (non-null on Win when open() succeeded;
    //                     nullptr during M0-M1)
    //   windowMonitor() — M2-C1 (non-null on Win; nullptr on macOS per §A19)
    //   inputMonitor()  — M2-C4 (same nullability story as windowMonitor())
    virtual CryptoService*        crypto()        = 0;
    virtual LockService*          lock()          = 0;
    virtual QmlService*           qml()           = 0;
    virtual Database*             database()      = 0;
    virtual WindowMonitorService* windowMonitor() = 0;
    virtual InputMonitorService*  inputMonitor()  = 0;
};

} // namespace Margin
