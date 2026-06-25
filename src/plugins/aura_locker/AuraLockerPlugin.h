// AuraLockerPlugin — Bluetooth proximity auto-lock (M1).
// Spec: docs/04-plugin-spec.md §8.2 + docs/11-roadmap.md M1.
//
// Composition:
//   BluetoothProximityTracker (BLE) ──rssiSampled──→ plugin
//                                                       │
//                                  scanning?            │
//                                  ├─ yes: collect ──── scannedDevices (QML)
//                                  └─ no:  forward ──── ProximityDetector
//                                                       │
//                                                       ├─ awayDetected ──→ EventBus
//                                                       ├─ backDetected  ──→ EventBus
//                                                       └─ lockRequested ──→ LockService
//
// Q_PROPERTY surface (registered as `aura` context property in onLoad):
//   pairedDeviceName  — current paired device (empty when unpaired)
//   proximityState    — string form of ProximityDetector::State
//   scanning          — true while a scan is in progress
//   scannedDevices    — QVariantList of {deviceId, name, rssi} during scan
//   rssiThresholdDbm  — away-threshold slider value (-90..-40)
//   awayDelaySec      — away-delay SpinBox (clamped 10..120)
//   cooldownSec       — cooldown SpinBox (clamped 30..300)
//   scanDurationSec   — scan window length (clamped 5..15)
//   recentEvents      — QVariantList of {timestampMs, kind, message} (latest
//                       30 state transitions / lock fires / warnings) — gives
//                       the user a visible "did it lock?" trail without
//                       forcing them to tail the log file.
//   lastLockMs        — epoch millis of the most recent successful lock
//                       trigger, or 0 if none since launch.
//
// Q_INVOKABLE:
//   startScan()  — kicks off a 5s passive BLE scan (no filter)
//   stopScan()   — early-stop the in-flight scan
//   pairDevice(deviceId, name) — writes encrypted settings + restarts monitor
//   unpair()     — clears settings + stops monitor

#pragma once

#include "BluetoothProximityTracker.h"
#include "ProximityDetector.h"

#include "Margin/PluginContext.h"
#include "Margin/DashboardTabContributor.h"
#include "Margin/PluginInterface.h"
#include "Margin/Result.h"
#include "Margin/SettingsPageContributor.h"
#include "Margin/TrayMenuContributor.h"

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>

#include <memory>

namespace Margin {
class Database;
} // namespace Margin

// Forward declaration for the friend seam in AuraLockerPlugin. Test TU lives
// at global namespace (test_aura_recent_events_persist.cpp). Declared outside
// Margin::Plugins::Aura so the `friend class ::TestAuraRecentEvents;` inside
// AuraLockerPlugin resolves to the same class the test defines.
class TestAuraRecentEvents;

namespace Margin::Plugins::Aura {

class AuraLockerPlugin : public QObject,
                        public PluginInterface,
                        public TrayMenuContributor,
                        public DashboardTabContributor,
                        public SettingsPageContributor {
    Q_OBJECT

    Q_PROPERTY(QString pairedDeviceName READ pairedDeviceName
               NOTIFY pairedDeviceChanged)
    Q_PROPERTY(QString proximityState  READ proximityState
               NOTIFY proximityStateChanged)
    Q_PROPERTY(bool     scanning       READ scanning
               NOTIFY scanningChanged)
    Q_PROPERTY(QVariantList scannedDevices READ scannedDevices
               NOTIFY scannedDevicesChanged)
    Q_PROPERTY(int rssiThresholdDbm READ rssiThresholdDbm
               WRITE setRssiThresholdDbm NOTIFY rssiThresholdChanged)
    Q_PROPERTY(int awayDelaySec READ awayDelaySec
               WRITE setAwayDelaySec NOTIFY awayDelayChanged)
    Q_PROPERTY(int cooldownSec  READ cooldownSec
               WRITE setCooldownSec NOTIFY cooldownChanged)
    Q_PROPERTY(int scanDurationSec READ scanDurationSec
               WRITE setScanDurationSec NOTIFY scanDurationChanged)
    Q_PROPERTY(QVariantList recentEvents READ recentEvents
               NOTIFY recentEventsChanged)
    Q_PROPERTY(qint64 lastLockMs READ lastLockMs
               NOTIFY lastLockChanged)
    // M4-C8: humanized "X 分钟前" / "刚刚" / "—" derived from lastLockMs.
    // Reuses lastLockChanged as NOTIFY — see known-staleness note in .cpp.
    Q_PROPERTY(QString lastLockSummary READ lastLockSummary
               NOTIFY lastLockChanged)

public:
    AuraLockerPlugin() = default;

    std::string id() const override;
    std::string version() const override;
    Result<void, std::string> onLoad(const PluginContext& ctx) override;
    void onConfigChange(const QJsonObject&) override {}
    void onUnload() override;

    // TrayMenuContributor
    TrayMenuContributor* asTrayMenu() override { return this; }
    QList<TrayItem> contributeTrayItems() override;
    void onTrayItemClicked(const std::string& id) override;

    // DashboardTabContributor
    DashboardTabContributor* asDashboardTab() override { return this; }
    TabInfo tabInfo() const override;

    // SettingsPageContributor (M5-C4a)
    SettingsPageContributor* asSettingsPage() override { return this; }
    PageInfo pageInfo() const override;

    // Q_PROPERTY reads
    QString pairedDeviceName() const;
    QString proximityState() const;
    bool    scanning() const { return m_scanning; }
    QVariantList scannedDevices() const { return m_cachedScanned; }
    int     rssiThresholdDbm() const { return m_rssiThreshold; }
    int     awayDelaySec() const { return m_awayDelaySec; }
    int     cooldownSec() const { return m_cooldownSec; }
    int     scanDurationSec() const { return m_scanDurationSec; }
    QVariantList recentEvents() const { return m_cachedRecentEvents; }
    qint64  lastLockMs() const { return m_lastLockMs; }
    QString lastLockSummary() const;

    // Q_PROPERTY writes — also persist to Settings. Each setter clamps to
    // its safe range (see kMinAwayDelay/kMaxAwayDelay etc. below) so a
    // manually-edited settings.json can't bypass the floor. The clamps
    // are the single source of truth — QML SpinBoxes use the same range.
    void setRssiThresholdDbm(int dbm);
    void setAwayDelaySec(int sec);
    void setCooldownSec(int sec);
    void setScanDurationSec(int sec);

    // Q_INVOKABLE — UI actions
    Q_INVOKABLE void startScan();
    Q_INVOKABLE void stopScan();
    Q_INVOKABLE void pairDevice(const QString& deviceId, const QString& name);
    Q_INVOKABLE void unpair();

    // ── Scan-list types exposed for unit tests ──
    // ScannedEntry is public + pruneStaleScanned is a pure helper so the
    // eviction algorithm can be tested without instantiating the full
    // plugin (which needs HostServices + a real tracker). Mirrors the
    // test_rssi_smoother pattern.
    struct ScannedEntry {
        QString deviceId;
        QString name;
        QString identHint;
        qint16  rssi;
        qint64  timestampMs;
    };

    /// A device whose last advertisement is older than this is considered
    /// gone from the scan window. Public so tests can assert against the
    /// exact threshold. 5s balances BLE beacon interval (some peripherals
    /// only advertise every 1–2s) vs. UX (a too-long window leaves stale
    /// rows visible).
    static constexpr qint64 kScanStaleMs = 5000;

    // ── Safe-range floors / ceilings for user-tunable settings ──
    // These exist to prevent a manually-edited settings.json from
    // bypassing the QML SpinBox floor. The two with safety implications:
    //   * kMinCooldownSec — below 30s the detector can yo-yo between
    //     Paired and Away, firing screen-lock repeatedly.
    //   * kMinAwayDelaySec — below 10s a momentary signal dip (someone
    //     walking past the receiver) triggers a lock.
    // The scan-duration / threshold bounds are UX-only.
    static constexpr int kMinAwayDelaySec    = 10;
    static constexpr int kMaxAwayDelaySec    = 120;
    static constexpr int kMinCooldownSec     = 30;
    static constexpr int kMaxCooldownSec     = 300;
    static constexpr int kMinScanDurationSec = 5;
    static constexpr int kMaxScanDurationSec = 15;

    /// Clamp helpers — single SSOT for the floor / ceiling math. Both the
    /// setters and the onLoad settings-load path go through these so a
    /// hand-edited settings.json (e.g. cooldown_seconds=5) hits the same
    /// wall as a QML SpinBox that somehow bypassed its own range. Pure
    /// functions; tested without instantiating the plugin.
    static int clampAwayDelaySec(int sec)    { return qBound(kMinAwayDelaySec,    sec, kMaxAwayDelaySec); }
    static int clampCooldownSec(int sec)     { return qBound(kMinCooldownSec,     sec, kMaxCooldownSec); }
    static int clampScanDurationSec(int sec) { return qBound(kMinScanDurationSec, sec, kMaxScanDurationSec); }

    /// Drop entries whose timestampMs is older than kScanStaleMs relative
    /// to nowMs. Returns the number removed. Pure function — no member
    /// state touched — so callers can drive it from any context. Defined
    /// inline in the header so test exes that don't link the full plugin
    /// DLL still see the symbol.
    static int pruneStaleScanned(QList<ScannedEntry>& list, qint64 nowMs) {
        // Walk back-to-front so removeAt(i) doesn't shift unvisited indices.
        // Use strict > (not >=) so a device exactly at the threshold stays
        // visible — BLE clock jitter makes "exact" racy and we'd rather
        // show one extra packet than drop a borderline device.
        int removed = 0;
        for (int i = list.size() - 1; i >= 0; --i) {
            if (nowMs - list[i].timestampMs > kScanStaleMs) {
                list.removeAt(i);
                ++removed;
            }
        }
        return removed;
    }

signals:
    void pairedDeviceChanged();
    void proximityStateChanged();
    void scanningChanged();
    void scannedDevicesChanged();
    void rssiThresholdChanged();
    void awayDelayChanged();
    void cooldownChanged();
    void scanDurationChanged();
    void recentEventsChanged();
    void lastLockChanged();

private:
    void wireTrackerSignals();
    void wireDetectorSignals();
    void startMonitoringForPairedDevice();
    void stopMonitoring();
    void onSampleInternal(const RssiSample& s);
    void onRadioStateChanged(RadioState state);

    // Cache rebuilders — these run on every data mutation so Q_PROPERTY
    // reads return a pre-built QVariantList instead of rebuilding per call.
    void rebuildScannedCache();
    void rebuildRecentEventsCache();

    // Scan helpers
    void finishScan();
    void persistThreshold(int dbm);
    void persistDelay(const char* key, int sec);

    // Bounded FIFO event log surfaced to QML as `recentEvents`. Capped at
    // kMaxRecentEvents — older entries drop off the tail. Kinds in use:
    //   "away"    — Paired→Away transition (smoother crossed threshold)
    //   "lock"    — LockService::lockScreen() actually invoked
    //   "back"    — Away→Cooldown (device returned to range)
    //   "warning" — BT radio off / lock suppressed / unsupported
    void appendEvent(const QString& kind, const QString& message);

    // PR7: persist recent events to aura_recent_event so the trail survives
    // restarts. Called once in onLoad() when Database is available; a failed
    // attach degrades to pure in-memory (covered by NullDatabaseGraceful
    // unit test). Not public — test access via friend declaration below.
    bool ensureRecentEventSchema();
    bool loadRecentEventsFromDb();

    PluginContext m_ctx;
    bool m_paused   = false;
    bool m_enabled  = true;
    // Safety guard (M1-C7): when the BT radio reports Off we suppress
    // away-locks until it returns On. The detector pauses on Off and
    // resumes on On so half-formed state from a partial away window
    // doesn't trigger a spurious lock when the radio blinks back.
    bool m_canLock = true;

    QString m_pairedId;        // BLE BluetoothAddress hex (MAC) — primary pairing key.
                               // Used to filter advertisements in onSampleInternal.
                               // Encrypted at rest (manifest §encrypted_settings).
    QString m_pairedName;      // Raw advertisedName captured at pair time; may be empty.
                               // Pure display convenience — never used for filtering.

    qint16  m_rssiThreshold = -65;
    int     m_awayDelaySec  = 30;
    int     m_cooldownSec   = 60;
    int     m_scanDurationSec = 10;

    // Diagnostic toggle: when true, every BLE packet the tracker emits
    // is logged at INFO. Default false to keep the log clean. Toggle via
    // settings key plugins.aura.ble_debug_log = true.
    bool    m_bleDebugLog = false;

    std::unique_ptr<BluetoothProximityTracker> m_tracker;
    ProximityDetector m_detector;

    // Scan state
    bool      m_scanning = false;
    QTimer    m_scanTimer;
    // deviceId → {name, identHint, lastRssi, timestampMs}. QVariantMap-friendly.
    // ScannedEntry is defined in the public section above.
    QList<ScannedEntry> m_scanned;
    // Pre-built cache of m_scanned — see rebuildScannedCache().
    QVariantList m_cachedScanned;

    // Visible event trail. QVariantMap-friendly: {timestampMs, kind, message}.
    // Capped; appendEvent() pops from the front when full.
    struct EventEntry {
        qint64  timestampMs;
        QString kind;
        QString message;
    };
    QList<EventEntry> m_recentEvents;
    // Pre-built cache of m_recentEvents (newest first) — see
    // rebuildRecentEventsCache().
    QVariantList m_cachedRecentEvents;
    qint64 m_lastLockMs = 0;

    // PR7: SQLite handle for recent-event persistence. Null when HostServices
    // has no Database (test mode or open() failed at bootstrap) — plugin
    // silently runs in-memory in that case.
    Database* m_db = nullptr;

    // Test seam — test_aura_recent_events_persist.cpp accesses private
    // ensureRecentEventSchema / loadRecentEventsFromDb / m_db directly so
    // it can exercise the DB path without instantiating BluetoothProximityTracker
    // / HostServices (which would turn a 30-line unit test into a 500-line
    // integration harness). Global-scope qualifier required: an unqualified
    // friend declaration would inject the name into Margin::Plugins::Aura
    // instead of matching the global-scope test class.
    friend class ::TestAuraRecentEvents;
};

} // namespace Margin::Plugins::Aura
