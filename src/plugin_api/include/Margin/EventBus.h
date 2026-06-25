// EventBus service interface — pub/sub bus for Host <-> Plugin events.
// Spec: docs/05-host-services.md §3.

#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>

#include <functional>
#include <memory>

namespace Margin {

class EventBus {
public:
    virtual ~EventBus() = default;

    /// Subscribe to a topic. MQTT-style wildcards:
    ///   '*' = single-level  (matches exactly one non-empty segment)
    ///   '#' = multi-level   (zero or more trailing segments; must be last)
    /// Topic filter must start with 'margin.'.
    ///
    /// If `subscriber` is non-null, the subscription is associated with that
    /// QObject and can be bulk-removed via unsubscribeAll(subscriber). This
    /// is how PluginManager (M0-C5) cleans up a plugin's subscriptions on
    /// unload. If null, the handler lives until the EventBus is destroyed.
    virtual void subscribe(const QString& topic,
                           std::function<void(const QJsonObject&)> handler,
                           QObject* subscriber = nullptr) = 0;

    /// Publish an event. Thread-safe; callable from any thread. Handlers
    /// are dispatched to the main thread via Qt::QueuedConnection. Payload
    /// is QJsonObject (implicitly shared -> cheap cross-thread copy).
    virtual void publish(const QString& topic, const QJsonObject& payload) = 0;

    /// Remove all subscriptions associated with `subscriber`. No-op if null
    /// or no matching subscriptions. PluginManager calls this on plugin
    /// unload; HostCore calls it during shutdown.
    virtual void unsubscribeAll(QObject* subscriber) = 0;

    /// Factory: construct the default EventBus implementation. Returns
    /// unique_ptr<EventBus> — destructor is visible in M0 static linking,
    /// so this is safe across host/plugin boundary today. (M0-C5 revisit
    /// when plugin DLLs land; see docs/12-deferred-items.md §A3.)
    static std::unique_ptr<EventBus> wire();
};

} // namespace Margin
