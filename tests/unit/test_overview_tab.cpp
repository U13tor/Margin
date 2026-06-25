// tests/unit/test_overview_tab.cpp
//
// M4-C8: OverviewTab.qml rewrite verification. Mirrors test_dashboard_shell's
// shell-load pattern — loads DashboardWindow.qml offscreen with a real
// DashboardTabRegistry and injects stub plugin context properties so the
// 3 StatusCards bind to live data. Verifies:
//   (1) typeof guards prevent ReferenceError when plugins are absent
//   (2) each card binds to the corresponding plugin Q_PROPERTY
//   (3) subtitle hides when value is "—" (mixed-signal prevention)
//   (4) invoking StatusCard.clicked() routes through navigateToTab →
//       Window.window.currentTab update (signal wiring, not MouseArea
//       dispatch — full end-to-end click testing needs QTest::mouseClick
//       + visible window geometry, not worth offscreen CI complexity)
//
// Note: to give every plugin's tab a resolvable contentQml, all 4 tabs in
// the stub registry point to qrc:/ui/OverviewTab.qml. The actual content
// doesn't matter — we only assert against Overview's own StatusCards, and
// the click test only needs dashboardTabs.tabs to contain the target id.

#include <QObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QRegularExpression>
#include <QString>
#include <QVariant>
#include <QTest>
#include <QUrl>

#include <memory>

#include "host/core/DashboardTabRegistry.h"

// ── Stubs ────────────────────────────────────────────────────────────────
// Minimal QObjects exposing the Q_PROPERTYs OverviewTab binds to. AUTOMOC
// generates metaobject code for each. Each stub lives on the stack of the
// test method that needs it; passed to the engine via setContextProperty.

// Minimal rhythm stub mirroring PomodoroTimer's Q_PROPERTY surface that
// OverviewTab.qml binds to (state / remainingSeconds / todayCompletedRounds).
// Defaults match a freshly loaded PomodoroTimer (state="idle", 0 remaining).
class RhythmStub : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(int remainingSeconds READ remainingSeconds NOTIFY remainingChanged)
    Q_PROPERTY(int todayCompletedRounds READ todayCompletedRounds
               NOTIFY todayCompletedRoundsChanged)
public:
    explicit RhythmStub(QObject* parent = nullptr) : QObject(parent) {}
    QString state() const { return m_state; }
    int remainingSeconds() const { return m_remaining; }
    int todayCompletedRounds() const { return m_rounds; }
    void setState(QString s) { m_state = std::move(s); emit stateChanged(); }
    void setRemainingSeconds(int r) { m_remaining = r; emit remainingChanged(); }
    void setTodayCompletedRounds(int r) { m_rounds = r; emit todayCompletedRoundsChanged(); }
signals:
    void stateChanged();
    void remainingChanged();
    void todayCompletedRoundsChanged();
private:
    QString m_state = QStringLiteral("idle");
    int m_remaining = 0;
    int m_rounds = 0;
};

class ScreenTimeStub : public QObject {
    Q_OBJECT
    Q_PROPERTY(qint64 todayFocusSeconds READ todayFocusSeconds
               NOTIFY todayFocusSecondsChanged)
    Q_PROPERTY(QVariantList topApps READ topApps
               NOTIFY topAppsChanged)
public:
    explicit ScreenTimeStub(qint64 sec = 0, QObject* parent = nullptr)
        : QObject(parent), m_sec(sec) {}
    qint64 todayFocusSeconds() const { return m_sec; }
    void setTodayFocusSeconds(qint64 s) { m_sec = s; emit todayFocusSecondsChanged(); }

    // topApps entries match the real ScreenTimePlugin cache shape:
    // { name: string, durationMs: qint64, category: string }. Category is
    // optional in production; tests omit it (C9 OverviewTab drops the
    // category tag — see docs/12-deferred-items.md §A25).
    QVariantList topApps() const { return m_topApps; }
    void setTopApps(QVariantList apps) { m_topApps = std::move(apps); emit topAppsChanged(); }
signals:
    void todayFocusSecondsChanged();
    void topAppsChanged();
private:
    qint64 m_sec;
    QVariantList m_topApps;
};

class AuraStub : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString pairedDeviceName READ pairedDeviceName
               NOTIFY pairedDeviceChanged)
    Q_PROPERTY(QString lastLockSummary READ lastLockSummary
               NOTIFY lastLockChanged)
public:
    explicit AuraStub(QString name = QString(), QString summary = QString(),
                      QObject* parent = nullptr)
        : QObject(parent), m_name(std::move(name)), m_summary(std::move(summary)) {}
    QString pairedDeviceName() const { return m_name; }
    QString lastLockSummary() const { return m_summary; }
    void setPairedDeviceName(QString n) { m_name = std::move(n); emit pairedDeviceChanged(); }
    void setLastLockSummary(QString s) { m_summary = std::move(s); emit lastLockChanged(); }
signals:
    void pairedDeviceChanged();
    void lastLockChanged();
private:
    QString m_name;
    QString m_summary;
};

// ActivityFeed stub mirrors the real ActivityFeed's Q_PROPERTY surface
// (QVariantList events + NOTIFY eventsChanged). Real service lives in
// margin_core (C9b.1) and is exercised by test_activity_feed.cpp; here
// we just need a controllable feed for OverviewTab to bind.
class ActivityFeedStub : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList events READ events NOTIFY eventsChanged)
public:
    explicit ActivityFeedStub(QObject* parent = nullptr) : QObject(parent) {}
    QVariantList events() const { return m_events; }
    void setEvents(QVariantList e) { m_events = std::move(e); emit eventsChanged(); }
signals:
    void eventsChanged();
private:
    QVariantList m_events;
};

// ── Test fixture ─────────────────────────────────────────────────────────

class TestOverviewTab : public QObject {
    Q_OBJECT

    // Engine + registry + 3 stubs. All are kept alive for the engine's
    // lifetime — releasing them in sync matters because QML holds raw
    // pointers to context property values.
    struct Bundle {
        std::unique_ptr<Margin::DashboardTabRegistry> registry;
        std::unique_ptr<QQmlApplicationEngine>       engine;
        std::unique_ptr<RhythmStub>                  rhythm;
        std::unique_ptr<ScreenTimeStub>              screenTime;
        std::unique_ptr<AuraStub>                    aura;
        std::unique_ptr<ActivityFeedStub>            activityFeed;
    };

    // If includeStubs is false, no plugin context properties are injected —
    // exercises the typeof-guard path (cards render "—" without warnings).
    // If registerPluginTabs is false, only the overview tab is in the
    // registry (pre-M1 dashboard shape — click navigation still resolves
    // because we register them unconditionally in this test variant).

    Bundle makeBundle(bool includeStubs = true) {
        Bundle b;
        b.registry = std::make_unique<Margin::DashboardTabRegistry>();
        // All 4 tabs point to OverviewTab.qml — content doesn't matter for
        // our assertions; we only need dashboardTabs.tabs to expose the
        // ids + indices so navigateToTab can find them.
        b.registry->addTab({
            QStringLiteral("overview"),
            QStringLiteral("首页"),
            QUrl(QStringLiteral("qrc:/icons/tab-home.svg")),
            QUrl(QStringLiteral("qrc:/ui/OverviewTab.qml")),
            10,
        });
        b.registry->addTab({
            QStringLiteral("aura"),
            QStringLiteral("Aura"),
            QUrl(QStringLiteral("qrc:/icons/tab-home.svg")),
            QUrl(QStringLiteral("qrc:/ui/OverviewTab.qml")),
            40,
        });
        b.registry->addTab({
            QStringLiteral("screen_time"),
            QStringLiteral("时长"),
            QUrl(QStringLiteral("qrc:/icons/tab-home.svg")),
            QUrl(QStringLiteral("qrc:/ui/OverviewTab.qml")),
            20,
        });
        b.registry->addTab({
            QStringLiteral("rhythm"),
            QStringLiteral("Rhythm"),
            QUrl(QStringLiteral("qrc:/icons/tab-home.svg")),
            QUrl(QStringLiteral("qrc:/ui/OverviewTab.qml")),
            30,
        });
        b.registry->sortByOrder();

        b.engine = std::make_unique<QQmlApplicationEngine>();
        b.engine->rootContext()->setContextProperty(
            QStringLiteral("dashboardTabs"), b.registry.get());

        if (includeStubs) {
            b.rhythm     = std::make_unique<RhythmStub>();
            b.screenTime = std::make_unique<ScreenTimeStub>();
            b.aura       = std::make_unique<AuraStub>();
            b.activityFeed = std::make_unique<ActivityFeedStub>();
            b.engine->rootContext()->setContextProperty(
                QStringLiteral("rhythm"), b.rhythm.get());
            b.engine->rootContext()->setContextProperty(
                QStringLiteral("screen_time"), b.screenTime.get());
            b.engine->rootContext()->setContextProperty(
                QStringLiteral("aura"), b.aura.get());
            b.engine->rootContext()->setContextProperty(
                QStringLiteral("activityFeed"), b.activityFeed.get());
        }
        return b;
    }

    // Repeater-created delegates live in the visual tree (QQuickItem::childItems),
    // not the QObject tree. findChild() misses them, so walk both trees.
    // Mirrors test_dashboard_shell's helper verbatim.
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

    // Walk visual + QObject trees for any node whose `text` property
    // matches `needle`. Used to assert that Repeater delegates rendered
    // the expected text — anonymous delegates can't be found by
    // objectName, so we look for the rendered string itself.
    static QObject* findByTextContent(QObject* node, const QString& needle) {
        if (!node) return nullptr;
        const QVariant v = node->property("text");
        if (v.isValid() && v.toString() == needle) return node;
        for (auto* child : node->children()) {
            if (QObject* found = findByTextContent(child, needle)) return found;
        }
        if (auto* item = qobject_cast<QQuickItem*>(node)) {
            for (QQuickItem* ci : item->childItems()) {
                if (QObject* found = findByTextContent(ci, needle)) return found;
            }
        }
        return nullptr;
    }

    QObject* rootOf(const Bundle& b) const {
        return b.engine->rootObjects().isEmpty()
                   ? nullptr
                   : b.engine->rootObjects().constFirst();
    }

    QUrl shellUrl() const {
        return QUrl(QStringLiteral("qrc:/ui/DashboardWindow.qml"));
    }

private slots:
    void loadsWithoutPluginsShowsDashes();
    void loadsWithRhythmWorkingShowsNextBreakTime();
    void loadsWithScreenTimeShowsFocus();
    void loadsWithAuraShowsPairedDevice();
    void subtitleHidesWhenValueIsDash();
    void clickSignalInvokesNavigateToTab();
    void secondClickAfterReturningToOverviewStillNavigates();
    void loadsWithTopAppsShowsBars();
    void clickTopAppsArrowNavigatesToScreenTime();
    void loadsWithoutActivityFeedHidesRecentEventsCard();
    void loadsWithEmptyActivityFeedShowsPlaceholder();
    void loadsWithActivityEventsShowsRecentRows();
    void loadsFourRowsMaxWhenActivityEventsExceed();
};

// (1) No plugin context props injected → all 3 cards render "—". Implicit
// proof that typeof guards prevent ReferenceError at binding-eval time —
// if they didn't, the engine would fail to load the tab at all.
void TestOverviewTab::loadsWithoutPluginsShowsDashes() {
    auto b = makeBundle(/*includeStubs=*/false);
    b.engine->load(shellUrl());
    QVERIFY2(!b.engine->rootObjects().isEmpty(),
             "shell QML failed to load with no plugins injected");
    QObject* root = rootOf(b);

    QObject* focusCard = findByName(root, QStringLiteral("overviewCardFocus"));
    QVERIFY2(focusCard, "overviewCardFocus not found");
    QCOMPARE(focusCard->property("value").toString(), QStringLiteral("—"));

    QObject* rhythmCard = findByName(root, QStringLiteral("overviewCardRhythm"));
    QVERIFY2(rhythmCard, "overviewCardRhythm not found");
    QCOMPARE(rhythmCard->property("value").toString(), QStringLiteral("—"));

    QObject* auraCard = findByName(root, QStringLiteral("overviewCardAura"));
    QVERIFY2(auraCard, "overviewCardAura not found");
    QCOMPARE(auraCard->property("value").toString(), QStringLiteral("—"));
}

// (2) rhythm.state="working" + remainingSeconds=1800 → value is HH:mm
//     (next break = now + 30 min, matching docs/06 §4.2 prototype), and
//     subtitle is "下次休息". We assert the format rather than the exact
//     wall-clock string because the binding captures Date.now() at load —
//     racing against the test host clock near minute boundaries would
//     flake.
void TestOverviewTab::loadsWithRhythmWorkingShowsNextBreakTime() {
    auto b = makeBundle();
    b.rhythm->setState(QStringLiteral("working"));
    b.rhythm->setRemainingSeconds(1800);  // 30 minutes
    b.engine->load(shellUrl());
    QVERIFY(!b.engine->rootObjects().isEmpty());
    QObject* root = rootOf(b);

    QObject* rhythmCard = findByName(root, QStringLiteral("overviewCardRhythm"));
    QVERIFY(rhythmCard);

    const QString value = rhythmCard->property("value").toString();
    static const QRegularExpression hhmm(QStringLiteral("^\\d{2}:\\d{2}$"));
    QVERIFY2(hhmm.match(value).hasMatch(),
             qPrintable(QStringLiteral("value '%1' should be HH:mm when "
                                       "state=working").arg(value)));
    QCOMPARE(rhythmCard->property("subtitle").toString(),
             QStringLiteral("下次休息"));
}

// (3) screen_time.todayFocusSeconds=1800 → value "30m".
void TestOverviewTab::loadsWithScreenTimeShowsFocus() {
    auto b = makeBundle();
    b.screenTime->setTodayFocusSeconds(1800);
    b.engine->load(shellUrl());
    QVERIFY(!b.engine->rootObjects().isEmpty());
    QObject* root = rootOf(b);

    QObject* focusCard = findByName(root, QStringLiteral("overviewCardFocus"));
    QVERIFY(focusCard);
    QCOMPARE(focusCard->property("value").toString(), QStringLiteral("30m"));
}

// (4) aura.pairedDeviceName="iPhone" + lastLockSummary="5 分钟前" → both bind.
void TestOverviewTab::loadsWithAuraShowsPairedDevice() {
    auto b = makeBundle();
    b.aura->setPairedDeviceName(QStringLiteral("iPhone"));
    b.aura->setLastLockSummary(QStringLiteral("5 分钟前"));
    b.engine->load(shellUrl());
    QVERIFY(!b.engine->rootObjects().isEmpty());
    QObject* root = rootOf(b);

    QObject* auraCard = findByName(root, QStringLiteral("overviewCardAura"));
    QVERIFY(auraCard);
    QCOMPARE(auraCard->property("value").toString(), QStringLiteral("iPhone"));
    QCOMPARE(auraCard->property("subtitle").toString(), QStringLiteral("5 分钟前"));
}

// (5) rhythm.todayCompletedRounds=0 → value "—", subtitle "" (hidden).
// Catches the mixed-signal bug where value says "no data" but subtitle
// says "已完成 0 / 5 番茄".
void TestOverviewTab::subtitleHidesWhenValueIsDash() {
    auto b = makeBundle();
    b.rhythm->setTodayCompletedRounds(0);  // explicitly 0
    b.engine->load(shellUrl());
    QVERIFY(!b.engine->rootObjects().isEmpty());
    QObject* root = rootOf(b);

    QObject* rhythmCard = findByName(root, QStringLiteral("overviewCardRhythm"));
    QVERIFY(rhythmCard);
    QCOMPARE(rhythmCard->property("value").toString(), QStringLiteral("—"));
    QCOMPARE(rhythmCard->property("subtitle").toString(), QStringLiteral(""));
}

// (6) StatusCard.clicked() → navigateToTab("screen_time") →
//     Window.window.currentTab = 2 (screen_time index in our stub).
// Invokes the signal by name; this tests signal→handler→state wiring,
// NOT MouseArea dispatch (already a C7 gap, not in this PR's scope).
// Also asserts the binding invariant: TabBar.currentTab follows
// Window.currentTab — this is the property test #7 nails down as a
// user-visible regression.
void TestOverviewTab::clickSignalInvokesNavigateToTab() {
    auto b = makeBundle();
    b.engine->load(shellUrl());
    QVERIFY(!b.engine->rootObjects().isEmpty());
    QObject* root = rootOf(b);

    QObject* focusCard = findByName(root, QStringLiteral("overviewCardFocus"));
    QObject* tabBar = findByName(root, QStringLiteral("tabBar"));
    QVERIFY(focusCard);
    QVERIFY(tabBar);

    // Pre-condition: currentTab starts at 0 (overview).
    QCOMPARE(root->property("currentTab").toInt(), 0);
    QCOMPARE(tabBar->property("currentTab").toInt(), 0);

    // Invoke the clicked signal directly. QMetaObject::invokeMethod with
    // the signal name routes through the connection chain — the OverviewTab's
    // onClicked handler runs and calls navigateToTab("screen_time").
    QVERIFY2(QMetaObject::invokeMethod(focusCard, "clicked"),
             "failed to invoke clicked() signal on focus card");

    // screen_time is at index 1 in our stub (overview=0, screen_time=1, rhythm=2, aura=3).
    QCOMPARE(root->property("currentTab").toInt(), 1);
    // Binding invariant: TabBar follows Window via `currentTab: root.currentTab`.
    QCOMPARE(tabBar->property("currentTab").toInt(), 1);
}

// (7) Regression: user-reported bug where the SECOND overview card click
// (after returning to overview via TabBar) didn't navigate. Root cause:
// TabBar's delegate used to write to its own `currentTab` directly, which
// breaks DashboardWindow's `currentTab: root.currentTab` binding (a direct
// write to a bound property replaces the binding with a literal value).
// After the binding was broken, Window.currentTab changes (from Overview
// card clicks) stopped propagating to TabBar.currentTab — the TabBar stayed
// visually stuck on the previous tab even though StackLayout followed the
// Window's currentTab correctly.
//
// Simulates the real user flow: card click → TabBar click → card click.
// The TabBar click is simulated by emitting `tabActivated(0)` — the same
// signal the delegate emits on click — rather than by writing to properties
// directly. Writing to root.currentTab would not exercise the binding path
// and would not catch this regression.
void TestOverviewTab::secondClickAfterReturningToOverviewStillNavigates() {
    auto b = makeBundle();
    b.engine->load(shellUrl());
    QVERIFY(!b.engine->rootObjects().isEmpty());
    QObject* root = rootOf(b);

    QObject* focusCard = findByName(root, QStringLiteral("overviewCardFocus"));
    QObject* rhythmCard = findByName(root, QStringLiteral("overviewCardRhythm"));
    QObject* tabBar = findByName(root, QStringLiteral("tabBar"));
    QVERIFY(focusCard);
    QVERIFY(rhythmCard);
    QVERIFY(tabBar);

    // Step 1: click focus card → currentTab=1 (screen_time).
    QVERIFY2(QMetaObject::invokeMethod(focusCard, "clicked"),
             "failed to invoke clicked() on focus card (1st click)");
    QCOMPARE(root->property("currentTab").toInt(), 1);
    // Binding invariant: TabBar.currentTab tracks Window.currentTab.
    QCOMPARE(tabBar->property("currentTab").toInt(), 1);

    // Step 2: user clicks TabBar Overview item → currentTab=0.
    QVERIFY2(QMetaObject::invokeMethod(tabBar, "tabActivated",
                                       Qt::DirectConnection, Q_ARG(int, 0)),
             "failed to emit tabActivated(0) on tabBar");
    QCOMPARE(root->property("currentTab").toInt(), 0);
    QCOMPARE(tabBar->property("currentTab").toInt(), 0);

    // Step 3: click rhythm card → expect currentTab=2.
    // Before the fix: Window.currentTab updates to 2 (StackLayout follows),
    // but TabBar.currentTab stays at 0 (broken binding) — user sees Aura
    // content under a "home" highlighted tab.
    QVERIFY2(QMetaObject::invokeMethod(rhythmCard, "clicked"),
             "failed to invoke clicked() on rhythm card (2nd click)");
    QCOMPARE(root->property("currentTab").toInt(), 2);
    QCOMPARE(tabBar->property("currentTab").toInt(), 2);
}

// (8) screen_time.topApps=[VS Code 4800000ms, Chrome 2700000ms, Slack 1200000ms]
//     → OverviewTab's Top-5 section becomes visible and MBarChart's model
//     has 3 entries. Verifies the M4-C9 data wiring end-to-end: stub
//     Q_PROPERTY → context property → QML binding → MBarChart.model.
//     Does NOT assert rendered text — MBarChart has its own coverage in
//     ScreenTimeTab tests; here we only assert the Overview binds the
//     right model.
void TestOverviewTab::loadsWithTopAppsShowsBars() {
    auto b = makeBundle();
    QVariantList apps;
    QVariantMap app1; app1[QStringLiteral("name")] = QStringLiteral("VS Code");
                    app1[QStringLiteral("durationMs")] = QVariant::fromValue<qint64>(4800000);
    QVariantMap app2; app2[QStringLiteral("name")] = QStringLiteral("Chrome");
                    app2[QStringLiteral("durationMs")] = QVariant::fromValue<qint64>(2700000);
    QVariantMap app3; app3[QStringLiteral("name")] = QStringLiteral("Slack");
                    app3[QStringLiteral("durationMs")] = QVariant::fromValue<qint64>(1200000);
    apps << app1 << app2 << app3;
    b.screenTime->setTopApps(apps);

    b.engine->load(shellUrl());
    QVERIFY(!b.engine->rootObjects().isEmpty());
    QObject* root = rootOf(b);

    QObject* topAppsCard = findByName(root, QStringLiteral("overviewCardTopApps"));
    QVERIFY2(topAppsCard, "overviewCardTopApps not found");
    QVERIFY2(topAppsCard->property("visible").toBool(),
             "Top-5 card should be visible when topApps has data");

    QObject* bar = findByName(root, QStringLiteral("topAppsBar"));
    QVERIFY2(bar, "topAppsBar (MBarChart) not found");
    QCOMPARE(bar->property("model").toList().size(), 3);
}

// (9) Header [→] MButton click → navigateToTab("screen_time") →
//     Window.currentTab = 2. Mirrors #6's signal-invocation pattern: we
//     invoke MButton's clicked() signal directly (not MouseArea dispatch)
//     because real mouse dispatch needs a visible Window + QTest::mouseClick
//     — overkill for testing signal→handler→state wiring.
//     Sanity: also asserts the Top-5 card is clickable itself — invoking
//     navigateToTab via the card-level MouseArea would require MouseEvent
//     dispatch, so we instead trust the MouseArea→navigateToTab wiring is
//     the same pattern as StatusCard (test #6 covers the equivalent path).
void TestOverviewTab::clickTopAppsArrowNavigatesToScreenTime() {
    auto b = makeBundle();
    // Inject 1 entry so the card is visible — without this the MButton
    // parent is invisible and click routing becomes undefined.
    QVariantList apps;
    QVariantMap app; app[QStringLiteral("name")] = QStringLiteral("VS Code");
                     app[QStringLiteral("durationMs")] = QVariant::fromValue<qint64>(4800000);
    apps << app;
    b.screenTime->setTopApps(apps);

    b.engine->load(shellUrl());
    QVERIFY(!b.engine->rootObjects().isEmpty());
    QObject* root = rootOf(b);

    QObject* arrow = findByName(root, QStringLiteral("topAppsArrow"));
    QObject* tabBar = findByName(root, QStringLiteral("tabBar"));
    QVERIFY2(arrow, "topAppsArrow MButton not found");
    QVERIFY(tabBar);

    QCOMPARE(root->property("currentTab").toInt(), 0);

    QVERIFY2(QMetaObject::invokeMethod(arrow, "clicked"),
             "failed to invoke clicked() signal on topAppsArrow");

    // screen_time is at index 1 in our stub registry (overview=0,
    // screen_time=1, rhythm=2, aura=3 — sortByOrder by `order` field).
    QCOMPARE(root->property("currentTab").toInt(), 1);
    QCOMPARE(tabBar->property("currentTab").toInt(), 1);
}

// (10) M4-C9b.2: typeof guard — when no activityFeed is injected (the
// includeStubs=false path), the recent-events card must be hidden and
// the shell must still load cleanly. Implicit ReferenceError check: if
// the typeof guard were missing, the binding eval would throw and the
// shell would fail to load.
void TestOverviewTab::loadsWithoutActivityFeedHidesRecentEventsCard() {
    auto b = makeBundle(/*includeStubs=*/false);
    b.engine->load(shellUrl());
    QVERIFY2(!b.engine->rootObjects().isEmpty(),
             "shell QML failed to load with no activityFeed injected");
    QObject* root = rootOf(b);

    QObject* card = findByName(root, QStringLiteral("overviewCardRecentEvents"));
    QVERIFY2(card, "overviewCardRecentEvents not found");
    QVERIFY2(!card->property("visible").toBool(),
             "recent-events card should be hidden when activityFeed is absent");
}

// (11) Empty events list → card visible with placeholder text (mirrors
// prototype docs/06 §4.2: "首次启动时显示'暂无数据'"). Card hides only
// when activityFeed itself is absent (test #10); empty feed shows the
// card with a centered placeholder so first-time users see the feature.
void TestOverviewTab::loadsWithEmptyActivityFeedShowsPlaceholder() {
    auto b = makeBundle();  // activityFeed injected with empty events
    b.engine->load(shellUrl());
    QVERIFY(!b.engine->rootObjects().isEmpty());
    QObject* root = rootOf(b);

    QObject* card = findByName(root, QStringLiteral("overviewCardRecentEvents"));
    QVERIFY(card);
    QVERIFY2(card->property("visible").toBool(),
             "recent-events card should be visible even when feed is empty");

    QObject* hint = findByName(root, QStringLiteral("recentEventsEmptyHint"));
    QVERIFY2(hint, "recentEventsEmptyHint not found");
    QVERIFY2(hint->property("visible").toBool(),
             "empty-state hint should be visible when feed has zero events");

    QObject* repeater = findByName(root, QStringLiteral("recentEventsRepeater"));
    QVERIFY(repeater);
    QCOMPARE(repeater->property("count").toInt(), 0);
}

// (12) Two events in feed → card visible, Repeater count = 2, first row
// renders the expected title text. The title-content walk is load-bearing:
// if the modelData.title binding were broken, Repeater count would still
// be 2 but the rendered text would be empty.
void TestOverviewTab::loadsWithActivityEventsShowsRecentRows() {
    auto b = makeBundle();
    QVariantList events;
    // Use a fixed epoch ms so the formatted time is deterministic.
    // 1749900000000 ms = 2025-06-14 12:00 UTC; local HH:mm varies by TZ,
    // so we only assert Repeater count + title text (not the time string).
    QVariantMap e1; e1[QStringLiteral("timeMs")]    = QVariant::fromValue<qint64>(1749900000000LL);
                    e1[QStringLiteral("topic")]      = QStringLiteral("margin.aura.away");
                    e1[QStringLiteral("title")]      = QStringLiteral("锁屏触发(设备离开)");
                    e1[QStringLiteral("colorRole")]  = QStringLiteral("accentDanger");
    QVariantMap e2; e2[QStringLiteral("timeMs")]    = QVariant::fromValue<qint64>(1749900060000LL);
                    e2[QStringLiteral("topic")]      = QStringLiteral("margin.rhythm.break_due");
                    e2[QStringLiteral("title")]      = QStringLiteral("休息时间到");
                    e2[QStringLiteral("colorRole")]  = QStringLiteral("accentBrand");
    events << e1 << e2;
    b.activityFeed->setEvents(events);

    b.engine->load(shellUrl());
    QVERIFY(!b.engine->rootObjects().isEmpty());
    QObject* root = rootOf(b);

    QObject* card = findByName(root, QStringLiteral("overviewCardRecentEvents"));
    QVERIFY2(card, "overviewCardRecentEvents not found");
    QVERIFY2(card->property("visible").toBool(),
             "card should be visible when feed has events");

    QObject* repeater = findByName(root, QStringLiteral("recentEventsRepeater"));
    QVERIFY2(repeater, "recentEventsRepeater not found");
    QCOMPARE(repeater->property("count").toInt(), 2);

    // Placeholder must hide once events arrive — otherwise we'd render
    // both the centered "暂无数据" text and the row list.
    QObject* hint = findByName(root, QStringLiteral("recentEventsEmptyHint"));
    QVERIFY(hint);
    QVERIFY2(!hint->property("visible").toBool(),
             "empty-state hint should hide when feed has events");

    // Walk the visual tree for the first delegate row's title Text. The
    // Repeater instantiates delegates as child items of its parent
    // (ColumnLayout); the title Text is nested inside the row's
    // RowLayout. findByName walks both QObject children and QQuickItem
    // childItems (Repeater-created items live in childItems, not children).
    // We can't find the Text by objectName (delegates are anonymous),
    // so walk for any node whose `text` property matches the first
    // event's title. Exposed as a static helper because C++ lambdas
    // can't be self-referential without std::function.
    QVERIFY2(findByTextContent(card, QStringLiteral("锁屏触发(设备离开)")),
             "first row title text not rendered — modelData.title binding may be broken");
}

// (13) Six events in feed → Repeater count still 4 (OverviewTab slices
// model to .slice(0, 4) per prototype dashboard_overview.png).
void TestOverviewTab::loadsFourRowsMaxWhenActivityEventsExceed() {
    auto b = makeBundle();
    QVariantList events;
    for (int i = 0; i < 6; ++i) {
        QVariantMap e;
        e[QStringLiteral("timeMs")] = QVariant::fromValue<qint64>(1749900000000LL + i * 60000);
        e[QStringLiteral("topic")] = QStringLiteral("margin.aura.away");
        e[QStringLiteral("title")] = QStringLiteral("event %1").arg(i);
        e[QStringLiteral("colorRole")] = QStringLiteral("accentDanger");
        events << e;
    }
    b.activityFeed->setEvents(events);

    b.engine->load(shellUrl());
    QVERIFY(!b.engine->rootObjects().isEmpty());
    QObject* root = rootOf(b);

    QObject* repeater = findByName(root, QStringLiteral("recentEventsRepeater"));
    QVERIFY2(repeater, "recentEventsRepeater not found");
    QCOMPARE(repeater->property("count").toInt(), 4);
}

QTEST_MAIN(TestOverviewTab)
#include "test_overview_tab.moc"
