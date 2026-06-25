// OverlayContainer — Layer 3 full-screen overlay host (docs/06 §3.1, docs/04
// §7). Top-level Window that stays above Layer 1 (DashboardWindow) and Layer 2
// (toast). Population: Repeater bound to `overlayRegistry.activeOverlays`
// (context property injected by HostCore).
//
// CRITICAL VISIBILITY MODEL (rewritten 2026-06-19 after black-screen regression
// on real Windows machines — see docs/15-dev-gotchas.md §B5):
//
//   - `visible: false` is HARDCODED static here, never changes from QML.
//   - No `visibility` property at all in QML.
//   - HostCore owns a pointer to this window and calls showFullScreen() /
//     hide() from C++ in response to OverlayRegistry::activeOverlaysChanged.
//   - This mirrors the RhythmToast pattern (C++ imperative show/hide), which
//     is empirically reliable. The earlier binding-based approach
//     (`visible: overlayRegistry.activeOverlays.length > 0`) compiled + ran
//     cleanly but still blanked the screen at startup on some machines —
//     Qt 6.5 Window binding eval timing is not bulletproof on Windows.

import QtQuick
import QtQuick.Window

Window {
    id: root
    objectName: "overlayContainer"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    color: "black"
    visible: false  // C++ controls show/hide via QQuickWindow::showFullScreen/hide

    Repeater {
        model: (typeof overlayRegistry !== "undefined" && overlayRegistry)
               ? overlayRegistry.activeOverlays : []
        // Loader defaults to 0×0; without anchors.fill the loaded Item has no
        // coordinate space and its anchors.centerIn children collapse to (0,0)
        // — see docs/15-dev-gotchas.md §B6.
        Loader {
            source: modelData.overlayQml
            anchors.fill: parent
        }
    }
}
