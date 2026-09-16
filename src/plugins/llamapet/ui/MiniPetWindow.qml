import QtQuick
import QtQuick.Window
import "components"

Window {
    id: win

    width: 96
    height: 96
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool

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
                    floatingWindows.switchForm(1); // 切换至 Dock
                }
            }
        }
    }
}
