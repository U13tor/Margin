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
#include <QSignalSpy>
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

class MemorySettings : public Margin::Settings {
public:
    QHash<QString, QVariant> store;
    QVariant get(const QString& k, const QVariant& dv) const override { return store.value(k, dv); }
    void set(const QString& k, const QVariant& v) override { store[k] = v; }
    void onChange(const QString&, std::function<void(const QVariant&)>) override {}
    void remove(const QString& k) override { store.remove(k); }
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
    MemorySettings      settings;
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
    void testDynamicUnloadOnePlugin();
    void testDynamicReloadOnePlugin();
    void testSetPluginEnabledPersistence();
    void testPluginManagerExposedList();
    void testDisabledPluginSkippedOnLoadAll();
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

void TestPluginLoadUnload::testDynamicUnloadOnePlugin() {
    Harness h;
    h.pm->loadAll({QStringLiteral(MARGIN_TEST_PLUGINS_DIR)});
    QVERIFY(h.pm->isPluginLoaded(QStringLiteral("fake")));
    QVERIFY(h.pm->isPluginLoaded(QStringLiteral("zlast")));

    int unloadedEventCount = 0;
    h.bus->subscribe(QStringLiteral("margin.fake.unloaded"),
                     [&unloadedEventCount](const QJsonObject&) { ++unloadedEventCount; });

    QSignalSpy spyUnloaded(h.pm.get(), &PluginManager::pluginUnloaded);
    QSignalSpy spyChanged(h.pm.get(), &PluginManager::pluginsChanged);

    bool ok = h.pm->unloadPlugin(QStringLiteral("fake"));
    QVERIFY(ok);
    QTRY_COMPARE_WITH_TIMEOUT(unloadedEventCount, 1, 2000);
    QCOMPARE(spyUnloaded.count(), 1);
    QCOMPARE(spyUnloaded.takeFirst().at(0).toString(), QStringLiteral("fake"));
    QCOMPARE(spyChanged.count(), 1);

    QVERIFY(!h.pm->isPluginLoaded(QStringLiteral("fake")));
    QVERIFY(h.pm->plugin("fake") == nullptr);
    // Ensure other plugins remain loaded
    QVERIFY(h.pm->isPluginLoaded(QStringLiteral("zlast")));
    QVERIFY(h.pm->plugin("zlast") != nullptr);
}

void TestPluginLoadUnload::testDynamicReloadOnePlugin() {
    Harness h;
    h.pm->loadAll({QStringLiteral(MARGIN_TEST_PLUGINS_DIR)});
    QVERIFY(h.pm->isPluginLoaded(QStringLiteral("fake")));

    h.pm->unloadPlugin(QStringLiteral("fake"));
    QVERIFY(!h.pm->isPluginLoaded(QStringLiteral("fake")));

    int reloadedEventCount = 0;
    h.bus->subscribe(QStringLiteral("margin.fake.loaded"),
                     [&reloadedEventCount](const QJsonObject&) { ++reloadedEventCount; });

    QSignalSpy spyLoaded(h.pm.get(), &PluginManager::pluginLoaded);
    QSignalSpy spyChanged(h.pm.get(), &PluginManager::pluginsChanged);

    bool ok = h.pm->loadPlugin(QStringLiteral("fake"));
    QVERIFY(ok);
    QTRY_COMPARE_WITH_TIMEOUT(reloadedEventCount, 1, 2000);
    QCOMPARE(spyLoaded.count(), 1);
    QCOMPARE(spyLoaded.takeFirst().at(0).toString(), QStringLiteral("fake"));
    QCOMPARE(spyChanged.count(), 1);

    QVERIFY(h.pm->isPluginLoaded(QStringLiteral("fake")));
    QVERIFY(h.pm->plugin("fake") != nullptr);
}

void TestPluginLoadUnload::testSetPluginEnabledPersistence() {
    Harness h;
    h.pm->loadAll({QStringLiteral(MARGIN_TEST_PLUGINS_DIR)});
    QVERIFY(h.pm->isPluginEnabled(QStringLiteral("fake")));
    QVERIFY(h.pm->isPluginLoaded(QStringLiteral("fake")));

    h.pm->setPluginEnabled(QStringLiteral("fake"), false);
    QVERIFY(!h.pm->isPluginEnabled(QStringLiteral("fake")));
    QVERIFY(!h.pm->isPluginLoaded(QStringLiteral("fake")));
    QCOMPARE(h.settings.get(QStringLiteral("plugins.fake.enabled"), true).toBool(), false);

    h.pm->setPluginEnabled(QStringLiteral("fake"), true);
    QVERIFY(h.pm->isPluginEnabled(QStringLiteral("fake")));
    QVERIFY(h.pm->isPluginLoaded(QStringLiteral("fake")));
    QCOMPARE(h.settings.get(QStringLiteral("plugins.fake.enabled"), false).toBool(), true);
}

void TestPluginLoadUnload::testPluginManagerExposedList() {
    Harness h;
    h.pm->loadAll({QStringLiteral(MARGIN_TEST_PLUGINS_DIR)});
    const QVariantList list = h.pm->pluginList();
    QVERIFY(list.size() >= 2);

    bool foundFake = false;
    for (const auto& item : list) {
        const QVariantMap map = item.toMap();
        if (map.value(QStringLiteral("id")).toString() == QLatin1String("fake")) {
            foundFake = true;
            QCOMPARE(map.value(QStringLiteral("version")).toString(), QStringLiteral("0.1.0"));
            QCOMPARE(map.value(QStringLiteral("isLoaded")).toBool(), true);
            QCOMPARE(map.value(QStringLiteral("isEnabled")).toBool(), true);
        }
    }
    QVERIFY(foundFake);
}

void TestPluginLoadUnload::testDisabledPluginSkippedOnLoadAll() {
    Harness h;
    h.settings.set(QStringLiteral("plugins.fake.enabled"), false);
    h.pm->loadAll({QStringLiteral(MARGIN_TEST_PLUGINS_DIR)});

    QVERIFY(!h.pm->isPluginLoaded(QStringLiteral("fake")));
    QVERIFY(h.pm->plugin("fake") == nullptr);
    QVERIFY(!h.pm->isPluginEnabled(QStringLiteral("fake")));

    // Other non-disabled plugins still load
    QVERIFY(h.pm->isPluginLoaded(QStringLiteral("zlast")));
    QVERIFY(h.pm->plugin("zlast") != nullptr);
    QVERIFY(h.pm->isPluginEnabled(QStringLiteral("zlast")));
}

QTEST_MAIN(TestPluginLoadUnload)
#include "test_plugin_load_unload.moc"
