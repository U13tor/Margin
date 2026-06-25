#include "host/core/SettingsRegistry.h"

#include <algorithm>

#include <QVariantList>
#include <QVariantMap>

namespace Margin {

void SettingsRegistry::addPage(Entry entry) {
    m_entries.append(std::move(entry));
    rebuildCache();
    emit pagesChanged();
}

void SettingsRegistry::sortByOrder() {
    // Sort by (section rank, order) — the sidebar ListView's section.delegate
    // groups consecutive entries by section, so sections MUST be contiguous
    // in the model. Pure order-sort would interleave "about"(order=10) with
    // "general"(order=10) and break grouping.
    static const auto sectionRank = [](const QString& s) -> int {
        if (s == QLatin1String("host"))    return 0;
        if (s == QLatin1String("plugins")) return 1;
        if (s == QLatin1String("about"))   return 2;
        return 99;
    };
    std::stable_sort(m_entries.begin(), m_entries.end(),
                     [](const Entry& a, const Entry& b) {
                         const int ra = sectionRank(a.section);
                         const int rb = sectionRank(b.section);
                         if (ra != rb) return ra < rb;
                         return a.order < b.order;
                     });
    rebuildCache();
    emit pagesChanged();
}

void SettingsRegistry::clear() {
    m_entries.clear();
    rebuildCache();
    emit pagesChanged();
}

void SettingsRegistry::rebuildCache() {
    QVariantList out;
    out.reserve(m_entries.size());
    for (const auto& e : m_entries) {
        QVariantMap m;
        m.insert(QStringLiteral("id"),         e.id);
        m.insert(QStringLiteral("title"),      e.title);
        m.insert(QStringLiteral("icon"),       e.icon.toDisplayString());
        m.insert(QStringLiteral("contentQml"), e.contentQml.toDisplayString());
        m.insert(QStringLiteral("section"),    e.section);
        m.insert(QStringLiteral("order"),      e.order);
        out.append(m);
    }
    m_cachedPages = std::move(out);
}

} // namespace Margin
