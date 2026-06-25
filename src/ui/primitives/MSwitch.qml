// MSwitch — toggle atom (docs/06 §6.2). Single boolean state, click to flip.
// API mirrors Qt Switch (checked + signal toggled(bool)) so future drop-in
// replacement of any Controls.Switch is a one-line import swap.
//
// API:
//   checked:  bool (default false)
//   enabled:  bool (false → muted style + click suppressed)
//   label:    a11y-only string, not rendered (atom is pure visual)
//   signal toggled(bool checked)
//
// Visual contract:
//   track 36×20 radius-full; accentBrand when checked, borderStrong when not
//   thumb 16×16 radius-full; x animates between space1 and width-width-space1
//   disabled → track bgHover + thumb fgMuted (matches MButton's mute pattern)
//
// Does NOT import QtQuick.Controls (docs/15 §B2 — only MouseArea from QtQuick
// is needed; pulling Controls 2 would inherit its light style). Click-only —
// no swipe gesture (touch gesture lands when a real touch use case arrives).

import QtQuick
import Margin.Ui.Primitives

Item {
    id: root

    property bool checked: false
    property bool enabled: true
    property string label: ""

    signal toggled(bool checked)

    implicitWidth: 36
    implicitHeight: 20

    readonly property color _trackColor: !root.enabled
                                          ? Theme.bgHover
                                          : (root.checked ? Theme.accentBrand : Theme.borderStrong)
    readonly property color _thumbColor: !root.enabled ? Theme.fgMuted : Theme.fgPrimary

    Rectangle {
        id: track
        anchors.fill: parent
        radius: Theme.radiusFull
        color: root._trackColor
        Behavior on color {
            ColorAnimation {
                duration: Theme.durationFast
                easing.bezierCurve: Theme.easeOut
            }
        }

        MouseArea {
            id: ma
            anchors.fill: parent
            cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            enabled: root.enabled
            onClicked: {
                root.checked = !root.checked
                root.toggled(root.checked)
            }
        }

        Rectangle {
            id: thumb
            width: 16
            height: 16
            radius: Theme.radiusFull
            color: root._thumbColor
            y: (parent.height - height) / 2
            x: root.checked ? (parent.width - width - Theme.space1) : Theme.space1
            Behavior on x {
                NumberAnimation {
                    duration: Theme.durationFast
                    easing.bezierCurve: Theme.easeOut
                }
            }
            Behavior on color {
                ColorAnimation {
                    duration: Theme.durationFast
                    easing.bezierCurve: Theme.easeOut
                }
            }
        }
    }
}
