// QmlServiceImpl — minimal concrete QmlService.
// Spec: docs/05-host-services.md §7.
//
// Borrows the host's QQmlApplicationEngine (owned by HostCore) and exposes
// its root context for plugin context-property registration. registerType
// is intentionally left as a stub — it's a deferred concern (no plugin
// currently needs to register custom QML types); when the first consumer
// arrives, fill it in (likely qmlRegisterType via a static map).

#pragma once

#include "Margin/QmlService.h"

#include <QQmlEngine>
#include <QQmlContext>
#include <QString>

namespace Margin {

class QmlServiceImpl : public QmlService {
public:
    explicit QmlServiceImpl(QQmlEngine* engine) : m_engine(engine) {}

    void registerContextProperty(const QString& name, QObject* object) override {
        if (!m_engine) return;
        // Plugins share the root context (no per-plugin isolation yet —
        // tracked in docs/12-deferred-items.md §A3b). setContextProperty
        // on the root context makes `name` visible to every plugin's QML.
        // Lifetime: object's destroyed() signal auto-removes the entry.
        m_engine->rootContext()->setContextProperty(name, object);
    }

    void registerType(const char* /*uri*/, int /*versionMajor*/,
                      int /*versionMinor*/, const char* /*qmlName*/) override {
        // Deferred — see file header. Will qmlRegisterType<T> when the
        // first consumer lands.
    }

    QQmlEngine* engine() override { return m_engine; }

private:
    QQmlEngine* m_engine;  // borrowed; HostCore owns lifetime
};

} // namespace Margin
