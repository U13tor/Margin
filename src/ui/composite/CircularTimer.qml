// CircularTimer — graphical countdown timer atom (docs/06 §6.3, PR2 round-2 #5a).
// Replaces the linear text-only TimerDisplay on RhythmTab with the
// rhythm_pomodoro.png prototype shape: a purple progress ring with the
// remaining time rendered in the center hole.
//
// API:
//   totalSeconds:     int   — full duration of the current phase (work or break)
//   remainingSeconds: int   — current countdown value; drives ring fill + center text
//   centerLabel:      string — small caption beneath the time (e.g. "下次休息")
//   ringColor:        color — foreground arc fill (default Theme.accentBrand)
//   trackColor:       color — background ring fill (default Theme.bgHover)
//   thickness:        int   — ring band thickness in px (default 8)
//
// Rendering uses a Canvas (same approach as MDonutChart.qml). The ring is
// two arcs (outer CW + inner CCW) closed as a donut slice from 12 o'clock
// clockwise by `progress` of the full circle. Center text stays crisp by
// being real QML Text (not painted into the canvas).

import QtQuick
import QtQuick.Layouts
import Margin.Ui.Primitives

Item {
    id: root

    property int totalSeconds: 60
    property int remainingSeconds: 60
    property string centerLabel: ""
    property color ringColor: Theme.accentBrand
    property color trackColor: Theme.bgHover
    property int thickness: 8

    implicitWidth: 200
    implicitHeight: 200

    readonly property real progress: totalSeconds > 0
        ? Math.max(0, Math.min(1, remainingSeconds / totalSeconds))
        : 0

    function _formatTime(sec) {
        if (sec < 0) sec = 0
        const m = Math.floor(sec / 60)
        const s = sec % 60
        return (m < 10 ? "0" : "") + m + ":" + (s < 10 ? "0" : "") + s
    }

    // Trigger canvas repaint when any driving property changes. Using explicit
    // handlers (rather than binding Canvas.onPaint to a formula) keeps the
    // paint path simple: requestPaint coalesces multiple triggers into one
    // repaint within the same frame, so back-to-back second-ticks stay cheap.
    onRemainingSecondsChanged: canvas.requestPaint()
    onTotalSecondsChanged: canvas.requestPaint()
    onRingColorChanged: canvas.requestPaint()
    onTrackColorChanged: canvas.requestPaint()
    onProgressChanged: canvas.requestPaint()

    Canvas {
        id: canvas
        anchors.fill: parent
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        Component.onCompleted: requestPaint()

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            const cx = width / 2
            const cy = height / 2
            const rOuter = Math.max(0, Math.min(cx, cy) - 2)
            const rInner = Math.max(0, rOuter - root.thickness)
            if (rOuter <= 0 || rInner <= 0) return

            // Background track ring (full circle, gray).
            ctx.beginPath()
            ctx.arc(cx, cy, rOuter, 0, 2 * Math.PI, false)
            ctx.arc(cx, cy, rInner, 0, 2 * Math.PI, true)
            ctx.fillStyle = root.trackColor
            ctx.fill()

            // Foreground progress ring (purple, arc from 12 o'clock CW).
            // Skip drawing when progress is 0 — arc with zero span leaves
            // artifacts in some Qt 6.5 Canvas backends.
            if (root.progress > 0) {
                const startAngle = -Math.PI / 2
                const endAngle = startAngle + 2 * Math.PI * root.progress
                ctx.beginPath()
                ctx.arc(cx, cy, rOuter, startAngle, endAngle, false)
                ctx.arc(cx, cy, rInner, endAngle, startAngle, true)
                ctx.closePath()
                ctx.fillStyle = root.ringColor
                ctx.fill()
            }
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 4

        Text {
            text: root._formatTime(root.remainingSeconds)
            color: Theme.fgPrimary
            font.pixelSize: Theme.text3xl
            font.weight: Font.DemiBold
            font.family: Theme.fontMono
            Layout.alignment: Qt.AlignHCenter
        }

        Text {
            text: root.centerLabel
            visible: root.centerLabel.length > 0
            color: Theme.fgMuted
            font.pixelSize: Theme.textXs
            Layout.alignment: Qt.AlignHCenter
        }
    }
}
