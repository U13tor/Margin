// tests/unit/test_settings_registry.cpp
//
// M5-C2: verifies the SettingsRegistry wiring pipeline end-to-end (without
// loading any real plugin DLL):
//
//   - SettingsRegistry addPage/sortByOrder round-trip + pagesChanged signal
//   - section field survives serialization (the field that differentiates
//     this registry from DashboardTabRegistry — sidebar grouping relies on it)
//   - SettingsPageContributor interface is implementable via MI (mirrors the
//     pattern Aura/Rhythm/ScreenTime will use), asSettingsPage() returns
//     non-null, and pageInfo() fields round-trip through the registry.
//
// The fake plugin here is intentionally NOT loaded by PluginManager — it
// stands in for a future Aura-style plugin to exercise the contract
// HostCore::bootstrap relies on when iterating contributors.

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
#include "host/core/SettingsRegistry.h"

namespace {

// Minimal stand-in for a future Aura-style plugin: multiply-inherits
// PluginInterface (so asSettingsPage() override is well-formed) and
// SettingsPageContributor. Only the settings-related surface is exercised;
// the other hooks are stubbed.
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
            QUrl("qrc:/icons/icon-settings.svg"),
            QUrl("qrc:/aura/qml/AuraSettingsPage.qml"),
            30,
        };
    }
};

} // namespace

class TestSettingsRegistry : public QObject {
    Q_OBJECT

private slots:
    void addPageEmitsSignal();
    void sortByOrderAscending();
    void pagesSerializationIncludesSection();
    void fakeContributorRoundTrip();
    void hostPlusContributorsGroupSeparately();
};

void TestSettingsRegistry::addPageEmitsSignal() {
    Margin::SettingsRegistry r;
    QSignalSpy spy(&r, &Margin::SettingsRegistry::pagesChanged);
    QCOMPARE(r.pages().size(), 0);

    r.addPage({QStringLiteral("a"), QStringLiteral("A"),
               QUrl("qrc:/icons/x.svg"), QUrl("qrc:/ui/X.qml"),
               QStringLiteral("host"), 50});
    QCOMPARE(spy.count(), 1);
    QCOMPARE(r.pages().size(), 1);
}

void TestSettingsRegistry::sortByOrderAscending() {
    Margin::SettingsRegistry r;
    QSignalSpy spy(&r, &Margin::SettingsRegistry::pagesChanged);

    // Inserted out of order on purpose.
    r.addPage({QStringLiteral("c"), QStringLiteral("C"), QUrl(), QUrl(),
               QStringLiteral("plugins"), 30});
    r.addPage({QStringLiteral("a"), QStringLiteral("A"), QUrl(), QUrl(),
               QStringLiteral("host"), 10});
    r.addPage({QStringLiteral("b"), QStringLiteral("B"), QUrl(), QUrl(),
               QStringLiteral("host"), 20});
    r.sortByOrder();

    QCOMPARE(spy.count(), 4);  // 3 addPage + 1 sort

    const QVariantList pages = r.pages();
    QCOMPARE(pages.size(), 3);
    QCOMPARE(pages[0].toMap().value("id").toString(), QStringLiteral("a"));
    QCOMPARE(pages[1].toMap().value("id").toString(), QStringLiteral("b"));
    QCOMPARE(pages[2].toMap().value("id").toString(), QStringLiteral("c"));
}

void TestSettingsRegistry::pagesSerializationIncludesSection() {
    Margin::SettingsRegistry r;
    r.addPage({QStringLiteral("general"), QStringLiteral("General"),
               QUrl("qrc:/icons/icon-settings.svg"),
               QUrl("qrc:/ui/SettingsGeneralPage.qml"),
               QStringLiteral("host"), 10});

    const QVariantList pages = r.pages();
    QCOMPARE(pages.size(), 1);
    const QVariantMap m = pages[0].toMap();
    QCOMPARE(m.value("id").toString(),         QStringLiteral("general"));
    QCOMPARE(m.value("title").toString(),      QStringLiteral("General"));
    QCOMPARE(m.value("icon").toString(),       QStringLiteral("qrc:/icons/icon-settings.svg"));
    QCOMPARE(m.value("contentQml").toString(), QStringLiteral("qrc:/ui/SettingsGeneralPage.qml"));
    // section is the field that differentiates SettingsRegistry from
    // DashboardTabRegistry — sidebar grouping depends on it.
    QCOMPARE(m.value("section").toString(),    QStringLiteral("host"));
    QCOMPARE(m.value("order").toInt(),         10);
}

void TestSettingsRegistry::fakeContributorRoundTrip() {
    FakeAuraPage plugin;
    Margin::PluginInterface* iface = &plugin;

    // HostCore's bootstrap path: iface->asSettingsPage() -> non-null ->
    // push pageInfo() into the registry (host assigns "plugins" section).
    auto* contributor = iface->asSettingsPage();
    QVERIFY(contributor != nullptr);

    const auto info = contributor->pageInfo();
    QCOMPARE(QString::fromStdString(info.id),    QStringLiteral("aura"));
    QCOMPARE(QString::fromStdString(info.title), QStringLiteral("Aura Locker"));
    QCOMPARE(info.order, 30);

    Margin::SettingsRegistry r;
    r.addPage({QString::fromStdString(info.id),
               QString::fromStdString(info.title),
               info.icon, info.content_qml,
               QStringLiteral("plugins"),  // host assigns this, not PageInfo
               info.order});
    QCOMPARE(r.pages().size(), 1);
    QCOMPARE(r.pages()[0].toMap().value("section").toString(), QStringLiteral("plugins"));
}

void TestSettingsRegistry::hostPlusContributorsGroupSeparately() {
    // Mirrors HostCore::bootstrap's pattern: Host's own General/Appearance/
    // About are added directly with "host"/"about" sections; a contributor's
    // page lands in "plugins". sortByOrder() puts them in stable order.
    // Sidebar ListView's section.delegate then renders group headers.
    FakeAuraPage plugin;
    auto* contributor = plugin.asSettingsPage();
    QVERIFY(contributor);

    Margin::SettingsRegistry r;
    r.addPage({QStringLiteral("general"),   QStringLiteral("General"),
               QUrl(), QUrl("qrc:/ui/SettingsGeneralPage.qml"),
               QStringLiteral("host"), 10});
    r.addPage({QStringLiteral("appearance"), QStringLiteral("Appearance"),
               QUrl(), QUrl("qrc:/ui/SettingsAppearancePage.qml"),
               QStringLiteral("host"), 20});
    const auto info = contributor->pageInfo();
    r.addPage({QString::fromStdString(info.id),
               QString::fromStdString(info.title),
               info.icon, info.content_qml,
               QStringLiteral("plugins"), info.order});
    r.addPage({QStringLiteral("about"), QStringLiteral("About"),
               QUrl(), QUrl("qrc:/ui/SettingsAboutPage.qml"),
               QStringLiteral("about"), 10});
    r.sortByOrder();

    const QVariantList pages = r.pages();
    QCOMPARE(pages.size(), 4);
    QCOMPARE(pages[0].toMap().value("id").toString(),       QStringLiteral("general"));
    QCOMPARE(pages[0].toMap().value("section").toString(), QStringLiteral("host"));
    QCOMPARE(pages[1].toMap().value("id").toString(),       QStringLiteral("appearance"));
    QCOMPARE(pages[1].toMap().value("section").toString(), QStringLiteral("host"));
    QCOMPARE(pages[2].toMap().value("id").toString(),       QStringLiteral("aura"));
    QCOMPARE(pages[2].toMap().value("section").toString(), QStringLiteral("plugins"));
    QCOMPARE(pages[3].toMap().value("id").toString(),       QStringLiteral("about"));
    QCOMPARE(pages[3].toMap().value("section").toString(), QStringLiteral("about"));
}

QTEST_MAIN(TestSettingsRegistry)
#include "test_settings_registry.moc"
