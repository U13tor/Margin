// MSlider — slider atom (docs/06 §6.2). Numeric range with draggable handle.
// API mirrors Qt Slider (from / to / value / stepSize + signal moved(real)) so
// drop-in replacement of Controls.Slider is a one-line import swap.
//
// API:
//   from:        real (default 0.0)
//   to:          real (default 1.0)
//   value:       real (default 0.0)
//   stepSize:    real (default 0.0 = continuous; >0 = discrete snap on drag)
//   enabled:     bool (false → suppresses updates, visual unchanged)
//   orientation: Qt.Horizontal (Vertical is TODO; Qt enum exposed for parity)
//   signal moved(real value)
//
// UX contract:
//   Click anywhere on track → value jumps to that position (common desktop UX)
//   Drag handle             → value follows mouse, clamped to [from, to]
//   stepSize > 0            → drag snaps to multiples of stepSize; external
//                             `value =` writes are NOT snapped (matches Qt Slider)
//
// MouseArea uses preventStealing: true (docs/15 §B8 — Flickable/ScrollView
// would otherwise swallow the horizontal drag). _setValueFromFraction is the
// single test-controllable entry point; drag and click both route through it.
// Does NOT import QtQuick.Controls (docs/15 §B2).

import QtQuick
import Margin.Ui.Primitives

Item {
    id: root

    property real from: 0.0
    property real to: 1.0
    property real value: 0.0
    property real stepSize: 0.0
    property bool enabled: true
    property int orientation: Qt.Horizontal

    signal moved(real value)

    implicitWidth: 200
    implicitHeight: 20

    readonly property real _range: Math.max(0.0001, root.to - root.from)
    readonly property real _rawFraction: (root.value - root.from) / root._range
    readonly property real _clampedFraction: Math.max(0, Math.min(1, root._rawFraction))

    function _setValueFromFraction(f) {
        if (!root.enabled) return
        let clamped = Math.max(0, Math.min(1, f))
        let v = root.from + clamped * root._range
        if (root.stepSize > 0) {
            v = root.from + Math.round((v - root.from) / root.stepSize) * root.stepSize
        }
        v = Math.max(root.from, Math.min(root.to, v))
        if (v !== root.value) {
            root.value = v
            root.moved(v)
        }
    }

    Rectangle {
        id: track
        height: 4
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.right: parent.right
        radius: Theme.radiusFull
        color: Theme.borderSubtle

        Rectangle {
            id: progress
            height: parent.height
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width * root._clampedFraction
            radius: Theme.radiusFull
            color: Theme.accentBrand
        }

        Rectangle {
            id: handle
            width: 16
            height: 16
            radius: Theme.radiusFull
            color: Theme.fgPrimary
            border.color: Theme.accentBrand
            border.width: 2
            anchors.verticalCenter: parent.verticalCenter
            x: root._clampedFraction * (parent.width - width)
        }
    }

    MouseArea {
        id: ma
        anchors.fill: parent
        preventStealing: true
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        enabled: root.enabled
        onPressed: (mouse) => root._setValueFromFraction(mouse.x / root.width)
        onPositionChanged: (mouse) => {
            if (ma.pressed) root._setValueFromFraction(mouse.x / root.width)
        }
    }
}
