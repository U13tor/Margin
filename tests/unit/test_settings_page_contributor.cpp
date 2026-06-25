// tests/unit/test_settings_page_contributor.cpp
//
// M5-C4e: verifies the SettingsPageContributor pipeline end-to-end (without
// loading any real plugin DLL):
//
//   - SettingsPageContributor interface is implementable via MI
//   - asSettingsPage() returns non-null when implemented
//   - pageInfo() fields round-trip through SettingsRegistry
//   - HostCore::bootstrap's forEachPlugin iteration pattern (add host pages,
//     iterate contributors, sort by section+order) produces the expected
//     sidebar model shape with "host" / "plugins" / "about" sections grouped.
//
// Mirrors test_dashboard_tab_contributor.cpp's structure. The fake plugin
// here stands in for an M1+ plugin to exercise the contract HostCore::
// bootstrap relies on when iterating contributors.

#include <QList>
#include <QObject>
#include <QSignalSpy>
#include <QString>
#include <QTest>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <string>

#include "Margin/PluginInterface.h"
#include "Margin/Result.h"
#include "Margin/SettingsPageContributor.h"
#include "host/core/SettingsRegistry.h"

namespace {

// Minimal stand-in for an Aura-style plugin: multiply-inherits
// PluginInterface + SettingsPageContributor. Only the page-related surface
// is exercised; other hooks are stubbed.
class FakeAuraPage : public Margin::PluginInterface,
                     public Margin::SettingsPageContributor {
public:
    std::string id() const override { return "fake_aura"; }
    std::string version() const override { return "0.0.0-test"; }
    Margin::Result<void, std::string> onLoad(const Margin::PluginContext&) override {
        return Margin::Result<void, std::string>::ok();
    }
    void onConfigChange(const QJsonObject&) override {}
    void onUnload() override {}

    Margin::SettingsPageContributor* asSettingsPage() override { return this; }

    PageInfo pageInfo() const override {
        return PageInfo{
            "aura",
            "Aura Locker",
            QUrl("qrc:/aura/icons/aura-tab.svg"),
            QUrl("qrc:/aura/qml/AuraSettingsPage.qml"),
            30,
        };
    }
};

} // namespace

class TestSettingsPageContributor : public QObject {
    Q_OBJECT

private slots:
    void fakeContributorRoundTrip();
    void asSettingsPageDefaultIsNull();
    void pageInfoFieldsPersist();
    void hostAndPluginPagesGroupBySection();
};

void TestSettingsPageContributor::fakeContributorRoundTrip() {
    FakeAuraPage plugin;
    Margin::PluginInterface* iface = &plugin;

    // HostCore::bootstrap's forEachPlugin pattern: iface->asSettingsPage()
    // → non-null → push pageInfo() into the registry.
    auto* contributor = iface->asSettingsPage();
    QVERIFY(contributor != nullptr);

    const auto info = contributor->pageInfo();
    QCOMPARE(QString::fromStdString(info.id),    QStringLiteral("aura"));
    QCOMPARE(QString::fromStdString(info.title), QStringLiteral("Aura Locker"));
    QCOMPARE(info.icon.toString(),       QStringLiteral("qrc:/aura/icons/aura-tab.svg"));
    QCOMPARE(info.content_qml.toString(), QStringLiteral("qrc:/aura/qml/AuraSettingsPage.qml"));
    QCOMPARE(info.order, 30);

    Margin::SettingsRegistry r;
    r.addPage({QString::fromStdString(info.id),
               QString::fromStdString(info.title),
               info.icon, info.content_qml,
               QStringLiteral("plugins"), info.order});
    QCOMPARE(r.pages().size(), 1);
}

void TestSettingsPageContributor::asSettingsPageDefaultIsNull() {
    // PluginInterface's default impl returns nullptr — this guards the
    // contract HostCore relies on (only plugins that actually implement
    // SettingsPageContributor land in the "plugins" section). A plugin
    // that doesn't override asSettingsPage() is silently skipped.
    class BarePlugin : public Margin::PluginInterface {
    public:
        std::string id() const override { return "bare"; }
        std::string version() const override { return "0.0.0-test"; }
        Margin::Result<void, std::string> onLoad(const Margin::PluginContext&) override {
            return Margin::Result<void, std::string>::ok();
        }
        void onConfigChange(const QJsonObject&) override {}
        void onUnload() override {}
    };

    BarePlugin p;
    QVERIFY(p.asSettingsPage() == nullptr);
}

void TestSettingsPageContributor::pageInfoFieldsPersist() {
    // Verify the SettingsRegistry cache serializes PageInfo correctly,
    // including the new "section" field (the M5-C2 addition over
    // DashboardTabRegistry).
    Margin::SettingsRegistry r;
    r.addPage({QStringLiteral("general"), QStringLiteral("General"),
               QUrl("qrc:/icons/settings.svg"),
               QUrl("qrc:/ui/SettingsGeneralPage.qml"),
               QStringLiteral("host"), 10});
    r.addPage({QStringLiteral("aura"), QStringLiteral("Aura Locker"),
               QUrl("qrc:/aura/icons/aura-tab.svg"),
               QUrl("qrc:/aura/qml/AuraSettingsPage.qml"),
               QStringLiteral("plugins"), 30});
    r.addPage({QStringLiteral("about"), QStringLiteral("About"),
               QUrl("qrc:/icons/icon-info.svg"),
               QUrl("qrc:/ui/SettingsAboutPage.qml"),
               QStringLiteral("about"), 10});

    const QVariantList pages = r.pages();
    QCOMPARE(pages.size(), 3);

    const QVariantMap general = pages[0].toMap();
    QCOMPARE(general.value("id").toString(),      QStringLiteral("general"));
    QCOMPARE(general.value("title").toString(),   QStringLiteral("General"));
    QCOMPARE(general.value("section").toString(), QStringLiteral("host"));
    QCOMPARE(general.value("order").toInt(),      10);

    const QVariantMap about = pages[2].toMap();
    QCOMPARE(about.value("section").toString(), QStringLiteral("about"));
}

void TestSettingsPageContributor::hostAndPluginPagesGroupBySection() {
    // Mirrors HostCore::bootstrap: host pages + plugins + about, then
    // sortByOrder() groups them by section rank first, then order within.
    // Even though "general" (host, order=10) and "about" (about, order=10)
    // share the same order value, section rank breaks the tie.
    FakeAuraPage plugin;
    auto* contributor = plugin.asSettingsPage();
    QVERIFY(contributor);

    Margin::SettingsRegistry r;
    // Deliberately added out of section order to prove sortByOrder groups.
    r.addPage({QStringLiteral("about"), QStringLiteral("About"),
               QUrl(), QUrl(),
               QStringLiteral("about"), 10});
    r.addPage({QStringLiteral("aura"), QString::fromStdString(contributor->pageInfo().title),
               contributor->pageInfo().icon, contributor->pageInfo().content_qml,
               QStringLiteral("plugins"), contributor->pageInfo().order});
    r.addPage({QStringLiteral("general"), QStringLiteral("General"),
               QUrl(), QUrl(),
               QStringLiteral("host"), 10});
    r.sortByOrder();

    const QVariantList pages = r.pages();
    QCOMPARE(pages.size(), 3);
    // host(10) → plugins(30) → about(10) — sections group regardless of order.
    QCOMPARE(pages[0].toMap().value("section").toString(), QStringLiteral("host"));
    QCOMPARE(pages[1].toMap().value("section").toString(), QStringLiteral("plugins"));
    QCOMPARE(pages[2].toMap().value("section").toString(), QStringLiteral("about"));
    QCOMPARE(pages[0].toMap().value("id").toString(),      QStringLiteral("general"));
    QCOMPARE(pages[1].toMap().value("id").toString(),      QStringLiteral("aura"));
    QCOMPARE(pages[2].toMap().value("id").toString(),      QStringLiteral("about"));
}

QTEST_MAIN(TestSettingsPageContributor)
#include "test_settings_page_contributor.moc"
