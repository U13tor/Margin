// AuraLockerPlugin impl — see AuraLockerPlugin.h.

#include "AuraLockerPlugin.h"

#include "Margin/Database.h"
#include "Margin/EventBus.h"
#include "Margin/HostServices.h"
#include "Margin/LockService.h"
#include "Margin/Logger.h"
#include "Margin/QmlService.h"
#include "Margin/Settings.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <QtGlobal>  // qBound

#include <utility>

namespace Margin::Plugins::Aura {

namespace {
constexpr const char* kId       = "aura";
constexpr const char* kVersion  = "0.1.0";
constexpr const char* kTag      = "aura";
constexpr const char* kToggleId = "toggle_pause";
constexpr const char* kQmlName  = "aura";  // context property name
constexpr int kMaxRecentEvents  = 30;

// Settings keys. paired_* are encrypted at rest (manifest §encrypted_settings).
constexpr const char* kKeyEnabled       = "plugins.aura.enabled";
constexpr const char* kKeyRssiThresh    = "plugins.aura.rssi_threshold";
constexpr const char* kKeyAwayDelay     = "plugins.aura.away_delay_seconds";
constexpr const char* kKeyCooldown      = "plugins.aura.cooldown_seconds";
constexpr const char* kKeyScanDuration  = "plugins.aura.scan_duration_seconds";
constexpr const char* kKeyPairedId      = "plugins.aura.paired_device_identifier";
constexpr const char* kKeyPairedName    = "plugins.aura.device_name";

constexpr qint16 kDefaultRssiThreshold = -65;
constexpr int    kDefaultAwayDelaySec    = 30;
constexpr int    kDefaultCooldownSec     = 60;
constexpr int    kDefaultScanDurationSec = 10;

// kScanStaleMs + kMin/Max* bounds live in the AuraLockerPlugin class scope
// (header) so the inline pruneStaleScanned helper + the public setter
// clamps share a single SSOT.

const char* stateToString(ProximityDetector::State s) {
    switch (s) {
        case ProximityDetector::State::Inactive: return "inactive";
        case ProximityDetector::State::Paired:   return "paired";
        case ProximityDetector::State::Away:     return "away";
        case ProximityDetector::State::Cooldown: return "cooldown";
    }
    return "unknown";
}
} // namespace

std::string AuraLockerPlugin::id() const      { return kId; }
std::string AuraLockerPlugin::version() const { return kVersion; }

Result<void, std::string> AuraLockerPlugin::onLoad(const PluginContext& ctx) {
    m_ctx = ctx;

    auto* log = ctx.host ? &ctx.host->logger() : nullptr;
    auto* settings = ctx.host ? &ctx.host->settings() : nullptr;

    if (settings) {
        m_enabled    = settings->get(kKeyEnabled, true).toBool();
        m_pairedId   = settings->get(kKeyPairedId).toString();
        m_pairedName = settings->get(kKeyPairedName).toString();
        m_rssiThreshold = static_cast<qint16>(
            settings->get(kKeyRssiThresh, kDefaultRssiThreshold).toInt());
        // Clamp on load — a hand-edited settings.json with cooldown=5
        // would otherwise pass straight through to the detector and let
        // it yo-yo between Paired and Away, firing screen-lock repeatedly.
        // Same logic as the Q_PROPERTY setters below.
        const int rawAway   = settings->get(kKeyAwayDelay, kDefaultAwayDelaySec).toInt();
        const int rawCool   = settings->get(kKeyCooldown, kDefaultCooldownSec).toInt();
        const int rawScan   = settings->get(kKeyScanDuration, kDefaultScanDurationSec).toInt();
        m_awayDelaySec    = clampAwayDelaySec(rawAway);
        m_cooldownSec     = clampCooldownSec(rawCool);
        m_scanDurationSec = clampScanDurationSec(rawScan);
        if (log) {
            if (m_awayDelaySec    != rawAway)   log->warn(QString::fromLatin1(kTag),
                QStringLiteral("away_delay_seconds=%1 out of range [%2..%3], clamped to %4")
                    .arg(rawAway).arg(kMinAwayDelaySec).arg(kMaxAwayDelaySec).arg(m_awayDelaySec));
            if (m_cooldownSec     != rawCool)   log->warn(QString::fromLatin1(kTag),
                QStringLiteral("cooldown_seconds=%1 out of range [%2..%3], clamped to %4")
                    .arg(rawCool).arg(kMinCooldownSec).arg(kMaxCooldownSec).arg(m_cooldownSec));
            if (m_scanDurationSec != rawScan)   log->warn(QString::fromLatin1(kTag),
                QStringLiteral("scan_duration_seconds=%1 out of range [%2..%3], clamped to %4")
                    .arg(rawScan).arg(kMinScanDurationSec).arg(kMaxScanDurationSec).arg(m_scanDurationSec));
        }
        m_bleDebugLog = settings->get("plugins.aura.ble_debug_log", false).toBool();
    } else {
        m_rssiThreshold   = kDefaultRssiThreshold;
        m_awayDelaySec    = kDefaultAwayDelaySec;
        m_cooldownSec     = kDefaultCooldownSec;
        m_scanDurationSec = kDefaultScanDurationSec;
    }
    m_detector.configure(m_rssiThreshold, m_awayDelaySec, m_cooldownSec);

    // PR7: attach Database for recent-event persistence. Database is host-
    // owned; a null return is a graceful degradation signal (test mode or
    // open() failed at bootstrap), not a hard error. Schema-load failures
    // downgrade m_db to nullptr so the rest of the plugin still works.
    if (ctx.host) {
        if (auto* db = ctx.host->database()) {
            m_db = db;
            if (!ensureRecentEventSchema()) {
                if (log) log->warn(QString::fromLatin1(kTag),
                    QStringLiteral("aura_recent_event schema setup failed: %1 — "
                                   "running in-memory only")
                        .arg(db->lastError()));
                m_db = nullptr;
            } else if (!loadRecentEventsFromDb()) {
                if (log) log->warn(QString::fromLatin1(kTag),
                    QStringLiteral("aura_recent_event load failed: %1 — "
                                   "starting with empty trail")
                        .arg(db->lastError()));
                m_db = nullptr;
            } else {
                rebuildRecentEventsCache();
                emit recentEventsChanged();
            }
        } else if (log) {
            log->info(QString::fromLatin1(kTag),
                QStringLiteral("Database unavailable — recent events will not "
                               "persist across restarts"));
        }
    }

    m_scanTimer.setSingleShot(true);
    m_scanTimer.setInterval(m_scanDurationSec * 1000);
    connect(&m_scanTimer, &QTimer::timeout, this, &AuraLockerPlugin::finishScan);

    wireDetectorSignals();

    m_tracker = BluetoothProximityTracker::create();
    if (!m_tracker) {
        if (log) log->warn(QString::fromLatin1(kTag),
            QStringLiteral("BLE tracker unavailable on this platform — Aura running in inert mode"));
    } else {
        wireTrackerSignals();
    }

    // Register self as `aura` context property so QML (AuraTab.qml +
    // AuraSettingsPage.qml) can read Q_PROPERTY + invoke Q_INVOKABLE.
    if (ctx.host && ctx.host->qml()) {
        ctx.host->qml()->registerContextProperty(
            QString::fromLatin1(kQmlName), this);
    } else if (log) {
        log->warn(QString::fromLatin1(kTag),
            QStringLiteral("QmlService unavailable — Settings UI will be inert"));
    }

    if (log) {
        log->info(QString::fromLatin1(kTag),
            QStringLiteral("AuraLockerPlugin onLoad paired=%1 thresh=%2dBm away=%3s cooldown=%4s")
                .arg(m_pairedId.isEmpty() ? QStringLiteral("no") : QStringLiteral("yes"))
                .arg(m_rssiThreshold).arg(m_awayDelaySec).arg(m_cooldownSec));
    }

    if (!m_enabled) {
        if (log) log->info(QString::fromLatin1(kTag),
            QStringLiteral("Aura disabled via settings; staying Inactive"));
        return Result<void, std::string>::ok();
    }

    startMonitoringForPairedDevice();
    return Result<void, std::string>::ok();
}

void AuraLockerPlugin::onUnload() {
    stopMonitoring();
    if (m_scanning) stopScan();
    if (m_ctx.host) {
        m_ctx.host->logger().info(
            QString::fromLatin1(kTag),
            QStringLiteral("AuraLockerPlugin onUnload"));
    }
}

void AuraLockerPlugin::wireTrackerSignals() {
    // Single handler branches on m_scanning — avoids disconnect/reconnect
    // churn when transitioning between away-detection and scan modes.
    connect(m_tracker.get(), &BluetoothProximityTracker::rssiSampled,
            this, [this](const RssiSample& s) { onSampleInternal(s); });

    connect(m_tracker.get(), &BluetoothProximityTracker::radioStateChanged,
            this, [this](RadioState s) { onRadioStateChanged(s); });
}

void AuraLockerPlugin::onRadioStateChanged(RadioState state) {
    if (state == RadioState::Off) {
        m_canLock = false;
        m_detector.pause();
        if (m_ctx.host) {
            QJsonObject p;
            p["reason"]    = QStringLiteral("bt_disabled");
            p["timestamp"] = QDateTime::currentMSecsSinceEpoch();
            m_ctx.host->eventBus().publish(
                QStringLiteral("margin.aura.warning"), p);
            m_ctx.host->logger().warn(
                QString::fromLatin1(kTag),
                QStringLiteral("BT radio off — away-locks suppressed until radio returns"));
        }
        appendEvent(QStringLiteral("warning"),
                    QStringLiteral("Bluetooth off — auto-lock paused"));
    } else if (state == RadioState::On) {
        m_canLock = true;
        if (!m_paused && !m_pairedId.isEmpty()) m_detector.resume();
        if (m_ctx.host) m_ctx.host->logger().info(
            QString::fromLatin1(kTag),
            QStringLiteral("BT radio on — away-locks re-enabled"));
        appendEvent(QStringLiteral("warning"),
                    QStringLiteral("Bluetooth on — auto-lock resumed"));
    }
}

void AuraLockerPlugin::onSampleInternal(const RssiSample& s) {
    if (m_bleDebugLog && m_ctx.host) {
        m_ctx.host->logger().info(
            QString::fromLatin1(kTag),
            QStringLiteral("[ble-debug] id=%1 name='%2' hint='%3' rssi=%4")
                .arg(s.deviceId)
                .arg(s.advertisedName.isEmpty() ? QStringLiteral("(none)") : s.advertisedName)
                .arg(s.identHint.isEmpty() ? QStringLiteral("(none)") : s.identHint)
                .arg(s.rssiDbm));
    }

    if (m_scanning) {
        // Prune entries whose last-seen timestamp fell outside the stale
        // window BEFORE running dedup. Otherwise a device that broadcast
        // once at t=0 and went silent would (a) stay in the list for the
        // entire 10s scan window and (b) resurrect as the same logical
        // entry if it broadcasts again at t=8s, hiding the 8s gap.
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (pruneStaleScanned(m_scanned, now) > 0) {
            rebuildScannedCache();
            emit scannedDevicesChanged();
        }

        // Dedup by deviceId; keep highest RSSI (closest = strongest signal)
        // and latest timestamp.
        for (auto& e : m_scanned) {
            if (e.deviceId == s.deviceId) {
                if (s.rssiDbm > e.rssi) e.rssi = s.rssiDbm;
                // Prefer the longest advertisedName we've seen. BLE has
                // two LocalName flavors: ShortenedLocalName rides in the
                // ADV_IND packet (truncated to save bytes), CompleteLocal-
                // Name rides in SCAN_RSP. Active scan receives both, in
                // either order — keep the longer one so the UI shows the
                // full "Xiaomi Watch 5 eSIM FE49" once the SCAN_RSP lands
                // instead of being stuck on the first-seen shortened form.
                if (s.advertisedName.length() > e.name.length()) {
                    e.name = s.advertisedName;
                }
                if (s.identHint.length() > e.identHint.length()) {
                    e.identHint = s.identHint;
                }
                e.timestampMs = s.timestampMs;
                rebuildScannedCache();
                emit scannedDevicesChanged();
                return;
            }
        }
        m_scanned.append({ s.deviceId,
                           s.advertisedName,
                           s.identHint,
                           s.rssiDbm,
                           s.timestampMs });
        rebuildScannedCache();
        emit scannedDevicesChanged();
        return;
    }
    // Away-detection mode: only feed the state machine samples that match
    // the paired device. The tracker receives ALL advertisements (no
    // LocalName filter — see startMonitoringForPairedDevice) because BLE
    // LocalName is not unique (two identical headsets share a model name)
    // and may be absent entirely. deviceId (MAC) is the real primary key.
    if (s.deviceId != m_pairedId) return;
    m_detector.onRssiSample(s.rssiDbm);
}

void AuraLockerPlugin::wireDetectorSignals() {
    connect(&m_detector, &ProximityDetector::stateChanged, this,
        [this](ProximityDetector::State) { emit proximityStateChanged(); });

    connect(&m_detector, &ProximityDetector::stateChanged, this,
        [this](ProximityDetector::State s) {
            if (!m_ctx.host) return;
            QJsonObject p;
            p["state"]     = QString::fromLatin1(stateToString(s));
            p["device"]    = m_pairedName;
            p["timestamp"] = QDateTime::currentMSecsSinceEpoch();
            m_ctx.host->eventBus().publish(
                QStringLiteral("margin.aura.state"), p);
        });

    connect(&m_detector, &ProximityDetector::awayDetected, this,
        [this](qint16 smoothed) {
            if (!m_ctx.host) return;
            QJsonObject p;
            p["device"]    = m_pairedName;
            p["rssi"]      = smoothed;
            p["timestamp"] = QDateTime::currentMSecsSinceEpoch();
            m_ctx.host->eventBus().publish(
                QStringLiteral("margin.aura.away"), p);
            appendEvent(QStringLiteral("away"),
                        QStringLiteral("Device out of range (RSSI %1 dBm)").arg(smoothed));
        });

    connect(&m_detector, &ProximityDetector::backDetected, this, [this]() {
        if (!m_ctx.host) return;
        QJsonObject p;
        p["device"]    = m_pairedName;
        p["timestamp"] = QDateTime::currentMSecsSinceEpoch();
        m_ctx.host->eventBus().publish(
            QStringLiteral("margin.aura.back"), p);
        appendEvent(QStringLiteral("back"),
                    QStringLiteral("Device back — %1s cooldown").arg(m_cooldownSec));
    });

    connect(&m_detector, &ProximityDetector::lockRequested, this, [this]() {
        if (!m_ctx.host) return;
        if (!m_canLock) {
            m_ctx.host->logger().warn(
                QString::fromLatin1(kTag),
                QStringLiteral("lockRequested while m_canLock=false (BT off?); suppressing"));
            appendEvent(QStringLiteral("warning"),
                        QStringLiteral("Lock skipped (Bluetooth off)"));
            return;
        }
        auto* lock = m_ctx.host->lock();
        if (!lock || !lock->isSupported()) {
            m_ctx.host->logger().warn(
                QString::fromLatin1(kTag),
                QStringLiteral("LockService unavailable or unsupported; away detected but no lock fired"));
            appendEvent(QStringLiteral("warning"),
                        QStringLiteral("Lock unavailable on this platform"));
            return;
        }
        lock->lockScreen();
        // TODO(abi-0.2): check return value once LockService::lockScreen
        // returns bool; on failure suppress m_lastLockMs update + emit
        // warning event instead of "Screen locked". See 12-deferred A18.
        m_lastLockMs = QDateTime::currentMSecsSinceEpoch();
        emit lastLockChanged();
        m_ctx.host->logger().info(
            QString::fromLatin1(kTag),
            QStringLiteral("Aura triggered screen lock (away)"));
        appendEvent(QStringLiteral("lock"),
                    QStringLiteral("Screen locked"));
    });
}

void AuraLockerPlugin::startMonitoringForPairedDevice() {
    if (!m_tracker) return;
    if (m_pairedId.isEmpty()) {
        if (m_ctx.host) m_ctx.host->logger().info(
            QString::fromLatin1(kTag),
            QStringLiteral("No paired device — Aura staying Inactive (use Settings UI to pair)"));
        return;
    }
    // Empty LocalName filter = receive every advertisement packet. Plugin
    // filters in onSampleInternal by m_pairedId. This avoids two traps:
    //   1) LocalName isn't unique — two identical headsets share it
    //   2) Many devices (beacons, LE-Privacy phones) have no LocalName at
    //      all, so a name filter would silently match nothing.
    m_tracker->startMonitoring(QString{});
    m_detector.start();
}

void AuraLockerPlugin::stopMonitoring() {
    if (m_tracker && m_tracker->isActive()) m_tracker->stopMonitoring();
    m_detector.stop();
}

QString AuraLockerPlugin::pairedDeviceName() const {
    // Same rule as scannedDevices(): real advertised name if we have it,
    // otherwise raw MAC. identHint is kept as a separate concept in the
    // status card rather than fused into the paired label.
    return m_pairedName.isEmpty() ? m_pairedId : m_pairedName;
}

QString AuraLockerPlugin::lastLockSummary() const {
    // M4-C8: humanize m_lastLockMs for the Tab1 Aura card's subtitle. Buckets
    // are coarse on purpose — the card only needs to signal "recently" vs
    // "earlier today" vs "a while ago".
    //
    // Known staleness: the string is computed at binding-eval time and only
    // re-evaluates when lastLockChanged fires (i.e. when m_lastLockMs itself
    // changes). A lock that happened 5 min ago keeps showing "刚刚" until
    // the next lock event — real-time decay (60s QTimer firing
    // lastLockSummaryChanged) is deferred to C9+ per the plan's Out-of-Scope.
    if (m_lastLockMs <= 0) return QString();
    const qint64 nowMs = QDateTime::currentDateTime().toMSecsSinceEpoch();
    const qint64 deltaSec = qMax<qint64>(0, (nowMs - m_lastLockMs) / 1000);
    if (deltaSec < 60)        return QStringLiteral("刚刚");
    if (deltaSec < 3600)      return QStringLiteral("%1 分钟前").arg(deltaSec / 60);
    if (deltaSec < 86400)     return QStringLiteral("%1 小时前").arg(deltaSec / 3600);
    return QStringLiteral("%1 天前").arg(deltaSec / 86400);
}

QString AuraLockerPlugin::proximityState() const {
    return QString::fromLatin1(stateToString(m_detector.state()));
}

void AuraLockerPlugin::rebuildScannedCache() {
    QVariantList out;
    out.reserve(m_scanned.size());
    for (const auto& e : m_scanned) {
        QVariantMap m;
        m["deviceId"]    = e.deviceId;
        // Display rule: real advertised name when present, otherwise the
        // raw MAC. We deliberately do NOT synthesize labels like
        // "Apple device" / "iBeacon device" / "(unnamed)" — those were
        // inconsistent with how the paired tab renders the same device
        // and made the list noisy. identHint stays in its own field so
        // the second row can still surface it.
        m["name"]        = e.name.isEmpty() ? e.deviceId : e.name;
        m["identHint"]   = e.identHint;
        m["rssi"]        = e.rssi;
        m["timestampMs"] = e.timestampMs;
        out.append(m);
    }
    m_cachedScanned = std::move(out);
}

void AuraLockerPlugin::rebuildRecentEventsCache() {
    QVariantList out;
    out.reserve(m_recentEvents.size());
    // Newest first — QML ListView shows index 0 at the top, so the user
    // sees the latest event without scrolling.
    for (auto it = m_recentEvents.rbegin(); it != m_recentEvents.rend(); ++it) {
        QVariantMap m;
        m["timestampMs"] = it->timestampMs;
        m["kind"]        = it->kind;
        m["message"]     = it->message;
        out.append(m);
    }
    m_cachedRecentEvents = std::move(out);
}

bool AuraLockerPlugin::ensureRecentEventSchema() {
    static const QStringList kStatements = {
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS aura_recent_event ("
            "    id           INTEGER PRIMARY KEY AUTOINCREMENT,"
            "    timestamp_ms INTEGER NOT NULL,"
            "    kind         TEXT    NOT NULL,"
            "    message      TEXT    NOT NULL"
            ")"),
        QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_aura_recent_ts "
            "ON aura_recent_event(timestamp_ms)"),
    };
    for (const auto& sql : kStatements) {
        if (!m_db->exec(sql)) return false;
    }
    return true;
}

bool AuraLockerPlugin::loadRecentEventsFromDb() {
    const auto rows = m_db->query(
        QStringLiteral("SELECT timestamp_ms, kind, message FROM aura_recent_event "
                       "ORDER BY id ASC LIMIT :limit"),
        {{QStringLiteral("limit"), kMaxRecentEvents}});
    if (rows.isEmpty() && !m_db->lastError().isEmpty()) {
        return false;
    }
    m_recentEvents.clear();
    for (const auto& row : rows) {
        EventEntry e;
        e.timestampMs = row.value(QStringLiteral("timestamp_ms")).toLongLong();
        e.kind        = row.value(QStringLiteral("kind")).toString();
        e.message     = row.value(QStringLiteral("message")).toString();
        m_recentEvents.append(std::move(e));
    }
    return true;
}

void AuraLockerPlugin::appendEvent(const QString& kind, const QString& message) {
    EventEntry e;
    e.timestampMs = QDateTime::currentMSecsSinceEpoch();
    e.kind        = kind;
    e.message     = message;

    if (m_db) {
        // INSERT + ring-buffer trim in a transaction so a crash between
        // them rolls back. Worst case without the wrap is a few stale
        // rows that the next startup's ASC LIMIT-clamped SELECT masks.
        //
        // IMPORTANT: do this BEFORE std::move(e) into m_recentEvents.
        // QSqlDatabase's SQLite driver treats an empty (moved-from)
        // QString as SQL NULL, which would trip the NOT NULL constraint
        // on kind/message and silently drop the row.
        m_db->transaction();
        m_db->exec(
            QStringLiteral("INSERT INTO aura_recent_event "
                           "(timestamp_ms, kind, message) "
                           "VALUES (:ts, :kind, :msg)"),
            {{QStringLiteral("ts"), QVariant::fromValue<qint64>(e.timestampMs)},
             {QStringLiteral("kind"), e.kind},
             {QStringLiteral("msg"), e.message}});
        m_db->exec(
            QStringLiteral("DELETE FROM aura_recent_event WHERE id NOT IN "
                           "(SELECT id FROM aura_recent_event "
                           " ORDER BY id DESC LIMIT :limit)"),
            {{QStringLiteral("limit"), kMaxRecentEvents}});
        m_db->commit();
    }

    m_recentEvents.append(std::move(e));
    // Cap the trail so a long-running session doesn't grow the list
    // unbounded. Pop from the front (oldest) — keep the latest 30.
    while (m_recentEvents.size() > kMaxRecentEvents) {
        m_recentEvents.removeFirst();
    }

    rebuildRecentEventsCache();
    emit recentEventsChanged();
}

void AuraLockerPlugin::startScan() {
    if (!m_tracker) {
        if (m_ctx.host) m_ctx.host->logger().warn(
            QString::fromLatin1(kTag),
            QStringLiteral("startScan: BLE tracker unavailable on this platform"));
        return;
    }
    if (m_scanning) return;

    // If actively monitoring, pause it for the scan window — only one
    // watcher can run on the underlying BLE adapter at a time.
    if (m_tracker->isActive()) {
        m_tracker->stopMonitoring();
        m_detector.pause();
    }

    m_scanned.clear();
    rebuildScannedCache();
    emit scannedDevicesChanged();

    m_scanning = true;
    emit scanningChanged();

    // Empty filter = receive every advertisement packet (passive scan).
    m_tracker->startMonitoring(QString{});
    m_scanTimer.start();

    if (m_ctx.host) m_ctx.host->logger().info(
        QString::fromLatin1(kTag),
        QStringLiteral("BLE scan started (%1 ms)").arg(m_scanDurationSec * 1000));
}

void AuraLockerPlugin::stopScan() {
    if (!m_scanning) return;
    m_scanTimer.stop();
    finishScan();
}

void AuraLockerPlugin::finishScan() {
    if (!m_scanning) return;
    if (m_tracker && m_tracker->isActive()) m_tracker->stopMonitoring();

    // Final prune so the displayed list at scan-end reflects only devices
    // seen in the last kScanStaleMs. Without this, a 10s scan window could
    // surface a device that broadcast only in the first second.
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (pruneStaleScanned(m_scanned, now) > 0) {
        rebuildScannedCache();
        emit scannedDevicesChanged();
    }

    m_scanning = false;
    emit scanningChanged();

    // If we paused the detector above, resume it now. Monitoring uses an
    // empty filter (see startMonitoringForPairedDevice) so this just
    // restarts the watcher and the plugin filters by m_pairedId.
    if (!m_pairedId.isEmpty() && !m_paused) {
        m_tracker->startMonitoring(QString{});
        m_detector.resume();
    }

    if (m_ctx.host) m_ctx.host->logger().info(
        QString::fromLatin1(kTag),
        QStringLiteral("BLE scan finished: %1 device(s)").arg(m_scanned.size()));
}

void AuraLockerPlugin::pairDevice(const QString& deviceId, const QString& /*name*/) {
    if (deviceId.isEmpty()) return;

    // Pull the raw advertisedName from the scan cache, not the QML display
    // string. QML passes whatever label it renders (the name or the MAC
    // when name is empty); we want the true advertised name (possibly
    // empty) so the paired tab can show "Mi" rather than the MAC after
    // restart.
    QString rawName;
    for (const auto& e : m_scanned) {
        if (e.deviceId == deviceId) {
            rawName = e.name;
            break;
        }
    }

    m_pairedId   = deviceId;
    m_pairedName = rawName;
    emit pairedDeviceChanged();

    if (m_ctx.host) {
        auto& s = m_ctx.host->settings();
        s.set(kKeyPairedId,   deviceId);
        s.set(kKeyPairedName, rawName);
    }
    if (m_scanning) stopScan();
    startMonitoringForPairedDevice();

    if (m_ctx.host) m_ctx.host->logger().info(
        QString::fromLatin1(kTag),
        QStringLiteral("Paired device id=%1 name='%2'")
            .arg(deviceId)
            .arg(rawName.isEmpty() ? QStringLiteral("(none)") : rawName));

    // Reset the trail — the prior device's events no longer apply.
    m_recentEvents.clear();
    m_lastLockMs = 0;
    rebuildRecentEventsCache();
    emit recentEventsChanged();
    emit lastLockChanged();
    appendEvent(QStringLiteral("paired"),
                QStringLiteral("Paired with %1")
                    .arg(rawName.isEmpty() ? deviceId : rawName));
}

void AuraLockerPlugin::unpair() {
    m_pairedId.clear();
    m_pairedName.clear();
    emit pairedDeviceChanged();

    stopMonitoring();

    if (m_ctx.host) {
        m_ctx.host->settings().remove(kKeyPairedId);
        m_ctx.host->settings().remove(kKeyPairedName);
    }
}

void AuraLockerPlugin::setRssiThresholdDbm(int dbm) {
    if (dbm == m_rssiThreshold) return;
    m_rssiThreshold = static_cast<qint16>(dbm);
    m_detector.configure(m_rssiThreshold, m_awayDelaySec, m_cooldownSec);
    persistThreshold(dbm);
    emit rssiThresholdChanged();
}

void AuraLockerPlugin::setAwayDelaySec(int sec) {
    const int clamped = clampAwayDelaySec(sec);
    if (clamped != sec && m_ctx.host) {
        m_ctx.host->logger().warn(
            QString::fromLatin1(kTag),
            QStringLiteral("away_delay_seconds=%1 below floor %2 (or above ceiling %3), clamped")
                .arg(sec).arg(kMinAwayDelaySec).arg(kMaxAwayDelaySec));
    }
    if (clamped == m_awayDelaySec) return;
    m_awayDelaySec = clamped;
    m_detector.configure(m_rssiThreshold, m_awayDelaySec, m_cooldownSec);
    persistDelay(kKeyAwayDelay, clamped);
    emit awayDelayChanged();
}

void AuraLockerPlugin::setCooldownSec(int sec) {
    // The cooldown floor is the load-bearing constraint here: below 30s
    // the detector can yo-yo Paired→Away→Paired on a single dip and fire
    // screen-lock repeatedly. The QML SpinBox already enforces 30..300
    // but a hand-edited settings.json bypasses the SpinBox; the setter
    // is the real enforcement point.
    const int clamped = clampCooldownSec(sec);
    if (clamped != sec && m_ctx.host) {
        m_ctx.host->logger().warn(
            QString::fromLatin1(kTag),
            QStringLiteral("cooldown_seconds=%1 below floor %2 (or above ceiling %3), clamped")
                .arg(sec).arg(kMinCooldownSec).arg(kMaxCooldownSec));
    }
    if (clamped == m_cooldownSec) return;
    m_cooldownSec = clamped;
    m_detector.configure(m_rssiThreshold, m_awayDelaySec, m_cooldownSec);
    persistDelay(kKeyCooldown, clamped);
    emit cooldownChanged();
}

void AuraLockerPlugin::setScanDurationSec(int sec) {
    const int clamped = clampScanDurationSec(sec);
    if (clamped != sec && m_ctx.host) {
        m_ctx.host->logger().warn(
            QString::fromLatin1(kTag),
            QStringLiteral("scan_duration_seconds=%1 out of range [%2..%3], clamped")
                .arg(sec).arg(kMinScanDurationSec).arg(kMaxScanDurationSec));
    }
    if (clamped == m_scanDurationSec) return;
    m_scanDurationSec = clamped;
    if (m_ctx.host) m_ctx.host->settings().set(kKeyScanDuration, clamped);
    // Apply to the timer immediately if it's idle. If a scan is in flight
    // we leave the active interval alone — changing the duration of an
    // already-running single-shot timer would either fire immediately
    // (if shortened) or be a no-op (Qt ignores setInterval on an active
    // timer); either way, the user gets the new value on the next scan.
    if (!m_scanning) m_scanTimer.setInterval(m_scanDurationSec * 1000);
    emit scanDurationChanged();
}

void AuraLockerPlugin::persistThreshold(int dbm) {
    if (m_ctx.host) m_ctx.host->settings().set(kKeyRssiThresh, dbm);
}

void AuraLockerPlugin::persistDelay(const char* key, int sec) {
    if (m_ctx.host) m_ctx.host->settings().set(QString::fromLatin1(key), sec);
}

QList<TrayMenuContributor::TrayItem> AuraLockerPlugin::contributeTrayItems() {
    TrayMenuContributor::TrayItem item;
    item.id        = kToggleId;
    // M4-C16: toggle label uses the "X: ON" / "X: OFF" format from
    // docs/06 §4.8. The native QMenu also shows a ✓ next to checkable
    // items, so the label is the only place state is spelled out.
    // PR3 i18n: both branches go through translate() so the tray menu
    // flips with the catalog. SystemTray::retranslate re-pulls items on
    // language change, so toggling between 中文 / English refreshes live.
    item.label     = (m_paused
        ? QCoreApplication::translate("AuraLockerPlugin", "Aura Locker: OFF")
        : QCoreApplication::translate("AuraLockerPlugin", "Aura Locker: ON")
    ).toStdString();
    item.checkable = true;
    item.checked   = !m_paused;
    return { item };
}

void AuraLockerPlugin::onTrayItemClicked(const std::string& id) {
    if (id != kToggleId) return;
    m_paused = !m_paused;
    if (m_paused) {
        if (m_tracker && m_tracker->isActive()) m_tracker->stopMonitoring();
        m_detector.pause();
    } else {
        startMonitoringForPairedDevice();
    }
    if (m_ctx.host) {
        m_ctx.host->logger().info(
            QString::fromLatin1(kTag),
            m_paused ? QStringLiteral("Aura paused") : QStringLiteral("Aura resumed"));
        // Pull a fresh tray contribution so the label flips ON↔OFF.
        m_ctx.host->tray().refreshPluginMenu(QString::fromLatin1(kId));
    }
}

DashboardTabContributor::TabInfo AuraLockerPlugin::tabInfo() const {
    return TabInfo{
        "aura",
        // PR3 i18n: tab label uses translate() so it flips with the active
        // catalog. Context "AuraLockerPlugin" matches the <context> block in
        // i18n/host_{en,zh_CN}.ts. toStdString() because TabInfo::title is
        // std::string across the DLL ABI.
        QCoreApplication::translate("AuraLockerPlugin", "Aura").toStdString(),
        QUrl("qrc:/aura/icons/aura-tab.svg"),
        QUrl("qrc:/aura/qml/AuraTab.qml"),
        40,
    };
}

SettingsPageContributor::PageInfo AuraLockerPlugin::pageInfo() const {
    // M5-C4a: id "aura" mirrors the dashboard tab id so the Settings sidebar
    // entry and the Dashboard tab visually pair. order=30 lands it before
    // rhythm (40) and screen_time (50) in the "plugins" section.
    PageInfo info;
    info.id = "aura";
    info.title = QCoreApplication::translate(
        "AuraLockerPlugin", "Aura Locker").toStdString();
    info.icon = QUrl("qrc:/aura/icons/aura-tab.svg");
    info.content_qml = QUrl("qrc:/aura/qml/AuraSettingsPage.qml");
    info.order = 30;
    return info;
}

} // namespace Margin::Plugins::Aura
