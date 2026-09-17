import QtQuick
import QtQuick.Window
import "components"

Window {
    id: win

    width: 96
    height: 96
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
        currentForm: 0
    }

    Item {
        id: container
        anchors.fill: parent

        Rectangle {
            id: bgCard
            anchors.fill: parent
            radius: 16
            color: "#30181825"
            border.color: mouseArea.containsMouse ? "#807EA6E0" : "#40313244"
            border.width: 1

            Behavior on border.color {
                ColorAnimation { duration: 150 }
            }
        }

        EmotionSprite {
            id: sprite
            anchors.fill: parent
            anchors.margins: 4
            emotion: (typeof telemetryService !== "undefined" && telemetryService) ? telemetryService.emotion : 0
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

            // 左上角：切换为 Dock 状态栏
            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.margins: 6
                width: 20
                height: 20
                radius: 10
                color: switchBtnHover.containsMouse ? "#89B4FA" : "#CC1E1E2E"
                border.color: "#8045475A"
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "⇄"
                    color: switchBtnHover.containsMouse ? "#11111B" : "#CDD6F4"
                    font.pixelSize: 11
                }

                MouseArea {
                    id: switchBtnHover
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (typeof floatingWindows !== "undefined") {
                            floatingWindows.switchForm(1);
                        }
                    }
                }
            }

            // 右上角：打开详情界面
            Rectangle {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 6
                width: 20
                height: 20
                radius: 10
                color: detailBtnHover.containsMouse ? "#7EA6E0" : "#CC1E1E2E"
                border.color: "#8045475A"
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "↗"
                    color: detailBtnHover.containsMouse ? "#11111B" : "#CDD6F4"
                    font.pixelSize: 12
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
