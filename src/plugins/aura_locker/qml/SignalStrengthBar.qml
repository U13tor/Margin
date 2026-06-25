// SignalStrengthBar — 10-segment RSSI visualization (plugin-internal).
// Lives under src/plugins/aura_locker/qml/, NOT in Margin.Ui.Primitives:
// docs/06-ui-design.md §4.5.2 records this as an aura-locker-scoped helper
// (not a generic primitive — only the BLE scan list consumes it).
//
// Range mapping: -100 dBm → 0 segments (effectively no signal), -30 dBm → 10
// segments (excellent). Linear in between — finer granularity isn't useful
// at this visual size. Color thresholds:
//   7+ segments → accentSuccess (strong / close)
//   3-6 segments → accentWarning (usable)
//   0-2 segments → accentDanger (weak / borderline)
//
// Layout: bars are BOTTOM-aligned (grow upward from a common baseline),
// matching the standard signal-strength convention. Using Item as the root
// (not Row) because Row's default top-alignment makes bars visually hang
// "upside down" — short bars leave a gap below, tall bars extend further
// down, which reads as inverted to users familiar with phone signal UIs.

import QtQuick
import Margin.Ui.Primitives

Item {
    id: root
    property int rssiDbm: -100

    readonly property int activeSegments: {
        if (rssiDbm <= -100) return 0
        if (rssiDbm >= -30) return 10
        return Math.round((rssiDbm + 100) / 7)
    }

    // Geometry constants — exposed as properties so the Repeater can read
    // them without traversing scope chains per-frame.
    readonly property real _barWidth: 4
    readonly property real _barGap: 2
    readonly property real _stepHeight: 2
    readonly property real _tallestBar: _barWidth + 9 * _stepHeight  // 4 + 18 = 22

    implicitWidth: 10 * _barWidth + 9 * _barGap     // 58
    implicitHeight: _tallestBar                      // 22

    Repeater {
        model: 10
        Rectangle {
            x: index * (root._barWidth + root._barGap)
            y: root.height - height                  // bottom-align
            width: root._barWidth
            height: root._barWidth + index * root._stepHeight
            radius: 1
            color: index < root.activeSegments
                   ? (root.activeSegments >= 7 ? Theme.accentSuccess
                      : root.activeSegments >= 3 ? Theme.accentWarning
                      : Theme.accentDanger)
                   : Theme.bgElevated
        }
    }
}
