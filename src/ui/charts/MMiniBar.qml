// MMiniBar — single thin progress bar for compact status cards (docs/06 §6.4,
// Tab1 overview "Top 5" rows). One value against a max; clamps to [0, 1].

import QtQuick
import Margin.Ui.Primitives

Rectangle {
    id: root

    property real value: 0
    property real maxValue: 1
    property color barColor: Theme.accentBrand

    implicitHeight: 6
    radius: 3
    color: Theme.bgHover

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: root.maxValue > 0
               ? Math.max(0, Math.min(1, root.value / root.maxValue)) * root.width
               : 0
        radius: 3
        color: root.barColor
    }
}
