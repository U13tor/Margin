// tests/integration/test_plugin_load_unload.cpp
//
// Drives PluginManager end-to-end against the fake_plugin fixture DLL.
// Verifies: scan discovers dll+manifest, ABI check passes, onLoad fires
// (observed via margin.fake.loaded event), lookup by id works, unloadAll
// triggers onUnload (observed via margin.fake.unloaded event).
//
// MARGIN_TEST_PLUGINS_DIR is injected by tests/integration/CMakeLists.txt
// and points at the directory containing margin-fake.dll + .manifest.json.

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTest>

#include <vector>

#include "Margin/EventBus.h"
#include "Margin/HostServices.h"
#include "Margin/Logger.h"
#include "Margin/PluginInterface.h"
#include "Margin/Settings.h"
#include "Margin/TrayService.h"

#include "fake_plugin.h"
#include "host/core/PluginManager.h"
#include "host/security/CryptoServicePool.h"

using Margin::CryptoServicePool;
using Margin::EventBus;
using Margin::HostServices;
using Margin::Logger;
using Margin::PluginInterface;
using Margin::PluginManager;

namespace {

// No-op service impls — the test cares about PluginManager behavior, not
// about what reaches the sinks.
class NullLogger : public Logger {
public:
    void log(Level, const QString&, const QString&) override {}
};

class NullSettings : public Margin::Settings {
public:
    QVariant get(const QString&, const QVariant& dv) const override { return dv; }
    void set(const QString&, const QVariant&) override {}
    void onChange(const QString&, std::function<void(const QVariant&)>) override {}
    void remove(const QString&) override {}
    void registerEncryptedKeys(const QSet<QString>&) override {}
};

class NullTray : public Margin::TrayService {
public:
    void showToast(const QString&, const QString&, int) override {}
    void setIconState(IconState) override {}
    void setTooltip(const QString&) override {}
    void refreshPluginMenu(const QString&) override {}
};

struct Harness {
    NullLogger          logger;
    std::unique_ptr<EventBus> bus;
    NullSettings        settings;
    NullTray            tray;
    std::unique_ptr<CryptoServicePool> pool;
    std::unique_ptr<PluginManager> pm;

    Harness()
        : bus(EventBus::wire())
        , pool(CryptoServicePool::create(std::vector<uint8_t>(32, 0xAA)))
        , pm(std::make_unique<PluginManager>(logger, *bus, settings, tray, *pool)) {}
};

} // namespace

class TestPluginLoadUnload : public QObject {
    Q_OBJECT

private slots:
    void testLoadReturnsInstance();
    void testOnLoadPublishesEvent();
    void testUnloadPublishesEvent();
    void testMissingDirIsNoOp();
    void testLoadOrderFollowsPriority();
    void testSortTiebreakById();
};

void TestPluginLoadUnload::testLoadReturnsInstance() {
    Harness h;
    h.pm->loadAll({QStringLiteral(MARGIN_TEST_PLUGINS_DIR)});

    PluginInterface* p = h.pm->plugin("fake");
    QVERIFY(p != nullptr);
    QCOMPARE(QString::fromStdString(p->id()),    QStringLiteral("fake"));
    QCOMPARE(QString::fromStdString(p->version()), QStringLiteral("0.1.0"));
}

void TestPluginLoadUnload::testOnLoadPublishesEvent() {
    Harness h;

    int loaded = 0;
    h.bus->subscribe(QStringLiteral("margin.fake.loaded"),
                     [&loaded](const QJsonObject&) { ++loaded; });

    h.pm->loadAll({QStringLiteral(MARGIN_TEST_PLUGINS_DIR)});
    QTRY_COMPARE_WITH_TIMEOUT(loaded, 1, 2000);
}

void TestPluginLoadUnload::testUnloadPublishesEvent() {
    Harness h;

    int unloaded = 0;
    h.bus->subscribe(QStringLiteral("margin.fake.unloaded"),
                     [&unloaded](const QJsonObject&) { ++unloaded; });

    h.pm->loadAll({QStringLiteral(MARGIN_TEST_PLUGINS_DIR)});
    QCOMPARE(unloaded, 0);   // not yet

    h.pm->unloadAll();       // explicit unload fires onUnload -> publish
    QTRY_COMPARE_WITH_TIMEOUT(unloaded, 1, 2000);
}

void TestPluginLoadUnload::testMissingDirIsNoOp() {
    Harness h;

    int loaded = 0;
    h.bus->subscribe(QStringLiteral("margin.fake.loaded"),
                     [&loaded](const QJsonObject&) { ++loaded; });

    // Non-existent dir must be silently skipped (PluginManager::discoverInDir
    // guards on dir.exists()).
    h.pm->loadAll({QStringLiteral("/non/existent/path")});
    QVERIFY(h.pm->plugin("fake") == nullptr);
    QTest::qWait(50);
    QCOMPARE(loaded, 0);
}

void TestPluginLoadUnload::testLoadOrderFollowsPriority() {
    Harness h;

    // Subscribe to each plugin's loaded topic and record arrival order. Events
    // are queued in onLoad-call order, so arrival order == load order.
    QStringList order;
    h.bus->subscribe(QStringLiteral("margin.fake.loaded"),
                     [&order](const QJsonObject&) { order << QStringLiteral("fake"); });
    h.bus->subscribe(QStringLiteral("margin.zlast.loaded"),
                     [&order](const QJsonObject&) { order << QStringLiteral("zlast"); });

    h.pm->loadAll({QStringLiteral(MARGIN_TEST_PLUGINS_DIR)});

    QTRY_COMPARE_WITH_TIMEOUT(order.size(), 2, 2000);
    // zlast(priority 10) must load before fake(priority 100) even though
    // "fake" < "zlast" alphabetically — proves loadAll honors priority.
    QCOMPARE(order, (QStringList{QStringLiteral("zlast"), QStringLiteral("fake")}));
}

void TestPluginLoadUnload::testSortTiebreakById() {
    // Equal priority -> id lexicographic ascending.
    std::vector<PluginManager::DiscoveredPlugin> v = {
        {QStringLiteral("charlie"), QStringLiteral("/c"), 100},
        {QStringLiteral("alpha"),   QStringLiteral("/a"), 100},
        {QStringLiteral("bravo"),   QStringLiteral("/b"), 100},
    };
    PluginManager::sortByLoadOrder(v);
    QCOMPARE(v[0].id, QStringLiteral("alpha"));
    QCOMPARE(v[1].id, QStringLiteral("bravo"));
    QCOMPARE(v[2].id, QStringLiteral("charlie"));
}

QTEST_MAIN(TestPluginLoadUnload)
#include "test_plugin_load_unload.moc"
