// SettingsGeneralPage — host "General" settings page (docs/06 §4.6, M5-C4d).
// Currently exposes log level (Debug / Info / Warn / Error / Fatal) + language
// (Auto / 中文 / English) + autostart (On / Off). Writes go through the
// `generalSettings` context property (HostGeneralSettings), which persists to
// general.log_level / general.language / general.auto_start + emits the
// matching *Changed signals. HostCore subscribes to those settings keys and
// re-runs applyLogLevel / applyLanguage / PlatformBackend::setAutoStartEnabled
// immediately — no restart required. applyLanguage also retranslates QML so
// the whole UI flips to the picked locale live.
//
// Log level + language pickers are segmented rows (selected = Primary
// variant, others = Ghost) rather than dropdowns. There's no MMenu atom
// yet and these lists don't need a dropdown's complexity — a segmented
// row reads the options at a glance. Autostart is a single bool so it
// uses MSwitch.
//
// Autostart is Win-only at the platform-backend level; on Mac/Linux the
// backend is nullptr so the toggle persists the preference but has no OS
// effect (HostCore guards the call).

import QtQuick
import QtQuick.Layouts
import Margin.Ui.Primitives

Rectangle {
    objectName: "settingsGeneralPage"
    color: Theme.bgBase

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 12

        Text {
            text: qsTr("General")
            color: Theme.fgPrimary
            font.pixelSize: Theme.textLg
            font.weight: Font.DemiBold
        }

        Text {
            text: qsTr("Host-level preferences.")
            color: Theme.fgMuted
            font.pixelSize: Theme.textSm
        }

        // ── Log level ─────────────────────────────────────────────────
        Text {
            text: qsTr("Diagnostics")
            color: Theme.fgSecondary
            font.pixelSize: Theme.textSm
            font.weight: Font.DemiBold
            Layout.topMargin: 12
        }

        Text {
            text: qsTr("Log level")
            color: Theme.fgPrimary
            font.pixelSize: Theme.textSm
        }

        RowLayout {
            objectName: "logLevelRow"
            Layout.fillWidth: true
            spacing: 4

            Repeater {
                model: [
                    { key: "Debug", label: qsTr("Debug") },
                    { key: "Info",  label: qsTr("Info") },
                    { key: "Warn",  label: qsTr("Warn") },
                    { key: "Error", label: qsTr("Error") },
                    { key: "Fatal", label: qsTr("Fatal") },
                ]
                MButton {
                    objectName: "logLevelButton_" + modelData.key
                    property bool isCurrent: generalSettings.logLevel === modelData.key
                    variant: isCurrent ? MButton.Variant.Primary
                                       : MButton.Variant.Ghost
                    text: modelData.label
                    onClicked: generalSettings.logLevel = modelData.key
                }
            }
        }

        Text {
            text: qsTr("Lower levels log more (Debug is noisiest). Changes apply immediately — open the log file from the About page.")
            color: Theme.fgMuted
            font.pixelSize: Theme.textXs
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        // ── System ────────────────────────────────────────────────────
        Text {
            text: qsTr("System")
            color: Theme.fgSecondary
            font.pixelSize: Theme.textSm
            font.weight: Font.DemiBold
            Layout.topMargin: 16
        }

        RowLayout {
            objectName: "autoStartRow"
            Layout.fillWidth: true
            spacing: 8

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    text: qsTr("Launch on system start")
                    color: Theme.fgPrimary
                    font.pixelSize: Theme.textSm
                }
                Text {
                    text: qsTr("Start Margin automatically when you sign in.")
                    color: Theme.fgMuted
                    font.pixelSize: Theme.textXs
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
            MSwitch {
                objectName: "autoStartSwitch"
                checked: generalSettings.autoStart
                onToggled: generalSettings.autoStart = checked
            }
        }

        // PR6 round-2 #7: Language segmented row. Auto / 中文 / English.
        // Auto maps to system locale; the two explicit options force one
        // of the bundled catalogs. Switching takes effect immediately
        // (HostCore::applyLanguage installs the QTranslator + retranlates
        // the engine — see HostCore.cpp).
        RowLayout {
            objectName: "languageRow"
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: qsTr("Language")
                color: Theme.fgPrimary
                font.pixelSize: Theme.textSm
                Layout.fillWidth: true
            }

            RowLayout {
                objectName: "languageButtons"
                spacing: 4

                Repeater {
                    model: [
                        { key: "auto",  label: qsTr("Auto") },
                        { key: "zh_CN", label: qsTr("中文") },
                        { key: "en",    label: qsTr("English") },
                    ]
                    MButton {
                        objectName: "languageButton_" + modelData.key
                        property bool isCurrent: generalSettings.language === modelData.key
                        variant: isCurrent ? MButton.Variant.Primary
                                           : MButton.Variant.Ghost
                        text: modelData.label
                        onClicked: generalSettings.language = modelData.key
                    }
                }
            }
        }

        Text {
            text: qsTr("Auto-detect from system locale. 中文 forces Simplified Chinese, English forces English.")
            color: Theme.fgMuted
            font.pixelSize: Theme.textXs
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Item { Layout.fillHeight: true }
    }
}
