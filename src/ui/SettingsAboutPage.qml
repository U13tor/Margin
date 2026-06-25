// SettingsAboutPage — host "About" settings page (docs/06 §4.6, M5-C4d).
// Mirrors AboutDialog's content (app icon + version + description + license).
// `marginVersion` context property is already injected by HostCore for
// StatusBar.qml; we reuse it here.
//
// Links to the project repository + log file path are shown as read-only
// text (no Clickable link atom yet — v1.1 will add MLowLink with proper
// URL handling per docs/12-deferred-items.md).

import QtQuick
import QtQuick.Layouts
import Margin.Ui.Primitives

Rectangle {
    objectName: "settingsAboutPage"
    color: Theme.bgBase

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 12

        // App icon + name + version row
        RowLayout {
            Layout.fillWidth: true
            spacing: 16

            Image {
                source: "qrc:/icons/tray.svg"
                sourceSize: Qt.size(64, 64)
                Layout.alignment: Qt.AlignTop
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    text: qsTr("Margin · 息间")
                    color: Theme.fgPrimary
                    font.pixelSize: Theme.textLg
                    font.weight: Font.DemiBold
                }

                Text {
                    text: qsTr("Version %1").arg(
                        (typeof marginVersion !== "undefined") ? marginVersion
                                                                : "0.0.0-dev")
                    color: Theme.fgSecondary
                    font.pixelSize: Theme.textSm
                    font.family: Theme.fontMono
                }
            }
        }

        // Description
        Text {
            text: qsTr("Cross-platform desktop wellness companion — tracks screen time, paces breaks, and auto-locks your screen via Bluetooth proximity when you step away.")
            color: Theme.fgSecondary
            font.pixelSize: Theme.textSm
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        // Tech stack
        Text {
            text: qsTr("Built with Qt 6.5 + QML. 100% local — no network calls, no telemetry, no crash-report uploads.")
            color: Theme.fgMuted
            font.pixelSize: Theme.textXs
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        // ── Diagnostics ─────────────────────────────────────────────
        Text {
            text: qsTr("Diagnostics")
            color: Theme.fgSecondary
            font.pixelSize: Theme.textSm
            font.weight: Font.DemiBold
            Layout.topMargin: 16
        }

        Text {
            text: qsTr("Log file")
            color: Theme.fgPrimary
            font.pixelSize: Theme.textSm
        }
        Text {
            objectName: "logFilePath"
            text: (typeof logFilePath !== "undefined") ? logFilePath
                                                       : qsTr("(host logger not initialized)")
            color: Theme.fgMuted
            font.pixelSize: Theme.textXs
            font.family: Theme.fontMono
            wrapMode: Text.WrapAnywhere
            Layout.fillWidth: true
        }

        // ── License ─────────────────────────────────────────────────
        Text {
            text: qsTr("License")
            color: Theme.fgSecondary
            font.pixelSize: Theme.textSm
            font.weight: Font.DemiBold
            Layout.topMargin: 16
        }
        Text {
            text: qsTr("LGPL-3.0-or-later. Source code available on the project repository.")
            color: Theme.fgMuted
            font.pixelSize: Theme.textXs
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Item { Layout.fillHeight: true }
    }
}
