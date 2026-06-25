// SettingsWindow — Layer 2 standalone window (docs/06 §4.6). 640×480 with
// a 200px sidebar + content area. Sidebar groups pages by `section`
// (host/plugins/about) using ListView's section.delegate to render "Plugins"
// / "About" headers. Content area is a StackLayout bound to sidebar's
// currentIndex; each page is a Loader pulling its QML from
// SettingsRegistry.pages[i].contentQml.
//
// Imperative show/hide mirrors OverlayContainer (M3-C4) — visible:false is
// hardcoded, C++ calls show()/raise()/requestActivate() via HostCore::
// openSettings(). This avoids the Qt 6.5 binding-eval timing issue where
// a binding-based visible flips true briefly at startup (docs/15 §B5).

import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import Margin.Ui.Primitives

Window {
    id: root
    objectName: "settingsWindow"

    width: 640
    height: 480
    minimumWidth: 640
    minimumHeight: 480

    visible: false  // C++ controls via show()/hide()
    color: Theme.bgElevated
    title: qsTr("Margin Settings")

    // QML-side open helper. Tray signal goes through HostCore::openSettings()
    // (which calls show/raise/requestActivate on this Window directly); the
    // DashboardWindow Ctrl+, shortcut can also call this via settingsRoot.
    //
    // pageId (optional): if non-empty and matches a page id in
    // settingsRegistry.pages, the sidebar pre-selects that page before show.
    // Empty/unknown pageId → keep currentIndex (General by default). Callers:
    //   tray About / StatusBar About → "about"
    //   tray Settings / Ctrl+, / StatusBar Settings → ""
    //   AuraTab/RhythmTab/ScreenTimeTab Settings buttons → plugin id
    function openSettings(pageId) {
        if (pageId === undefined) pageId = ""
        if (pageId !== "" && typeof settingsRegistry !== "undefined" && settingsRegistry) {
            const pages = settingsRegistry.pages
            for (let i = 0; i < pages.length; i++) {
                if (pages[i].id === pageId) {
                    sidebar.currentIndex = i
                    break
                }
            }
        }
        root.show();
        root.raise();
        root.requestActivate();
    }

    // Veto close → just hide (mirror DashboardWindow pattern). The host
    // process stays alive via quitOnLastWindowClosed=false (set in main.cpp).
    onClosing: function(close) {
        close.accepted = false;
        root.visible = false;
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ── Sidebar (200px) ──────────────────────────────────────────
        ListView {
            id: sidebar
            objectName: "settingsSidebar"
            Layout.fillHeight: true
            Layout.preferredWidth: 200
            Layout.minimumWidth: 200
            clip: true
            model: (typeof settingsRegistry !== "undefined" && settingsRegistry)
                   ? settingsRegistry.pages : []
            currentIndex: 0
            section.property: "section"
            section.delegate: Rectangle {
                width: sidebar.width
                height: sectionLbl.implicitHeight + 16
                color: Theme.bgBase
                Text {
                    id: sectionLbl
                    anchors.left: parent.left
                    anchors.leftMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    text: section === "host" ? ""
                          : section === "plugins" ? qsTr("Plugins")
                          : section === "about"   ? qsTr("About")
                                                  : ""
                    color: Theme.fgMuted
                    font.pixelSize: Theme.textXs
                    font.weight: Font.DemiBold
                    font.capitalization: Font.AllUppercase
                    visible: text.length > 0
                }
            }
            delegate: MListItem {
                objectName: "settingsEntry_" + modelData.id
                width: sidebar.width
                title: modelData.title
                highlighted: ListView.view.currentIndex === index
                onClicked: ListView.view.currentIndex = index
            }
        }

        Rectangle {
            Layout.fillHeight: true
            width: 1
            color: Theme.borderStrong
        }

        // ── Content (StackLayout) ────────────────────────────────────
        StackLayout {
            id: contentStack
            objectName: "settingsContent"
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: sidebar.currentIndex
            Repeater {
                model: (typeof settingsRegistry !== "undefined" && settingsRegistry)
                       ? settingsRegistry.pages : []
                Loader {
                    source: modelData.contentQml
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    onLoaded: {
                        if (item) {
                            item.width = width;
                            item.height = height;
                        }
                    }
                    Binding {
                        target: item
                        property: "width"
                        value: width
                        when: status === Loader.Ready
                    }
                    Binding {
                        target: item
                        property: "height"
                        value: height
                        when: status === Loader.Ready
                    }
                }
            }
        }
    }
}
