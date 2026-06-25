// RhythmSettingsPage — Pomodoro timing controls (docs/06 §4.6, M5-C4b).
// Sliders bind to PomodoroTimer Q_PROPERTY setters (registered as the
// `rhythm` context property in RhythmPlugin::onLoad). Each setter persists
// to Settings on write, so closing the window + reopening reflects the
// new value immediately. Range constants mirror PomodoroTimer's kMin/kMax
// (header) — single SSOT. Autostart is intentionally omitted for v1.0
// (the feature itself isn't wired beyond settings-load; will land with
// the OS autostart integration in v1.1, see docs/12-deferred-items.md).
//
// PR2 (round-2 #5b) "开发者" section: Start/Stop dogfood buttons moved
// here from RhythmTab.qml so the main tab's control row stays a clean
// 3-button prototype-matching layout. Functionality preserved verbatim.

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
            text: qsTr("Rhythm")
            color: Theme.fgPrimary
            font.pixelSize: Theme.textBase
            font.weight: Font.DemiBold
        }

        Text {
            text: qsTr("Pomodoro work / break cycle.")
            color: Theme.fgMuted
            font.pixelSize: Theme.textSm
        }

        // Work minutes (1..180, default 45)
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Text {
                text: qsTr("Work minutes")
                color: Theme.fgMuted
                font.pixelSize: Theme.textXs
            }
            MSlider {
                objectName: "workMinutesSlider"
                from: 1
                to: 180
                stepSize: 1
                value: rhythm.workMinutes
                onMoved: function(v) { rhythm.workMinutes = v }
                Layout.fillWidth: true
            }
            Text {
                text: qsTr("%1 min").arg(rhythm.workMinutes)
                color: Theme.fgSecondary
                font.pixelSize: Theme.textXs
                font.family: Theme.fontMono
                Layout.preferredWidth: 70
            }
        }

        // Break minutes (1..30, default 5)
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Text {
                text: qsTr("Break minutes")
                color: Theme.fgMuted
                font.pixelSize: Theme.textXs
            }
            MSlider {
                objectName: "breakMinutesSlider"
                from: 1
                to: 30
                stepSize: 1
                value: rhythm.breakMinutes
                onMoved: function(v) { rhythm.breakMinutes = v }
                Layout.fillWidth: true
            }
            Text {
                text: qsTr("%1 min").arg(rhythm.breakMinutes)
                color: Theme.fgSecondary
                font.pixelSize: Theme.textXs
                font.family: Theme.fontMono
                Layout.preferredWidth: 70
            }
        }

        // Max postpones (0..10, default 3)
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Text {
                text: qsTr("Max postpones per break")
                color: Theme.fgMuted
                font.pixelSize: Theme.textXs
            }
            MSlider {
                objectName: "maxPostponesSlider"
                from: 0
                to: 10
                stepSize: 1
                value: rhythm.maxPostpones
                onMoved: function(v) { rhythm.maxPostpones = v }
                Layout.fillWidth: true
            }
            Text {
                text: qsTr("%1").arg(rhythm.maxPostpones)
                color: Theme.fgSecondary
                font.pixelSize: Theme.textXs
                font.family: Theme.fontMono
                Layout.preferredWidth: 70
            }
        }

        // Target rounds (1..12, default 5)
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Text {
                text: qsTr("Target rounds per day")
                color: Theme.fgMuted
                font.pixelSize: Theme.textXs
            }
            MSlider {
                objectName: "targetRoundsSlider"
                from: 1
                to: 12
                stepSize: 1
                value: rhythm.targetRounds
                onMoved: function(v) { rhythm.targetRounds = v }
                Layout.fillWidth: true
            }
            Text {
                text: qsTr("%1").arg(rhythm.targetRounds)
                color: Theme.fgSecondary
                font.pixelSize: Theme.textXs
                font.family: Theme.fontMono
                Layout.preferredWidth: 70
            }
        }

        // ── 开发者 (dogfood Start/Stop, PR2 round-2 #5b) ─────────
        // Relocated from RhythmTab.qml so the main tab control row stays a
        // clean 3-button layout matching rhythm_pomodoro.png. Start/Stop
        // verify state transitions without waiting through a full work phase.
        // Gated by `rhythm.state` so the disabled one is always the opposite
        // of the current state — Start enabled only when idle, Stop enabled
        // only when running.
        Text {
            text: qsTr("开发者")
            color: Theme.fgSecondary
            font.pixelSize: Theme.textSm
            font.weight: Font.DemiBold
            Layout.topMargin: 16
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space2

            MButton {
                objectName: "startButton"
                text: qsTr("Start")
                variant: MButton.Variant.Ghost
                enabled: rhythm.state === "idle"
                onClicked: rhythm.start()
            }

            MButton {
                objectName: "stopButton"
                text: qsTr("Stop")
                variant: MButton.Variant.Ghost
                enabled: rhythm.state !== "idle"
                onClicked: rhythm.stop()
            }

            Item { Layout.fillWidth: true }
        }

        Item { Layout.fillHeight: true }
    }
}
