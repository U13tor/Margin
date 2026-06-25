// MCard — card container atom (docs/06 §6.2). Minimal surface: padded rounded
// rectangle with the design-system elevated background + subtle border. Caller
// owns the content via the default children slot.
//
// API:
//   padding:   int (default Theme.space3 = 12)
//   radius:    int (default Theme.radiusSm = 4 — matches existing hand-rolled
//                 Rectangle cards so migration is visually zero-change)
//   elevation: int (default 0; reserved for future shadow — see trade-off)
//   default children slot: any QML Items caller adds
//
// elevation trade-off:
//   dark theme + Qt Quick has no built-in drop-shadow (MultiEffect doesn't do
//   shadow; DropShadow is in Qt5.Compatibility / third-party). elevation > 0
//   currently renders visually identical to elevation = 0 — the property is
//   kept in the API so future shadow work doesn't break callers. CLAUDE.md
//   §5 rule 2 still holds: the atom is fully working, elevation is just a
//   no-op visual flag in the current theme.
//
// Does NOT import QtQuick.Controls (docs/15 §B2). No MouseArea — pure container.

import QtQuick
import Margin.Ui.Primitives

Item {
    id: root

    property int padding: Theme.space3
    property int radius: Theme.radiusSm
    property int elevation: 0

    default property alias children: content.children

    Rectangle {
        id: background
        anchors.fill: parent
        radius: root.radius
        color: Theme.bgElevated
        border.color: Theme.borderSubtle
        border.width: 1

        Item {
            id: content
            anchors.fill: parent
            anchors.margins: root.padding
        }
    }
}
