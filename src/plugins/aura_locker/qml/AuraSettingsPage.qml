// AuraSettingsPage — scan, device picker, threshold sliders.
// Routes all actions through the `aura` context property exposed by
// AuraLockerPlugin.
//
// M5-C4a: page is now loaded by SettingsWindow.qml's StackLayout Repeater
// via qrc:/aura/qml/AuraSettingsPage.qml (registered as a Contributor in
// manifest). The M4 StackView-based Back button + QtQuick.Controls import
// are gone — the Settings window itself is the top-level container, so
// pop()'ing would have no meaningful target.
//
// Root is an Item (not ColumnLayout) so the page can be loaded inside a
// StackLayout without anchor conflicts; the inner ColumnLayout then fills
// that Item and adds the margins.

import QtQuick
import QtQuick.Layouts
import Margin.Ui.Primitives

Item {
    id: root

    width: parent ? parent.width : 640
    height: parent ? parent.height : 480

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Text {
            text: qsTr("Aura Settings")
            color: Theme.fgPrimary
            font.pixelSize: Theme.textBase
            font.weight: Font.DemiBold
        }

        // --- Paired device row -----------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: aura.pairedDeviceName.length > 0
                      ? qsTr("Paired: %1").arg(aura.pairedDeviceName)
                      : qsTr("Not paired")
                color: Theme.fgSecondary
                font.pixelSize: Theme.textSm
                Layout.fillWidth: true
            }

            MButton {
                objectName: "unpairButton"
                variant: MButton.Variant.Ghost
                iconSource: "qrc:/icons/icon-trash.svg"
                text: qsTr("Unpair")
                enabled: aura.pairedDeviceName.length > 0
                onClicked: aura.unpair()
            }
        }

        // --- Scan controls + device list -------------------------------------
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            MButton {
                objectName: "scanButton"
                variant: MButton.Variant.Primary
                iconSource: "qrc:/icons/icon-bluetooth.svg"
                text: aura.scanning ? qsTr("Scanning…") : qsTr("Scan for devices")
                enabled: !aura.scanning
                onClicked: aura.startScan()
            }

            MButton {
                objectName: "stopButton"
                variant: MButton.Variant.Secondary
                iconSource: "qrc:/icons/icon-close.svg"
                text: qsTr("Stop")
                enabled: aura.scanning
                onClicked: aura.stopScan()
            }

            Text {
                text: aura.scanning ? qsTr("Listening for BLE advertisements (%1 s window)").arg(aura.scanDurationSec)
                                    : qsTr("Click Scan to discover nearby BLE devices. Headphones / watches / input devices broadcast continuously; phones usually do not.")
                color: Theme.fgMuted
                font.pixelSize: Theme.textXs
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
        }

        ListView {
            objectName: "scanList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: aura.scannedDevices
            spacing: Theme.space1

            delegate: MListItem {
                objectName: "scanItem_" + index
                width: ListView.view.width
                title: modelData.name
                subtitle: modelData.deviceId + "  ·  " + modelData.rssi + " dBm"
                       + (modelData.identHint && modelData.identHint.length > 0
                          ? "  ·  " + modelData.identHint
                          : "")

                // trailing slot: signal strength bar + Pair button.
                // SignalStrengthBar is plugin-internal (see header doc).
                Row {
                    spacing: Theme.space2

                    SignalStrengthBar {
                        anchors.verticalCenter: parent.verticalCenter
                        rssiDbm: modelData.rssi
                    }

                    MButton {
                        objectName: "pairButton_" + index
                        variant: MButton.Variant.Primary
                        iconSource: "qrc:/icons/icon-plus.svg"
                        text: qsTr("Pair")
                        onClicked: aura.pairDevice(modelData.deviceId, modelData.name)
                    }
                }
            }
        }

        // --- Advanced thresholds ---------------------------------------------
        Text {
            text: qsTr("Thresholds")
            color: Theme.fgSecondary
            font.pixelSize: Theme.textSm
            font.weight: Font.DemiBold
        }

        // RSSI threshold — pre-existing (M3-C8) MSlider, untouched by C14b
        // except for the objectName that tests now key on.
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: qsTr("Away RSSI threshold")
                color: Theme.fgMuted
                font.pixelSize: Theme.textXs
            }
            MSlider {
                objectName: "rssiSlider"
                from: -90
                to: -40
                stepSize: 1
                value: aura.rssiThresholdDbm
                onMoved: function(v) { aura.rssiThresholdDbm = v }
                Layout.fillWidth: true
            }
            Text {
                text: qsTr("%1 dBm").arg(aura.rssiThresholdDbm)
                color: Theme.fgSecondary
                font.pixelSize: Theme.textXs
                font.family: Theme.fontMono
                Layout.preferredWidth: 80
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: qsTr("Away delay (seconds)")
                color: Theme.fgMuted
                font.pixelSize: Theme.textXs
            }
            MSlider {
                objectName: "awayDelaySlider"
                from: 10
                to: 120
                stepSize: 2
                value: aura.awayDelaySec
                onMoved: function(v) { aura.awayDelaySec = v }
                Layout.fillWidth: true
            }
            Text {
                text: qsTr("%1 s").arg(aura.awayDelaySec)
                color: Theme.fgSecondary
                font.pixelSize: Theme.textXs
                font.family: Theme.fontMono
                Layout.preferredWidth: 60
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: qsTr("Cooldown (seconds)")
                color: Theme.fgMuted
                font.pixelSize: Theme.textXs
            }
            MSlider {
                objectName: "cooldownSlider"
                from: 30
                to: 300
                stepSize: 5
                value: aura.cooldownSec
                onMoved: function(v) { aura.cooldownSec = v }
                Layout.fillWidth: true
            }
            Text {
                text: qsTr("%1 s").arg(aura.cooldownSec)
                color: Theme.fgSecondary
                font.pixelSize: Theme.textXs
                font.family: Theme.fontMono
                Layout.preferredWidth: 60
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: qsTr("Scan duration (seconds)")
                color: Theme.fgMuted
                font.pixelSize: Theme.textXs
            }
            MSlider {
                objectName: "scanDurationSlider"
                from: 5
                to: 15
                stepSize: 1
                value: aura.scanDurationSec
                onMoved: function(v) { aura.scanDurationSec = v }
                Layout.fillWidth: true
            }
            Text {
                text: qsTr("%1 s").arg(aura.scanDurationSec)
                color: Theme.fgSecondary
                font.pixelSize: Theme.textXs
                font.family: Theme.fontMono
                Layout.preferredWidth: 60
            }
        }

        // Back button removed in M5-C4a — this page is now loaded by the
        // SettingsWindow's StackLayout as a top-level entry, so the previous
        // StackView pop() pattern no longer applies.
    }
}
