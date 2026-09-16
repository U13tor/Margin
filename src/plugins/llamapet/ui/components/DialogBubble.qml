import QtQuick

Item {
    id: root

    property string text: ""
    visible: text.length > 0
    implicitWidth: bubble.implicitWidth
    implicitHeight: bubble.implicitHeight

    Rectangle {
        id: bubble
        implicitWidth: Math.min(Math.max(content.implicitWidth + 16, 40), 180)
        implicitHeight: content.implicitHeight + 10
        radius: 8
        color: "#1E1E2E"
        border.color: "#45475A"
        border.width: 1

        Text {
            id: content
            anchors.centerIn: parent
            text: root.text
            color: "#CDD6F4"
            font.pixelSize: 11
            font.family: "Inter"
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }
    }
}
