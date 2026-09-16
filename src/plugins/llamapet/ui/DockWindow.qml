import QtQuick
import QtQuick.Window
import "components"

Window {
    id: win

    width: 260
    height: 50
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool

    Rectangle {
        id: card
        anchors.fill: parent
        radius: 8
        color: "#181825"
        border.color: "#313244"
        border.width: 1

        Row {
            anchors.fill: parent
            anchors.margins: 5
            spacing: 6

            // 左侧：40x40 微型表情精怪
            EmotionSprite {
                id: sprite
                width: 40
                height: 40
                anchors.verticalCenter: parent.verticalCenter
                emotion: (typeof telemetryService !== "undefined" && telemetryService) ? telemetryService.emotion : 0
            }

            // 右侧：指标面板
            Column {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - 40 - 12
                spacing: 4

                // 第一行：显存文字 + TPS
                Row {
                    width: parent.width
                    spacing: 8

                    MetricValue {
                        label: "VRAM"
                        value: (typeof telemetryService !== "undefined" && telemetryService) ?
                               (telemetryService.vramUsedMb / 1024.0).toFixed(1) + "/" + (telemetryService.vramTotalMb / 1024.0).toFixed(0) : "0/0"
                        unit: "G"
                        valueColor: (typeof telemetryService !== "undefined" && telemetryService && telemetryService.vramPercent >= 95) ? "#FF3366" :
                                    ((typeof telemetryService !== "undefined" && telemetryService && telemetryService.vramPercent >= 85) ? "#FFAA00" : "#F3CBA5")
                    }

                    Item { width: 1; height: 1 } // 占位弹性空间

                    MetricValue {
                        label: "TPS"
                        value: (typeof telemetryService !== "undefined" && telemetryService) ? telemetryService.currentTps.toFixed(1) : "0.0"
                        unit: "t/s"
                        valueColor: "#00F0FF"
                    }
                }

                // 第二行：显存进度条 + 槽位灯
                Row {
                    width: parent.width
                    spacing: 8

                    MiniProgress {
                        width: parent.width - 50
                        height: 4
                        anchors.verticalCenter: parent.verticalCenter
                        value: (typeof telemetryService !== "undefined" && telemetryService) ? telemetryService.vramPercent : 0.0
                    }

                    SlotLights {
                        anchors.verticalCenter: parent.verticalCenter
                        activeSlots: (typeof telemetryService !== "undefined" && telemetryService) ? telemetryService.activeSlots : 0
                        totalSlots: (typeof telemetryService !== "undefined" && telemetryService) ? telemetryService.totalSlots : 4
                    }
                }
            }
        }

        MouseArea {
            id: mouseArea
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            hoverEnabled: true

            property point clickPos: Qt.point(0, 0)
            property bool moved: false

            onEntered: {
                if (typeof floatingWindows !== "undefined") {
                    floatingWindows.handleMouseEnter(win);
                }
            }

            onExited: {
                if (typeof floatingWindows !== "undefined") {
                    floatingWindows.handleMouseLeave(win);
                }
            }

            onPressed: function(mouse) {
                if (mouse.button === Qt.LeftButton) {
                    clickPos = Qt.point(mouse.x, mouse.y);
                    moved = false;
                    if (typeof floatingWindows !== "undefined") {
                        floatingWindows.startSystemMove(win);
                    }
                } else if (mouse.button === Qt.RightButton) {
                    if (typeof floatingWindows !== "undefined") {
                        floatingWindows.showContextMenu(win);
                    }
                }
            }

            onPositionChanged: function(mouse) {
                if (Math.abs(mouse.x - clickPos.x) > 3 || Math.abs(mouse.y - clickPos.y) > 3) {
                    moved = true;
                }
            }

            onClicked: function(mouse) {
                if (mouse.button === Qt.LeftButton && !moved) {
                    sprite.triggerHeart();
                }
            }

            onDoubleClicked: function(mouse) {
                if (mouse.button === Qt.LeftButton && typeof floatingWindows !== "undefined") {
                    floatingWindows.switchForm(0); // 切换回 MiniPet
                }
            }
        }
    }
}
