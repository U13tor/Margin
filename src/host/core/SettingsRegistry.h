// SettingsRegistry — host-side registry for the Layer 2 Settings window
// (docs/06 §4.6). Mirrors DashboardTabRegistry's shape, with one extra
// field: `section`, used by the sidebar ListView's section.delegate to
// render group headers ("Plugins" / "About"). Host-owned pages populate
// host/about sections directly; plugin-contributed pages land in the
// "plugins" section implicitly (HostCore assigns it from PageInfo, which
// does not expose section — see docs/04-plugin-spec.md §7).

#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>

namespace Margin {

class SettingsRegistry : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList pages READ pages NOTIFY pagesChanged)

public:
    struct Entry {
        QString id;
        QString title;
        QUrl    icon;
        QUrl    contentQml;
        QString section;   // "host" / "plugins" / "about"
        int     order = 0;
    };

    void addPage(Entry entry);
    void sortByOrder();

    /// Remove all entries. Emits pagesChanged once. Paired with
    /// DashboardTabRegistry::clear to re-fresh C++ translated sidebar
    /// titles on language change.
    void clear();

    QVariantList pages() const { return m_cachedPages; }

signals:
    void pagesChanged();

private:
    void rebuildCache();

    QList<Entry>   m_entries;
    QVariantList   m_cachedPages;
};

} // namespace Margin
