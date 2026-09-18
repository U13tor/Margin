import QtQuick
import QtQuick.Window
import QtQuick.Layouts

Window {
    id: menuWin

    width: 180
    height: menuCol.implicitHeight + 12
    color: "transparent"
    flags: Qt.Popup | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint

    property int currentForm: (typeof floatingWindows !== "undefined" && floatingWindows)
                              ? floatingWindows.currentForm : 0

    function popupFor(targetWin) {
        if (!targetWin) return;

        // 获取浮窗所在显示器的可用几何矩形（排除任务栏，适配双屏/多屏虚拟坐标偏移）
        var sRect = (targetWin.screen && targetWin.screen.availableGeometry)
                    ? targetWin.screen.availableGeometry
                    : { x: 0, y: 0, width: Screen.desktopAvailableWidth, height: Screen.desktopAvailableHeight };

        var sLeft = sRect.x;
        var sTop = sRect.y;
        var sRight = sRect.x + sRect.width;
        var sBottom = sRect.y + sRect.height;

        var winX = targetWin.x;
        var winY = targetWin.y;
        var winW = targetWin.width;
        var winH = targetWin.height;

        var menuW = menuWin.width;
        var menuH = menuWin.height;
        var margin = 6;

        // 1. 垂直 Y 轴决策：默认置于浮窗左下角下方，不遮挡浮窗；若下方空间不足则向上翻转
        var targetY = winY + winH + margin;
        if (targetY + menuH > sBottom - 4) {
            if (winY - margin - menuH >= sTop + 4) {
                targetY = winY - margin - menuH; // 翻转到浮窗上方
            } else {
                var spaceBelow = sBottom - (winY + winH);
                var spaceAbove = winY - sTop;
                if (spaceAbove > spaceBelow) {
                    targetY = Math.max(sTop + 4, winY - margin - menuH);
                } else {
                    targetY = Math.min(sBottom - 4 - menuH, winY + winH + margin);
                }
            }
        }

        // 2. 水平 X 轴决策：首选与浮窗左边缘对齐（即位于浮窗左下角）
        var targetX = winX;
        // 防超出当前屏幕右边缘（若靠右，先尝试与浮窗右边缘对齐，再贴紧当前屏幕右边缘）
        if (targetX + menuW > sRight - 4) {
            targetX = winX + winW - menuW;
            if (targetX + menuW > sRight - 4) {
                targetX = sRight - 4 - menuW;
            }
        }
        // 防超出当前屏幕左边缘（防止跨屏到左侧副屏）
        if (targetX < sLeft + 4) {
            targetX = sLeft + 4;
        }

        menuWin.x = targetX;
        menuWin.y = targetY;
        menuWin.show();
        menuWin.requestActivate();
    }

    function popup(globalPos) {
        if (!globalPos) return;
        menuWin.x = globalPos.x;
        menuWin.y = globalPos.y;
        menuWin.show();
        menuWin.requestActivate();
    }

    Rectangle {
        id: bgCard
        anchors.fill: parent
        radius: 8
        color: "#F0181825"
        border.color: "#50313244"
        border.width: 1

        ColumnLayout {
            id: menuCol
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 6
            spacing: 2

            // 1. 打开仪表盘详情（主操作，加粗高亮）
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 28
                radius: 4
                color: itemHover1.containsMouse ? "#307EA6E0" : "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 8

                    Text {
                        text: "❖"
                        color: "#7EA6E0"
                        font.pixelSize: 13
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("打开仪表盘详情")
                        color: itemHover1.containsMouse ? "#FFFFFF" : "#CDD6F4"
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                    }
                }

                MouseArea {
                    id: itemHover1
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        menuWin.hide();
                        if (typeof llamapet !== "undefined" && llamapet) {
                            llamapet.openDetail();
                        } else if (typeof dashboardRoot !== "undefined" && dashboardRoot) {
                            dashboardRoot.openDashboard("llamapet");
                        }
                    }
                }
            }

            // 2. 形态切换 (MiniPet ↔ Dock)
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 28
                radius: 4
                color: itemHover2.containsMouse ? "#2089B4FA" : "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 8

                    Text {
                        text: "⇄"
                        color: "#89B4FA"
                        font.pixelSize: 13
                    }

                    Text {
                        Layout.fillWidth: true
                        text: menuWin.currentForm === 0 ? qsTr("切换至 Dock 状态栏") : qsTr("切换至 MiniPet 桌面宠物")
                        color: itemHover2.containsMouse ? "#FFFFFF" : "#BAC2DE"
                        font.pixelSize: 12
                    }
                }

                MouseArea {
                    id: itemHover2
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        menuWin.hide();
                        if (typeof floatingWindows !== "undefined") {
                            floatingWindows.switchForm(menuWin.currentForm === 0 ? 1 : 0);
                        }
                    }
                }
            }

            // 3. 插件设置
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 28
                radius: 4
                color: itemHover3.containsMouse ? "#2089B4FA" : "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 8

                    Text {
                        text: "⚙"
                        color: "#A6ADC8"
                        font.pixelSize: 13
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("LlamaPet 设置")
                        color: itemHover3.containsMouse ? "#FFFFFF" : "#BAC2DE"
                        font.pixelSize: 12
                    }
                }

                MouseArea {
                    id: itemHover3
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        menuWin.hide();
                        if (typeof llamapet !== "undefined" && llamapet) {
                            llamapet.openSettings();
                        } else if (typeof settingsRoot !== "undefined" && settingsRoot) {
                            settingsRoot.openSettings("llamapet");
                        }
                    }
                }
            }

            // 分割线
            Rectangle {
                Layout.fillWidth: true
                Layout.topMargin: 2
                Layout.bottomMargin: 2
                height: 1
                color: "#313244"
            }

            // 4. 总在最前 (Always on Top)
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 26
                radius: 4
                color: itemHover4.containsMouse ? "#2089B4FA" : "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 8

                    Text {
                        text: (typeof floatingWindows !== "undefined" && floatingWindows && floatingWindows.miniPetWindow
                               && (floatingWindows.miniPetWindow.flags & Qt.WindowStaysOnTopHint)) ? "☑" : "☐"
                        color: "#89DCEB"
                        font.pixelSize: 12
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("总在最前")
                        color: itemHover4.containsMouse ? "#FFFFFF" : "#BAC2DE"
                        font.pixelSize: 12
                    }
                }

                MouseArea {
                    id: itemHover4
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        menuWin.hide();
                        if (typeof llamapet !== "undefined" && llamapet) {
                            var currentOnTop = (floatingWindows.miniPetWindow.flags & Qt.WindowStaysOnTopHint) !== 0;
                            llamapet.setAlwaysOnTop(!currentOnTop);
                        }
                    }
                }
            }

            // 5. 鼠标穿透 (Click-through)
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 26
                radius: 4
                color: itemHover5.containsMouse ? "#2089B4FA" : "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 8

                    Text {
                        text: (typeof floatingWindows !== "undefined" && floatingWindows && floatingWindows.isClickThrough()) ? "☑" : "☐"
                        color: "#FAB387"
                        font.pixelSize: 12
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("鼠标穿透")
                        color: itemHover5.containsMouse ? "#FFFFFF" : "#BAC2DE"
                        font.pixelSize: 12
                    }
                }

                MouseArea {
                    id: itemHover5
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        menuWin.hide();
                        if (typeof llamapet !== "undefined" && llamapet && typeof floatingWindows !== "undefined") {
                            llamapet.setClickThrough(!floatingWindows.isClickThrough());
                        }
                    }
                }
            }

            // 分割线
            Rectangle {
                Layout.fillWidth: true
                Layout.topMargin: 2
                Layout.bottomMargin: 2
                height: 1
                color: "#313244"
            }

            // 6. 隐藏 (Hide)
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 26
                radius: 4
                color: itemHover6.containsMouse ? "#30F38BA8" : "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 8

                    Text {
                        text: "✕"
                        color: "#F38BA8"
                        font.pixelSize: 11
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("隐藏悬浮窗")
                        color: itemHover6.containsMouse ? "#FFFFFF" : "#F38BA8"
                        font.pixelSize: 12
                    }
                }

                MouseArea {
                    id: itemHover6
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        menuWin.hide();
                        if (typeof floatingWindows !== "undefined") {
                            floatingWindows.toggleVisibility();
                        }
                    }
                }
            }
        }
    }
}

