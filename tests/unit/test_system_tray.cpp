// tests/unit/test_system_tray.cpp
//
// M4-C16: verifies the 5-section menu structure (Header / Toggle group /
// Preview group / Open+About / Quit) and the refresh-on-demand mechanism
// (setContributorLookup + refreshPluginMenu). Runs without a window manager
// — QSystemTrayIcon is constructed but never shown.

#include <QAction>
#include <QList>
#include <QMenu>
#include <QObject>
#include <QSignalSpy>
#include <QString>
#include <QTest>

#include <functional>
#include <string>

#include "Margin/TrayMenuContributor.h"
#include "host/core/SystemTray.h"

namespace {

class FakeContributor : public Margin::TrayMenuContributor {
public:
    std::function<QList<Margin::TrayMenuContributor::TrayItem>()> gen;
    QList<Margin::TrayMenuContributor::TrayItem> contributeTrayItems() override {
        return gen ? gen() : QList<Margin::TrayMenuContributor::TrayItem>{};
    }
    void onTrayItemClicked(const std::string&) override {}
};

QAction* findActionByText(QMenu* menu, const QString& text) {
    const auto actions = menu->actions();
    for (QAction* a : actions) {
        if (a->text() == text) return a;
    }
    return nullptr;
}

} // namespace

class TestSystemTray : public QObject {
    Q_OBJECT

private slots:
    void headerIsFirstAndDisabled() {
        Margin::SystemTray tray;
        QMenu* m = tray.menuForTesting();
        QVERIFY(m);
        const auto actions = m->actions();
        QVERIFY(!actions.isEmpty());
        QAction* header = actions.first();
        QVERIFY(!header->isEnabled());
        QVERIFY(header->text().contains(QStringLiteral("Margin")));
    }

    void quitIsLast() {
        Margin::SystemTray tray;
        const auto actions = tray.menuForTesting()->actions();
        QVERIFY(!actions.isEmpty());
        QCOMPARE(actions.last()->text(), QStringLiteral("Quit"));
    }

    void aboutActionPresent() {
        Margin::SystemTray tray;
        QVERIFY(findActionByText(tray.menuForTesting(),
                                 QStringLiteral("About")) != nullptr);
    }

    void openDashboardActionPresent() {
        Margin::SystemTray tray;
        QVERIFY(findActionByText(tray.menuForTesting(),
                                 QStringLiteral("Open Dashboard...")) != nullptr);
    }

    void toggleItemsRenderedCheckable() {
        Margin::SystemTray tray;
        Margin::TrayMenuContributor::TrayItem item;
        item.id = "toggle";
        item.label = "Aura Locker: ON";
        item.checkable = true;
        item.checked = true;
        tray.addPluginItems("aura", { item });

        QAction* a = findActionByText(tray.menuForTesting(),
                                      QStringLiteral("Aura Locker: ON"));
        QVERIFY(a);
        QVERIFY(a->isCheckable());
        QVERIFY(a->isChecked());
        QVERIFY(a->isEnabled());
    }

    void readOnlyItemsRenderedDisabled() {
        Margin::SystemTray tray;
        Margin::TrayMenuContributor::TrayItem item;
        item.id = "preview_focus";
        item.label = "Today's Focus: 2h 15m";
        item.read_only = true;
        tray.addPluginItems("screen_time", { item });

        QAction* a = findActionByText(tray.menuForTesting(),
                                      QStringLiteral("Today's Focus: 2h 15m"));
        QVERIFY(a);
        QVERIFY(!a->isEnabled());
        QVERIFY(!a->isCheckable());
    }

    void toggleGroupRenderedBeforePreviewGroup() {
        // Aura toggle + ScreenTime preview together: header → toggle → preview
        // → actions → quit. Confirm toggle appears before preview in the menu.
        Margin::SystemTray tray;
        Margin::TrayMenuContributor::TrayItem toggle;
        toggle.id = "toggle";
        toggle.label = "Aura Locker: ON";
        toggle.checkable = true;
        toggle.checked = true;
        Margin::TrayMenuContributor::TrayItem preview;
        preview.id = "preview_focus";
        preview.label = "Today's Focus: 2h 15m";
        preview.read_only = true;
        tray.addPluginItems("aura", { toggle });
        tray.addPluginItems("screen_time", { preview });

        const auto actions = tray.menuForTesting()->actions();
        int toggleIdx = -1, previewIdx = -1;
        for (int i = 0; i < actions.size(); ++i) {
            if (actions[i]->text() == "Aura Locker: ON") toggleIdx = i;
            if (actions[i]->text() == "Today's Focus: 2h 15m") previewIdx = i;
        }
        QVERIFY(toggleIdx >= 0);
        QVERIFY(previewIdx >= 0);
        QVERIFY(toggleIdx < previewIdx);
    }

    void refreshUpdatesLabel() {
        Margin::SystemTray tray;
        FakeContributor c;
        bool off = true;
        c.gen = [&]() {
            Margin::TrayMenuContributor::TrayItem item;
            item.id = "toggle";
            item.label = off ? "Aura Locker: OFF" : "Aura Locker: ON";
            item.checkable = true;
            item.checked = !off;
            return QList<Margin::TrayMenuContributor::TrayItem>{ item };
        };
        tray.setContributorLookup(
            [&](const QString&) -> Margin::TrayMenuContributor* { return &c; });
        tray.addPluginItems("aura", c.contributeTrayItems());

        QVERIFY(findActionByText(tray.menuForTesting(),
                                 QStringLiteral("Aura Locker: OFF")) != nullptr);
        QVERIFY(findActionByText(tray.menuForTesting(),
                                 QStringLiteral("Aura Locker: ON")) == nullptr);

        off = false;
        tray.refreshPluginMenu(QStringLiteral("aura"));

        QVERIFY(findActionByText(tray.menuForTesting(),
                                 QStringLiteral("Aura Locker: ON")) != nullptr);
        QVERIFY(findActionByText(tray.menuForTesting(),
                                 QStringLiteral("Aura Locker: OFF")) == nullptr);
    }

    void refreshWithoutLookupIsSafe() {
        // Defensive: refreshPluginMenu without a lookup must not crash.
        Margin::SystemTray tray;
        tray.refreshPluginMenu(QStringLiteral("any"));
        QVERIFY(true);
    }

    void toggleClickEmitsSignal() {
        Margin::SystemTray tray;
        Margin::TrayMenuContributor::TrayItem item;
        item.id = "toggle";
        item.label = "Aura Locker: ON";
        item.checkable = true;
        tray.addPluginItems("aura", { item });

        QSignalSpy spy(&tray, &Margin::SystemTray::pluginItemClicked);
        QAction* toggleAction = findActionByText(
            tray.menuForTesting(), QStringLiteral("Aura Locker: ON"));
        QVERIFY(toggleAction);
        emit toggleAction->triggered();
        QCOMPARE(spy.count(), 1);
        const auto args = spy.takeFirst();
        QCOMPARE(args.at(0).toString(), QStringLiteral("aura"));
        QCOMPARE(args.at(1).toString(), QStringLiteral("toggle"));
    }

    void aboutActionEmitsOpenSettingsAbout() {
        // PR1: tray About routes through openSettingsRequested("about") so
        // HostCore turns it into a SettingsWindow → About page navigation,
        // retiring the legacy native QDialog path.
        Margin::SystemTray tray;
        QSignalSpy spy(&tray, &Margin::SystemTray::openSettingsRequested);
        QAction* aboutAction = findActionByText(tray.menuForTesting(),
                                                QStringLiteral("About"));
        QVERIFY(aboutAction);
        emit aboutAction->triggered();
        QCOMPARE(spy.count(), 1);
        const auto args = spy.takeFirst();
        QCOMPARE(args.at(0).toString(), QStringLiteral("about"));
    }

    void settingsActionEmitsOpenSettingsEmpty() {
        // PR1: tray Settings passes empty pageId so SettingsWindow keeps the
        // sidebar on the default (General) — no behaviour change vs. pre-PR1
        // except the signal now carries an explicit empty arg.
        Margin::SystemTray tray;
        QSignalSpy spy(&tray, &Margin::SystemTray::openSettingsRequested);
        QAction* settingsAction = findActionByText(tray.menuForTesting(),
                                                   QStringLiteral("Settings..."));
        QVERIFY(settingsAction);
        emit settingsAction->triggered();
        QCOMPARE(spy.count(), 1);
        const auto args = spy.takeFirst();
        QCOMPARE(args.at(0).toString(), QString());
    }
};

QTEST_MAIN(TestSystemTray)
#include "test_system_tray.moc"
