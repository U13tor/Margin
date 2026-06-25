// tests/unit/test_aura_tab.cpp
//
// M4-C14a + C14b: AuraTab.qml + AuraSettingsPage.qml fidelity-pass.
// Loads qrc:/aura/qml/AuraTab.qml (and, for C14b, AuraSettingsPage.qml)
// offscreen with an AuraStub as the `aura` context property (mirrors
// AuraLockerPlugin::onLoad's QmlService registration). Verifies:
//   (1) Status card + state dot + state label render
//   (2) State-dot color flips with proximityState (paired → success,
//       away → warning, inactive → muted)
//   (3) Event log ListView renders one MListItem delegate per recentEvents
//       entry, with title = message and subtitle = formatted time
//   (4) "Settings" MButton.clicked() pushes AuraSettingsPage onto StackView
//       (signal wiring, not MouseArea dispatch)
//   (5) AuraSettingsPage renders 4 MButtons + 4 MSliders (C14b migration
//       completeness — confirms no Button/SpinBox slipped through)
//   (6) awayDelay MSlider ↔ aura.awayDelaySec binding: stub value flows
//       to slider.value, _setValueFromFraction(0.5) round-trips back
//   (7) Back MButton in AuraSettingsPage pops the StackView (depth 2 → 1).
//       Regression guard for a real dogfood bug: C14b had dropped
//       `import QtQuick.Controls` from AuraSettingsPage.qml, which left
//       the StackView attached property (`root.StackView.view`) unresolved
//       and made the Back button silently no-op.
//
// Pattern: links Primitives qml-module plugin (Theme + MCard + MButton +
// MListItem + MIcon), registers aura_locker.qrc + host.qrc as sources so
// qrc:/aura/qml/... and qrc:/icons/... URLs resolve. QtQuick.Controls
// resolves transitively via QQmlApplicationEngine's default import path
// (RhythmTab.qml uses the same StackView pattern; test_rhythm_tab.cpp is
// the working precedent).

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
#include <memory>

// ── Stub ─────────────────────────────────────────────────────────────────
// Minimal QObject exposing the Q_PROPERTYs AuraTab.qml + AuraSettingsPage.qml
// bind to. AUTOMOC generates metaobject code. Lives on the stack of the test
// method that needs it; passed to the engine via setContextProperty. C14a
// only exercises the main view, but the openSettings test pushes
// AuraSettingsPage via StackView — so we expose the settings-page properties
// too, otherwise its bindings resolve to `undefined` and the push fails.

class AuraStub : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString pairedDeviceName READ pairedDeviceName
               NOTIFY pairedDeviceChanged)
    Q_PROPERTY(QString proximityState READ proximityState
               NOTIFY proximityStateChanged)
    Q_PROPERTY(qint64 lastLockMs READ lastLockMs
               NOTIFY lastLockChanged)
    Q_PROPERTY(QVariantList recentEvents READ recentEvents
               NOTIFY recentEventsChanged)
    Q_PROPERTY(int awayDelaySec READ awayDelaySec WRITE setAwayDelaySec
               NOTIFY awayDelayChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(int scanDurationSec READ scanDurationSec WRITE setScanDurationSec
               NOTIFY scanDurationSecChanged)
    Q_PROPERTY(QVariantList scannedDevices READ scannedDevices
               NOTIFY scannedDevicesChanged)
    Q_PROPERTY(int rssiThresholdDbm READ rssiThresholdDbm WRITE setRssiThresholdDbm
               NOTIFY rssiThresholdChanged)
    Q_PROPERTY(int cooldownSec READ cooldownSec WRITE setCooldownSec
               NOTIFY cooldownChanged)
public:
    explicit AuraStub(QObject* parent = nullptr) : QObject(parent) {}

    QString pairedDeviceName() const { return m_pairedName; }
    void setPairedDeviceName(QString n) { m_pairedName = std::move(n); emit pairedDeviceChanged(); }

    QString proximityState() const { return m_proximityState; }
    void setProximityState(QString s) { m_proximityState = std::move(s); emit proximityStateChanged(); }

    qint64 lastLockMs() const { return m_lastLockMs; }
    void setLastLockMs(qint64 v) { m_lastLockMs = v; emit lastLockChanged(); }

    QVariantList recentEvents() const { return m_recentEvents; }
    void setRecentEvents(QVariantList e) { m_recentEvents = std::move(e); emit recentEventsChanged(); }

    int awayDelaySec() const { return m_awayDelaySec; }
    void setAwayDelaySec(int v) { if (m_awayDelaySec != v) { m_awayDelaySec = v; emit awayDelayChanged(); } }

    bool scanning() const { return m_scanning; }
    int scanDurationSec() const { return m_scanDurationSec; }
    void setScanDurationSec(int v) { if (m_scanDurationSec != v) { m_scanDurationSec = v; emit scanDurationSecChanged(); } }

    QVariantList scannedDevices() const { return m_scannedDevices; }
    int rssiThresholdDbm() const { return m_rssiThresholdDbm; }
    void setRssiThresholdDbm(int v) { if (m_rssiThresholdDbm != v) { m_rssiThresholdDbm = v; emit rssiThresholdChanged(); } }

    int cooldownSec() const { return m_cooldownSec; }
    void setCooldownSec(int v) { if (m_cooldownSec != v) { m_cooldownSec = v; emit cooldownChanged(); } }

public slots:
    // AuraSettingsPage invokes these on click — no-op bodies; C14a only
    // asserts the push happens, not that the calls have side effects.
    void unpair() {}
    void startScan() {}
    void stopScan() {}
    void pairDevice(const QString&, const QString&) {}

signals:
    void pairedDeviceChanged();
    void proximityStateChanged();
    void lastLockChanged();
    void recentEventsChanged();
    void awayDelayChanged();
    void scanningChanged();
    void scanDurationSecChanged();
    void scannedDevicesChanged();
    void rssiThresholdChanged();
    void cooldownChanged();

private:
    QString m_pairedName;
    QString m_proximityState = QStringLiteral("inactive");
    qint64 m_lastLockMs = 0;
    QVariantList m_recentEvents;
    int m_awayDelaySec = 30;
    bool m_scanning = false;
    int m_scanDurationSec = 10;
    QVariantList m_scannedDevices;
    int m_rssiThresholdDbm = -75;
    int m_cooldownSec = 60;
};

class TestAuraTab : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void loadsAndRendersStatusCard();
    void stateDotColorFollowsProximityState();
    void eventListRendersItemsAsMListItem();
    void openSettingsButtonPushesSettingsPage();
    void settingsPageRendersMigratedAtoms();
    void awayDelaySliderBindsToAuraProperty();
    void backButtonPopsStackView();

private:
    static QObject* findByName(QObject* node, const QString& name);
};

void TestAuraTab::initTestCase() {
    // Dump QML warnings to stderr if load fails — Windows MSVC QtTest writes
    // to OutputDebugString by default (invisible to ctest/LastTest.log), so
    // we capture via QSignalSpy in each test method and flush manually.
    // Setting the message pattern keeps qWarning/qCritical concise.
    qSetMessagePattern("%{if-debug}DBG%{endif}%{if-warning}WARN%{endif}%{if-critical}CRIT%{endif}%{if-fatal}FATAL%{endif}: %{message}");
}

// Walk QObject children + QQuickItem childItems (Repeater delegates live in
// childItems, not children — see docs/15-dev-gotchas.md feedback note).
QObject* TestAuraTab::findByName(QObject* node, const QString& name) {
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

// (1) Default load (inactive state, no paired device, empty event trail):
//     the status card renders with a state dot + "State: inactive" label.
//     The state-dot color comes from the stateColor() function in AuraTab —
//     we don't assert the exact QColor here (that's a theme-token test's
//     job); we assert the binding resolved (color is a valid QColor, not
//     the default Qt.transparent that an unbound Rectangle would report).
void TestAuraTab::loadsAndRendersStatusCard() {
    fprintf(stderr, "[test_aura_tab] loadsAndRendersStatusCard\n"); fflush(stderr);
    AuraStub aura;
    QQmlApplicationEngine engine;
    QSignalSpy warnSpy(&engine, &QQmlEngine::warnings);
    engine.rootContext()->setContextProperty(QStringLiteral("aura"), &aura);
    engine.load(QUrl(QStringLiteral("qrc:/aura/qml/AuraTab.qml")));
    if (engine.rootObjects().isEmpty()) {
        for (const QList<QVariant>& args : warnSpy) {
            const auto errs = args.first().value<QList<QQmlError>>();
            for (const QQmlError& e : errs) {
                fprintf(stderr, "[qml] %s\n",
                        e.toString().toLocal8Bit().constData());
            }
        }
        fflush(stderr);
    }
    QVERIFY2(!engine.rootObjects().isEmpty(), "AuraTab.qml failed to load");
    QObject* root = engine.rootObjects().constFirst();

    QObject* card = findByName(root, QStringLiteral("auraStatusCard"));
    if (!card) { fprintf(stderr, "[diag] card not found\n"); fflush(stderr); }
    QVERIFY2(card, "auraStatusCard MCard not found");

    QObject* dot = findByName(root, QStringLiteral("stateDot"));
    if (!dot) { fprintf(stderr, "[diag] dot not found\n"); fflush(stderr); }
    QVERIFY2(dot, "stateDot Rectangle not found");

    QObject* label = findByName(root, QStringLiteral("stateLabel"));
    if (!label) { fprintf(stderr, "[diag] label not found\n"); fflush(stderr); }
    QVERIFY2(label, "stateLabel Text not found");
    fprintf(stderr, "[diag] label.text=%s\n",
            label->property("text").toString().toLocal8Bit().constData());
    fflush(stderr);
    QCOMPARE(label->property("text").toString(), QStringLiteral("State: inactive"));
}

// (2) proximityState flips → dot.color changes. Asserts the binding fires
//     by reading color at three states and checking they're not all equal.
//     We don't hardcode QColor values here — Theme.accentSuccess etc. are
//     themselves asserted by test_theme_tokens, so we only check "the
//     binding produced a different color when state changed."
void TestAuraTab::stateDotColorFollowsProximityState() {
    fprintf(stderr, "[test_aura_tab] stateDotColorFollowsProximityState\n"); fflush(stderr);
    AuraStub aura;
    aura.setProximityState(QStringLiteral("paired"));
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("aura"), &aura);
    engine.load(QUrl(QStringLiteral("qrc:/aura/qml/AuraTab.qml")));
    QVERIFY2(!engine.rootObjects().isEmpty(), "AuraTab.qml failed to load");
    QObject* root = engine.rootObjects().constFirst();

    QObject* dot = findByName(root, QStringLiteral("stateDot"));
    QVERIFY2(dot, "stateDot Rectangle not found");

    const QColor pairedColor = dot->property("color").value<QColor>();
    QVERIFY2(pairedColor.isValid(), "paired-state dot color is not a valid QColor");

    aura.setProximityState(QStringLiteral("away"));
    const QColor awayColor = dot->property("color").value<QColor>();
    QVERIFY2(awayColor.isValid(), "away-state dot color is not a valid QColor");

    aura.setProximityState(QStringLiteral("inactive"));
    const QColor inactiveColor = dot->property("color").value<QColor>();
    QVERIFY2(inactiveColor.isValid(), "inactive-state dot color is not a valid QColor");

    QVERIFY2(pairedColor != awayColor,
             "state-dot color should differ between paired and away");
    QVERIFY2(pairedColor != inactiveColor,
             "state-dot color should differ between paired and inactive");
    QVERIFY2(awayColor != inactiveColor,
             "state-dot color should differ between away and inactive");
}

// (3) Inject two recentEvents — the ListView must render two delegates
//     whose titles match the messages we set. subtitle binding is exercised
//     indirectly (MListItem's _hasSubtitle flag flips when subtitle is
//     non-empty — if formatTime returned "" the subtitle Text would be
//     hidden but not crash; this test only locks title + count).
void TestAuraTab::eventListRendersItemsAsMListItem() {
    fprintf(stderr, "[test_aura_tab] eventListRendersItemsAsMListItem\n"); fflush(stderr);
    AuraStub aura;
    aura.setProximityState(QStringLiteral("paired"));
    aura.setPairedDeviceName(QStringLiteral("Test Device"));

    QVariantList events;
    // Match AuraLockerPlugin::rebuildRecentEventsCache() ordering: newest
    // first (it uses rbegin() / rend()). The stub feeds them in display
    // order so ListView index 0 is the newest event.
    QVariantMap e2;
    e2.insert(QStringLiteral("timestampMs"), qint64(1740000060000));
    e2.insert(QStringLiteral("kind"), QStringLiteral("back"));
    e2.insert(QStringLiteral("message"), QStringLiteral("Device returned"));
    events.append(e2);
    QVariantMap e1;
    e1.insert(QStringLiteral("timestampMs"), qint64(1740000000000));
    e1.insert(QStringLiteral("kind"), QStringLiteral("lock"));
    e1.insert(QStringLiteral("message"), QStringLiteral("Screen locked"));
    events.append(e1);
    aura.setRecentEvents(events);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("aura"), &aura);
    engine.load(QUrl(QStringLiteral("qrc:/aura/qml/AuraTab.qml")));
    QVERIFY2(!engine.rootObjects().isEmpty(), "AuraTab.qml failed to load");
    QObject* root = engine.rootObjects().constFirst();

    QObject* list = findByName(root, QStringLiteral("eventList"));
    QVERIFY2(list, "eventList ListView not found");

    QCOMPARE(list->property("count").toInt(), 2);

    // index 0 = newest = "Device returned" (back → icon-check).
    QObject* item0 = findByName(root, QStringLiteral("eventItem_0"));
    QVERIFY2(item0, "eventItem_0 delegate not found");
    QCOMPARE(item0->property("title").toString(), QStringLiteral("Device returned"));
    QCOMPARE(item0->property("iconSource").toString(),
             QStringLiteral("qrc:/icons/icon-check.svg"));
}

// (4) openSettingsButton regression guard. M5-C4a changed the button's
//     semantics: it used to push AuraSettingsPage into a local StackView
//     (depth 1 → 2); now it calls settingsRoot.openSettings() (top-level
//     Settings window) and depth stays at 1. Unit tests don't wire
//     settingsRoot, so the typeof guard skips the call cleanly.
//     Test verifies the button still renders + clicks without warnings.
void TestAuraTab::openSettingsButtonPushesSettingsPage() {
    fprintf(stderr, "[test_aura_tab] openSettingsButtonPushesSettingsPage (M5-C4a: depth no longer changes)\n"); fflush(stderr);
    AuraStub aura;
    QQmlApplicationEngine engine;
    QSignalSpy warnSpy(&engine, &QQmlEngine::warnings);
    engine.rootContext()->setContextProperty(QStringLiteral("aura"), &aura);
    engine.load(QUrl(QStringLiteral("qrc:/aura/qml/AuraTab.qml")));
    QVERIFY2(!engine.rootObjects().isEmpty(), "AuraTab.qml failed to load");
    QObject* root = engine.rootObjects().constFirst();

    QObject* stack = findByName(root, QStringLiteral("stack"));
    QVERIFY2(stack, "StackView (objectName=stack) not found");
    QCOMPARE(stack->property("depth").toInt(), 1);

    QObject* btn = findByName(root, QStringLiteral("openSettingsButton"));
    QVERIFY2(btn, "openSettingsButton MButton not found");

    QVERIFY2(QMetaObject::invokeMethod(btn, "clicked"),
             "failed to invoke clicked() on openSettingsButton");

    // Depth stays at 1 — settingsRoot isn't wired in unit-test context so
    // the typeof guard skips the call. Production wires settingsRoot from
    // HostCore; the Settings integration test (M5-C4e) covers that path.
    QCOMPARE(stack->property("depth").toInt(), 1);
    QVERIFY2(warnSpy.isEmpty(), "QML warnings emitted during openSettingsButton click");
}

// (5) Load AuraSettingsPage.qml directly (not via StackView push) and
//     assert presence of the 4 migrated MButtons + 4 MSliders by objectName.
//     If a future edit reverts an MButton to a plain Button or drops an
//     MSlider, the corresponding findByName returns null and this fails.
//     SpinBox-based variants of yore had no objectNames at all — their
//     absence is implicit (the Slider objectNames wouldn't exist either
//     way, but a reverted SpinBox wouldn't carry these names).
void TestAuraTab::settingsPageRendersMigratedAtoms() {
    fprintf(stderr, "[test_aura_tab] settingsPageRendersMigratedAtoms\n"); fflush(stderr);
    AuraStub aura;
    QQmlApplicationEngine engine;
    QSignalSpy warnSpy(&engine, &QQmlEngine::warnings);
    engine.rootContext()->setContextProperty(QStringLiteral("aura"), &aura);
    engine.load(QUrl(QStringLiteral("qrc:/aura/qml/AuraSettingsPage.qml")));
    if (engine.rootObjects().isEmpty()) {
        for (const QList<QVariant>& args : warnSpy) {
            const auto errs = args.first().value<QList<QQmlError>>();
            for (const QQmlError& e : errs) {
                fprintf(stderr, "[qml] %s\n",
                        e.toString().toLocal8Bit().constData());
            }
        }
        fflush(stderr);
    }
    QVERIFY2(!engine.rootObjects().isEmpty(), "AuraSettingsPage.qml failed to load");
    QObject* root = engine.rootObjects().constFirst();

    const QStringList buttonNames = {
        QStringLiteral("unpairButton"),
        QStringLiteral("scanButton"),
        QStringLiteral("stopButton"),
        // backButton removed in M5-C4a — AuraSettingsPage is now loaded by
        // the top-level SettingsWindow (no StackView pop pattern).
    };
    for (const QString& name : buttonNames) {
        QObject* btn = findByName(root, name);
        if (!btn) { fprintf(stderr, "[diag] %s not found\n", name.toLocal8Bit().constData()); fflush(stderr); }
        QVERIFY2(btn, qPrintable(QStringLiteral("%1 MButton not found").arg(name)));
    }

    const QStringList sliderNames = {
        QStringLiteral("rssiSlider"),
        QStringLiteral("awayDelaySlider"),
        QStringLiteral("cooldownSlider"),
        QStringLiteral("scanDurationSlider"),
    };
    for (const QString& name : sliderNames) {
        QObject* sld = findByName(root, name);
        if (!sld) { fprintf(stderr, "[diag] %s not found\n", name.toLocal8Bit().constData()); fflush(stderr); }
        QVERIFY2(sld, qPrintable(QStringLiteral("%1 MSlider not found").arg(name)));
    }
}

// (6) MSlider ↔ aura.awayDelaySec two-way binding. Stub-side: set property,
//     assert slider.value mirrors it. Slider-side: invoke _setValueFromFraction
//     (the test-controllable entry point per MSlider.qml), assert the stub's
//     WRITE setter fired via QSignalSpy and the final value matches the
//     expected snapped stepSize=2 result.
//
//     Fraction 0.5 over [10,120] → raw 65 → snapped to stepSize 2 → 66.
void TestAuraTab::awayDelaySliderBindsToAuraProperty() {
    fprintf(stderr, "[test_aura_tab] awayDelaySliderBindsToAuraProperty\n"); fflush(stderr);
    AuraStub aura;
    aura.setAwayDelaySec(60);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("aura"), &aura);
    engine.load(QUrl(QStringLiteral("qrc:/aura/qml/AuraSettingsPage.qml")));
    QVERIFY2(!engine.rootObjects().isEmpty(), "AuraSettingsPage.qml failed to load");
    QObject* root = engine.rootObjects().constFirst();

    QObject* slider = findByName(root, QStringLiteral("awayDelaySlider"));
    QVERIFY2(slider, "awayDelaySlider MSlider not found");

    // Stub → slider: initial 60 should reflect via Q_PROPERTY read.
    QCOMPARE(slider->property("value").toInt(), 60);

    // Slider → stub: invoke _setValueFromFraction(0.5) and assert signal.
    // QML functions exposed from .qml files marshal their args as QVariant
    // when invoked via QMetaObject (qmlcachegen rejects `function(real)`
    // in Qt 6.5 — see test_mslider.cpp for the same workaround).
    QSignalSpy spy(&aura, &AuraStub::awayDelayChanged);
    QVERIFY2(QMetaObject::invokeMethod(slider, "_setValueFromFraction",
                                       Qt::AutoConnection,
                                       Q_ARG(QVariant, QVariant(0.5))),
             "failed to invoke _setValueFromFraction on awayDelaySlider");
    QCOMPARE(spy.count(), 1);
    QCOMPARE(aura.awayDelaySec(), 66);  // 10 + 0.5*110=65, snapped to 66
}

// (7) Settings entry regression guard. Previously (M4) the openSettingsButton
//     pushed AuraSettingsPage into a local StackView with a Back button; in
//     M5-C4a that pattern was removed in favor of opening the top-level
//     SettingsWindow via settingsRoot.openSettings(). This test now just
//     confirms the button still renders and clicks without error — the
//     StackView depth assertion is gone because depth no longer changes
//     (settingsRoot isn't wired in unit tests so the typeof guard skips).
void TestAuraTab::backButtonPopsStackView() {
    fprintf(stderr, "[test_aura_tab] backButtonPopsStackView (renamed intent: openSettingsButton still works)\n"); fflush(stderr);
    AuraStub aura;
    QQmlApplicationEngine engine;
    QSignalSpy warnSpy(&engine, &QQmlEngine::warnings);
    engine.rootContext()->setContextProperty(QStringLiteral("aura"), &aura);
    engine.load(QUrl(QStringLiteral("qrc:/aura/qml/AuraTab.qml")));
    QVERIFY2(!engine.rootObjects().isEmpty(), "AuraTab.qml failed to load");
    QObject* root = engine.rootObjects().constFirst();

    // StackView still exists as the tab's container, but openSettingsButton
    // no longer pushes into it — depth stays at 1 before and after click.
    QObject* stack = findByName(root, QStringLiteral("stack"));
    QVERIFY2(stack, "StackView (objectName=stack) not found");
    QCOMPARE(stack->property("depth").toInt(), 1);

    QObject* openBtn = findByName(root, QStringLiteral("openSettingsButton"));
    QVERIFY2(openBtn, "openSettingsButton MButton not found");
    QVERIFY2(QMetaObject::invokeMethod(openBtn, "clicked"),
             "failed to invoke clicked() on openSettingsButton");
    // Depth stays 1 — click is a no-op when settingsRoot is undefined
    // (typeof guard in QML skips the call in unit-test context).
    QCOMPARE(stack->property("depth").toInt(), 1);
    QVERIFY2(warnSpy.isEmpty(), "QML warnings emitted during openSettingsButton click");
}

QTEST_MAIN(TestAuraTab)
#include "test_aura_tab.moc"
