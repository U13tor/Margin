// test_aura_scan_stale_eviction — A3 ghost-device eviction.
// Verifies that AuraLockerPlugin::pruneStaleScanned() drops entries whose
// last-seen timestamp is older than the stale window. Without this prune,
// a device that broadcasts once and goes silent would stay visible for
// the entire 10s scan window — letting the user "pair" a ghost.
//
// Pure-algorithm test: no HostServices, no real tracker. The static
// helper is compiled directly into the test exe (mirrors test_rssi_smoother
// pattern) because plugins don't dllexport their C++ symbols.

#include "plugins/aura_locker/AuraLockerPlugin.h"

#include <QList>
#include <QTest>

#include <cstdio>

using namespace Margin::Plugins::Aura;

namespace {
AuraLockerPlugin::ScannedEntry makeEntry(const char* id, qint64 ts) {
    AuraLockerPlugin::ScannedEntry e;
    e.deviceId    = QString::fromLatin1(id);
    e.name        = QString::fromLatin1(id);
    e.identHint   = QString();
    e.rssi        = -60;
    e.timestampMs = ts;
    return e;
}
} // namespace

class TestAuraScanStaleEviction : public QObject {
    Q_OBJECT

private slots:
    void freshEntrySurvives();
    void staleEntryRemoved();
    void mixedList();
    void emptyListNoop();
    void boundaryExactMs();
};

void TestAuraScanStaleEviction::freshEntrySurvives() {
    QList<AuraLockerPlugin::ScannedEntry> list;
    list.append(makeEntry("AA:BB:CC:DD:EE:01", /*ts*/ 1000));

    // now=1500ms, age=500ms, well under 5000ms stale window.
    const int removed = AuraLockerPlugin::pruneStaleScanned(list, /*now*/ 1500);
    fprintf(stderr, "[fresh] removed=%d size=%d\n", removed, int(list.size()));
    QCOMPARE(removed, 0);
    QCOMPARE(list.size(), 1);
}

void TestAuraScanStaleEviction::staleEntryRemoved() {
    QList<AuraLockerPlugin::ScannedEntry> list;
    list.append(makeEntry("AA:BB:CC:DD:EE:01", /*ts*/ 1000));

    // now=7000ms, age=6000ms, past 5000ms window → gone.
    const int removed = AuraLockerPlugin::pruneStaleScanned(list, /*now*/ 7000);
    fprintf(stderr, "[stale] removed=%d size=%d\n", removed, int(list.size()));
    QCOMPARE(removed, 1);
    QCOMPARE(list.size(), 0);
}

void TestAuraScanStaleEviction::mixedList() {
    QList<AuraLockerPlugin::ScannedEntry> list;
    list.append(makeEntry("AA:BB:CC:DD:EE:01", /*ts*/ 1000));   // stale (6s old)
    list.append(makeEntry("AA:BB:CC:DD:EE:02", /*ts*/ 5500));   // fresh (1.5s old)
    list.append(makeEntry("AA:BB:CC:DD:EE:03", /*ts*/ 1500));   // stale (5.5s old)

    const int removed = AuraLockerPlugin::pruneStaleScanned(list, /*now*/ 7000);
    fprintf(stderr, "[mixed] removed=%d size=%d remaining_id=%s\n",
            removed, int(list.size()),
            qPrintable(list.isEmpty() ? QStringLiteral("(empty)") : list[0].deviceId));
    QCOMPARE(removed, 2);
    QCOMPARE(list.size(), 1);
    QCOMPARE(list[0].deviceId, QStringLiteral("AA:BB:CC:DD:EE:02"));
}

void TestAuraScanStaleEviction::emptyListNoop() {
    QList<AuraLockerPlugin::ScannedEntry> list;
    const int removed = AuraLockerPlugin::pruneStaleScanned(list, /*now*/ 99999);
    fprintf(stderr, "[empty] removed=%d size=%d\n", removed, int(list.size()));
    QCOMPARE(removed, 0);
    QCOMPARE(list.size(), 0);
}

void TestAuraScanStaleEviction::boundaryExactMs() {
    // Boundary: now - ts == kScanStaleMs exactly should NOT prune (use >,
    // not >=, so a device exactly at the threshold is still considered
    // visible — BLE clock jitter means "exact" is racy and we'd rather
    // show one extra packet than drop a borderline device).
    QList<AuraLockerPlugin::ScannedEntry> list;
    list.append(makeEntry("AA:BB:CC:DD:EE:01", /*ts*/ 2000));

    // now=7000ms, age=5000ms = exactly kScanStaleMs → keep.
    int removed = AuraLockerPlugin::pruneStaleScanned(list, /*now*/ 7000);
    fprintf(stderr, "[boundary] exact=%dms removed=%d size=%d\n",
            7000 - 2000, removed, int(list.size()));
    QCOMPARE(removed, 0);
    QCOMPARE(list.size(), 1);

    // now=7001ms, age=5001ms > kScanStaleMs → prune.
    removed = AuraLockerPlugin::pruneStaleScanned(list, /*now*/ 7001);
    fprintf(stderr, "[boundary] one-past=%dms removed=%d size=%d\n",
            7001 - 2000, removed, int(list.size()));
    QCOMPARE(removed, 1);
    QCOMPARE(list.size(), 0);
}

QTEST_MAIN(TestAuraScanStaleEviction)
#include "test_aura_scan_stale_eviction.moc"
