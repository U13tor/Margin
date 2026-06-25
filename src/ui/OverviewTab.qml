// OverviewTab — Layer 1 Tab 1 content (docs/06 §3.1, §4.2). Host-owned
// summary view. M4-C8 replaces the inline OverviewCard component (3 ×
// hardcoded "—") with three StatusCard instances bound to root-context
// plugin properties (screen_time / rhythm / aura). typeof guards let the
// tab load even when a plugin is absent (regression-friendly: test dashboards
// inject stubs for just the plugins they need). Clicking a card switches
// to that plugin's tab via Window.window — pure QML, zero Host changes.
//
// Promoted out of M0-skeleton status in M4-C8. The inline `component
// OverviewCard` is gone; the Margin.Ui.Composite.StatusCard atom (landed
// C7) is the canonical surface now.

import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls
import Margin.Ui.Primitives
import Margin.Ui.Composite
import Margin.Ui.Charts

Rectangle {
    objectName: "overviewTab"
    color: Theme.bgBase

    width: parent ? parent.width : 480
    height: parent ? parent.height : 720

    // Helpers hoisted to root Rectangle scope so they're created once per
    // OverviewTab instance (not per ColumnLayout instance) and callable
    // from any StatusCard child. Idiomatic QML — avoids re-creating
    // closures per StatusCard.

    // Navigate to tab by id via the Window.window attached property.
    // OverviewTab is loaded by Loader inside StackLayout inside the
    // DashboardWindow — Window.window walks the parent chain through
    // contentItem and resolves to the enclosing Window regardless of
    // nesting depth. (Item.parent chain alone stops at contentItem and
    // never reaches the Window itself, so attached prop is required.)
    // dashboardTabs.tabs is sorted by `order` 1:1 with StackLayout index,
    // so finding the array index is equivalent to finding the tab index.
    function navigateToTab(tabId) {
        var win = Window.window
        if (!win) return
        var tabs = dashboardTabs.tabs
        for (var i = 0; i < tabs.length; i++) {
            if (tabs[i]["id"] === tabId) {
                win.currentTab = i
                return
            }
        }
    }

    // Format seconds → "Xh Ym" / "Xm" / "—"
    function formatFocus(sec) {
        if (sec <= 0) return "—"
        var h = Math.floor(sec / 3600)
        var m = Math.floor((sec % 3600) / 60)
        if (h > 0) return h + "h " + m + "m"
        return m + "m"
    }

    // PR3: ColumnLayout is wrapped in a ScrollView so the "最近事件" card at
    // the bottom stays reachable when the window shrinks. Mirrors the
    // ScreenTimeTab.qml pattern. Width is bound to scrollView.width so the
    // layout tracks the viewport (and the scrollbar doesn't overlap content).
    ScrollView {
        id: scrollView
        anchors.fill: parent
        anchors.margins: 16
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width: scrollView.availableWidth
            spacing: 12

        // PR1 (round-2 #2a): 3 status cards in one row — matches
        // dashboard_overview.png prototype layout. Layout.fillWidth on each
        // StatusCard inside RowLayout = equal 1/3 width share. Card height
        // comes from StatusCard.implicitHeight (90px) — RowLayout honors the
        // tallest child, so all three align.
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space2

            StatusCard {
                objectName: "overviewCardFocus"
                Layout.fillWidth: true
                title: qsTr("今日专注")
                value: typeof screen_time !== "undefined" && screen_time.todayFocusSeconds > 0
                       ? formatFocus(screen_time.todayFocusSeconds)
                       : "—"
                // Hide subtitle when value is "—" — avoids the mixed signal of
                // "—" value + "点击查看详情" subtitle.
                subtitle: value !== "—"
                          ? qsTr("点击查看详情")
                          : ""
                onClicked: navigateToTab("screen_time")
            }

            StatusCard {
                objectName: "overviewCardRhythm"
                Layout.fillWidth: true
                title: qsTr("健康节律")
                // Show the next break's wall-clock time (HH:mm) while a
                // pomodoro is running so the card carries actionable info
                // even before any round completes today — matches docs/06
                // §4.2 prototype ("12:30" / "下次休息"). Falls back to "—"
                // when idle or when the break is already due/active (those
                // states surface in Tab3 + the toast/overlay).
                // Edge case: when the timer is paused mid-work, the binding
                // only re-evaluates on remainingSeconds changes; the displayed
                // time goes stale until the timer resumes. Accepted for now —
                // refreshing under pause would need a wall-clock Timer.
                value: typeof rhythm !== "undefined" && rhythm.state === "working"
                       ? Qt.formatDateTime(
                             new Date(Date.now() + rhythm.remainingSeconds * 1000),
                             "HH:mm")
                       : "—"
                subtitle: value !== "—" ? qsTr("下次休息") : ""
                onClicked: navigateToTab("rhythm")
            }

            StatusCard {
                objectName: "overviewCardAura"
                Layout.fillWidth: true
                title: qsTr("蓝牙锁屏")
                value: typeof aura !== "undefined" && aura.pairedDeviceName !== ""
                       ? aura.pairedDeviceName
                       : "—"
                // Hide subtitle when value is "—" — covers the edge case of
                // device unpaired after a lock (would otherwise show "—" + "5 分钟前").
                subtitle: value !== "—" && typeof aura !== "undefined" && aura.lastLockSummary !== ""
                          ? aura.lastLockSummary
                          : ""
                onClicked: navigateToTab("aura")
            }
        }

        // Top-5 apps section (M4-C9). Hidden when screen_time is absent
        // or has no topApps yet — typeof guard mirrors the 3 status cards.
        // Category tag (per-row) deferred — see docs/12-deferred-items.md
        // §A25 (MBarChart has no trailing slot; not worth extending for
        // one caller).
        MCard {
            id: topAppsCard
            objectName: "overviewCardTopApps"
            Layout.fillWidth: true
            implicitHeight: 220
            visible: typeof screen_time !== "undefined"
                     && screen_time.topApps.length > 0

            // Background click → Tab2 (same target as header [→]).
            // MouseArea sits underneath the content ColumnLayout — MButton
            // in header wins first via Qt's top-down hit test (same
            // pattern as StatusCard.qml:54-59).
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: navigateToTab("screen_time")
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: Theme.space2

                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        text: qsTr("应用时长 · 今日 Top 5")
                        color: Theme.fgSecondary
                        font.pixelSize: Theme.textSm
                        font.weight: Font.DemiBold
                        Layout.fillWidth: true
                    }

                    MButton {
                        objectName: "topAppsArrow"
                        variant: MButton.Variant.Ghost
                        iconSource: "qrc:/icons/icon-arrow-right.svg"
                        onClicked: navigateToTab("screen_time")
                    }
                }

                MBarChart {
                    objectName: "topAppsBar"
                    Layout.fillWidth: true
                    model: screen_time.topApps.slice(0, 5)
                    valueKey: "durationMs"
                    labelKey: "name"
                    iconKey: "exePath"  // PR4 round-2 #2b: image://appicon/<exePath>
                    barColor: Theme.accentBrand
                    // Plugin exposes durationMs (ms); formatFocus takes
                    // seconds — divide inline. Math.floor inside formatFocus
                    // tolerates the float result.
                    valueFormatter: function(ms) { return formatFocus(ms / 1000) }
                }
            }
        }

        // Recent events card (M4-C9b.2). Cross-plugin timeline driven by
        // Host's ActivityFeed service (context property `activityFeed`).
        // Hidden only when feed is absent (typeof guard) — empty feed
        // shows an empty-state hint so first-time users see the feature
        // exists (prototype docs/06 §4.2: "首次启动时显示'暂无数据'").
        // Delegate is a custom inline RowLayout rather than MListItem
        // because the row shape "time + per-event-colored dot +
        // description" doesn't fit MListItem's icon/title/subtitle/
        // trailing slots (plan §D5).
        MCard {
            id: recentEventsCard
            objectName: "overviewCardRecentEvents"
            Layout.fillWidth: true
            visible: typeof activityFeed !== "undefined"

            ColumnLayout {
                anchors.fill: parent
                spacing: Theme.space2

                Text {
                    Layout.fillWidth: true
                    text: qsTr("最近事件")
                    color: Theme.fgSecondary
                    font.pixelSize: Theme.textSm
                    font.weight: Font.DemiBold
                }

                // Empty-state hint. Hidden once any event lands;
                // Repeater below instantiates zero delegates when the
                // model is empty so no row-vs-hint collision.
                Text {
                    objectName: "recentEventsEmptyHint"
                    Layout.fillWidth: true
                    visible: activityFeed.events.length === 0
                    text: qsTr("暂无数据,使用后自动出现")
                    color: Theme.fgMuted
                    font.pixelSize: Theme.textXs
                    horizontalAlignment: Text.AlignHCenter
                    topPadding: Theme.space3
                    bottomPadding: Theme.space3
                }

                Repeater {
                    objectName: "recentEventsRepeater"
                    // Feed buffer is 20 (Host-side cap); card shows the
                    // 4 most recent per prototype (dashboard_overview.png).
                    model: typeof activityFeed !== "undefined"
                           ? activityFeed.events.slice(0, 4)
                           : []

                    delegate: RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2

                        // Time: HH:mm (24h, locale-safe via Qt.formatDateTime).
                        // Fixed width so descriptions align across rows.
                        Text {
                            text: Qt.formatDateTime(new Date(modelData.timeMs), "HH:mm")
                            color: Theme.fgMuted
                            font.pixelSize: Theme.textXs
                            font.family: Theme.fontMono
                            Layout.preferredWidth: 42
                        }

                        // Colored dot — colorRole is a string token resolved
                        // against Theme (accentDanger/accentSuccess/etc.).
                        // Fallback to fgMuted catches typos and any future
                        // topic whose rule forgets to set a color.
                        Rectangle {
                            Layout.preferredWidth: 6
                            Layout.preferredHeight: 6
                            radius: 3
                            color: Theme[modelData.colorRole] || Theme.fgMuted
                        }

                        Text {
                            Layout.fillWidth: true
                            text: modelData.title
                            color: Theme.fgPrimary
                            font.pixelSize: Theme.textSm
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }
        }
    }
}
