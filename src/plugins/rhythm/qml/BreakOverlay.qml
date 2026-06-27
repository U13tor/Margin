// BreakOverlay — standalone top-level stretch guide window (M4-C13a/c).
//
// Lifecycle (M4-C13a):
//   - Root is a Qt.Tool top-level Window, created lazily from
//     RhythmPlugin::createBreakOverlayWindow() via QQmlComponent.
//   - C++ controls visibility: breakStarted → showBreakOverlay,
//     skipped → hideBreakOverlay. Window flags (FramelessWindowHint |
//     WindowStaysOnTopHint | Qt::Tool) are set from C++ — don't redeclare
//     here or they override the C++ ones.
//   - On natural break completion (breakEnded), C++ does NOT hide — QML
//     catches the signal and switches to `mode: "done"` so the user sees
//     a completion card instead of the window vanishing. The done card
//     auto-closes after `doneCountdownSec` and can be dismissed early via
//     the close button. Both paths call rhythmHost.dismissBreakOverlay().
//
// Card interior (M4-C13c, M4-C13d timing fix):
//   - Pose progression is bound to rhythm.remainingSeconds so one full
//     8-pose round fills the entire break (per-step duration =
//     breakMinutes * 60 / 8, e.g. 5min break ≈ 37.5s/pose). Pauses are
//     handled implicitly because remainingSeconds stops ticking during
//     pause (e.g. Aura away).
//
// UX note on 推迟 in BreakActive (2026-06-27): the previous "推迟 5 分钟"
// button was removed because postponeBreak() is a no-op in BreakActive
// (its API semantics are "don't start break yet" — BreakDue → Working).
// The button silently fell back to endBreakEarly() (skip semantics), so
// the label lied about what would happen. A true "extend break by 5 min"
// semantics needs a new PomodoroTimer API; the 跳过 button covers the
// dismiss-early intent until then.

import QtQuick
import QtQuick.Layouts
import QtQuick.Window
import Margin.Ui.Primitives

Window {
    id: root
    objectName: "breakOverlay"
    visible: false
    width: 520
    height: 480
    color: Theme.bgElevated

    // "active" → pose cycle; "done" → completion card shown after natural
    // break end (gives the user a buffer instead of the window vanishing).
    property string mode: "active"

    // Auto-close delay for the done card. 10 s is long enough to register
    // the "completed" message but short enough to not block the next work
    // session's focus.
    property int doneCountdownSec: 10
    property int doneRemaining: doneCountdownSec

    // Watch PomodoroTimer signals to drive mode transitions. breakStarted
    // resets to "active" (next break cycle after a previous done card) AND
    // rewinds doneRemaining so a second done-card cycle counts down from
    // doneCountdownSec instead of starting at 0 (Bug P, 2026-06-27).
    // breakEnded → "done"; skipped → C++ hides immediately, mode reset is
    // just defensive.
    Connections {
        target: rhythm
        function onBreakStarted() {
            root.mode = "active"
            root.doneRemaining = root.doneCountdownSec
        }
        function onBreakEnded() { root.mode = "done" }
        function onSkipped() { root.mode = "active" }
    }

    // Compute current pose from elapsed break time so one full round
    // matches the total break duration. perStep = breakSeconds / 8.
    function computeCurrentStep(): int {
        const totalSec = rhythm.breakMinutes * 60
        if (totalSec <= 0) return 1
        const elapsed = totalSec - rhythm.remainingSeconds
        const perStep = totalSec / 8
        const step = Math.floor(elapsed / perStep) + 1
        return Math.max(1, Math.min(8, step))
    }

    // Auto-close timer for done mode. Started/stopped via the running
    // binding; onTriggered dismisses the overlay.
    Timer {
        id: doneCountdown
        interval: 1000
        repeat: true
        running: root.mode === "done" && root.visible
        onTriggered: {
            if (root.doneRemaining > 1) {
                root.doneRemaining--
            } else {
                rhythmHost.dismissBreakOverlay()
            }
        }
    }

    // Active mode: full 8-pose cycle.
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space6
        spacing: Theme.space4
        visible: root.mode === "active"

        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: Theme.space2
            text: qsTr("颈椎放松操")
            color: Theme.fgPrimary
            font.pixelSize: Theme.textLg
            font.weight: Font.DemiBold
        }

        NeckStretchAnimator {
            id: animator
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 200
            Layout.preferredHeight: 200
            currentStep: root.computeCurrentStep()
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("动作 %1/8 · %2").arg(animator.currentStep).arg(animator.currentName)
            color: Theme.fgSecondary
            font.pixelSize: Theme.textBase
            font.weight: Font.Medium
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            maximumLineCount: 3
            elide: Text.ElideRight
            text: animator.currentDescription
            color: Theme.fgSecondary
            font.pixelSize: Theme.textXs
        }

        ProgressDots {
            Layout.alignment: Qt.AlignHCenter
            total: 8
            active: animator.currentStep
            dotSize: 10
            spacing: 8
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: {
                const total = rhythm.remainingSeconds
                const m = Math.floor(total / 60).toString().padStart(2, "0")
                const s = (total % 60).toString().padStart(2, "0")
                return m + ":" + s
            }
            color: Theme.fgMuted
            font.pixelSize: Theme.textSm
            font.weight: Font.DemiBold
        }

        Item { Layout.fillHeight: true }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: Theme.space2
            spacing: Theme.space3

            MButton {
                text: qsTr("跳过")
                iconSource: "qrc:/icons/icon-skip.svg"
                variant: MButton.Variant.Secondary
                onClicked: rhythm.endBreakEarly()
            }
        }
    }

    // Done mode: completion card with 10s auto-close.
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space6
        spacing: Theme.space4
        visible: root.mode === "done"

        Item { Layout.fillHeight: true }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("🎉")
            font.pixelSize: 64
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("本节休息已完成")
            color: Theme.fgPrimary
            font.pixelSize: Theme.textLg
            font.weight: Font.DemiBold
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: qsTr("颈椎八式一轮完成。下一节工作番茄 %1 分钟后开始。")
                .arg(rhythm.workMinutes)
            color: Theme.fgSecondary
            font.pixelSize: Theme.textSm
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("%1 秒后自动关闭").arg(root.doneRemaining)
            color: Theme.fgMuted
            font.pixelSize: Theme.textXs
        }

        Item { Layout.fillHeight: true }

        MButton {
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: Theme.space2
            text: qsTr("立即关闭")
            iconSource: "qrc:/icons/icon-play.svg"
            variant: MButton.Variant.Primary
            onClicked: rhythmHost.dismissBreakOverlay()
        }
    }

    // Escape key = dismiss. In active mode: endBreakEarly (skip path,
    // C++ hides immediately). In done mode: dismissBreakOverlay (close
    // path).
    Shortcut {
        sequences: ["Esc", "Escape"]
        onActivated: {
            if (root.mode === "done") {
                rhythmHost.dismissBreakOverlay()
            } else {
                rhythm.endBreakEarly()
            }
        }
    }
}
