// MListItem — list item atom (docs/06 §6.2). Four-slot Row layout: optional
// leading icon, title + subtitle stacked in the middle (filling remaining
// width), optional trailing slot for action buttons / badges / icons via the
// default children alias.
//
// API:
//   title:       string (main line)
//   subtitle:    string (secondary line; empty → only title renders)
//   iconSource:  qrc URL (empty → icon slot hidden). Named iconSource (not
//                icon) to match MButton.qml:32 — both atoms take qrc URL
//                strings, callers shouldn't have to remember two names.
//   spacing:     int (default Theme.space2 = 8)
//   highlighted: bool (default false). M5-C3: selection state for sidebar
//                usage; swaps background to bgHover + border to accentBrand
//                so the active entry stands out without a separate variant.
//   clicked():   signal. M5-C3: emitted on left-click anywhere in the row.
//                Existing non-interactive callers (event lists, scan results)
//                simply don't connect — backward-compatible.
//   default trailing slot: any Item caller adds (MButton, MIcon, Badge...)
//
// Uses QtQuick.Layouts (RowLayout) so the middle ColumnLayout can fillWidth
// and push the trailing slot to the right edge. Plan §A.4 originally floated
// "plain Row only", but Row has no fillWidth concept — RowLayout is the
// idiomatic way to get this layout, and it's already imported by every caller
// file so the dependency is not new.
//
// Does NOT import QtQuick.Controls (docs/15 §B2).

import QtQuick
import QtQuick.Layouts
import Margin.Ui.Primitives

Item {
    id: root

    property string title: ""
    property string subtitle: ""
    property string iconSource: ""
    property int spacing: Theme.space2
    property bool highlighted: false

    signal clicked()

    default property alias trailing: trailingSlot.children

    implicitWidth: 200
    implicitHeight: 48

    readonly property bool _hasIcon: root.iconSource !== ""
    readonly property bool _hasSubtitle: root.subtitle !== ""

    Rectangle {
        id: background
        anchors.fill: parent
        radius: Theme.radiusSm
        color: root.highlighted ? Theme.bgHover : Theme.bgElevated
        border.color: root.highlighted ? Theme.accentBrand : Theme.borderSubtle
        border.width: 1

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: root.clicked()
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.space3
            anchors.rightMargin: Theme.space3
            spacing: root.spacing

            MIcon {
                source: root.iconSource
                size: Theme.textBase
                color: Theme.fgSecondary
                visible: root._hasIcon
                Layout.alignment: Qt.AlignVCenter
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.alignment: Qt.AlignVCenter
                spacing: Theme.space1

                Text {
                    text: root.title
                    color: root.highlighted ? Theme.fgPrimary : Theme.fgPrimary
                    font.pixelSize: Theme.textSm
                    font.family: Theme.fontSans
                    font.weight: root.highlighted ? Font.DemiBold : Font.Normal
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                }

                Text {
                    text: root.subtitle
                    color: Theme.fgMuted
                    font.pixelSize: Theme.textXs
                    font.family: Theme.fontSans
                    visible: root._hasSubtitle
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                }
            }

            Item {
                id: trailingSlot
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredWidth: childrenRect.width
                Layout.preferredHeight: childrenRect.height
                Layout.minimumWidth: childrenRect.width
            }
        }
    }
}
