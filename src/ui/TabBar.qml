// TabBar — main-panel tab strip (docs/06 §4.1, height 48).
// Styled as a floating capsule matching the mockup.
// Active tab: Theme.accentBrand (purple) icon + 15% opacity purple background.
// Inactive tab: Theme.fgSecondary icon, no background (subtle hover state).
//
// SVG icons use currentColor; MultiEffect recolors each delegate's Image to
// the active/inactive color.

import QtQuick
import QtQuick.Effects
import Margin.Ui.Primitives

Rectangle {
    id: root
    objectName: "tabBar"
    implicitHeight: 56
    color: "transparent" // Floating style has no solid full-width background

    property int currentTab: 0
    signal tabActivated(int index)

    readonly property int _tabCount: dashboardTabs ? dashboardTabs.tabs.length : 0

    Rectangle {
        id: capsule
        anchors {
            left: parent.left
            right: parent.right
            leftMargin: Theme.space4
            rightMargin: Theme.space4
            top: parent.top
            bottom: parent.bottom
            topMargin: Theme.space2
            bottomMargin: Theme.space2
        }
        color: Theme.bgElevated
        radius: Theme.radiusMd
        border.color: Theme.borderSubtle
        border.width: 1

        Row {
            anchors.fill: parent

            Repeater {
                model: dashboardTabs ? dashboardTabs.tabs : []

                delegate: MouseArea {
                    id: mouseArea
                    width: root._tabCount > 0 ? parent.width / root._tabCount : 0
                    height: parent.height
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true

                    // Propagate tab clicks up via tabActivated
                    onClicked: root.tabActivated(index)

                    // Rounded highlight background (selection/hover)
                    Rectangle {
                        anchors {
                            fill: parent
                            margins: 4
                        }
                        color: root.currentTab === index ? Theme.accentBrand : Theme.bgHover
                        opacity: root.currentTab === index ? 0.15 : (mouseArea.containsMouse ? 1.0 : 0.0)
                        radius: 6

                        Behavior on opacity {
                            NumberAnimation { duration: Theme.durationFast }
                        }
                        Behavior on color {
                            ColorAnimation { duration: Theme.durationFast }
                        }
                    }

                    Image {
                        id: tabIcon
                        anchors.centerIn: parent
                        visible: false
                        source: modelData.icon
                        sourceSize: Qt.size(18, 18)
                    }

                    MultiEffect {
                        anchors.fill: tabIcon
                        source: tabIcon
                        colorizationColor: root.currentTab === index
                                           ? Theme.accentBrand
                                           : Theme.fgSecondary
                        colorization: 1.0
                    }
                }
            }
        }
    }
}
