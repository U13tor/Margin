// StatusCard — composite summary card (docs/06 §6.3). Wraps the MCard atom
// with a fixed title/value/subtitle three-slot layout + optional leading
// icon + default children slot for extras (progress bar / badge / action).
// Emits clicked() when the card background is clicked — interactive children
// placed in the default slot still receive their own clicks first because
// the MouseArea is declared before the content ColumnLayout, so it sits
// underneath in paint order; Qt dispatches to the top-most MouseArea under
// the cursor, and the caller's MButton (deeper in the item tree) wins first.
//
// API:
//   title:       string (top label, e.g. "今日专注")
//   value:       string (big middle line, e.g. "2h 15m"; mono font for stable
//                digit width — matches TimerDisplay's number styling)
//   subtitle:    string (bottom caption, e.g. "生产力 65%"; empty → hidden)
//   iconSource:  qrc URL (empty → icon slot hidden). Named iconSource to
//                match MButton / MListItem atoms — all three take qrc URLs,
//                callers shouldn't have to remember two property names.
//   default children slot: any Items caller adds below subtitle
//   signal clicked()
//
// Built from MCard + MIcon atoms (do not reimplement Rectangle+radius —
// that's MCard's job). Does NOT import QtQuick.Controls (docs/15 §B2).

import QtQuick
import QtQuick.Layouts
import Margin.Ui.Primitives

Item {
    id: root

    property string title: ""
    property string value: ""
    property string subtitle: ""
    property string iconSource: ""

    default property alias extras: extrasSlot.children

    signal clicked()

    implicitWidth: 160
    implicitHeight: 90

    readonly property bool _hasIcon: root.iconSource !== ""
    readonly property bool _hasSubtitle: root.subtitle !== ""

    MCard {
        anchors.fill: parent

        // Background click layer — declared first so it stays under the
        // ColumnLayout in paint order. Interactive extras (MButton /
        // MSwitch) placed in the default slot below still receive their
        // own clicks first: Qt hit-tests top-down and the caller's
        // MouseArea lives deeper in the subtree than this one.
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            hoverEnabled: true
            onClicked: root.clicked()
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: Theme.space1

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.space2

                MIcon {
                    source: root.iconSource
                    size: Theme.textBase
                    color: Theme.fgSecondary
                    visible: root._hasIcon
                    Layout.alignment: Qt.AlignVCenter
                }

                Text {
                    text: root.title
                    color: Theme.fgSecondary
                    font.pixelSize: Theme.textSm
                    font.family: Theme.fontSans
                    Layout.fillWidth: true
                }
            }

            Text {
                text: root.value
                color: Theme.fgPrimary
                font.pixelSize: Theme.textXl
                font.weight: Font.DemiBold
                font.family: Theme.fontMono
                Layout.fillWidth: true
                elide: Text.ElideRight
                maximumLineCount: 1
                fontSizeMode: Text.HorizontalFit
                minimumPixelSize: Theme.textXs
            }

            Text {
                text: root.subtitle
                color: Theme.fgMuted
                font.pixelSize: Theme.textXs
                font.family: Theme.fontSans
                visible: root._hasSubtitle
                Layout.fillWidth: true
            }

            Item {
                id: extrasSlot
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }
    }
}
