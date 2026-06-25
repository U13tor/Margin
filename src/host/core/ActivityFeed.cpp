// ActivityFeed impl — see header.

#include "host/core/ActivityFeed.h"

#include "Margin/Database.h"
#include "Margin/EventBus.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonObject>
#include <QMap>
#include <QMessageLogger>
#include <QStringList>
#include <QVariantMap>

namespace Margin {

// Curated topic → (title, colorRole) table. Color tokens must exist on
// Theme.qml — fallback in QML is Theme.fgMuted if a typo slips through.
// Topics mirror what plugins actually publish today (manifest.json
// events.publish arrays). Rhythm's pomo-complete / stretch-complete are
// NOT yet published — see docs/12-deferred-items.md §A26.
//
// `title` is the translation SOURCE string (qsTr convention: source =
// Chinese, host_en.ts carries the English rendering). It MUST go through
// renderTitle() — which calls QCoreApplication::translate("ActivityFeed",
// ...) — before reaching QML, otherwise the language switch can't flip
// already-rendered entries. See retranslate() for the second half of
// this contract.
//
// Built as a static-local std::vector (NOT constexpr): QString is a
// ref-counted type with a user-defined destructor, so it isn't a literal
// type and can't live in a constexpr array. The vector is constructed
// once on first call (thread-safe per C++11 §6.7), lives till process
// exit.
const std::vector<ActivityFeed::TopicRule>& ActivityFeed::topicRules() {
    static const std::vector<TopicRule> kTable = {
        { QStringLiteral("margin.aura.away"),
          QStringLiteral("锁屏触发(设备离开)"),
          QStringLiteral("accentDanger") },
        { QStringLiteral("margin.aura.back"),
          QStringLiteral("锁屏解除(设备回到范围)"),
          QStringLiteral("accentSuccess") },
        { QStringLiteral("margin.aura.warning"),
          QStringLiteral("蓝牙信号警告"),
          QStringLiteral("accentWarning") },
        { QStringLiteral("margin.rhythm.break_due"),
          QStringLiteral("休息时间到"),
          QStringLiteral("accentBrand") },
        { QStringLiteral("margin.rhythm.break_started"),
          QStringLiteral("颈椎操开始"),
          QStringLiteral("accentBrand") },
        { QStringLiteral("margin.rhythm.break_dismissed"),
          QStringLiteral("休息提醒已忽略"),
          QStringLiteral("fgMuted") },
    };
    return kTable;
}

// Render a topic rule's title under the currently-installed translator.
// Context name "ActivityFeed" is semantic (matches DashboardTabs /
// SettingsPages convention). lupdate does not scan
// QCoreApplication::translate calls, so the corresponding <context> in
// i18n/host_{en,zh_CN}.ts is hand-maintained — mirrors how tab/page
// titles work (see HostCore.cpp registerHostTabs comment).
static QString renderTitle(const QString& source) {
    return QCoreApplication::translate("ActivityFeed",
                                       source.toUtf8().constData());
}

ActivityFeed::ActivityFeed(EventBus& bus, QObject* parent)
    : QObject(parent), m_bus(&bus) {
    for (const auto& rule : topicRules()) {
        // Capture `rule` by value — it's a small POD, and the lambda
        // outlives any single subscribe() call (bus holds it until
        // unsubscribeAll). `this` as subscriber identity lets the dtor
        // bulk-clean every subscription in one call.
        bus.subscribe(rule.topic,
                      [this, rule](const QJsonObject&) { handleEvent(rule); },
                      this);
    }
}

ActivityFeed::~ActivityFeed() {
    if (m_bus) m_bus->unsubscribeAll(this);
}

void ActivityFeed::attachDatabase(Database& db) {
    m_db = &db;
    if (!ensureSchema()) {
        m_db = nullptr;
        return;
    }
    if (!loadFromDb()) {
        m_db = nullptr;
        return;
    }
    emit eventsChanged();
}

void ActivityFeed::retranslate() {
    // Build topic → source-string index once so the re-render stays O(n)
    // even if topicRules() grows. Mirrors the byTopic pattern in
    // loadFromDb.
    QMap<QString, const TopicRule*> byTopic;
    for (const auto& r : topicRules()) {
        byTopic.insert(r.topic, &r);
    }

    // QVariantMap values aren't directly mutable through QVariantList
    // iterators — take a copy, update, put back. Cheap at kBufferSize=20.
    bool changed = false;
    for (int i = 0; i < m_events.size(); ++i) {
        const QVariantMap entry = m_events.at(i).toMap();
        const QString topic = entry.value(QStringLiteral("topic")).toString();
        const auto it = byTopic.constFind(topic);
        if (it == byTopic.constEnd()) continue;  // orphan — leave as-is
        QVariantMap updated = entry;
        updated.insert(QStringLiteral("title"), renderTitle(it.value()->title));
        m_events.replace(i, updated);
        changed = true;
    }
    if (changed) emit eventsChanged();
}

bool ActivityFeed::ensureSchema() {
    static const QStringList kStatements = {
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS host_activity_events ("
            "    id      INTEGER PRIMARY KEY AUTOINCREMENT,"
            "    time_ms INTEGER NOT NULL,"
            "    topic   TEXT    NOT NULL"
            ")"),
        QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_host_activity_time "
            "ON host_activity_events(time_ms)"),
    };
    for (const auto& sql : kStatements) {
        if (!m_db->exec(sql)) return false;
    }
    return true;
}

bool ActivityFeed::loadFromDb() {
    const auto rows = m_db->query(
        QStringLiteral("SELECT time_ms, topic FROM host_activity_events "
                       "ORDER BY id DESC LIMIT :limit"),
        {{QStringLiteral("limit"), kBufferSize}});
    // query() returns empty on error too — distinguish via lastError().
    if (rows.isEmpty() && !m_db->lastError().isEmpty()) {
        return false;
    }

    // Build a topic → rule index once so loads stay O(n) even if
    // topicRules() grows.
    QMap<QString, const TopicRule*> byTopic;
    for (const auto& r : topicRules()) {
        byTopic.insert(r.topic, &r);
    }

    QVariantList loaded;
    int droppedOrphans = 0;
    for (const auto& row : rows) {
        const QString topic = row.value(QStringLiteral("topic")).toString();
        const auto it = byTopic.constFind(topic);
        if (it == byTopic.constEnd()) {
            // A topic that was removed from topicRules() since the row
            // was written. Drop it from the feed rather than render a
            // blank entry; this is how renames clean up organically.
            ++droppedOrphans;
            continue;
        }
        const TopicRule* rule = it.value();
        QVariantMap entry;
        entry.insert(QStringLiteral("timeMs"), row.value(QStringLiteral("time_ms")));
        entry.insert(QStringLiteral("topic"), topic);
        entry.insert(QStringLiteral("title"), renderTitle(rule->title));
        entry.insert(QStringLiteral("colorRole"), rule->colorRole);
        // rows are id DESC (newest first); append preserves that order
        // so index 0 remains the newest, matching handleEvent's prepend.
        loaded.append(entry);
    }
    m_events = std::move(loaded);
    if (droppedOrphans > 0) {
        qWarning("ActivityFeed: dropped %d orphan row(s) with no matching topic rule",
                 droppedOrphans);
    }
    return true;
}

void ActivityFeed::handleEvent(const TopicRule& rule) {
    QVariantMap entry;
    const qint64 ts = QDateTime::currentMSecsSinceEpoch();
    entry.insert(QStringLiteral("timeMs"), QVariant::fromValue<qint64>(ts));
    entry.insert(QStringLiteral("topic"), rule.topic);
    entry.insert(QStringLiteral("title"), renderTitle(rule.title));
    entry.insert(QStringLiteral("colorRole"), rule.colorRole);

    m_events.prepend(entry);
    while (m_events.size() > kBufferSize) {
        m_events.removeLast();
    }

    if (m_db) {
        // Wrap INSERT + ring-buffer trim in a transaction so a crash
        // between them can roll back. Worst case without the wrap is a
        // few stale rows that the next startup's LIMIT-clamped SELECT
        // would mask anyway — the transaction is belt-and-suspenders.
        m_db->transaction();
        m_db->exec(
            QStringLiteral("INSERT INTO host_activity_events (time_ms, topic) "
                           "VALUES (:ts, :topic)"),
            {{QStringLiteral("ts"), QVariant::fromValue<qint64>(ts)},
             {QStringLiteral("topic"), rule.topic}});
        m_db->exec(
            QStringLiteral("DELETE FROM host_activity_events WHERE id NOT IN "
                           "(SELECT id FROM host_activity_events "
                           " ORDER BY id DESC LIMIT :limit)"),
            {{QStringLiteral("limit"), kBufferSize}});
        m_db->commit();
    }

    emit eventsChanged();
}

} // namespace Margin
