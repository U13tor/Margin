import QtQuick

Item {
    id: root

    property real value: 0.0 // 0.0 ~ 100.0
    property real warnThreshold: 85.0
    property real dangerThreshold: 95.0

    implicitWidth: 100
    implicitHeight: 4

    Rectangle {
        id: bg
        anchors.fill: parent
        radius: height / 2
        color: "#282A36"
    }

    Rectangle {
        id: fill
        height: parent.height
        radius: height / 2
        width: Math.min(Math.max(0, root.value / 100.0 * parent.width), parent.width)

        color: {
            if (root.value >= root.dangerThreshold) return "#FF3366"; // Danger Red
            if (root.value >= root.warnThreshold) return "#FFAA00";   // Warn Orange
            return "#00FF88"; // Normal Green
        }

        Behavior on width {
            NumberAnimation { duration: 250; easing.type: Easing.OutQuad }
        }
    }
}
