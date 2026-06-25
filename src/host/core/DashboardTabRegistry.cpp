// DashboardTabRegistry impl — see header.

#include "host/core/DashboardTabRegistry.h"

#include <algorithm>

#include <QVariantList>
#include <QVariantMap>

namespace Margin {

void DashboardTabRegistry::addTab(Entry entry) {
    m_entries.append(std::move(entry));
    rebuildCache();
    emit tabsChanged();
}

void DashboardTabRegistry::sortByOrder() {
    std::stable_sort(m_entries.begin(), m_entries.end(),
                     [](const Entry& a, const Entry& b) {
                         return a.order < b.order;
                     });
    rebuildCache();
    emit tabsChanged();
}

void DashboardTabRegistry::clear() {
    m_entries.clear();
    rebuildCache();
    emit tabsChanged();
}

void DashboardTabRegistry::rebuildCache() {
    // Pre-serialize to QVariantMap once per mutation so QML ListView reads
    // are O(1) instead of rebuilding the list on every binding refresh.
    // Q_PROPERTY READ with NOTIFY only fires once per mutation, but a
    // defensive cache avoids the tail-latency case where Qt internals
    // re-read the property outside the NOTIFY path.
    QVariantList out;
    out.reserve(m_entries.size());
    for (const auto& e : m_entries) {
        QVariantMap m;
        m.insert(QStringLiteral("id"),         e.id);
        m.insert(QStringLiteral("title"),      e.title);
        m.insert(QStringLiteral("icon"),       e.icon.toDisplayString());
        m.insert(QStringLiteral("contentQml"), e.contentQml.toDisplayString());
        m.insert(QStringLiteral("order"),      e.order);
        out.append(m);
    }
    m_cachedTabs = std::move(out);
}

} // namespace Margin
