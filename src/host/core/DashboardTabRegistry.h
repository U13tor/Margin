// DashboardTabRegistry — single source of truth for the Layer 1 main-panel
// tab list (docs/06 §3.1, docs/04 §7). Aggregates Host's own Overview tab
// (added directly) with tabs contributed by plugins via
// DashboardTabContributor. Exposed to QML as the `dashboardTabs` context
// property; TabBar and StackLayout share this model so host tabs and plugin
// tabs render through one code path.
//
// Order: TabInfo::order (ascending). Host calls sortByOrder() once after all
// addTab() calls so M1+ plugin tabs slot into deterministic positions
// (Overview=10, Aura=20, Screen Time=30, Rhythm=40) without host-side
// hardcoding.

#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>

namespace Margin {

class DashboardTabRegistry : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList tabs READ tabs NOTIFY tabsChanged)

public:
    struct Entry {
        QString id;
        QString title;
        QUrl    icon;
        QUrl    contentQml;
        int     order = 0;
    };

    /// Append a tab. Does NOT re-sort — call sortByOrder() once after all
    /// addTab() calls are done. Emits tabsChanged.
    void addTab(Entry entry);

    /// Stable ascending sort by Entry::order. Emits tabsChanged.
    void sortByOrder();

    /// Remove all entries. Emits tabsChanged once after the cache is wiped.
    /// Used by HostCore::reemitRegistries to refresh C++ translated tab
    /// titles when the active language changes: clear() → re-addTab() with
    /// QCoreApplication::translate() picking up the newly installed catalog.
    void clear();

    /// All entries as a list of QVariantMaps keyed by id/title/icon/
    /// contentQml/order. icon/contentQml are stringified (toDisplayString)
    /// so QML can use them directly as `source:` values. Returns the
    /// pre-built cache — O(1) per read, rebuilt only on addTab/sortByOrder.
    QVariantList tabs() const { return m_cachedTabs; }

signals:
    void tabsChanged();

private:
    void rebuildCache();

    QList<Entry>   m_entries;
    QVariantList   m_cachedTabs;
};

} // namespace Margin
