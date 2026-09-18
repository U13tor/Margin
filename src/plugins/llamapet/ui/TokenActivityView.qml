import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Margin.Ui.Primitives
import "components"

ColumnLayout {
    id: root
    spacing: Theme.space3
    width: parent ? parent.width : 480

    property var summaryData: (typeof llamapet !== "undefined" && llamapet) ? llamapet.tokenSummary() : null

    function formatTokenCount(num) {
        if (!num || num <= 0) return "0"
        var isZh = Qt.locale().name.indexOf("zh") === 0;
        if (isZh) {
            if (num >= 100000000) return (num / 100000000.0).toFixed(1) + " 亿";
            if (num >= 10000) return (num / 10000.0).toFixed(1) + " 万";
        } else {
            if (num >= 1000000000) return (num / 1000000000.0).toFixed(1) + " B";
            if (num >= 1000000) return (num / 1000000.0).toFixed(1) + " M";
            if (num >= 1000) return (num / 1000.0).toFixed(1) + " K";
        }
        return num.toLocaleString()
    }

    function formatDuration(sec) {
        if (!sec || sec <= 0) return qsTr("0h")
        var totalMins = Math.floor(sec / 60)
        var days = Math.floor(totalMins / (24 * 60))
        var remMins = totalMins % (24 * 60)
        var hours = Math.floor(remMins / 60)
        var mins = remMins % 60

        if (days > 0) {
            return qsTr("%1d %2h").arg(days).arg(hours)
        }
        if (hours > 0) {
            return qsTr("%1h %2m").arg(hours).arg(mins)
        }
        return qsTr("%1m").arg(mins > 0 ? mins : 1)
    }

    function refreshData() {
        if (typeof llamapet !== "undefined" && llamapet) {
            root.summaryData = llamapet.tokenSummary()
            heatmapComponent.heatmapModel = llamapet.tokenHeatmap()
            trendComponent.trendData = llamapet.tokenTrends(trendComponent.trendDays)
        }
    }

    Timer {
        interval: 5000
        running: true
        repeat: true
        onTriggered: root.refreshData()
    }

    // 1. 活跃度概览卡片 (5 Cards)
    MCard {
        id: summaryCard
        Layout.fillWidth: true
        implicitHeight: sumCol.implicitHeight + 2 * padding

        ColumnLayout {
            id: sumCol
            anchors.fill: parent
            spacing: Theme.space2

            // Title & Status Row
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.space2

                Text {
                    text: qsTr("Activity")
                    color: Theme.fgPrimary
                    font.pixelSize: Theme.textSm
                    font.weight: Font.DemiBold
                }

                Text {
                    text: qsTr("(Local model inference statistics)")
                    color: Theme.fgMuted
                    font.pixelSize: 11
                }

                Item { Layout.fillWidth: true }

                RowLayout {
                    spacing: 4
                    visible: (typeof llamapet !== "undefined" && llamapet && llamapet.isUsingMetrics())

                    Rectangle {
                        width: 6
                        height: 6
                        radius: 3
                        color: "#00FF88"
                    }

                    Text {
                        text: qsTr("High-precision /metrics")
                        color: Theme.fgMuted
                        font.pixelSize: 10
                    }
                }
            }

            // 5 Metrics Row
            RowLayout {
                Layout.fillWidth: true
                spacing: 0

                // 1. 累计 Token 数
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Text {
                        text: root.formatTokenCount(root.summaryData ? root.summaryData.totalTokens : 0)
                        color: Theme.fgPrimary
                        font.pixelSize: 16
                        font.weight: Font.Bold
                        font.family: Theme.fontMono
                    }
                    Text {
                        text: qsTr("Total Tokens")
                        color: Theme.fgSecondary
                        font.pixelSize: 11
                    }
                }

                Rectangle { width: 1; height: 32; color: "#313244"; Layout.rightMargin: 8 }

                // 2. 峰值 Token 数
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Text {
                        text: root.formatTokenCount(root.summaryData ? root.summaryData.peakTokens : 0)
                        color: "#00F0FF"
                        font.pixelSize: 16
                        font.weight: Font.Bold
                        font.family: Theme.fontMono
                    }
                    RowLayout {
                        spacing: 2
                        Text {
                            text: qsTr("Peak Tokens")
                            color: Theme.fgSecondary
                            font.pixelSize: 11
                        }
                        Text {
                            text: "ⓘ"
                            color: Theme.fgMuted
                            font.pixelSize: 11
                        }
                    }
                }

                Rectangle { width: 1; height: 32; color: "#313244"; Layout.rightMargin: 8 }

                // 3. 累计使用时长
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Text {
                        text: root.formatDuration(root.summaryData ? root.summaryData.totalActiveSeconds : 0)
                        color: "#00FF88"
                        font.pixelSize: 16
                        font.weight: Font.Bold
                        font.family: Theme.fontMono
                    }
                    Text {
                        text: qsTr("Total Active Time")
                        color: Theme.fgSecondary
                        font.pixelSize: 11
                    }
                }

                Rectangle { width: 1; height: 32; color: "#313244"; Layout.rightMargin: 8 }

                // 4. 当前连续天数
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Text {
                        text: qsTr("%1 Days").arg(root.summaryData ? root.summaryData.currentStreak : 0)
                        color: Theme.fgPrimary
                        font.pixelSize: 16
                        font.weight: Font.Bold
                        font.family: Theme.fontMono
                    }
                    Text {
                        text: qsTr("Current Streak")
                        color: Theme.fgSecondary
                        font.pixelSize: 11
                    }
                }

                Rectangle { width: 1; height: 32; color: "#313244"; Layout.rightMargin: 8 }

                // 5. 最长连续天数
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Text {
                        text: qsTr("%1 Days").arg(root.summaryData ? root.summaryData.longestStreak : 0)
                        color: Theme.fgPrimary
                        font.pixelSize: 16
                        font.weight: Font.Bold
                        font.family: Theme.fontMono
                    }
                    Text {
                        text: qsTr("Longest Streak")
                        color: Theme.fgSecondary
                        font.pixelSize: 11
                    }
                }
            }
        }
    }

    // 2. Token 活动卡片 (GitHub 风格热力图)
    MCard {
        id: heatmapCard
        Layout.fillWidth: true
        implicitHeight: heatCol.implicitHeight + 2 * padding

        ColumnLayout {
            id: heatCol
            anchors.fill: parent
            spacing: Theme.space2

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.space2

                Text {
                    text: qsTr("Token Activity")
                    color: Theme.fgPrimary
                    font.pixelSize: Theme.textSm
                    font.weight: Font.DemiBold
                }

                Text {
                    text: qsTr("Past 52 weeks generation distribution")
                    color: Theme.fgMuted
                    font.pixelSize: 11
                }

                Item { Layout.fillWidth: true }
            }

            TokenHeatmap {
                id: heatmapComponent
                Layout.fillWidth: true
            }
        }
    }

    // 3. 用量趋势卡片
    MCard {
        id: trendCard
        Layout.fillWidth: true
        implicitHeight: trendCol.implicitHeight + 2 * padding

        ColumnLayout {
            id: trendCol
            anchors.fill: parent
            spacing: Theme.space2

            TokenTrendChart {
                id: trendComponent
                Layout.fillWidth: true
            }
        }
    }
}

