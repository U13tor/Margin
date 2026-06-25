// SettingsAppearancePage — host "Appearance" settings page (docs/06 §4.6,
// M5-C4d). Dark is the only supported theme in v1.0; Light + System are
// shown as disabled segmented buttons with a "v1.1" tag.
//
// Density and accent color customization deferred to v1.1 (docs/12).

import QtQuick
import QtQuick.Layouts
import Margin.Ui.Primitives

Rectangle {
    objectName: "settingsAppearancePage"
    color: Theme.bgBase

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 12

        Text {
            text: qsTr("Appearance")
            color: Theme.fgPrimary
            font.pixelSize: Theme.textLg
            font.weight: Font.DemiBold
        }

        Text {
            text: qsTr("Theme and density.")
            color: Theme.fgMuted
            font.pixelSize: Theme.textSm
        }

        Text {
            text: qsTr("Theme")
            color: Theme.fgSecondary
            font.pixelSize: Theme.textSm
            font.weight: Font.DemiBold
            Layout.topMargin: 12
        }

        RowLayout {
            objectName: "themeRow"
            Layout.fillWidth: true
            spacing: 4

            MButton {
                objectName: "themeButton_dark"
                variant: MButton.Variant.Primary
                text: qsTr("Dark")
            }

            MButton {
                objectName: "themeButton_light"
                variant: MButton.Variant.Ghost
                text: qsTr("Light")
                enabled: false

                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 4
                    anchors.top: parent.top
                    anchors.topMargin: 2
                    text: qsTr("v1.1")
                    color: Theme.fgMuted
                    font.pixelSize: Theme.textXs
                }
            }

            MButton {
                objectName: "themeButton_system"
                variant: MButton.Variant.Ghost
                text: qsTr("System")
                enabled: false
            }
        }

        Text {
            text: qsTr("Dark is the only theme in v1.0. Light and System themes land in v1.1 (the design tokens are built for both, but the swap machinery + theme-dark-only assets aren't wired yet).")
            color: Theme.fgMuted
            font.pixelSize: Theme.textXs
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Text {
            text: qsTr("Density")
            color: Theme.fgSecondary
            font.pixelSize: Theme.textSm
            font.weight: Font.DemiBold
            Layout.topMargin: 16
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: qsTr("Compact mode")
                color: Theme.fgPrimary
                font.pixelSize: Theme.textSm
                Layout.fillWidth: true
            }
            Text {
                text: qsTr("v1.1")
                color: Theme.fgMuted
                font.pixelSize: Theme.textXs
            }
            MButton {
                variant: MButton.Variant.Ghost
                text: qsTr("Off")
                enabled: false
            }
        }

        Item { Layout.fillHeight: true }
    }
}
