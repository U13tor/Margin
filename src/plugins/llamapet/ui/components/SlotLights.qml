import QtQuick

Item {
    id: root

    property int activeSlots: 0
    property int totalSlots: 4

    implicitWidth: row.implicitWidth
    implicitHeight: row.implicitHeight

    Row {
        id: row
        spacing: 4
        anchors.verticalCenter: parent.verticalCenter

        Repeater {
            model: Math.min(Math.max(root.totalSlots, 1), 8)

            Rectangle {
                width: 6
                height: 6
                radius: 3
                color: index < root.activeSlots ? "#00F0FF" : "#3B4252"
                border.color: index < root.activeSlots ? "#7EA6E0" : "transparent"
                border.width: 1
            }
        }
    }
}
