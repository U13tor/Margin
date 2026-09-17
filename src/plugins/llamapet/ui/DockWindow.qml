import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import "components"

Window {
    id: win

    width: 280
    height: 50
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool

    function openDetail() {
        if (typeof llamapet !== "undefined" && llamapet) {
            llamapet.openDetail();
        } else if (typeof dashboardRoot !== "undefined" && dashboardRoot) {
            dashboardRoot.openDashboard("llamapet");
        }
    }

    PetContextMenu {
        id: contextMenu
        currentForm: 1
    }

    Rectangle {
        id: card
        anchors.fill: parent
        radius: 8
        color: "#181825"
        border.color: "#313244"
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.margins: 6
            spacing: 8

            // 左侧：38x38 微型表情精怪
            EmotionSprite {
                id: sprite
                Layout.preferredWidth: 38
                Layout.preferredHeight: 38
                Layout.alignment: Qt.AlignVCenter
                emotion: (typeof telemetryService !== "undefined" && telemetryService) ? telemetryService.emotion : 0
            }

            // 右侧：指标面板（弹性撑满剩余宽度）
            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                spacing: 3

                // 第一行：显存文字 + 弹性空白 + TPS（彻底杜绝截断）
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    MetricValue {
                        label: "VRAM"
                        value: (typeof telemetryService !== "undefined" && telemetryService) ?
                               (telemetryService.vramUsedMb / 1024.0).toFixed(1) + "/" + (telemetryService.vramTotalMb / 1024.0).toFixed(0) : "0/0"
                        unit: "G"
                        valueColor: (typeof telemetryService !== "undefined" && telemetryService && telemetryService.vramPercent >= 95) ? "#FF3366" :
                                    ((typeof telemetryService !== "undefined" && telemetryService && telemetryService.vramPercent >= 85) ? "#FFAA00" : "#F3CBA5")
                    }

                    Item {
                        Layout.fillWidth: true // 弹性间隔，自适应各屏幕比例与字宽
                    }

                    MetricValue {
                        label: "TPS"
                        value: (typeof telemetryService !== "undefined" && telemetryService) ? telemetryService.currentTps.toFixed(1) : "0.0"
                        unit: "t/s"
                        valueColor: "#00F0FF"
                    }
                }

                // 第二行：显存进度条 + 槽位灯
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    MiniProgress {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 4
                        Layout.alignment: Qt.AlignVCenter
                        value: (typeof telemetryService !== "undefined" && telemetryService) ? telemetryService.vramPercent : 0.0
                    }

                    SlotLights {
                        Layout.alignment: Qt.AlignVCenter
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
                    contextMenu.popupFor(win);
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
                if (mouse.button === Qt.LeftButton) {
                    win.openDetail();
                }
            }
        }

        // ── 悬停快捷按钮层 ──────────────────────────────────────────
        Item {
            id: hoverControls
            anchors.fill: parent
            opacity: (mouseArea.containsMouse || detailBtnHover.containsMouse || switchBtnHover.containsMouse) ? 1.0 : 0.0

            Behavior on opacity {
                NumberAnimation { duration: 150 }
            }

            Row {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 4
                spacing: 4

                // 切换为 MiniPet 桌面宠物
                Rectangle {
                    width: 18
                    height: 18
                    radius: 9
                    color: switchBtnHover.containsMouse ? "#89B4FA" : "#CC1E1E2E"
                    border.color: "#8045475A"
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "⇄"
                        color: switchBtnHover.containsMouse ? "#11111B" : "#CDD6F4"
                        font.pixelSize: 10
                    }

                    MouseArea {
                        id: switchBtnHover
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (typeof floatingWindows !== "undefined") {
                                floatingWindows.switchForm(0);
                            }
                        }
                    }
                }

                // 打开详情界面
                Rectangle {
                    width: 18
                    height: 18
                    radius: 9
                    color: detailBtnHover.containsMouse ? "#7EA6E0" : "#CC1E1E2E"
                    border.color: "#8045475A"
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "↗"
                        color: detailBtnHover.containsMouse ? "#11111B" : "#CDD6F4"
                        font.pixelSize: 11
                        font.bold: true
                    }

                    MouseArea {
                        id: detailBtnHover
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            win.openDetail();
                        }
                    }
                }
            }
        }
    }
}
