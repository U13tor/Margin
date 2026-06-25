// tests/unit/test_dashboard_shell.cpp
//
// M0-C10a/b: loads the dashboard shell QML offscreen and verifies (a) it
// instantiates with the Margin.Ui.Primitives module resolved, (b) the status
// bar binds the injected version (and falls back when absent), (c) the
// registry drives a single host tab, and (d) the StackLayout's Repeater
// instantiated OverviewTab.qml at index 0. CI cannot verify on-screen
// visibility, so this guards the QML contract — a parse error or a broken
// import fails the load.
//
// Note on visual-child traversal: Repeater creates delegates in the visual
// childItems() tree, NOT the QObject children() tree. findChild<QObject*>
// only walks the QObject tree, so it cannot locate loaded tab content. We
// recurse via QQuickItem::childItems() instead.

#include <QObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QString>
#include <QTest>
#include <QUrl>

#include <memory>

#include "host/core/DashboardTabRegistry.h"

class TestDashboardShell : public QObject {
    Q_OBJECT

    QUrl shellUrl() const {
        return QUrl(QStringLiteral("qrc:/ui/DashboardWindow.qml"));
    }

    // Build a registry seeded exactly like HostCore::bootstrap does (one host
    // Overview tab, order 10). Returned alongside the engine so the registry
    // outlives the QML load.
    struct EngineAndRegistry {
        std::unique_ptr<Margin::DashboardTabRegistry> registry;
        std::unique_ptr<QQmlApplicationEngine>       engine;
    };

    EngineAndRegistry makeEngine(const char* version = "9.9.9-test") {
        EngineAndRegistry out;
        out.registry = std::make_unique<Margin::DashboardTabRegistry>();
        out.registry->addTab({
            QStringLiteral("overview"),
            QStringLiteral("首页"),
            QUrl(QStringLiteral("qrc:/icons/tab-home.svg")),
            QUrl(QStringLiteral("qrc:/ui/OverviewTab.qml")),
            10,
        });
        out.registry->sortByOrder();

        out.engine = std::make_unique<QQmlApplicationEngine>();
        if (version) {
            out.engine->rootContext()->setContextProperty(
                QStringLiteral("marginVersion"),
                QString::fromLatin1(version));
        }
        out.engine->rootContext()->setContextProperty(
            QStringLiteral("dashboardTabs"), out.registry.get());
        return out;
    }

    // Repeater-created delegates live in the visual tree (QQuickItem::childItems),
    // not the QObject tree. findChild() misses them, so walk both trees.
    static QObject* findByName(QObject* node, const QString& name) {
        if (!node) return nullptr;
        if (!node->objectName().isEmpty() && node->objectName() == name) return node;
        for (auto* child : node->children()) {
            if (QObject* found = findByName(child, name)) return found;
        }
        if (auto* item = qobject_cast<QQuickItem*>(node)) {
            for (QQuickItem* ci : item->childItems()) {
                if (QObject* found = findByName(ci, name)) return found;
            }
        }
        return nullptr;
    }

private slots:
    void loadsWithInjectedVersion();
    void fallsBackWhenVersionMissing();
    void rendersSingleHostTab();
    void overviewTabLoadsIntoContentArea();

private:
    QObject* rootOf(const EngineAndRegistry& er) const {
        return er.engine->rootObjects().isEmpty()
                   ? nullptr
                   : er.engine->rootObjects().constFirst();
    }
};

void TestDashboardShell::loadsWithInjectedVersion() {
    auto er = makeEngine("9.9.9-test");
    er.engine->load(shellUrl());

    QVERIFY2(!er.engine->rootObjects().isEmpty(),
             "shell QML failed to load (parse error or unresolved import)");
    QObject* root = rootOf(er);

    QVERIFY2(findByName(root, QStringLiteral("tabBar")),
             "shell must contain a tab bar");

    QObject* statusBar = findByName(root, QStringLiteral("statusBar"));
    QVERIFY2(statusBar, "shell must contain a status bar");

    // M4-C11 split the old single status Text into version + dot + mode +
    // duration atoms. The version Text carries the `statusBarVersion`
    // objectName; verify the injected version string rendered there.
    QObject* versionText = findByName(statusBar, QStringLiteral("statusBarVersion"));
    QVERIFY2(versionText, "status bar must expose a version Text (objectName=statusBarVersion)");
    const QString versionString = versionText->property("text").toString();
    QVERIFY2(versionString.contains(QStringLiteral("v9.9.9-test")),
             qPrintable(QStringLiteral("version text was: %1").arg(versionString)));
}

void TestDashboardShell::fallsBackWhenVersionMissing() {
    auto er = makeEngine(nullptr);  // no marginVersion context property
    er.engine->load(shellUrl());

    QVERIFY(!er.engine->rootObjects().isEmpty());
    QObject* statusBar = findByName(rootOf(er), QStringLiteral("statusBar"));
    QVERIFY(statusBar);
    QObject* versionText = findByName(statusBar, QStringLiteral("statusBarVersion"));
    QVERIFY(versionText);
    const QString versionString = versionText->property("text").toString();
    QVERIFY2(versionString.contains(QStringLiteral("0.0.0-dev")),
             qPrintable(QStringLiteral("version text was: %1").arg(versionString)));
}

void TestDashboardShell::rendersSingleHostTab() {
    auto er = makeEngine();
    er.engine->load(shellUrl());
    QVERIFY(!er.engine->rootObjects().isEmpty());

    // DashboardWindow._tabCount is bound to dashboardTabs.tabs.length (1 here).
    QObject* root = rootOf(er);
    QVERIFY2(root->property("_tabCount").toInt() == 1,
             "M0 dashboard must expose exactly one host tab");
}

void TestDashboardShell::overviewTabLoadsIntoContentArea() {
    auto er = makeEngine();
    er.engine->load(shellUrl());
    QVERIFY(!er.engine->rootObjects().isEmpty());

    QObject* root = rootOf(er);

    // StackLayout currentIndex mirrors currentTab (default 0 → Overview).
    QObject* contentArea = findByName(root, QStringLiteral("contentArea"));
    QVERIFY(contentArea);
    QCOMPARE(contentArea->property("currentIndex").toInt(), 0);

    // The Repeater inside contentArea should have instantiated the Overview
    // tab's Loader, whose item is OverviewTab.qml (objectName "overviewTab").
    // The Loader itself lives in the visual tree, so findByName walks both
    // QObject children and QQuickItem childItems.
    QVERIFY2(findByName(root, QStringLiteral("overviewTab")),
             "OverviewTab failed to load into the content area");
}

QTEST_MAIN(TestDashboardShell)
#include "test_dashboard_shell.moc"
