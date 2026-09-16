// SettingsPluginsPage — host "Plugins" management page.
// Allows users to view all discovered plugins and dynamically load/unload
// them live without restarting the application.
// Changes persist to settings.json via plugins.<id>.enabled.

import QtQuick
import QtQuick.Layouts
import Margin.Ui.Primitives

Rectangle {
    id: root
    objectName: "settingsPluginsPage"
    color: Theme.bgBase

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        // ── Header ───────────────────────────────────────────────────
        ColumnLayout {
            spacing: 4
            Layout.fillWidth: true

            Text {
                text: qsTr("Plugins")
                color: Theme.fgPrimary
                font.pixelSize: Theme.textLg
                font.weight: Font.DemiBold
            }

            Text {
                text: qsTr("Manage installed plugins and dynamic loading.")
                color: Theme.fgMuted
                font.pixelSize: Theme.textSm
            }
        }

        // ── Plugin Cards List (Scrollable) ───────────────────────────
        Flickable {
            id: flickable
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: width
            contentHeight: cardsColumn.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ColumnLayout {
                id: cardsColumn
                width: flickable.width
                spacing: 10

                Repeater {
                    model: (typeof pluginManager !== "undefined" && pluginManager)
                           ? pluginManager.plugins : []

                    Rectangle {
                        objectName: "pluginCard_" + modelData.id
                        Layout.fillWidth: true
                        implicitHeight: cardContent.implicitHeight + 24
                        color: Theme.bgElevated
                        radius: Theme.radiusSm
                        border.color: modelData.isLoaded ? Theme.borderStrong : Theme.borderSubtle
                        border.width: 1

                        RowLayout {
                            id: cardContent
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 12
                            spacing: 12

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4

                                RowLayout {
                                    spacing: 8
                                    Layout.fillWidth: true

                                    Text {
                                        text: modelData.name ? modelData.name : modelData.id
                                        color: Theme.fgPrimary
                                        font.pixelSize: Theme.textSm
                                        font.weight: Font.DemiBold
                                    }

                                    // Version pill
                                    Rectangle {
                                        implicitWidth: verText.implicitWidth + 8
                                        implicitHeight: 18
                                        radius: 9
                                        color: Theme.bgHover

                                        Text {
                                            id: verText
                                            anchors.centerIn: parent
                                            text: "v" + (modelData.version ? modelData.version : "0.1.0")
                                            color: Theme.fgMuted
                                            font.pixelSize: Theme.textXs
                                        }
                                    }

                                    // Status pill
                                    RowLayout {
                                        spacing: 4
                                        Rectangle {
                                            width: 6
                                            height: 6
                                            radius: 3
                                            color: modelData.isLoaded ? "#10B981" : Theme.fgMuted
                                        }
                                        Text {
                                            text: modelData.isLoaded ? qsTr("Active") : qsTr("Disabled")
                                            color: modelData.isLoaded ? "#10B981" : Theme.fgMuted
                                            font.pixelSize: Theme.textXs
                                        }
                                    }
                                }

                                Text {
                                    text: modelData.description ? modelData.description : qsTr("No description available.")
                                    color: Theme.fgMuted
                                    font.pixelSize: Theme.textXs
                                    wrapMode: Text.WordWrap
                                    Layout.fillWidth: true
                                }

                                // Permissions list
                                RowLayout {
                                    visible: modelData.permissions && modelData.permissions.length > 0
                                    spacing: 4
                                    Layout.topMargin: 2

                                    Repeater {
                                        model: modelData.permissions ? modelData.permissions : []
                                        Rectangle {
                                            implicitWidth: permText.implicitWidth + 6
                                            implicitHeight: 16
                                            radius: 3
                                            color: Theme.bgBase
                                            border.color: Theme.borderSubtle
                                            border.width: 1

                                            Text {
                                                id: permText
                                                anchors.centerIn: parent
                                                text: modelData
                                                color: Theme.fgMuted
                                                font.pixelSize: 10
                                            }
                                        }
                                    }
                                }
                            }

                            // Toggle switch
                            MSwitch {
                                objectName: "pluginSwitch_" + modelData.id
                                checked: modelData.isEnabled
                                onToggled: function(state) {
                                    if (typeof pluginManager !== "undefined" && pluginManager) {
                                        pluginManager.setPluginEnabled(modelData.id, state);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

