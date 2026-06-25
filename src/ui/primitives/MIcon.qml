// MIcon — icon atom (docs/06 §7 + §6.2). Renders an SVG from qrc:/icons/* at a
// square size with a currentColor tint applied via MultiEffect. Mirrors the
// pattern TabBar.qml pioneered (its inline Image + MultiEffect pair is the
// canonical colorization recipe); MIcon is the reusable extraction so every
// status bar / button row / list item can stop hand-rolling the same block.
//
// API:
//   source: qrc URL string, e.g. "qrc:/icons/icon-bell.svg"
//   size:   square edge in px (defaults to 16; minimum recommended 12 — below
//           that MultiEffect antialiasing degrades)
//   color:  currentColor tint, defaults to Theme.fgPrimary
//
// Invalid source: Image.status flips to Error, MultiEffect renders empty.
// No exceptions, no QML warnings beyond Qt's own Image load failure log.

import QtQuick
import QtQuick.Effects
import Margin.Ui.Primitives

Item {
    id: root

    property string source: ""
    property int size: 16
    property color color: Theme.fgPrimary

    // Exposed so callers + tests can render a fallback when the icon failed
    // to load (status === Error). Values mirror Image.status: Null=0,
    // Ready=1, Loading=2, Error=3.
    readonly property int imageStatus: img.status

    implicitWidth: size
    implicitHeight: size

    Image {
        id: img
        anchors.fill: parent
        visible: false
        source: root.source
        sourceSize: Qt.size(root.size, root.size)
        // invalid source → status = Error, no exception
    }

    MultiEffect {
        anchors.fill: parent
        source: img
        colorizationColor: root.color
        colorization: 1.0
    }
}
