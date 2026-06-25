// ScreenTimeSettingsPage — idle threshold + data export (docs/06 §4.6, M5-C4c).
// Two sections:
//   1. Idle threshold slider (60-1800s, 30s step) — bound to
//      `screen_time.idleThresholdSec` R/W Q_PROPERTY. Setter clamps +
//      persists to plugins.screen_time.idle_threshold_sec.
//   2. Data export / clear-all — embeds ExportClearDialog (overlay sheet)
//      so we reuse the FileDialog + confirm-dialog wiring already shipped
//      in M2-C7. The "..." button toggles the sheet's visibility.
//
// Category overrides (advanced JSON editor) deferred to v1.1 — see
// docs/12-deferred-items.md §A26. Current matcher ships with sensible
// defaults (9 categories); advanced override will land as a dedicated
// page when there's a real UI pattern for editing nested JSON safely.

import QtQuick
import QtQuick.Layouts
import Margin.Ui.Primitives

Item {
    id: root

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Text {
            text: qsTr("Screen Time")
            color: Theme.fgPrimary
            font.pixelSize: Theme.textBase
            font.weight: Font.DemiBold
        }

        Text {
            text: qsTr("Idle detection and data export.")
            color: Theme.fgMuted
            font.pixelSize: Theme.textSm
        }

        // ── Idle threshold ───────────────────────────────────────────
        Text {
            text: qsTr("Idle")
            color: Theme.fgSecondary
            font.pixelSize: Theme.textSm
            font.weight: Font.DemiBold
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Text {
                text: qsTr("Idle threshold (seconds)")
                color: Theme.fgMuted
                font.pixelSize: Theme.textXs
            }
            MSlider {
                objectName: "idleThresholdSlider"
                from: 60
                to: 1800
                stepSize: 30
                value: screen_time.idleThresholdSec
                onMoved: function(v) { screen_time.idleThresholdSec = v }
                Layout.fillWidth: true
            }
            Text {
                text: qsTr("%1 s").arg(screen_time.idleThresholdSec)
                color: Theme.fgSecondary
                font.pixelSize: Theme.textXs
                font.family: Theme.fontMono
                Layout.preferredWidth: 70
            }
        }

        Text {
            text: qsTr("Below this idle time, you're considered still active. Longer = fewer pickup events; shorter = more accurate focus tracking.")
            color: Theme.fgMuted
            font.pixelSize: Theme.textXs
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        // ── Data export ──────────────────────────────────────────────
        Text {
            text: qsTr("Data")
            color: Theme.fgSecondary
            font.pixelSize: Theme.textSm
            font.weight: Font.DemiBold
        }

        MButton {
            objectName: "openExportDialogButton"
            variant: MButton.Variant.Secondary
            iconSource: "qrc:/icons/icon-export.svg"
            text: qsTr("Export / Clear Data...")
            onClicked: exportClearDialog.open()
            Layout.fillWidth: true
        }

        Text {
            text: qsTr("Export decrypts the window titles of every session into the file you pick. Data is yours — mind where you save it.")
            color: Theme.fgMuted
            font.pixelSize: Theme.textXs
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        // ── Category overrides (deferred) ────────────────────────────
        Text {
            text: qsTr("Advanced")
            color: Theme.fgSecondary
            font.pixelSize: Theme.textSm
            font.weight: Font.DemiBold
        }
        Text {
            text: qsTr("Category overrides: 9 built-in rules cover common apps. Per-app JSON overrides land in v1.1.")
            color: Theme.fgMuted
            font.pixelSize: Theme.textXs
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Item { Layout.fillHeight: true }
    }

    // Embedded overlay sheet — sibling of ColumnLayout (not child) so the
    // layout doesn't try to manage it. anchors.fill: root covers the whole
    // page with the dim backdrop; visible:false until open() flips it.
    ExportClearDialog {
        id: exportClearDialog
        objectName: "exportClearDialog"
        anchors.fill: parent
    }
}
