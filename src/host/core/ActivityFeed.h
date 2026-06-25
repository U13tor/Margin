// ActivityFeed — Host-side aggregator that subscribes to a curated set of
// cross-plugin EventBus topics and exposes the most recent events to QML
// via a Q_PROPERTY. Drives the "最近事件" card on OverviewTab (docs/06
// §4.2). Mirrors DashboardTabRegistry's shape: QObject + Q_PROPERTY +
// setContextProperty("activityFeed", ...) from HostCore::bootstrap.
//
// Why Host-side and not plugin-side: OverviewTab is Host-owned (Layer 1
// main panel) and the feed is cross-plugin by definition (Aura + Rhythm +
// future). EventBus itself is a Host service, so aggregation belongs here.
//
// Topic subscription is an explicit list (not a `margin.aura.#` wildcard).
// Wildcards would also catch `margin.aura.state` — a high-frequency
// internal state machine topic that's noise to humans. Explicit also
// forces each new event source to consciously pick (title, color), which
// keeps the feed legible.
//
// Buffer policy: ring buffer of 20 (kBufferSize). UI slices to 4 for the
// card; the larger buffer leaves room for a future detail view without a
// schema change. On overflow the oldest entry is dropped (prepend newest,
// truncate tail).

#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

#include <vector>

namespace Margin {

class EventBus;
class Database;

class ActivityFeed : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList events READ events NOTIFY eventsChanged)

public:
    struct TopicRule {
        QString topic;          // e.g. "margin.aura.away"
        QString title;          // pre-rendered description (no payload substitution yet)
        QString colorRole;      // Theme token name, e.g. "accentDanger"
    };

    /// Max entries retained. Newest at index 0; oldest dropped past this.
    static constexpr int kBufferSize = 20;

    /// Subscribes to all curated topics on `bus`. `this` is registered as
    /// the subscriber identity so the destructor can bulk-unsubscribe.
    explicit ActivityFeed(EventBus& bus, QObject* parent = nullptr);
    ~ActivityFeed() override;

    QVariantList events() const { return m_events; }

    /// Test seam: the curated topic → (title, colorRole) table. Public so
    /// tests can assert the mapping without re-deriving it.
    static const std::vector<TopicRule>& topicRules();

    /// Wire up SQLite persistence. Called by HostCore after construction
    /// once m_database is known good. Idempotent. On schema/load failure
    /// m_db stays null and the feed degrades to pure in-memory behavior
    /// (PR6 back-compat — covered by NullDatabaseGraceful unit test).
    void attachDatabase(Database& db);

    /// Re-render every entry's title via QCoreApplication::translate under
    /// the currently-installed catalog and emit eventsChanged. Called by
    /// HostCore::applyLanguage after the new translator is installed so
    /// the feed flips with the language switch (mirrors reemitRegistries
    /// for tab/page titles). No-op semantic if m_events is empty.
    void retranslate();

signals:
    void eventsChanged();

private:
    void handleEvent(const TopicRule& rule);

    bool ensureSchema();
    bool loadFromDb();

    EventBus* m_bus;
    Database* m_db = nullptr;
    QVariantList m_events;
};

} // namespace Margin
