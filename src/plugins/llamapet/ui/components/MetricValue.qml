import QtQuick

Item {
    id: root

    property string label: ""
    property string value: ""
    property string unit: ""
    property color valueColor: "#F3CBA5"

    implicitWidth: row.implicitWidth
    implicitHeight: row.implicitHeight

    Row {
        id: row
        spacing: 3
        anchors.verticalCenter: parent.verticalCenter

        Text {
            text: root.label
            color: "#8292B0"
            font.pixelSize: 10
            font.family: "Inter"
            anchors.baseline: valText.baseline
        }

        Text {
            id: valText
            text: root.value
            color: root.valueColor
            font.pixelSize: 11
            font.bold: true
            font.family: "JetBrains Mono"
        }

        Text {
            text: root.unit
            color: "#8292B0"
            font.pixelSize: 9
            font.family: "Inter"
            visible: root.unit.length > 0
            anchors.baseline: valText.baseline
        }
    }
}
