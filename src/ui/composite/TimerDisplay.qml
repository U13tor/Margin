// TimerDisplay — composite big-mono countdown / duration display
// (docs/06 §6.3). Renders a large mono-font time string (text3xl = 32px,
// JetBrains Mono, DemiBold) with an optional caption label below.
//
// API:
//   seconds: int (default 0; negative clamps to 0 so a briefly negative
//                 remainingSeconds from a tick/state-transition race
//                 doesn't render "-00:01")
//   format:  TimerDisplay.Format.MmSs (default) | TimerDisplay.Format.HhMmSs
//   text:    string override (non-empty wins over seconds + format — lets
//            callers reuse TimerDisplay's visual treatment for non-duration
//            strings like a clock time "12:30" or a percentage)
//   label:   string caption below the number (empty → hidden)
//
// Not a card — caller composes it inside an MCard. Mirrors the pre-M4-C7
// countdown Text block in RhythmTab.qml but pulled out as a reusable
// composite so other tabs (Screen Time totals, future Overview cards) can
// share the same large-mono styling without re-rolling font properties.
//
// Does NOT import QtQuick.Controls (docs/15 §B2). ColumnLayout root lets it
// stack vertically with its own caption; Layout.fillWidth: true makes it
// fill the host card horizontally when embedded in another Layout.

import QtQuick
import QtQuick.Layouts
import Margin.Ui.Primitives

ColumnLayout {
    id: root

    // Fill the host card's width so the inner AlignHCenter actually centers
    // the number visually (otherwise TimerDisplay hugs its content and the
    // "centered" number reads as left-aligned in the card). Attached Layout
    // properties are no-ops when the parent isn't a Layout, so standalone
    // test probes aren't affected.
    Layout.fillWidth: true

    enum Format { MmSs, HhMmSs }

    property int seconds: 0
    property int format: TimerDisplay.Format.MmSs
    property string text: ""
    property string label: ""

    readonly property bool _hasLabel: root.label !== ""

    readonly property string _derivedText: {
        var sec = root.seconds < 0 ? 0 : Math.floor(root.seconds)
        var h = Math.floor(sec / 3600)
        var m = Math.floor((sec % 3600) / 60)
        var s = sec % 60
        function pad(n) { return (n < 10 ? "0" : "") + n }
        if (root.format === TimerDisplay.Format.HhMmSs) {
            return pad(h) + ":" + pad(m) + ":" + pad(s)
        }
        return pad(m) + ":" + pad(s)
    }

    readonly property string _displayText: root.text !== "" ? root.text : root._derivedText

    spacing: Theme.space1

    Text {
        text: root._displayText
        color: Theme.fgPrimary
        font.pixelSize: Theme.text3xl
        font.weight: Font.DemiBold
        font.family: Theme.fontMono
        Layout.alignment: Qt.AlignHCenter
    }

    Text {
        text: root.label
        color: Theme.fgMuted
        font.pixelSize: Theme.textXs
        font.family: Theme.fontSans
        visible: root._hasLabel
        Layout.alignment: Qt.AlignHCenter
    }
}
