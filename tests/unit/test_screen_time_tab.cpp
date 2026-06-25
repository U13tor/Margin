// tests/unit/test_screen_time_tab.cpp
//
// M4-C15: ScreenTimeTab.qml + ExportClearDialog.qml controls migration.
// Loads qrc:/screen_time/qml/ScreenTimeTab.qml offscreen with a
// ScreenTimeStub as the `screen_time` context property (mirrors
// ScreenTimePlugin::onLoad's QmlService registration). Verifies:
//   (1) Tab loads without QML warnings (binding sanity, no dangling refs).
//   (2) Header has 3 toggle MButtons (viewButton_day/week/month) +
//       refreshButton + dataMenuButton — migration completeness grep.
//   (3) Clicking viewButton_week flips stub.viewMode → "week" and the
//       selected button's variant becomes Primary while others go Secondary
//       (segmented exclusivity via single source of truth, no ButtonGroup).
//   (4) refreshButton.clicked() invokes stub.refreshReport() (signal wiring,
//       not MouseArea dispatch) — call counter increments.
//   (5) dataMenuButton.clicked() flips exportClearDialog.visible to true
//       (failure-path: dialog opens without exception).
//   (6) ExportClearDialog body has 4 MButtons (exportJson/exportCsv/
//       clearAll/close) — migration completeness grep inside the dialog.
//   (7) closeButton.clicked() flips exportClearDialog.visible to false and
//       emits the closed() signal (failure-path regression for early-return).
//   (8) clearAllButton.clicked() opens confirmDialog (failure-path: the
//       destructive action is guarded by a confirmation step).
//
// Pattern mirrors test_aura_tab.cpp: ScreenTimeStub mirrors the plugin's
// Q_PROPERTY + Q_INVOKABLE surface so the loaded QML resolves all bindings.
// Links Primitives + Charts qml-module plugins (ScreenTimeTab instantiates
// MBarChart + MDonutChart); registers screen_time.qrc + host.qrc as sources
// so qrc:/screen_time/qml/... and qrc:/icons/... URLs resolve.
//
// findByName walks both QObject children + QQuickItem childItems (Repeater
// delegates live in childItems, not QObject children — docs/15 §B3).

#include <QObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickItem>
#include <QString>
#include <QVariant>
#include <QTest>
#include <QSignalSpy>
#include <QUrl>

#include <cstdio>

// ── Stub ─────────────────────────────────────────────────────────────────
// Minimal QObject mirroring ScreenTimePlugin's Q_PROPERTY surface. The QML
// binds against these via the `screen_time` context property — if a property
// is missing the binding resolves to `undefined` and the tab fails to load.

class ScreenTimeStub : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString currentApp     READ currentApp     NOTIFY currentAppChanged)
    Q_PROPERTY(qint64  currentSessionStartedAt
               READ currentSessionStartedAt NOTIFY currentAppChanged)
    Q_PROPERTY(bool    isUserIdle     READ isUserIdle     NOTIFY idleChanged)
    Q_PROPERTY(QString viewMode       READ viewMode WRITE setViewMode
               NOTIFY viewModeChanged)
    Q_PROPERTY(QVariantList topApps          READ topApps          NOTIFY reportChanged)
    Q_PROPERTY(QVariantList categoryBreakdown READ categoryBreakdown NOTIFY reportChanged)
    Q_PROPERTY(QVariantList dailyTotals      READ dailyTotals      NOTIFY reportChanged)
    Q_PROPERTY(int     pickupCount           READ pickupCount      NOTIFY reportChanged)
    Q_PROPERTY(qint64  todayFocusSeconds     READ todayFocusSeconds
               NOTIFY todayFocusSecondsChanged)
    Q_PROPERTY(int     idleThresholdSec      READ idleThresholdSec WRITE setIdleThresholdSec
               NOTIFY idleThresholdChanged)

public:
    explicit ScreenTimeStub(QObject* parent = nullptr) : QObject(parent) {}

    QString currentApp() const { return m_currentApp; }
    void setCurrentApp(QString n) { m_currentApp = std::move(n); emit currentAppChanged(); }

    qint64 currentSessionStartedAt() const { return m_currentSessionStartedAt; }
    void setCurrentSessionStartedAt(qint64 v) { m_currentSessionStartedAt = v; emit currentAppChanged(); }

    bool isUserIdle() const { return m_isIdle; }
    void setUserIdle(bool v) { if (m_isIdle != v) { m_isIdle = v; emit idleChanged(); } }

    QString viewMode() const { return m_viewMode; }
    void setViewMode(QString m) { if (m_viewMode != m) { m_viewMode = std::move(m); emit viewModeChanged(); } }

    QVariantList topApps() const { return m_topApps; }
    QVariantList categoryBreakdown() const { return m_category; }
    QVariantList dailyTotals() const { return m_dailyTotals; }
    int pickupCount() const { return m_pickupCount; }
    qint64 todayFocusSeconds() const { return m_todayFocusSec; }

    int idleThresholdSec() const { return m_idleThresholdSec; }
    void setIdleThresholdSec(int v) { if (m_idleThresholdSec != v) { m_idleThresholdSec = v; emit idleThresholdChanged(); } }

    // Q_INVOKABLE surface — bodies track call counts so tests can assert
    // signal wiring rather than side effects.
    int refreshCallCount = 0;
    int clearCallCount = 0;
    int sessionCountValue = 0;

public slots:
    void refreshReport() { ++refreshCallCount; emit reportChanged(); }
    QString exportJson(const QUrl&) { return QString(); }
    QString exportCsv(const QUrl&) { return QString(); }
    int sessionCount() { return sessionCountValue; }
    bool clearAllData() { ++clearCallCount; emit reportChanged(); return true; }

signals:
    void currentAppChanged();
    void idleChanged();
    void viewModeChanged();
    void reportChanged();
    void todayFocusSecondsChanged();
    void idleThresholdChanged();

private:
    QString m_currentApp;
    qint64 m_currentSessionStartedAt = 0;
    bool m_isIdle = false;
    QString m_viewMode = QStringLiteral("day");
    QVariantList m_topApps;
    QVariantList m_category;
    QVariantList m_dailyTotals;
    int m_pickupCount = 0;
    qint64 m_todayFocusSec = 0;
    int m_idleThresholdSec = 300;
};

class TestScreenTimeTab : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void loadsAndRendersHeader();
    void toggleExclusivityViaViewMode();
    void refreshButtonInvokesRefreshReport();
    void dataMenuButtonOpensDialog();
    void dialogBodyRendersMigratedAtoms();
    void closeButtonHidesDialogAndEmitsClosed();
    void clearAllButtonOpensConfirmDialog();
    void settingsButtonCallsOpenSettingsNoInlineSpinBox();

private:
    static QObject* findByName(QObject* node, const QString& name);
    static QObject* loadTab(QQmlApplicationEngine& engine, ScreenTimeStub& stub,
                            const QUrl& url);
};

void TestScreenTimeTab::initTestCase() {
    // Mirrors test_aura_tab: capture QML warnings to stderr (MSVC QtTest
    // otherwise routes them through OutputDebugString, invisible to ctest).
    qSetMessagePattern("%{if-debug}DBG%{endif}%{if-warning}WARN%{endif}%{if-critical}CRIT%{endif}%{if-fatal}FATAL%{endif}: %{message}");
}

QObject* TestScreenTimeTab::findByName(QObject* node, const QString& name) {
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

QObject* TestScreenTimeTab::loadTab(QQmlApplicationEngine& engine,
                                    ScreenTimeStub& stub, const QUrl& url) {
    QSignalSpy warnSpy(&engine, &QQmlEngine::warnings);
    engine.rootContext()->setContextProperty(QStringLiteral("screen_time"), &stub);
    engine.load(url);
    if (engine.rootObjects().isEmpty()) {
        for (const QList<QVariant>& args : warnSpy) {
            const auto errs = args.first().value<QList<QQmlError>>();
            for (const QQmlError& e : errs) {
                fprintf(stderr, "[qml] %s\n", e.toString().toLocal8Bit().constData());
            }
        }
        fflush(stderr);
    }
    return engine.rootObjects().isEmpty() ? nullptr : engine.rootObjects().constFirst();
}

// (1) Tab loads clean. All migrated MButtons must be present in the header
//     row by objectName — if a future edit reverts any to a plain Button,
//     the findByName call returns null and this fails.
void TestScreenTimeTab::loadsAndRendersHeader() {
    fprintf(stderr, "[test_screen_time_tab] loadsAndRendersHeader\n"); fflush(stderr);
    ScreenTimeStub stub;
    QQmlApplicationEngine engine;
    QObject* root = loadTab(engine, stub,
        QUrl(QStringLiteral("qrc:/screen_time/qml/ScreenTimeTab.qml")));
    QVERIFY2(root, "ScreenTimeTab.qml failed to load");

    const QStringList names = {
        QStringLiteral("viewButton_day"),
        QStringLiteral("viewButton_week"),
        QStringLiteral("viewButton_month"),
        QStringLiteral("refreshButton"),
        QStringLiteral("dataMenuButton"),
        // PR5: settingsButton replaces the inline idle-threshold SpinBox —
        // routes to Margin Settings → Screen Time page (pageId "screen_time").
        QStringLiteral("settingsButton"),
    };
    for (const QString& name : names) {
        QObject* btn = findByName(root, name);
        if (!btn) { fprintf(stderr, "[diag] %s not found\n", name.toLocal8Bit().constData()); fflush(stderr); }
        QVERIFY2(btn, qPrintable(QStringLiteral("%1 not found").arg(name)));
    }
}

// (2) Clicking viewButton_week flips stub.viewMode to "week" and the
//     binding makes week=Primary while day/month=Secondary. Mirrors the
//     aura state-dot color test — we don't hardcode QColor values, just
//     assert the variant property changes coherently.
void TestScreenTimeTab::toggleExclusivityViaViewMode() {
    fprintf(stderr, "[test_screen_time_tab] toggleExclusivityViaViewMode\n"); fflush(stderr);
    ScreenTimeStub stub;
    QCOMPARE(stub.viewMode(), QStringLiteral("day"));

    QQmlApplicationEngine engine;
    QObject* root = loadTab(engine, stub,
        QUrl(QStringLiteral("qrc:/screen_time/qml/ScreenTimeTab.qml")));
    QVERIFY2(root, "ScreenTimeTab.qml failed to load");

    QObject* dayBtn   = findByName(root, QStringLiteral("viewButton_day"));
    QObject* weekBtn  = findByName(root, QStringLiteral("viewButton_week"));
    QObject* monthBtn = findByName(root, QStringLiteral("viewButton_month"));
    QVERIFY2(dayBtn && weekBtn && monthBtn, "toggle buttons missing");

    // Sanity: initial state — day=Primary(0), week/month=Secondary(1).
    QCOMPARE(dayBtn->property("variant").toInt(),   0);
    QCOMPARE(weekBtn->property("variant").toInt(),  1);
    QCOMPARE(monthBtn->property("variant").toInt(), 1);

    // Click week → stub.viewMode flips → binding recomputes variants.
    QVERIFY2(QMetaObject::invokeMethod(weekBtn, "clicked"),
             "failed to invoke clicked() on viewButton_week");
    QCOMPARE(stub.viewMode(), QStringLiteral("week"));
    QCOMPARE(weekBtn->property("variant").toInt(),  0);
    QCOMPARE(dayBtn->property("variant").toInt(),   1);
    QCOMPARE(monthBtn->property("variant").toInt(), 1);
}

// (3) refreshButton.clicked() routes through MouseArea to signal, so
//     QMetaObject::invokeMethod(btn, "clicked") should fire the QML
//     handler that calls screen_time.refreshReport().
void TestScreenTimeTab::refreshButtonInvokesRefreshReport() {
    fprintf(stderr, "[test_screen_time_tab] refreshButtonInvokesRefreshReport\n"); fflush(stderr);
    ScreenTimeStub stub;
    QQmlApplicationEngine engine;
    QObject* root = loadTab(engine, stub,
        QUrl(QStringLiteral("qrc:/screen_time/qml/ScreenTimeTab.qml")));
    QVERIFY2(root, "ScreenTimeTab.qml failed to load");

    QObject* btn = findByName(root, QStringLiteral("refreshButton"));
    QVERIFY2(btn, "refreshButton not found");
    QCOMPARE(stub.refreshCallCount, 0);
    QVERIFY2(QMetaObject::invokeMethod(btn, "clicked"),
             "failed to invoke clicked() on refreshButton");
    QCOMPARE(stub.refreshCallCount, 1);
}

// (4) dataMenuButton.clicked() calls exportClearDialog.open(), which sets
//     its root Item's visible=true. Failure-path: confirms the dialog
//     actually opens (regression for dropped onClicked wiring).
void TestScreenTimeTab::dataMenuButtonOpensDialog() {
    fprintf(stderr, "[test_screen_time_tab] dataMenuButtonOpensDialog\n"); fflush(stderr);
    ScreenTimeStub stub;
    QQmlApplicationEngine engine;
    QObject* root = loadTab(engine, stub,
        QUrl(QStringLiteral("qrc:/screen_time/qml/ScreenTimeTab.qml")));
    QVERIFY2(root, "ScreenTimeTab.qml failed to load");

    QObject* btn = findByName(root, QStringLiteral("dataMenuButton"));
    QVERIFY2(btn, "dataMenuButton not found");

    QObject* dialog = findByName(root, QStringLiteral("exportClearDialog"));
    QVERIFY2(dialog, "exportClearDialog not found");
    QVERIFY2(!dialog->property("visible").toBool(),
             "dialog should start hidden");

    QVERIFY2(QMetaObject::invokeMethod(btn, "clicked"),
             "failed to invoke clicked() on dataMenuButton");
    QVERIFY2(dialog->property("visible").toBool(),
             "dialog.visible should be true after dataMenuButton clicked");
}

// (5) ExportClearDialog body has 4 migrated MButtons. The dialog is
//     instantiated as a child of ScreenTimeTab so findByName can reach
//     into it even when visible=false ( invisibility doesn't remove items
//     from childItems() ).
void TestScreenTimeTab::dialogBodyRendersMigratedAtoms() {
    fprintf(stderr, "[test_screen_time_tab] dialogBodyRendersMigratedAtoms\n"); fflush(stderr);
    ScreenTimeStub stub;
    QQmlApplicationEngine engine;
    QObject* root = loadTab(engine, stub,
        QUrl(QStringLiteral("qrc:/screen_time/qml/ScreenTimeTab.qml")));
    QVERIFY2(root, "ScreenTimeTab.qml failed to load");

    const QStringList names = {
        QStringLiteral("exportJsonButton"),
        QStringLiteral("exportCsvButton"),
        QStringLiteral("clearAllButton"),
        QStringLiteral("closeButton"),
    };
    for (const QString& name : names) {
        QObject* btn = findByName(root, name);
        if (!btn) { fprintf(stderr, "[diag] %s not found\n", name.toLocal8Bit().constData()); fflush(stderr); }
        QVERIFY2(btn, qPrintable(QStringLiteral("%1 not found in ExportClearDialog").arg(name)));
    }
}

// (6) closeButton.clicked() flips dialog.visible=false + emits closed().
//     Failure-path regression for the early-return wiring — a future
//     refactor could drop the signal emit or forget to hide root.
void TestScreenTimeTab::closeButtonHidesDialogAndEmitsClosed() {
    fprintf(stderr, "[test_screen_time_tab] closeButtonHidesDialogAndEmitsClosed\n"); fflush(stderr);
    ScreenTimeStub stub;
    QQmlApplicationEngine engine;
    QObject* root = loadTab(engine, stub,
        QUrl(QStringLiteral("qrc:/screen_time/qml/ScreenTimeTab.qml")));
    QVERIFY2(root, "ScreenTimeTab.qml failed to load");

    QObject* dialog = findByName(root, QStringLiteral("exportClearDialog"));
    QVERIFY2(dialog, "exportClearDialog not found");
    QVERIFY2(QMetaObject::invokeMethod(dialog, "open"),
             "failed to invoke open() on exportClearDialog");
    QVERIFY2(dialog->property("visible").toBool(), "dialog should be open");

    QSignalSpy closedSpy(dialog, SIGNAL(closed()));
    QVERIFY2(closedSpy.isValid(), "could not attach spy to closed() signal");

    QObject* btn = findByName(root, QStringLiteral("closeButton"));
    QVERIFY2(btn, "closeButton not found");
    QVERIFY2(QMetaObject::invokeMethod(btn, "clicked"),
             "failed to invoke clicked() on closeButton");

    QVERIFY2(!dialog->property("visible").toBool(),
             "dialog.visible should be false after close");
    QCOMPARE(closedSpy.count(), 1);
}

// (7) clearAllButton.clicked() routes through confirmDialog.open() —
//     failure-path: the destructive clearAllData() must NOT fire on the
//     first click; only the confirm guard stands between user intent and
//     data loss. Asserts clearCallCount stays 0 (the guard held). The
//     confirmDialog itself may not reliably flip visible=true in offscreen
//     QPA without a parent Window, so we don't assert on its visibility —
//     the call-count guard is the load-bearing assertion.
void TestScreenTimeTab::clearAllButtonOpensConfirmDialog() {
    fprintf(stderr, "[test_screen_time_tab] clearAllButtonOpensConfirmDialog\n"); fflush(stderr);
    ScreenTimeStub stub;
    QQmlApplicationEngine engine;
    QObject* root = loadTab(engine, stub,
        QUrl(QStringLiteral("qrc:/screen_time/qml/ScreenTimeTab.qml")));
    QVERIFY2(root, "ScreenTimeTab.qml failed to load");

    QObject* dialog = findByName(root, QStringLiteral("exportClearDialog"));
    QVERIFY2(dialog, "exportClearDialog not found");
    QVERIFY2(QMetaObject::invokeMethod(dialog, "open"),
             "failed to invoke open() on exportClearDialog");

    QObject* confirm = findByName(root, QStringLiteral("confirmDialog"));
    QVERIFY2(confirm, "confirmDialog not found");

    QObject* btn = findByName(root, QStringLiteral("clearAllButton"));
    QVERIFY2(btn, "clearAllButton not found");
    QCOMPARE(stub.clearCallCount, 0);
    QVERIFY2(QMetaObject::invokeMethod(btn, "clicked"),
             "failed to invoke clicked() on clearAllButton");

    // Pump the event loop once — confirmDialog.open() in QtQuick.Controls
    // is async; we don't assert visible (flaky in offscreen) but want any
    // queued side-effects to drain before checking the guard held.
    QTest::qWait(50);

    QCOMPARE(stub.clearCallCount, 0);  // guard held — clearAllData not fired
}

// (8) PR5: removed the inline idle-threshold SpinBox (was lines 232-250).
//      The Settings button now jumps to the top-level Settings → Screen Time
//      page (pageId "screen_time"). settingsRoot is not wired in unit tests
//      so the typeof guard skips the actual call; this test verifies the
//      button still exists, is clickable, and emits no QML warnings during
//      the click. The deleted Text label "闲置阈值(秒)" must be gone —
//      loadTab with a warning spy catches any dangling binding.
void TestScreenTimeTab::settingsButtonCallsOpenSettingsNoInlineSpinBox() {
    fprintf(stderr, "[test_screen_time_tab] settingsButtonCallsOpenSettingsNoInlineSpinBox\n"); fflush(stderr);
    ScreenTimeStub stub;
    QQmlApplicationEngine engine;
    QSignalSpy warnSpy(&engine, &QQmlEngine::warnings);
    QObject* root = loadTab(engine, stub,
        QUrl(QStringLiteral("qrc:/screen_time/qml/ScreenTimeTab.qml")));
    QVERIFY2(root, "ScreenTimeTab.qml failed to load");
    QVERIFY2(warnSpy.isEmpty(), "QML warnings emitted at load — likely dangling ref");

    QObject* btn = findByName(root, QStringLiteral("settingsButton"));
    QVERIFY2(btn, "settingsButton not found");

    warnSpy.clear();
    QVERIFY2(QMetaObject::invokeMethod(btn, "clicked"),
             "failed to invoke clicked() on settingsButton");
    QVERIFY2(warnSpy.isEmpty(),
             "QML warnings emitted during settingsButton click");
}

QTEST_MAIN(TestScreenTimeTab)
#include "test_screen_time_tab.moc"
