// ProgressDots — n-dot progress indicator atom (docs/06 §6.2).
//
// Renders a horizontal row of `total` discs; the first `active` get the
// activeColor, the rest get inactiveColor. Used by RhythmTab's "#N/M"
// pomodoro row (5 dots) and BreakOverlay's 8-step stretch cycle.
//
// API:
//   total:          disc count (>= 0)
//   active:         how many leading discs are "filled" (clamped to [0..total])
//   activeColor:    Theme.accentBrand by default
//   inactiveColor:  Theme.borderSubtle by default
//   dotSize:        disc diameter in px (8 default)
//   spacing:        gap between discs in px (6 default = Theme.space1+2)
//
// Out-of-range `active` is clamped on read so callers don't have to guard
// against off-by-one or negative values from upstream state.

import QtQuick
import Margin.Ui.Primitives

Row {
    id: root

    property int total: 5
    property int active: 0
    property color activeColor: Theme.accentBrand
    property color inactiveColor: Theme.borderSubtle
    property int dotSize: 8
    spacing: 6

    readonly property int _clampedActive: Math.max(0, Math.min(root.active, root.total))

    Repeater {
        model: Math.max(0, root.total)
        Rectangle {
            width: root.dotSize
            height: root.dotSize
            radius: width / 2
            color: index < root._clampedActive ? root.activeColor : root.inactiveColor
            Behavior on color {
                ColorAnimation {
                    duration: Theme.durationFast
                    easing.bezierCurve: Theme.easeOut
                }
            }
        }
    }
}
