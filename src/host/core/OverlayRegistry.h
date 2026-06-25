// OverlayRegistry — host-side aggregator for plugin-contributed Layer 3
// overlays (docs/06 §3.1, docs/04 §7). Mirrors DashboardTabRegistry's pattern:
// built once in HostCore::bootstrap by iterating plugins via forEachPlugin,
// then exposed to QML as the `overlayRegistry` context property.
//
// Polling model: HostCore owns a 500ms QTimer that calls pollAll(), which
// walks each OverlayContributor and asks shouldShow(). Active overlays
// (those returning true) are exposed via the activeOverlays Q_PROPERTY as a
// QVariantList of { overlayQml: url } maps. OverlayContainer.qml binds a
// Repeater to this list, so a contributor flipping shouldShow() true/false
// results in its Loader being created/destroyed within the next poll.
//
// Polling instead of signals: see OverlayContributor.h header for rationale
// (ABI-stable across DLL boundaries, breaks are not latency-sensitive).

#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>

#include <vector>

namespace Margin {

class OverlayContributor;

class OverlayRegistry : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList activeOverlays READ activeOverlays NOTIFY activeOverlaysChanged)

public:
    /// Append a contributor. Lifetime of the pointer is owned by the plugin;
    /// registry holds raw pointers and stops touching them after removePlugin.
    void addContributor(OverlayContributor* c);

    /// Drop all contributors. Called during shutdown before plugins unload.
    void clear();

    /// Walk each contributor, ask shouldShow(), rebuild the active list.
    /// Emits activeOverlaysChanged only when the set of active overlays
    /// actually changes — stable polling, no signal noise.
    void pollAll();

    /// Active overlays as QVariantMaps: { overlayQml: <display string> }.
    /// OverlayContainer.qml binds a Repeater.model to this.
    QVariantList activeOverlays() const { return m_cachedActive; }

signals:
    void activeOverlaysChanged();

private:
    void rebuildCache();

    std::vector<OverlayContributor*> m_contributors;
    // Currently-active QML URLs (the source of truth that drives QML).
    // We track these to detect "no change" between polls and skip the emit.
    std::vector<QUrl> m_activeUrls;
    QVariantList     m_cachedActive;
};

} // namespace Margin
