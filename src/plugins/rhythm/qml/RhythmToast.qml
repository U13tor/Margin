// RhythmToast — M3-C3 top-level toast window. Shown when the Pomodoro work
// countdown hits zero (breakDue signal). Lives outside DashboardWindow so
// it stays visible even when the dashboard is hidden to the tray.
//
// Lifecycle:
//   PomodoroTimer::breakDue ──► plugin creates + show()s this window
//                               (positioned bottom-right of primary screen)
//   user clicks "开始做操" ───► rhythm.startBreak() ─► plugin hide()s window
//   user clicks "推迟 5 分钟" ► rhythm.postponeBreak() ─► plugin hide()s
//   user closes via OS ─────► treated as postpone (work resumes)
//
// `rhythm` is the PomodoroTimer context property; we read its state + invoke
// its Q_INVOKABLE actions. The plugin's showBreakToast()/hideBreakToast()
// control visibility from the C++ side (Window.visible), not from QML —
// this keeps the trigger logic in one place (the plugin) so future
// enhancements (e.g. play a sound) live with the trigger source.

import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import Margin.Ui.Primitives

Window {
    objectName: "rhythmToast"
    width: 360
    height: 156
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool
    color: "transparent"
    visible: false

    // Container Rectangle is what the user actually sees — the Window itself
    // is transparent so the rounded card can cast its own border + shadow.
    Rectangle {
        id: card
        anchors.fill: parent
        radius: 8
        color: Theme.bgElevated
        border.color: Theme.borderSubtle
        border.width: 1

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 6

            Text {
                text: qsTr("该起身活动一下了")
                color: Theme.fgPrimary
                font.pixelSize: Theme.textBase
                font.weight: Font.DemiBold
            }

            Text {
                text: qsTr("已连续工作 %1 分钟").arg(rhythm.workMinutes)
                color: Theme.fgSecondary
                font.pixelSize: Theme.textXs
            }

            Text {
                text: rhythm.postponesRemaining > 0
                      ? qsTr("还可推迟 %1 次").arg(rhythm.postponesRemaining)
                      : qsTr("本次不能再推迟")
                color: Theme.fgMuted
                font.pixelSize: Theme.textXs
                visible: rhythm.state === "break_due"
            }

            Item { Layout.fillHeight: true }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                MButton {
                    objectName: "postponeButton"
                    text: qsTr("推迟 5 分钟")
                    variant: MButton.Variant.Secondary
                    enabled: rhythm.postponesRemaining > 0
                    onClicked: rhythm.postponeBreak()
                }

                Item { Layout.fillWidth: true }

                MButton {
                    objectName: "startBreakButton"
                    text: qsTr("开始做操")
                    variant: MButton.Variant.Primary
                    onClicked: rhythm.startBreak()
                }
            }
        }
    }
}
