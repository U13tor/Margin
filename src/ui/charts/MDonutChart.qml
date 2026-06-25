// MDonutChart — category distribution ring + legend (docs/06 §6.4).
//
// The ring is drawn on a QML Canvas (no third-party chart lib). Slice colours
// cycle Theme.categoryPalette by index. The legend reads each row's
// pre-computed `percentage` (supplied by the caller). Center text is laid out
// as real QML Text over the hole so it stays crisp.

import QtQuick
import QtQuick.Layouts
import Margin.Ui.Primitives

RowLayout {
    id: root

    // Array of objects (e.g. [{ category, durationMs, percentage }, ...]).
    property var model: []
    property string valueKey: "durationMs"
    property string labelKey: "category"
    property var colors: Theme.categoryPalette
    property int thickness: 18
    property int diameter: 132
    property string centerPrimary: ""
    property string centerSecondary: ""

    spacing: 16

    readonly property real _total: {
        let t = 0
        for (let i = 0; i < model.length; ++i) t += model[i][valueKey]
        return t
    }

    onModelChanged: canvas.requestPaint()

    Item {
        Layout.preferredWidth: root.diameter
        Layout.preferredHeight: root.diameter

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
                const rOuter = Math.min(cx, cy)
                const rInner = Math.max(0, rOuter - root.thickness)

                if (root._total <= 0) {
                    ctx.beginPath()
                    ctx.arc(cx, cy, rOuter, 0, 2 * Math.PI, false)
                    ctx.arc(cx, cy, rInner, 0, 2 * Math.PI, true)
                    ctx.fillStyle = Theme.bgHover
                    ctx.fill()
                    return
                }

                let a0 = -Math.PI / 2
                for (let i = 0; i < root.model.length; ++i) {
                    const v = root.model[i][root.valueKey]
                    const a1 = a0 + 2 * Math.PI * v / root._total
                    ctx.beginPath()
                    ctx.arc(cx, cy, rOuter, a0, a1, false)
                    ctx.arc(cx, cy, rInner, a1, a0, true)
                    ctx.closePath()
                    ctx.fillStyle = root.colors[i % root.colors.length]
                    ctx.fill()
                    a0 = a1
                }
            }
        }

        ColumnLayout {
            anchors.centerIn: parent
            spacing: 0

            Text {
                text: root.centerPrimary
                visible: root.centerPrimary.length > 0
                color: Theme.fgPrimary
                font.pixelSize: Theme.textXl
                font.weight: Font.DemiBold
                Layout.alignment: Qt.AlignHCenter
            }
            Text {
                text: root.centerSecondary
                visible: root.centerSecondary.length > 0
                color: Theme.fgMuted
                font.pixelSize: Theme.textXs
                Layout.alignment: Qt.AlignHCenter
            }
        }
    }

    ColumnLayout {
        Layout.alignment: Qt.AlignVCenter
        spacing: 4

        Repeater {
            model: root.model
            delegate: RowLayout {
                required property var modelData
                required property int index
                spacing: 6

                Rectangle {
                    width: 10
                    height: 10
                    radius: 2
                    color: root.colors[index % root.colors.length]
                }
                Text {
                    text: modelData[root.labelKey]
                    color: Theme.fgPrimary
                    font.pixelSize: Theme.textXs
                    Layout.preferredWidth: 120
                    elide: Text.ElideRight
                }
                Text {
                    text: Math.round(modelData.percentage) + "%"
                    color: Theme.fgSecondary
                    font.pixelSize: Theme.textXs
                }
            }
        }
    }
}
