// PluginContext - onLoad injection handle. Spec: docs/04-plugin-spec.md §2.
//
// Host constructs one PluginContext per loaded plugin. id/version come from
// manifest; host points to the HostServices facade (from M0-C5b, host holds
// all service references).
//
// subscriberIdentity (added M0-C5): per-plugin QObject* created and owned
// by PluginManager. Plugins pass it as the subscriber argument when calling
// EventBus::subscribe. On unload, PluginManager calls
// eventBus.unsubscribeAll(subscriberIdentity) to bulk-clean this plugin's
// subscriptions. See docs/05-host-services.md §3 and docs/04 §2.

#pragma once

#include <QObject>

#include <string>
#include <vector>

namespace Margin {

class HostServices;  // forward decl, see Margin/HostServices.h

struct PluginContext {
    std::string id;                               // = manifest.id
    std::string version;                          // = manifest.version
    std::vector<std::string> grantedPermissions;  // user-granted perms (see §6)
    HostServices* host = nullptr;                 // service bus (Host owns lifetime)
    QObject* subscriberIdentity = nullptr;        // EventBus subscription identity (M0-C5)
};

} // namespace Margin
