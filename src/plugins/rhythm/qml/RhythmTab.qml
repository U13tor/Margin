// RhythmTab — M4-C12 pomodoro-fidelity root container.
//
// Layout (top-to-bottom):
//   1. Status card: "#N/M" header + ProgressDots ●●●○○ + CircularTimer
//      (purple progress ring, PR2 round-2 #5a) + Work/Break meta line +
//      control row (Pause + Skip + Settings) — all in the SAME card so the
//      prototype rhythm_pomodoro.png layout is reproduced faithfully.
//   2. Quick stats: today's pomodoros vs target + postpones left.
//
// N = todayCompletedRounds + 1 (the pomodoro in progress); M = targetRounds.
// Skip is state-gated (visible only in BreakDue/BreakActive) — Working state
// has no skip semantic that isn't already Stop.
//
// Dogfood Start/Stop row moved to Margin Settings → Rhythm page (PR2
// round-2 #5b) so main tab visual aligns with prototype's 3-button row.
//
// Settings live in Margin Settings → Rhythm page (pageId "rhythm"). The
// inline SpinBox settingsCard was removed in PR5 (round-1) — settings are
// unified in Margin Settings per user bug #4 "设置重复".
//
// `rhythm` context property is the PomodoroTimer instance (registered by
// RhythmPlugin::onLoad). QML binds to its Q_PROPERTYs and invokes its
// public slots (setPaused/skipBreak/endBreakEarly).

import QtQuick
import QtQuick.Layouts
import Margin.Ui.Primitives
import Margin.Ui.Composite

Rectangle {
    id: root
    objectName: "rhythmTab"
    color: Theme.bgBase

    width: parent ? parent.width : 480
    height: parent ? parent.height : 720

    // State-gated skip visibility — reused by both the control-row button
    // and (potentially) future tray menu entries.
    readonly property bool _skipVisible: rhythm.state === "break_due"
                                       || rhythm.state === "break_active"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space4
        spacing: Theme.space3

        Text {
            text: qsTr("Rhythm & Health")
            color: Theme.fgPrimary
            font.pixelSize: Theme.textBase
            font.weight: Font.DemiBold
        }

        Text {
            text: qsTr("Work / break cycle")
            color: Theme.fgMuted
            font.pixelSize: Theme.textXs
        }

        // ── Status card (timer + controls integrated) ──
        // MCard is an Item with default implicitHeight 0; without a hint the
        // outer ColumnLayout collapses this card to 0 and the anchored inner
        // ColumnLayout follows — but the 200×200 CircularTimer still renders
        // at its preferred size and overflows into the quick-stats card below
        // (the "卡片和圆形时间重合" bug). Bind implicitHeight to the inner
        // layout's natural height + padding so the card wraps its content.
        MCard {
            objectName: "rhythmStatusCard"
            Layout.fillWidth: true
            implicitHeight: statusCol.implicitHeight + 2 * padding

            ColumnLayout {
                id: statusCol
                anchors.fill: parent
                spacing: Theme.space3

                // Paused chip — visible only when something is holding the
                // countdown frozen. Text comes from `rhythm.pauseReasonsText`,
                // which returns the highest-priority active reason
                // (AuraAway > Idle > User) so a brief BLE dropout is visibly
                // distinguishable from "user hit pause". Without this chip
                // the user can't tell whether the timer is broken, idle,
                // or paused — the original "很多时候不自动开始" complaint.
                Rectangle {
                    visible: rhythm.paused
                    Layout.fillWidth: true
                    radius: Theme.radiusMd
                    color: Theme.bgElevated
                    implicitHeight: pauseLabel.implicitHeight + 2 * Theme.space2

                    Text {
                        id: pauseLabel
                        anchors.centerIn: parent
                        text: rhythm.pauseReasonsText === ""
                            ? qsTr("已暂停")
                            : qsTr("已暂停 · %1").arg(rhythm.pauseReasonsText)
                        color: Theme.fgSecondary
                        font.pixelSize: Theme.textXs
                    }
                }

                // Pomodoro header: "#N/M" with N = todayCompletedRounds + 1.
                // Allowing N > M when the user exceeded the daily goal is
                // an honest signal (setTargetRounds comment in PomodoroTimer.cpp).
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space2

                    Text {
                        objectName: "pomodoroHeader"
                        text: "#%1/%2".arg(rhythm.todayCompletedRounds + 1)
                                       .arg(rhythm.targetRounds)
                        color: Theme.fgPrimary
                        font.pixelSize: Theme.textXl
                        font.weight: Font.DemiBold
                        font.family: Theme.fontMono
                    }

                    Item { Layout.fillWidth: true }

                    ProgressDots {
                        objectName: "pomodoroDots"
                        total: rhythm.targetRounds
                        active: rhythm.todayCompletedRounds
                        dotSize: 10
                    }
                }

                // PR2 (round-2 #5a): circular progress ring with centered time
                // text, replacing the linear text-only TimerDisplay. Total =
                // current phase length (work or break). When phase is break,
                // remainingSeconds still ticks against break length — the ring
                // refill-on-phase-change is implicit (remainingSeconds resets).
                CircularTimer {
                    objectName: "pomodoroTimer"
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 200
                    Layout.preferredHeight: 200
                    totalSeconds: rhythm.workMinutes * 60
                    remainingSeconds: rhythm.remainingSeconds
                    centerLabel: qsTr("下次休息")
                }

                Text {
                    text: qsTr("Work %1 min · Break %2 min").arg(rhythm.workMinutes).arg(rhythm.breakMinutes)
                    color: Theme.fgMuted
                    font.pixelSize: Theme.textXs
                    Layout.alignment: Qt.AlignHCenter
                }

                // ── Control row (same card, 3 buttons per prototype) ──
                RowLayout {
                    objectName: "rhythmControlRow"
                    Layout.fillWidth: true
                    spacing: Theme.space2

                    MButton {
                        objectName: "pauseButton"
                        // 三态 (2026-06-27): Idle shows "开始" (seeds a fresh
                        // work session), paused shows "继续" (forceResume
                        // clears the entire pauseMask — the only way out of
                        // a paused-by-Aura-only state since the legacy
                        // setPaused(false) path just toggles the User bit),
                        // Working/Break* shows "暂停".
                        text: rhythm.state === "idle" ? qsTr("开始")
                            : rhythm.paused ? qsTr("继续")
                            : qsTr("暂停")
                        iconSource: rhythm.state === "idle" ? "qrc:/icons/icon-play.svg"
                                    : rhythm.paused ? "qrc:/icons/icon-play.svg"
                                                    : "qrc:/icons/icon-pause.svg"
                        variant: MButton.Variant.Primary
                        onClicked: {
                            if (rhythm.state === "idle") rhythm.start()
                            else if (rhythm.paused) rhythm.forceResume()
                            else rhythm.setPaused(true)
                        }
                    }

                    MButton {
                        objectName: "skipButton"
                        text: qsTr("跳过")
                        iconSource: "qrc:/icons/icon-skip.svg"
                        variant: MButton.Variant.Secondary
                        visible: root._skipVisible
                        onClicked: {
                            if (rhythm.state === "break_due") rhythm.skipBreak()
                            else rhythm.endBreakEarly()  // break_active
                        }
                    }

                    Item { Layout.fillWidth: true }

                    MButton {
                        objectName: "settingsButton"
                        text: qsTr("设置")
                        iconSource: "qrc:/icons/settings.svg"
                        // PR2 (round-2 #5b): Secondary (was Ghost) so all three
                        // control-row buttons share the Secondary/Primary family
                        // — Pause Primary is the only colored action, matching
                        // rhythm_pomodoro.png prototype's 3-button row.
                        variant: MButton.Variant.Secondary
                        onClicked: {
                            if (typeof settingsRoot !== "undefined" && settingsRoot) {
                                settingsRoot.openSettings("rhythm")
                            }
                        }
                    }
                }
            }
        }

        // ── Quick stats ──
        MCard {
            Layout.fillWidth: true
            implicitHeight: 88

            RowLayout {
                anchors.fill: parent
                spacing: Theme.space6

                ColumnLayout {
                    spacing: 2
                    Text {
                        text: qsTr("今日番茄 / 目标")
                        color: Theme.fgMuted
                        font.pixelSize: Theme.textXs
                    }
                    Text {
                        text: "%1 / %2".arg(rhythm.todayCompletedRounds)
                                        .arg(rhythm.targetRounds)
                        color: Theme.fgPrimary
                        font.pixelSize: Theme.textBase
                        font.weight: Font.DemiBold
                        font.family: Theme.fontMono
                    }
                }

                ColumnLayout {
                    spacing: 2
                    Text {
                        text: qsTr("推迟剩余")
                        color: Theme.fgMuted
                        font.pixelSize: Theme.textXs
                    }
                    Text {
                        text: "%1 / %2".arg(rhythm.postponesRemaining)
                                        .arg(rhythm.maxPostpones)
                        color: Theme.fgPrimary
                        font.pixelSize: Theme.textBase
                        font.weight: Font.DemiBold
                        font.family: Theme.fontMono
                    }
                }

                Item { Layout.fillWidth: true }
            }
        }

        Item { Layout.fillHeight: true }
    }
}

