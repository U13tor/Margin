import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Margin.Ui.Primitives

Item {
    id: root

    property var heatmapModel: (typeof llamapet !== "undefined" && llamapet) ? llamapet.tokenHeatmap() : []
    readonly property int weeks: 52
    readonly property int daysPerWeek: 7
    readonly property int cellSize: 11
    readonly property int cellGap: 3
    readonly property int colWidth: cellSize + cellGap

    implicitWidth: parent ? parent.width : 480
    implicitHeight: mainCol.implicitHeight

    function formatNumber(num) {
        if (!num) return "0"
        return num.toLocaleString()
    }

    function formatDuration(sec) {
        if (!sec || sec <= 0) return qsTr("0m")
        var mins = Math.floor(sec / 60)
        var hours = Math.floor(mins / 60)
        var remMins = mins % 60
        if (hours > 0) {
            return qsTr("%1h %2m").arg(hours).arg(remMins)
        }
        return qsTr("%1m").arg(mins > 0 ? mins : 1)
    }

    function formatMonth(monthNum) {
        var isZh = Qt.locale().name.indexOf("zh") === 0;
        if (isZh) {
            return monthNum + "月";
        }
        var monthsEn = ["", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"];
        return (monthNum >= 1 && monthNum <= 12) ? monthsEn[monthNum] : "";
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

    function levelColor(level) {
        switch (level) {
        case 1: return "#1e3a5f"
        case 2: return "#2563eb"
        case 3: return "#60a5fa"
        case 4: return "#93c5fd"
        default: return "#1e1e2e"
        }
    }

    ColumnLayout {
        id: mainCol
        anchors.fill: parent
        spacing: 8

        // 1. Horizontally scrollable heatmap grid
        Flickable {
            id: gridFlick
            Layout.fillWidth: true
            Layout.preferredHeight: (daysPerWeek * (cellSize + cellGap)) + 26
            contentWidth: weeks * colWidth + 10
            contentHeight: height
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            Component.onCompleted: {
                Qt.callLater(function() {
                    gridFlick.contentX = Math.max(0, gridFlick.contentWidth - gridFlick.width)
                })
            }

            onWidthChanged: {
                if (gridFlick.contentWidth > gridFlick.width) {
                    gridFlick.contentX = Math.max(0, gridFlick.contentWidth - gridFlick.width)
                }
            }

            Item {
                id: gridContainer
                width: root.weeks * root.colWidth
                height: parent.height

                // 7 rows x 52 columns
                Row {
                    id: weeksRow
                    spacing: root.cellGap

                    Repeater {
                        model: root.weeks
                        delegate: Column {
                            id: weekCol
                            property int weekIdx: index
                            spacing: root.cellGap

                            Repeater {
                                model: root.daysPerWeek
                                delegate: Rectangle {
                                    id: cell
                                    property int dayIdx: index
                                    property int modelIdx: weekCol.weekIdx * root.daysPerWeek + dayIdx
                                    property var cellData: (root.heatmapModel && modelIdx < root.heatmapModel.length)
                                                           ? root.heatmapModel[modelIdx] : null

                                    width: root.cellSize
                                    height: root.cellSize
                                    radius: 2
                                    color: cellData ? root.levelColor(cellData.level) : "#1e1e2e"
                                    border.width: 1
                                    border.color: (cellData && cellData.level > 0) ? "transparent" : "#313244"

                                    MouseArea {
                                        id: cellMouse
                                        anchors.fill: parent
                                        hoverEnabled: true

                                        onEntered: {
                                            if (cell.cellData) {
                                                var pt = cell.mapToItem(root, cell.width / 2, 0)
                                                tooltip.show(pt.x, pt.y, cell.cellData)
                                            }
                                        }

                                        onExited: {
                                            tooltip.hide()
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // Month labels below grid
                Item {
                    id: monthLabels
                    anchors.top: weeksRow.bottom
                    anchors.topMargin: 4
                    width: parent.width
                    height: 18

                    Repeater {
                        model: root.weeks
                        delegate: Item {
                            property int weekIdx: index
                            property var firstDay: (root.heatmapModel && (weekIdx * 7) < root.heatmapModel.length)
                                                   ? root.heatmapModel[weekIdx * 7] : null
                            property var prevFirstDay: (root.heatmapModel && ((weekIdx - 1) * 7) >= 0 && ((weekIdx - 1) * 7) < root.heatmapModel.length)
                                                       ? root.heatmapModel[(weekIdx - 1) * 7] : null
                            property bool isNewMonth: firstDay && (!prevFirstDay || firstDay.month !== prevFirstDay.month)

                            x: weekIdx * root.colWidth
                            width: root.colWidth
                            height: 18
                            visible: isNewMonth

                            Text {
                                anchors.left: parent.left
                                anchors.verticalCenter: parent.verticalCenter
                                text: parent.firstDay ? root.formatMonth(parent.firstDay.month) : ""
                                color: Theme.fgMuted
                                font.pixelSize: 10
                                font.weight: Font.Medium
                            }
                        }
                    }
                }
            }
        }

        // 2. Bottom info & legend bar
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space2

            Text {
                text: qsTr("Hover over block for daily tokens and duration")
                color: Theme.fgMuted
                font.pixelSize: 11
                Layout.fillWidth: true
                elide: Text.ElideRight
            }

            RowLayout {
                spacing: 4

                Text {
                    text: qsTr("Less")
                    color: Theme.fgMuted
                    font.pixelSize: 11
                }

                Rectangle { width: 10; height: 10; radius: 2; color: "#1e1e2e"; border.width: 1; border.color: "#313244" }
                Rectangle { width: 10; height: 10; radius: 2; color: "#1e3a5f" }
                Rectangle { width: 10; height: 10; radius: 2; color: "#2563eb" }
                Rectangle { width: 10; height: 10; radius: 2; color: "#60a5fa" }
                Rectangle { width: 10; height: 10; radius: 2; color: "#93c5fd" }

                Text {
                    text: qsTr("More")
                    color: Theme.fgMuted
                    font.pixelSize: 11
                }
            }
        }
    }

    // 3. Floating Tooltip
    Rectangle {
        id: tooltip
        visible: false
        z: 100
        radius: Theme.radiusSm
        color: "#181825"
        border.color: "#45475a"
        border.width: 1

        width: tipCol.implicitWidth + 20
        height: tipCol.implicitHeight + 14

        property var currentData: null

        function show(targetX, targetY, data) {
            currentData = data
            // Tooltip placement: above cell, bounded horizontally within root
            var px = targetX - width / 2
            px = Math.max(6, Math.min(px, root.width - width - 6))
            var py = targetY - height - 8
            if (py < 0) {
                py = targetY + root.cellSize + 8
            }
            x = px
            y = py
            visible = true
        }

        function hide() {
            visible = false
        }

        ColumnLayout {
            id: tipCol
            anchors.centerIn: parent
            spacing: 2

            Text {
                text: tooltip.currentData ? root.formatDate(tooltip.currentData.date) : ""
                color: Theme.fgPrimary
                font.pixelSize: 11
                font.weight: Font.DemiBold
            }

            Text {
                text: {
                    if (!tooltip.currentData) return ""
                    if (tooltip.currentData.tokens > 0) {
                        return qsTr("%1 tokens · Active %2")
                               .arg(root.formatNumber(tooltip.currentData.tokens))
                               .arg(root.formatDuration(tooltip.currentData.activeSeconds))
                    }
                    return qsTr("No inference activity")
                }
                color: (tooltip.currentData && tooltip.currentData.tokens > 0) ? "#00F0FF" : Theme.fgMuted
                font.pixelSize: 11
                font.family: Theme.fontMono
                font.weight: Font.Medium
            }

            Text {
                visible: tooltip.currentData && tooltip.currentData.tokens > 0
                text: {
                    if (!tooltip.currentData || tooltip.currentData.tokens <= 0) return ""
                    return qsTr("Prompt: %1 / Decoded: %2")
                           .arg(root.formatNumber(tooltip.currentData.promptTokens))
                           .arg(root.formatNumber(tooltip.currentData.completionTokens))
                }
                color: Theme.fgSecondary
                font.pixelSize: 10
                font.family: Theme.fontMono
            }
        }
    }
}

