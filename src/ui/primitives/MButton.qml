// MButton — button atom (docs/06 §6.2). Three variants keyed off an enum so
// typos surface at QML load time instead of silently mis-styling. Optional
// leading icon via iconSource (a qrc URL string); MIcon is instantiated
// internally so the variant color matrix stays authoritative — callers don't
// get to override icon size/color and break the visual contract.
//
// API:
//   variant:     MButton.Variant.Primary | Secondary | Ghost
//   text:        label string
//   iconSource:  optional qrc URL, e.g. "qrc:/icons/icon-play.svg"
//   enabled:     bool (false → muted style + click suppressed)
//   signal clicked()
//
// Variant matrix:
//   Primary    accentBrand bg / fgPrimary text  + darker on press
//   Secondary  transparent / borderStrong 1px  + bgHover on press
//   Ghost      transparent / fgSecondary text  + bgHover on hover
//
// Does NOT import QtQuick.Controls (docs/15 §B2 — only MouseArea from
// QtQuick is needed; pulling Controls 2 would inherit its light style).

import QtQuick
import Margin.Ui.Primitives

Item {
    id: root

    enum Variant { Primary, Secondary, Ghost }

    property int variant: MButton.Variant.Primary
    property string text: ""
    property string iconSource: ""
    property bool enabled: true
    property int horizontalPadding: Theme.space3

    signal clicked()

    implicitHeight: 32
    implicitWidth: row.implicitWidth + 2 * horizontalPadding

    readonly property color _bg: {
        if (!root.enabled) return Theme.bgHover
        switch (root.variant) {
            case MButton.Variant.Primary:
                return ma.containsPress ? Qt.darker(Theme.accentBrand, 1.15) : Theme.accentBrand
            case MButton.Variant.Secondary:
                return ma.containsPress ? Theme.bgHover : "transparent"
            case MButton.Variant.Ghost:
                return ma.containsMouse ? Theme.bgHover : "transparent"
        }
        return Theme.accentBrand
    }
    readonly property color _fg: {
        if (!root.enabled) return Theme.fgMuted
        return root.variant === MButton.Variant.Ghost ? Theme.fgSecondary : Theme.fgPrimary
    }
    readonly property bool _hasBorder: !root.enabled
                                       ? false
                                       : root.variant === MButton.Variant.Secondary

    Rectangle {
        id: background
        anchors.fill: parent
        radius: Theme.radiusMd
        color: root._bg
        border.color: root._hasBorder ? Theme.borderStrong : "transparent"
        border.width: root._hasBorder ? 1 : 0
        Behavior on color {
            ColorAnimation {
                duration: Theme.durationFast
                easing.bezierCurve: Theme.easeOut
            }
        }

        MouseArea {
            id: ma
            anchors.fill: parent
            cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            hoverEnabled: true
            enabled: root.enabled
            onClicked: root.clicked()
        }

        Row {
            id: row
            anchors.centerIn: parent
            spacing: (root.iconSource !== "" && root.text !== "") ? Theme.space1 : 0

            MIcon {
                visible: root.iconSource !== ""
                anchors.verticalCenter: parent.verticalCenter
                source: root.iconSource
                size: Theme.textBase
                color: root._fg
            }

            Text {
                text: root.text
                color: root._fg
                font.pixelSize: Theme.textBase
                font.family: Theme.fontSans
            }
        }
    }
}
