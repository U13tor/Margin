// QmlService - per-plugin bridge for exposing QObject / QML types to the
// plugin's own QML files. Spec: docs/05-host-services.md §7.
//
// INTERFACE ONLY. The implementation is deferred — HostServices::qml() still
// returns nullptr until the first consumer (host-side or plugin-side) actually
// needs it. Tracked in docs/12-deferred-items.md §A.
//
// Re-evaluation trigger: any plugin manifest declares a `qml_contributions`
// field, or Host grows a QML-facing controller that needs strong-typed (i.e.
// non-EventBus) QML ↔ C++ bridging. Until then, QML ↔ plugin commands flow
// exclusively through EventBus (docs/04-plugin-spec.md §5), which is
// sufficient for M0.

#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>

namespace Margin {

class QmlService {
public:
    virtual ~QmlService() = default;

    /// Register a QObject as a context property of THIS plugin's QML root
    /// context. Host isolates each plugin's context (child of root), so
    /// plugin A's QML cannot see plugin B's objects. Lifetime: Host calls
    /// deleteLater() on the object during this plugin's onUnload.
    virtual void registerContextProperty(const QString& name, QObject* object) = 0;

    /// Register a QML type (equivalent to qmlRegisterType<T>). Must be called
    /// before any plugin QML is loaded — i.e. inside onLoad.
    virtual void registerType(const char* uri, int versionMajor,
                              int versionMinor, const char* qmlName) = 0;

    /// Borrow this plugin's QQmlEngine (e.g. for installing a custom
    /// QQuickImageProvider). Pointer ownership stays with Host; do not delete.
    virtual QQmlEngine* engine() = 0;
};

} // namespace Margin
