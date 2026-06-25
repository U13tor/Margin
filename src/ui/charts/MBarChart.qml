// MBarChart — horizontal bar list with one shared scale (docs/06 §6.4).
//
// Every bar is measured against a single `_maxValue` computed once per model
// change, so comparison is honest and rendering is O(n) (not O(n²) per row).
// Drop into a ColumnLayout with Layout.fillWidth: true.

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Margin.Ui.Primitives

Item {
    id: root
    implicitWidth: 200
    implicitHeight: col.implicitHeight

    // Array of objects (e.g. [{ name, durationMs }, ...]).
    property var model: []
    property string valueKey: "durationMs"
    property string labelKey: "name"
    property color barColor: Theme.accentBrand
    // (value) -> string for the trailing label + tooltip.
    property var valueFormatter: function(v) { return "" + v }
    // (row) -> string for the leading label; defaults to row[labelKey].
    property var labelFormatter: null
    // PR4 (round-2 #2b/4a): optional icon slot. When iconKey is non-empty
    // and a row's value at iconKey is a non-empty string, MBarChart
    // renders a 16×16 Image before the nameLabel using the
    // `image://appicon/<encoded-path>` scheme registered by
    // AppIconProvider. Empty/missing → no icon, nameLabel takes the full
    // labelWidth (no indent). Default empty preserves existing call sites.
    property string iconKey: ""

    // Selection support properties
    property var selectedValue: undefined
    property string selectKey: ""

    signal clicked(var rowData, int index)

    // PR1 (round-2 #4c): labelWidth / valueWidth were fixed 140 + 80 px, which
    // summed with the 8px×2 bar-track margins to 236px of non-flex space. At
    // a 480px window (default), the bar track squeezed to near-zero and
    // valueLabel's right edge could still get elided to "9m" / "4m". Making
    // both responsive to root.width (capped to the old fixed values at the
    // high end, floored to legible minima at the low end) keeps the track
    // visible and the trailing duration fully readable across 360–1200px.
    property int labelWidth: Math.max(80, Math.min(140, Math.floor(root.width * 0.35)))
    property int valueWidth: Math.max(85, Math.min(120, Math.floor(root.width * 0.20)))
    property int barHeight: 12
    property int spacing: 6

    // Single pass; re-evaluates only when model / valueKey change.
    readonly property real _maxValue: {
        let mx = 0
        for (let i = 0; i < model.length; ++i) {
            const v = model[i][valueKey]
            if (v > mx) mx = v
        }
        return mx
    }

    function _label(row) {
        return labelFormatter ? labelFormatter(row) : row[labelKey]
    }

    // PR4: build the image://appicon/<path> URL for a row. Returns ""
    // when iconKey is unset or the row's value is missing/empty — that
    // drives Image.visible so QML doesn't log a "failed to get image"
    // warning for every delegate without an icon. encodeURIComponent
    // matters because Windows paths contain ':' and '\' which would
    // otherwise confuse the URL parser.
    function _iconSource(row) {
        if (!iconKey || iconKey.length === 0) return ""
        const v = row[iconKey]
        if (!v || v.length === 0) return ""
        return "image://appicon/" + encodeURIComponent(v)
    }

    Column {
        id: col
        anchors.fill: parent
        spacing: root.spacing

        Repeater {
            model: root.model
            delegate: Item {
                id: delegateItem
                required property var modelData
                width: root.width
                height: Math.max(root.barHeight, valueLabel.implicitHeight)

                readonly property bool isInteractive: root.selectKey !== ""
                readonly property bool isSelected: isInteractive && modelData[root.selectKey] === root.selectedValue

                Rectangle {
                    anchors.fill: parent
                    radius: 4
                    z: -1
                    color: delegateItem.isSelected ? Theme.bgHover : (rowMouseArea.containsMouse && delegateItem.isInteractive ? Theme.bgHover : "transparent")
                    border.color: delegateItem.isSelected ? Theme.accentBrand : "transparent"
                    border.width: 1
                }

                MouseArea {
                    id: rowMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: delegateItem.isInteractive ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: {
                        if (delegateItem.isInteractive) {
                            root.clicked(modelData, index)
                        }
                    }
                    ToolTip.visible: containsMouse
                    ToolTip.text: root._label(modelData) + " · "
                                  + root.valueFormatter(modelData[root.valueKey])
                }

                // PR4: icon sits before nameLabel when iconKey is set and the
                // row carries a non-empty value. Empty/missing icon falls back
                // to nameLabel-left-aligned-to-parent-left, so call sites that
                // don't opt into icons see no geometry change.
                Image {
                    id: iconImage
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    width: 16
                    height: 16
                    source: root._iconSource(modelData)
                    fillMode: Image.PreserveAspectFit
                    visible: source.toString().length > 0
                }

                // PR8: we abandoned RowLayout here because Qt Quick Layouts
                // silently grows a Text item past Layout.preferredWidth when
                // its content's natural width is larger (Layout's default
                // minimumWidth = implicitWidth). That pushed the duration
                // text off the right edge at every window width we tried,
                // including 850px. Manual anchoring with explicit widths
                // is the only thing that actually enforces the geometry.
                Text {
                    id: nameLabel
                    anchors.left: parent.left
                    anchors.leftMargin: (root.iconKey !== "") ? 22 : 0
                    anchors.verticalCenter: parent.verticalCenter
                    width: root.labelWidth - anchors.leftMargin
                    text: root._label(modelData)
                    color: Theme.fgPrimary
                    font.pixelSize: Theme.textXs
                    elide: Text.ElideRight
                }

                Rectangle {
                    id: track
                    anchors.left: nameLabel.right
                    anchors.leftMargin: 8
                    anchors.right: valueLabel.left
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    height: root.barHeight
                    radius: 2
                    color: Theme.bgHover

                    Rectangle {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: root._maxValue > 0
                               ? Math.max(2, (modelData[root.valueKey] / root._maxValue) * track.width)
                               : 0
                        radius: 2
                        color: root.barColor
                    }
                }

                Text {
                    id: valueLabel
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    width: root.valueWidth
                    text: root.valueFormatter(modelData[root.valueKey])
                    color: Theme.fgSecondary
                    font.pixelSize: Theme.textXs
                    horizontalAlignment: Text.AlignRight
                    elide: Text.ElideRight
                }
            }
        }
    }
}
