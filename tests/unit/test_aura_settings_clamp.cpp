// test_aura_settings_clamp — B5 setter clamp regression.
// Verifies that the floor / ceiling bounds on user-tunable settings are
// enforced at the SSOT (the static clamp* helpers), not just at the QML
// SpinBox layer. The risk: a user hand-edits settings.json and writes
// cooldown_seconds=5 — without the clamp the detector yo-yos Paired↔Away
// and fires screen-lock every 5 seconds.
//
// Also covers B4: scan_duration_sec shares the same clamp pattern.

#include "plugins/aura_locker/AuraLockerPlugin.h"

#include <QTest>

#include <cstdio>

using namespace Margin::Plugins::Aura;

class TestAuraSettingsClamp : public QObject {
    Q_OBJECT

private slots:
    void cooldownFloorAt30();
    void cooldownCeilingAt300();
    void cooldownInRangeUnchanged();
    void awayDelayFloorAt10();
    void awayDelayCeilingAt120();
    void scanDurationBounds();
};

void TestAuraSettingsClamp::cooldownFloorAt30() {
    QCOMPARE(AuraLockerPlugin::clampCooldownSec(0),   30);
    QCOMPARE(AuraLockerPlugin::clampCooldownSec(5),   30);
    QCOMPARE(AuraLockerPlugin::clampCooldownSec(29),  30);
    QCOMPARE(AuraLockerPlugin::clampCooldownSec(30),  30);  // exact floor
    fprintf(stderr, "[cooldownFloor] 0/5/29/30 → all clamp to 30 OK\n");
}

void TestAuraSettingsClamp::cooldownCeilingAt300() {
    QCOMPARE(AuraLockerPlugin::clampCooldownSec(300),   300);  // exact ceiling
    QCOMPARE(AuraLockerPlugin::clampCooldownSec(301),   300);
    QCOMPARE(AuraLockerPlugin::clampCooldownSec(1000),  300);
    QCOMPARE(AuraLockerPlugin::clampCooldownSec(99999), 300);
    fprintf(stderr, "[cooldownCeiling] 300/301/1000/99999 → all clamp to 300 OK\n");
}

void TestAuraSettingsClamp::cooldownInRangeUnchanged() {
    QCOMPARE(AuraLockerPlugin::clampCooldownSec(30),   30);
    QCOMPARE(AuraLockerPlugin::clampCooldownSec(60),   60);
    QCOMPARE(AuraLockerPlugin::clampCooldownSec(120),  120);
    QCOMPARE(AuraLockerPlugin::clampCooldownSec(300),  300);
    fprintf(stderr, "[cooldownInRange] 30/60/120/300 unchanged OK\n");
}

void TestAuraSettingsClamp::awayDelayFloorAt10() {
    QCOMPARE(AuraLockerPlugin::clampAwayDelaySec(0),   10);
    QCOMPARE(AuraLockerPlugin::clampAwayDelaySec(5),   10);
    QCOMPARE(AuraLockerPlugin::clampAwayDelaySec(9),   10);
    QCOMPARE(AuraLockerPlugin::clampAwayDelaySec(10),  10);  // exact floor
    fprintf(stderr, "[awayFloor] 0/5/9/10 → all clamp to 10 OK\n");
}

void TestAuraSettingsClamp::awayDelayCeilingAt120() {
    QCOMPARE(AuraLockerPlugin::clampAwayDelaySec(120),  120);  // exact ceiling
    QCOMPARE(AuraLockerPlugin::clampAwayDelaySec(121),  120);
    QCOMPARE(AuraLockerPlugin::clampAwayDelaySec(300),  120);
    fprintf(stderr, "[awayCeiling] 120/121/300 → all clamp to 120 OK\n");
}

void TestAuraSettingsClamp::scanDurationBounds() {
    // Floor 5s — below this, the scan window is too short to reliably
    // catch peripherals that only beacon every 1–2s.
    QCOMPARE(AuraLockerPlugin::clampScanDurationSec(0),  5);
    QCOMPARE(AuraLockerPlugin::clampScanDurationSec(4),  5);
    QCOMPARE(AuraLockerPlugin::clampScanDurationSec(5),  5);

    // Ceiling 15s — above this, the user holds their breath waiting for
    // the scan to finish; not a correctness issue but a UX one.
    QCOMPARE(AuraLockerPlugin::clampScanDurationSec(15),  15);
    QCOMPARE(AuraLockerPlugin::clampScanDurationSec(20),  15);
    QCOMPARE(AuraLockerPlugin::clampScanDurationSec(60),  15);

    // Mid-range untouched.
    QCOMPARE(AuraLockerPlugin::clampScanDurationSec(10),  10);
    fprintf(stderr, "[scanDuration] floor 5, ceiling 15, 10 unchanged OK\n");
}

QTEST_MAIN(TestAuraSettingsClamp)
#include "test_aura_settings_clamp.moc"
