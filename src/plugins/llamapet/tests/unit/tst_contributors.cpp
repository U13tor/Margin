#include <QTest>
#include "app/LlamaPetPlugin.h"

using namespace Margin::Plugins::LlamaPet;

class TestContributors : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testDashboardTabInfo() {
        LlamaPetPlugin plugin;
        auto* tab = plugin.asDashboardTab();
        QVERIFY(tab != nullptr);

        auto info = tab->tabInfo();
        QCOMPARE(QString::fromStdString(info.id), QStringLiteral("llamapet"));
        QCOMPARE(info.content_qml.toString(), QStringLiteral("qrc:/llamapet/ui/HudTab.qml"));
        QCOMPARE(info.order, 50);
    }

    void testSettingsPageInfo() {
        LlamaPetPlugin plugin;
        auto* page = plugin.asSettingsPage();
        QVERIFY(page != nullptr);

        auto info = page->pageInfo();
        QCOMPARE(QString::fromStdString(info.id), QStringLiteral("llamapet"));
        QCOMPARE(info.content_qml.toString(), QStringLiteral("qrc:/llamapet/ui/SettingsPage.qml"));
        QCOMPARE(info.order, 50);
    }

    void testTrayMenuContributeAndClick() {
        LlamaPetPlugin plugin;
        auto* tray = plugin.asTrayMenu();
        QVERIFY(tray != nullptr);

        auto items = tray->contributeTrayItems();
        QVERIFY(!items.isEmpty());

        QStringList ids;
        for (const auto& item : items) {
            ids.append(QString::fromStdString(item.id));
        }

        QVERIFY(ids.contains(QStringLiteral("toggle_always_on_top")));
        QVERIFY(ids.contains(QStringLiteral("toggle_click_through")));
        QVERIFY(ids.contains(QStringLiteral("toggle_auto_dock")));
        QVERIFY(ids.contains(QStringLiteral("form_mini_pet")));
        QVERIFY(ids.contains(QStringLiteral("form_dock")));
        QVERIFY(ids.contains(QStringLiteral("copy_restart_cmd")));
        QVERIFY(ids.contains(QStringLiteral("erase_idle_kv")));

        // 验证点击无崩溃
        tray->onTrayItemClicked("copy_restart_cmd");
        tray->onTrayItemClicked("toggle_always_on_top");
        tray->onTrayItemClicked("form_dock");
        tray->onTrayItemClicked("erase_idle_kv");
    }
};

QTEST_MAIN(TestContributors)
#include "tst_contributors.moc"
