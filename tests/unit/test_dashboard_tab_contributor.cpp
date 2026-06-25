// tests/unit/test_dashboard_tab_contributor.cpp
//
// M0-C10b: verifies the DashboardTab wiring pipeline end-to-end (without
// loading any real plugin DLL):
//
//   - DashboardTabRegistry addTab/sortByOrder round-trip + tabsChanged signal
//   - DashboardTabContributor interface is implementable via MI (mirrors the
//     pattern Hello uses for TrayMenuContributor), asDashboardTab() returns
//     non-null, and tabInfo() fields round-trip through the registry.
//
// The fake plugin here is intentionally NOT loaded by PluginManager — it
// stands in for an M1+ plugin to exercise the contract HostCore::bootstrap
// relies on when iterating contributors.

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
#include "host/core/DashboardTabRegistry.h"

namespace {

// Minimal stand-in for a future Aura-style plugin: multiply-inherits
// PluginInterface (so asDashboardTab() override is well-formed) and
// DashboardTabContributor. Only the tab-related surface is exercised; the
// other hooks are stubbed.
class FakeAuraTab : public Margin::PluginInterface,
                    public Margin::DashboardTabContributor {
public:
    std::string id() const override { return "fake_aura"; }
    std::string version() const override { return "0.0.0-test"; }
    Margin::Result<void, std::string> onLoad(const Margin::PluginContext&) override {
        return Margin::Result<void, std::string>::ok();
    }
    void onConfigChange(const QJsonObject&) override {}
    void onUnload() override {}

    Margin::DashboardTabContributor* asDashboardTab() override { return this; }

    TabInfo tabInfo() const override {
        return TabInfo{
            "aura",
            "蓝牙锁屏",
            QUrl("qrc:/icons/icon-lock.svg"),
            QUrl("qrc:/ui/OverviewTab.qml"),  // any valid qrc URL works for the test
            20,
        };
    }
};

} // namespace

class TestDashboardTabContributor : public QObject {
    Q_OBJECT

private slots:
    void registryAddTabEmitsSignal();
    void registrySortsByOrderAscending();
    void registryTabsSerialization();
    void fakeContributorRoundTrip();
    void hostTabPlusContributorsSortCorrectly();
};

void TestDashboardTabContributor::registryAddTabEmitsSignal() {
    Margin::DashboardTabRegistry r;
    QSignalSpy spy(&r, &Margin::DashboardTabRegistry::tabsChanged);
    QCOMPARE(r.tabs().size(), 0);

    r.addTab({QStringLiteral("a"), QStringLiteral("A"),
              QUrl("qrc:/icons/x.svg"), QUrl("qrc:/ui/X.qml"), 50});
    QCOMPARE(spy.count(), 1);
    QCOMPARE(r.tabs().size(), 1);
}

void TestDashboardTabContributor::registrySortsByOrderAscending() {
    Margin::DashboardTabRegistry r;
    QSignalSpy spy(&r, &Margin::DashboardTabRegistry::tabsChanged);

    // Inserted out of order on purpose.
    r.addTab({QStringLiteral("c"), QStringLiteral("C"), QUrl(), QUrl(), 30});
    r.addTab({QStringLiteral("a"), QStringLiteral("A"), QUrl(), QUrl(), 10});
    r.addTab({QStringLiteral("b"), QStringLiteral("B"), QUrl(), QUrl(), 20});
    r.sortByOrder();

    QCOMPARE(spy.count(), 4);  // 3 addTab + 1 sort

    const QVariantList tabs = r.tabs();
    QCOMPARE(tabs.size(), 3);
    QCOMPARE(tabs[0].toMap().value("id").toString(), QStringLiteral("a"));
    QCOMPARE(tabs[1].toMap().value("id").toString(), QStringLiteral("b"));
    QCOMPARE(tabs[2].toMap().value("id").toString(), QStringLiteral("c"));
}

void TestDashboardTabContributor::registryTabsSerialization() {
    Margin::DashboardTabRegistry r;
    r.addTab({QStringLiteral("overview"), QStringLiteral("首页"),
              QUrl("qrc:/icons/tab-home.svg"),
              QUrl("qrc:/ui/OverviewTab.qml"), 10});

    const QVariantList tabs = r.tabs();
    QCOMPARE(tabs.size(), 1);
    const QVariantMap m = tabs[0].toMap();
    QCOMPARE(m.value("id").toString(),    QStringLiteral("overview"));
    QCOMPARE(m.value("title").toString(), QStringLiteral("首页"));
    // QUrl::toDisplayString preserves the qrc: scheme prefix; QML accepts
    // both "qrc:/path" and "/path" forms for Image source, and the qrc:
    // form is unambiguous.
    QCOMPARE(m.value("icon").toString(),       QStringLiteral("qrc:/icons/tab-home.svg"));
    QCOMPARE(m.value("contentQml").toString(), QStringLiteral("qrc:/ui/OverviewTab.qml"));
    QCOMPARE(m.value("order").toInt(), 10);
}

void TestDashboardTabContributor::fakeContributorRoundTrip() {
    FakeAuraTab plugin;
    Margin::PluginInterface* iface = &plugin;

    // HostCore's bootstrap path: iface->asDashboardTab() -> non-null ->
    // push tabInfo() into the registry.
    auto* contributor = iface->asDashboardTab();
    QVERIFY(contributor != nullptr);

    const auto info = contributor->tabInfo();
    QCOMPARE(QString::fromStdString(info.id),    QStringLiteral("aura"));
    QCOMPARE(QString::fromStdString(info.title), QStringLiteral("蓝牙锁屏"));
    QCOMPARE(info.order, 20);

    Margin::DashboardTabRegistry r;
    r.addTab({QString::fromStdString(info.id),
              QString::fromStdString(info.title),
              info.icon, info.content_qml, info.order});
    QCOMPARE(r.tabs().size(), 1);
}

void TestDashboardTabContributor::hostTabPlusContributorsSortCorrectly() {
    // Mirrors HostCore::bootstrap's pattern: Host's own Overview is added
    // first, then a contributor's tab; sortByOrder() puts Overview first
    // (order 10) and the Aura tab second (order 20).
    FakeAuraTab plugin;
    auto* contributor = plugin.asDashboardTab();
    QVERIFY(contributor);

    Margin::DashboardTabRegistry r;
    r.addTab({QStringLiteral("overview"), QStringLiteral("首页"),
              QUrl("qrc:/icons/tab-home.svg"),
              QUrl("qrc:/ui/OverviewTab.qml"), 10});
    const auto info = contributor->tabInfo();
    r.addTab({QString::fromStdString(info.id),
              QString::fromStdString(info.title),
              info.icon, info.content_qml, info.order});
    r.sortByOrder();

    const QVariantList tabs = r.tabs();
    QCOMPARE(tabs.size(), 2);
    QCOMPARE(tabs[0].toMap().value("id").toString(), QStringLiteral("overview"));
    QCOMPARE(tabs[1].toMap().value("id").toString(), QStringLiteral("aura"));
}

QTEST_MAIN(TestDashboardTabContributor)
#include "test_dashboard_tab_contributor.moc"
