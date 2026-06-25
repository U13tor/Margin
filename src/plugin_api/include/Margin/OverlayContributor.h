// OverlayContributor — host-pull interface for plugins to contribute a Layer 3
// full-screen overlay (docs/06 §3.1, docs/04 §7). Spec: docs/04-plugin-spec.md
// §7 "4. 全屏遮罩贡献者".
//
// Plugins override PluginInterface::asOverlay() to return `this` (after
// multiply-inheriting OverlayContributor). Host's OverlayRegistry polls each
// contributor's shouldShow() every ~500ms; when true, OverlayContainer.qml
// loads the returned overlayUrl() in a Repeater. dismiss() is called when the
// user hits Esc or the overlay's own "跳过" button — the plugin is responsible
// for transitioning out of the break state (e.g. PomodoroTimer::skipBreak()).
//
// Why polling instead of a signal: contributors live across DLL boundaries and
// Qt signals across ABI boundaries are fragile (moc-generated metadata must
// match). Polling + the 500ms cadence is acceptable because breaks are not
// latency-sensitive (the user is already paused). Same trade-off as M3-C3's
// toast visibility — keep trigger logic in one place.

#pragma once

#include <QUrl>

namespace Margin {

class OverlayContributor {
public:
    virtual ~OverlayContributor() = default;

    /// Host calls this every poll tick (500ms). Returning true causes the
    /// host's OverlayContainer to Loader-load the overlayUrl(). Idempotent —
    /// returning true while already visible is a no-op on the host side.
    virtual bool shouldShow() const = 0;

    /// QML URL for the overlay content. Loaded into a Loader whose parent is
    /// the host's top-level OverlayContainer Window. Must reference a qrc:
    /// path (no runtime network — red line 1).
    virtual QUrl overlayUrl() const = 0;

    /// User-initiated dismissal (Esc key in OverlayContainer, or the
    /// overlay's own "跳过" button). Plugin is responsible for state
    /// transition (e.g. PomodoroTimer::skipBreak).
    virtual void dismiss() = 0;
};

} // namespace Margin
