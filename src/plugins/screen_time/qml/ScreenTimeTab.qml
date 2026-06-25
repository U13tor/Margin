// ScreenTimeTab — M2-C6 report UI.
//
// Three views driven by `screen_time` Q_PROPERTYs:
//   - day:   top apps (MBarChart) + category breakdown (MDonutChart)
//   - week:  daily totals (MBarChart, last 7 days)
//   - month: daily totals (MBarChart, last 30 days)
//
// A periodic QTimer (60s, plugin-owned) refreshes the report cache; the user
// can also hit the refresh button. The live "已持续" counter ticks off a local
// 1s Timer (nowMs) so it advances without a DB round-trip.
//
// `screen_time` is the context property registered by
// ScreenTimePlugin::onLoad via QmlService::registerContextProperty.

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Margin.Ui.Primitives
import Margin.Ui.Charts

Rectangle {
    id: root
    objectName: "screenTimeTab"
    color: Theme.bgBase
    // PR8: clip so month view's 30 bars (which can overflow at minimum window
    // height) don't bleed past the panel into the StatusBar. The day view
    // (default) fits comfortably; week fits; month is the only one that gets
    // near the limit.
    clip: true
    // PR8: bind to parent (Loader) size. StackLayout resizes its child Loader
    // when the window resizes, but the Loader does NOT auto-propagate its
    // size to the loaded item — so without this binding the Rectangle (and
    // everything anchored inside it) stays at the initial window size while
    // the user drags the window wider, producing the "empty space on the
    // right" + "value text cut off" symptom. Other tabs (Aura/Overview) hide
    // this because their content fits at 480px; ScreenTimeTab's horizontal
    // bar chart exposes it.
    width: parent ? parent.width : 480
    height: parent ? parent.height : 720

    // Ticking clock for the live "已持续 X" label. A bare Date.now() inside a
    // binding never re-evaluates on its own, so the label needs a real
    // property dependency that a 1s Timer advances.
    property double nowMs: Date.now()
    readonly property int textThreshold: qsTr("刷新") === "Refresh" ? 640 : 520

    function formatDuration(ms) {
        if (ms <= 0) return "0s"
        const totalSec = Math.floor(ms / 1000)
        const h = Math.floor(totalSec / 3600)
        const m = Math.floor((totalSec % 3600) / 60)
        const s = totalSec % 60
        if (h > 0)  return qsTr("%1h %2m").arg(h).arg(m)
        if (m > 0)  return qsTr("%1m %2s").arg(m).arg(s)
        return qsTr("%1s").arg(s)
    }

    // YYYYMMDD int → "MM-DD".
    function formatDay(d) {
        const mm = Math.floor((d % 10000) / 100)
        const dd = d % 100
        return qsTr("%1-%2").arg(String(mm).padStart(2, '0'))
                              .arg(String(dd).padStart(2, '0'))
    }

    // Sum today's category durations for the donut centre label.
    function categoryTotalMs() {
        let t = 0
        const m = screen_time.categoryBreakdown
        for (let i = 0; i < m.length; ++i) t += m[i].durationMs
        return t
    }

    Timer {
        interval: 1000
        running: !screen_time.isUserIdle && screen_time.currentApp.length > 0
        repeat: true
        onTriggered: nowMs = Date.now()
    }

    // PR8 (user feedback "下面有个条子太丑了"): the outer ScrollView was the
    // source of the bottom scrollbar — when content (header + live status +
    // top apps + donut + pickup line) exceeded 720px it popped a vertical
    // scrollbar whose lower thumb sat ugly at the bottom of the panel. Fix
    // is structural, not stylistic: drop the scroll chrome and cap the bar
    // chart at 5 entries (plugin side, ScreenTimePlugin::rebuildReportCache)
    // so the whole view fits one viewport. Mirrors AuraTab's pattern
    // (anchors.fill ColumnLayout, no ScrollView).
    ColumnLayout {
        id: rootLayout
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        // ── Header + view toggle ──────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Text {
                text: qsTr("应用时长")
                color: Theme.fgPrimary
                font.pixelSize: Theme.textBase
                font.weight: Font.DemiBold
            }
            Item { Layout.fillWidth: true }

            // Segmented toggle. Exclusivity comes from the single source
            // of truth (screen_time.viewMode) — no ButtonGroup needed.
            // Selected = Primary accent fill, others = Secondary outline.
            Repeater {
                model: [
                    { key: "day",   label: qsTr("今日") },
                    { key: "week",  label: qsTr("本周") },
                    { key: "month", label: qsTr("本月") }
                ]
                delegate: MButton {
                    objectName: "viewButton_" + modelData.key
                    text: modelData.label
                    horizontalPadding: Theme.space2
                    variant: screen_time.viewMode === modelData.key
                             ? MButton.Variant.Primary
                             : MButton.Variant.Secondary
                    onClicked: screen_time.viewMode = modelData.key
                }
            }

            MButton {
                objectName: "refreshButton"
                variant: MButton.Variant.Secondary
                iconSource: "qrc:/icons/icon-update.svg"
                text: root.width > root.textThreshold ? qsTr("刷新") : ""
                onClicked: screen_time.refreshReport()
            }
            MButton {
                objectName: "dataMenuButton"
                variant: MButton.Variant.Secondary
                iconSource: "qrc:/icons/icon-database.svg"
                text: root.width > root.textThreshold ? qsTr("数据…") : ""
                onClicked: exportClearDialog.open()
            }
            MButton {
                objectName: "settingsButton"
                variant: MButton.Variant.Ghost
                iconSource: "qrc:/icons/settings.svg"
                text: root.width > root.textThreshold ? qsTr("设置") : ""
                // PR5: open Margin Settings → Screen Time page (pageId
                // "screen_time", mirrors SettingsPageContributor::PageInfo.id
                // in ScreenTimePlugin.cpp). The inline idle-threshold SpinBox
                // was removed — idle threshold is now unified in Margin
                // Settings per user bug #4 "设置重复". typeof guard mirrors
                // AuraTab.qml's openSettingsButton.
                onClicked: {
                    if (typeof settingsRoot !== "undefined" && settingsRoot) {
                        settingsRoot.openSettings("screen_time")
                    }
                }
            }
        }

        // ── Live status ────────────────────────────────────────
        MCard {
            Layout.fillWidth: true
            implicitHeight: 88

            ColumnLayout {
                anchors.fill: parent
                spacing: 4

                Text {
                    text: screen_time.isUserIdle
                          ? qsTr("💤 闲置中")
                          : (screen_time.currentApp.length > 0
                             ? qsTr("前台: %1").arg(screen_time.currentApp)
                             : qsTr("等待第一个窗口切换…"))
                    color: Theme.fgPrimary
                    font.pixelSize: Theme.textSm
                    font.weight: Font.DemiBold
                }
                Text {
                    visible: !screen_time.isUserIdle && screen_time.currentApp.length > 0
                    text: qsTr("已持续 %1").arg(formatDuration(nowMs - screen_time.currentSessionStartedAt))
                    color: Theme.fgSecondary
                    font.pixelSize: Theme.textXs
                }
                Text {
                    // Pickup count moved here from the bottom-of-tab float
                    // (was an orphan "拿起次数(从闲置恢复): N" line). Desktop-
                    // appropriate copy — "闲置恢复次数" mirrors the code path
                    // InputMonitorService::userIdleStateChanged(false).
                    // Day-view only: pickupCountByDay() queries today, so
                    // week/month would show a stale/meaningless number.
                    visible: screen_time.viewMode === "day"
                    text: qsTr("闲置恢复次数: %1").arg(screen_time.pickupCount)
                    color: Theme.fgMuted
                    font.pixelSize: Theme.textXs
                }
                Item { Layout.fillHeight: true }
            }
        }

        // PR1 (round-2 #4b): pie + list merged into a single MCard.
        // User explicitly: "扇形图在上 列表在下 并且在同一个框里面，没有割裂感"
        // — order reversed from previous (which had list first). Both sections
        // share one elevated surface. Single visibility gate on the card —
        // empty sections still render their own empty state inside.
        // ── Week / month view: daily totals ───────────────────
        Text {
            Layout.fillWidth: true
            visible: screen_time.viewMode !== "day" && screen_time.dailyTotals.length > 0
            text: screen_time.viewMode === "week" ? qsTr("每日总时长(近 7 天)") : qsTr("每日总时长(近 30 天)")
            color: Theme.fgSecondary
            font.pixelSize: Theme.textSm
            font.weight: Font.DemiBold
        }
        ScrollView {
            id: dailyTotalsScroll
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(dailyTotalsChart.implicitHeight, 154) // max 7 rows (7 * 22)
            visible: screen_time.viewMode !== "day" && screen_time.dailyTotals.length > 0
            clip: true
            ScrollBar.vertical.policy: ScrollBar.AsNeeded

            MBarChart {
                id: dailyTotalsChart
                width: dailyTotalsScroll.width - (dailyTotalsScroll.ScrollBar.vertical.visible ? dailyTotalsScroll.ScrollBar.vertical.width : 0)
                model: screen_time.dailyTotals
                valueKey: "durationMs"
                labelKey: "day"
                barColor: Theme.accentSuccess
                barHeight: 12
                labelFormatter: function(row) { return formatDay(row.day) }
                valueFormatter: formatDuration

                selectKey: "day"
                selectedValue: screen_time.selectedDay
                onClicked: function(rowData) {
                    if (screen_time.selectedDay === rowData.day) {
                        screen_time.selectedDay = 0
                    } else {
                        screen_time.selectedDay = rowData.day
                    }
                }
            }
        }

        // PR1 (round-2 #4b): pie + list merged into a single MCard.
        // User explicitly: "扇形图在上 列表在下 并且在同一个框里面，没有割裂感"
        // — order reversed from previous (which had list first). Both sections
        // share one elevated surface. Single visibility gate on the card —
        // empty sections still render their own empty state inside.
        MCard {
            id: statsCard
            Layout.fillWidth: true
            implicitHeight: statsLayout.implicitHeight + 2 * padding
            visible: screen_time.topApps.length > 0
                     || screen_time.categoryBreakdown.length > 0

            ColumnLayout {
                id: statsLayout
                anchors.fill: parent
                spacing: Theme.space3

                // ── Range view header (top) ────────────────────────────
                RowLayout {
                    Layout.fillWidth: true
                    visible: screen_time.viewMode !== "day"

                    Text {
                        text: {
                            if (screen_time.selectedDay > 0) {
                                return qsTr("单日详情 (%1)").arg(formatDay(screen_time.selectedDay))
                            } else {
                                return screen_time.viewMode === "week" ? qsTr("本周汇总") : qsTr("本月汇总")
                            }
                        }
                        color: Theme.fgPrimary
                        font.pixelSize: Theme.textSm
                        font.weight: Font.DemiBold
                    }

                    Item { Layout.fillWidth: true }

                    MButton {
                        visible: screen_time.selectedDay > 0
                        text: screen_time.viewMode === "week" ? qsTr("返回周汇总") : qsTr("返回月汇总")
                        variant: MButton.Variant.Secondary
                        horizontalPadding: Theme.space2
                        onClicked: screen_time.selectedDay = 0
                    }
                }

                // ── Category donut (top) ────────────────────────────
                MDonutChart {
                    Layout.alignment: Qt.AlignHCenter
                    visible: screen_time.categoryBreakdown.length > 0
                    model: screen_time.categoryBreakdown
                    valueKey: "durationMs"
                    labelKey: "category"
                    centerPrimary: formatDuration(categoryTotalMs())
                    centerSecondary: qsTr("总计")
                }

                // ── App ranking (bottom) ───────────────────────────
                // No "应用排行" sub-header — it anchored to the card's left
                // edge while the bar tracks start at nameLabel.right (~148px
                // in), producing the "文字和柱状图没左对齐" misalignment bug.
                // OverviewTab.qml's topAppsCard follows the same pattern:
                // card title above, MBarChart directly below, no mid title.
                MBarChart {
                    Layout.fillWidth: true
                    visible: screen_time.topApps.length > 0
                    model: screen_time.topApps
                    valueKey: "durationMs"
                    labelKey: "name"
                    iconKey: "exePath"  // PR4 round-2 #4a: image://appicon/<exePath>
                    barColor: Theme.accentBrand
                    valueFormatter: formatDuration
                }
            }
        }

        // ── Empty-state hint ──────────────────────────────────
        Text {
            Layout.fillWidth: true
            visible: (screen_time.viewMode === "day" &&
                      screen_time.topApps.length === 0) ||
                     (screen_time.viewMode !== "day" &&
                      screen_time.dailyTotals.length === 0)
            text: qsTr("还没有数据 — 切换几个窗口让 Margin 记录一些活动")
            color: Theme.fgMuted
            font.pixelSize: Theme.textXs
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }

        // Spring — pushes nothing into the scrollbar (there is no scroll
        // bar now), but keeps the layout honest if future sections get
        // added above. Without this, a Layout's children stay top-aligned
        // with a gap at the bottom, which is what we want.
        Item { Layout.fillHeight: true; visible: rootLayout.height > 0 }
    }

    ExportClearDialog {
        id: exportClearDialog
        objectName: "exportClearDialog"
        anchors.fill: parent
    }
}
