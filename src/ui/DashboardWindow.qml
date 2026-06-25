// DashboardWindow — Layer 1 main panel shell (docs/06 §3.1, §4.1). Starts
// hidden and is opened from the tray (docs/06 §3.2 "Layer 0 → Layer 1"); the
// close button hides it back to the tray rather than quitting. TabBar (top)
// + content area (StackLayout, middle) + StatusBar (bottom). Tab list and
// per-tab content come from the `dashboardTabs` context property injected by
// HostCore (host's Overview plus any plugin-contributed tabs). M0 ships
// exactly one tab (Overview); M1+ plugin tabs slot in dynamically.

import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import Margin.Ui.Primitives

Window {
    id: root
    objectName: "dashboardWindow"

    // §3.4 default size 480 × 720 (Raycast-like tall panel).
    width: 480
    height: 720
    minimumWidth: 360
    minimumHeight: 480

    visible: false
    color: Theme.bgBase
    title: qsTr("Margin · 息间")

    property int currentTab: 0

    readonly property int _tabCount: dashboardTabs ? dashboardTabs.tabs.length : 0

    // Invoked from the host (SystemTray::openDashboardRequested). Show + focus.
    function openDashboard() {
        root.show();
        root.raise();
        root.requestActivate();
    }

    // Close → hide to tray. quitOnLastWindowClosed is false (set by the host),
    // so vetoing the close keeps the process alive in the tray.
    onClosing: function(close) {
        close.accepted = false;
        root.visible = false;
    }

    Column {
        anchors.fill: parent

        TabBar {
            id: tabBar
            width: parent.width
            // One-way binding from Window.currentTab → TabBar.currentTab.
            // Never write to tabBar.currentTab directly — doing so breaks
            // this binding (QML replaces the binding with the literal value).
            // Tab clicks come up via tabActivated; we set Window.currentTab
            // and the binding propagates the value back into TabBar.
            currentTab: root.currentTab
            onTabActivated: function(index) { root.currentTab = index }
        }

        StackLayout {
            id: contentArea
            objectName: "contentArea"
            width: parent.width
            height: parent.height - tabBar.height - statusBar.height
            currentIndex: root.currentTab

            // Single model shared with TabBar — host tab + plugin tabs render
            // through one Repeater (docs/06 §3.1).
            Repeater {
                model: dashboardTabs ? dashboardTabs.tabs : []
                Loader {
                    source: modelData.contentQml
                    width: parent.width
                    height: parent.height
                }
            }
        }

        StatusBar {
            id: statusBar
            width: parent.width
            // marginVersion context property is injected by the host; fall back
            // to a dev marker if the shell is loaded without it (e.g. tests).
            version: (typeof marginVersion !== "undefined") ? marginVersion
                                                            : "0.0.0-dev"
        }
    }

    // Ctrl+Tab / Ctrl+Shift+Tab (docs/06 §3.3, M0-C10). Loops based on the
    // live tab count; with one tab the shortcuts are no-ops (no error).
    Shortcut {
        sequence: "Ctrl+Tab"
        onActivated: {
            const n = root._tabCount
            if (n > 1) root.currentTab = (root.currentTab + 1) % n
        }
    }
    Shortcut {
        sequence: "Ctrl+Shift+Tab"
        onActivated: {
            const n = root._tabCount
            if (n > 1) root.currentTab = (root.currentTab - 1 + n) % n
        }
    }

    // Ctrl+, opens the Settings window (docs/06 §4.6, M5-C3). settingsRoot
    // is the SettingsWindow's QWindow exposed by HostCore as a context
    // property — its openSettings() QML helper mirrors this Window's
    // openDashboard(). typeof guard keeps the shortcut a no-op (no error)
    // when settingsRoot is missing (e.g., bare test shells).
    Shortcut {
        sequence: "Ctrl+,"
        onActivated: {
            if (typeof settingsRoot !== "undefined" && settingsRoot) {
                settingsRoot.openSettings()
            }
        }
    }
}
