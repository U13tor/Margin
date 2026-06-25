// EventBus impl — see Margin/EventBus.h for spec.
//
// Threading: publish() locks m_mutex only long enough to snapshot matched
// subscriptions into a local vector, then releases before dispatching.
// Mirrors Settings::set (src/host/services/Settings.cpp) — handlers fire
// outside the mutex so they can re-enter subscribe/publish/unsubscribeAll
// without deadlock.
//
// EventBusImpl does NOT inherit QObject (would need AUTOMOC to scan this
// .cpp). Dispatch goes through qApp, which is already a QObject, via
// QMetaObject::invokeMethod + Qt::QueuedConnection.

#include "Margin/EventBus.h"

#include <QCoreApplication>
#include <QHash>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QString>
#include <QStringList>
#include <QVector>

#include <algorithm>

namespace Margin {
namespace {

struct Subscription {
    QObject* subscriber;
    std::function<void(const QJsonObject&)> handler;
};

// MQTT-style match: '*' = single non-empty segment, '#' = zero or more
// trailing segments (only valid as the last filter segment).
bool topicMatches(const QString& filter, const QString& topic) {
    const QStringList f = filter.split(QLatin1Char('.'));
    const QStringList t = topic.split(QLatin1Char('.'));
    int i = 0;
    while (i < f.size()) {
        if (f[i] == QLatin1String("#")) {
            return i == f.size() - 1;
        }
        if (i >= t.size()) return false;
        if (f[i] == QLatin1String("*")) {
            if (t[i].isEmpty()) return false;
        } else if (f[i] != t[i]) {
            return false;
        }
        ++i;
    }
    return i == t.size();
}

bool filterIsValid(const QString& filter) {
    if (!filter.startsWith(QLatin1String("margin."))) return false;
    const QStringList parts = filter.split(QLatin1Char('.'));
    for (int i = 0; i < parts.size(); ++i) {
        if (parts[i].isEmpty()) return false;
        if (parts[i] == QLatin1String("#") && i != parts.size() - 1) return false;
    }
    return true;
}

class EventBusImpl : public EventBus {
public:
    void subscribe(const QString& topic,
                   std::function<void(const QJsonObject&)> handler,
                   QObject* subscriber) override {
        if (!filterIsValid(topic)) {
            qWarning("EventBus: refusing subscribe to malformed topic '%s'",
                     qPrintable(topic));
            return;
        }
        QMutexLocker lock(&m_mutex);
        m_subscribers[topic].append({subscriber, std::move(handler)});
    }

    void publish(const QString& topic, const QJsonObject& payload) override {
        if (!topic.startsWith(QLatin1String("margin."))) {
            qWarning("EventBus: refusing publish to topic '%s' (must start 'margin.')",
                     qPrintable(topic));
            return;
        }
        QVector<Subscription> matched;
        {
            QMutexLocker lock(&m_mutex);
            for (auto it = m_subscribers.constBegin();
                 it != m_subscribers.constEnd(); ++it) {
                if (topicMatches(it.key(), topic)) {
                    for (const auto& s : it.value()) matched.append(s);
                }
            }
        }
        // Dispatch on the main thread via qApp. QJsonObject is implicitly
        // shared -> payload copy is a refcount bump, safe across thread
        // boundary.
        for (const auto& s : matched) {
            QMetaObject::invokeMethod(qApp,
                [handler = s.handler, payload]() { handler(payload); },
                Qt::QueuedConnection);
        }
    }

    void unsubscribeAll(QObject* subscriber) override {
        if (subscriber == nullptr) return;
        QMutexLocker lock(&m_mutex);
        for (auto it = m_subscribers.begin(); it != m_subscribers.end(); ++it) {
            auto& vec = it.value();
            vec.erase(std::remove_if(vec.begin(), vec.end(),
                [subscriber](const Subscription& s) {
                    return s.subscriber == subscriber;
                }), vec.end());
        }
    }

private:
    QMutex m_mutex;
    QHash<QString, QVector<Subscription>> m_subscribers;
};

} // namespace

std::unique_ptr<EventBus> EventBus::wire() {
    return std::unique_ptr<EventBus>(new EventBusImpl());
}

} // namespace Margin
