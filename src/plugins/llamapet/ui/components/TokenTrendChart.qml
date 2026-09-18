import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Margin.Ui.Primitives

Item {
    id: root

    property int trendDays: 7
    property var trendData: (typeof llamapet !== "undefined" && llamapet) ? llamapet.tokenTrends(trendDays) : []

    implicitWidth: parent ? parent.width : 480
    implicitHeight: chartCol.implicitHeight

    function formatNumber(n) {
        if (!n) return "0"
        return n.toLocaleString()
    }

    function formatDate(dStr) {
        if (!dStr) return "";
        var parts = dStr.split("-");
        if (parts.length < 3) return dStr;
        var year = parts[0];
        var month = parseInt(parts[1], 10);
        var day = parseInt(parts[2], 10);
        var isZh = Qt.locale().name.indexOf("zh") === 0;
        if (isZh) {
            return year + "年" + month + "月" + day + "日";
        }
        var monthsEn = ["", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"];
        var mName = monthsEn[month] || month;
        return mName + " " + day + ", " + year;
    }

    readonly property real maxTokens: {
        var m = 1
        if (trendData && trendData.length > 0) {
            for (var i = 0; i < trendData.length; ++i) {
                if (trendData[i].totalTokens > m) {
                    m = trendData[i].totalTokens
                }
            }
        }
        return m
    }

    ColumnLayout {
        id: chartCol
        anchors.fill: parent
        spacing: Theme.space3

        // 1. Header with legend & 7d/30d switch
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space2

            Text {
                text: qsTr("Usage Trends")
                color: Theme.fgPrimary
                font.pixelSize: Theme.textSm
                font.weight: Font.DemiBold
            }

            RowLayout {
                spacing: 8
                Rectangle { width: 8; height: 8; radius: 2; color: "#00F0FF" }
                Text { text: qsTr("Decoded"); color: Theme.fgSecondary; font.pixelSize: 11 }
                Rectangle { width: 8; height: 8; radius: 2; color: "#2563EB" }
                Text { text: qsTr("Prompt Eval"); color: Theme.fgSecondary; font.pixelSize: 11 }
            }

            Item { Layout.fillWidth: true }

            // Segmented switch (7d vs 30d)
            Rectangle {
                Layout.preferredWidth: 120
                Layout.preferredHeight: 24
                implicitWidth: 120
                implicitHeight: 24
                radius: 4
                color: "#181825"
                border.color: "#313244"
                border.width: 1

                Row {
                    anchors.fill: parent
                    anchors.margins: 2
                    spacing: 2

                    Rectangle {
                        width: (parent.width - 2) / 2
                        height: parent.height
                        radius: 3
                        color: root.trendDays === 7 ? "#313244" : (mouse7d.containsMouse ? "#2089B4FA" : "transparent")

                        Text {
                            anchors.centerIn: parent
                            text: qsTr("Last 7 Days")
                            color: root.trendDays === 7 ? Theme.fgPrimary : Theme.fgMuted
                            font.pixelSize: 11
                            font.weight: root.trendDays === 7 ? Font.Medium : Font.Normal
                        }

                        MouseArea {
                            id: mouse7d
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                root.trendDays = 7
                                if (typeof llamapet !== "undefined" && llamapet) {
                                    root.trendData = llamapet.tokenTrends(7)
                                }
                            }
                        }
                    }

                    Rectangle {
                        width: (parent.width - 2) / 2
                        height: parent.height
                        radius: 3
                        color: root.trendDays === 30 ? "#313244" : (mouse30d.containsMouse ? "#2089B4FA" : "transparent")

                        Text {
                            anchors.centerIn: parent
                            text: qsTr("Last 30 Days")
                            color: root.trendDays === 30 ? Theme.fgPrimary : Theme.fgMuted
                            font.pixelSize: 11
                            font.weight: root.trendDays === 30 ? Font.Medium : Font.Normal
                        }

                        MouseArea {
                            id: mouse30d
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                root.trendDays = 30
                                if (typeof llamapet !== "undefined" && llamapet) {
                                    root.trendData = llamapet.tokenTrends(30)
                                }
                            }
                        }
                    }
                }
            }
        }

        // 2. Bar Chart Area
        Item {
            id: chartArea
            Layout.fillWidth: true
            Layout.preferredHeight: 120
            implicitHeight: 120

            // Baseline reference line
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 20
                height: 1
                color: "#313244"
            }

            RowLayout {
                anchors.fill: parent
                spacing: root.trendDays === 7 ? 12 : 2

                Repeater {
                    model: root.trendData ? root.trendData : []
                    delegate: Item {
                        id: barItem
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        property var itemData: modelData
                        property real totalVal: itemData ? itemData.totalTokens : 0
                        property real compVal: itemData ? itemData.completionTokens : 0
                        property real promptVal: itemData ? itemData.promptTokens : 0

                        // Max height for the bar (excluding bottom label space of 20px)
                        readonly property real maxBarHeight: chartArea.height - 24
                        property real barHeight: totalVal > 0 ? Math.max(4, (totalVal / root.maxTokens) * maxBarHeight) : 2

                        // Stacked Bar Container
                        Item {
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: 21
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: Math.max(4, Math.min(parent.width - 2, root.trendDays === 7 ? 28 : 12))
                            height: barItem.barHeight

                            // Completion part (cyan, top)
                            Rectangle {
                                anchors.top: parent.top
                                anchors.left: parent.left
                                anchors.right: parent.right
                                height: barItem.totalVal > 0 ? Math.max(2, parent.height * (barItem.compVal / barItem.totalVal)) : parent.height
                                radius: 2
                                color: "#00F0FF"
                                opacity: barMouse.containsMouse ? 1.0 : 0.85
                            }

                            // Prompt part (blue, bottom)
                            Rectangle {
                                anchors.bottom: parent.bottom
                                anchors.left: parent.left
                                anchors.right: parent.right
                                height: barItem.totalVal > 0 ? Math.max(0, parent.height * (barItem.promptVal / barItem.totalVal)) : 0
                                radius: 2
                                color: "#2563EB"
                                opacity: barMouse.containsMouse ? 1.0 : 0.85
                            }
                        }

                        // Date label below bar
                        Text {
                            anchors.bottom: parent.bottom
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: {
                                if (!barItem.itemData) return ""
                                if (root.trendDays === 30 && index % 5 !== 0 && index !== (root.trendData.length - 1)) {
                                    return "" // In 30d view, skip some labels to avoid crowding
                                }
                                return barItem.itemData.displayDate
                            }
                            color: barMouse.containsMouse ? Theme.fgPrimary : Theme.fgMuted
                            font.pixelSize: 10
                            font.family: Theme.fontMono
                        }

                        MouseArea {
                            id: barMouse
                            anchors.fill: parent
                            hoverEnabled: true

                            onEntered: {
                                if (barItem.itemData) {
                                    var pt = barItem.mapToItem(root, barItem.width / 2, 0)
                                    trendTooltip.show(pt.x, pt.y, barItem.itemData)
                                }
                            }

                            onExited: {
                                trendTooltip.hide()
                            }
                        }
                    }
                }
            }
        }
    }

    // 3. Floating Tooltip for bar
    Rectangle {
        id: trendTooltip
        visible: false
        z: 100
        radius: Theme.radiusSm
        color: "#181825"
        border.color: "#45475a"
        border.width: 1

        width: tCol.implicitWidth + 20
        height: tCol.implicitHeight + 14

        property var currentData: null

        function show(targetX, targetY, data) {
            currentData = data
            var px = targetX - width / 2
            px = Math.max(6, Math.min(px, root.width - width - 6))
            x = px
            y = Math.max(4, targetY - height - 6)
            visible = true
        }

        function hide() {
            visible = false
        }

        ColumnLayout {
            id: tCol
            anchors.centerIn: parent
            spacing: 2

            Text {
                text: trendTooltip.currentData ? root.formatDate(trendTooltip.currentData.date) : ""
                color: Theme.fgPrimary
                font.pixelSize: 11
                font.weight: Font.DemiBold
            }

            Text {
                text: qsTr("Total: %1 tokens").arg(root.formatNumber(trendTooltip.currentData ? trendTooltip.currentData.totalTokens : 0))
                color: "#00F0FF"
                font.pixelSize: 11
                font.family: Theme.fontMono
                font.weight: Font.Medium
            }

            Text {
                text: qsTr("Decoded: %1 / Prompt: %2")
                       .arg(root.formatNumber(trendTooltip.currentData ? trendTooltip.currentData.completionTokens : 0))
                       .arg(root.formatNumber(trendTooltip.currentData ? trendTooltip.currentData.promptTokens : 0))
                color: Theme.fgSecondary
                font.pixelSize: 10
                font.family: Theme.fontMono
            }
        }
    }
}

