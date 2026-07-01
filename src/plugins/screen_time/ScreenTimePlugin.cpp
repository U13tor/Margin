// ScreenTimePlugin impl — see ScreenTimePlugin.h.
//
// State machine (M2-C3):
//
//   activeWindowChanged(pid, name, title):
//     1. closeCurrentSession(now, isIdleEnd=false)
//     2. m_currentRowId = openSession(name, encrypt(title), "", now)
//     3. m_currentApp = name
//
//   userIdleStateChanged(true):
//     1. closeCurrentSession(now, isIdleEnd=true)
//     2. m_idleSinceMs = now
//
//   userIdleStateChanged(false):
//     1. prev_idle = m_idleSinceMs ? now - m_idleSinceMs : 0
//     2. insertPickup(now, prev_idle)
//     3. m_idleSinceMs = 0
//     (session stays closed — next activeWindowChanged reopens it)

#include "ScreenTimePlugin.h"

#include "Margin/CryptoService.h"
#include "Margin/Database.h"
#include "Margin/HostServices.h"
#include "Margin/InputMonitorService.h"
#include "Margin/Logger.h"
#include "Margin/QmlService.h"
#include "Margin/Settings.h"
#include "Margin/WindowMonitorService.h"

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <QVariant>

#include <algorithm>

namespace Margin::Plugins::ScreenTime {

namespace {
constexpr const char* kTag = "screen_time";
}

std::string ScreenTimePlugin::id() const { return "screen_time"; }
std::string ScreenTimePlugin::version() const { return "0.1.0"; }

Result<void, std::string> ScreenTimePlugin::onLoad(const PluginContext& ctx) {
    m_ctx = ctx;

    if (ctx.host) {
        // Expose ourselves as a QML context property so ScreenTimeTab.qml
        // can read Q_PROPERTY / call Q_INVOKABLE directly. M2-C8 skeleton
        // has no Q_PROPERTY yet — the registration path is wired so M2-C3
        // can add fields without touching onLoad.
        if (auto* qml = ctx.host->qml()) {
            qml->registerContextProperty(QStringLiteral("screen_time"), this);
        }
        m_database = ctx.host->database();
        m_crypto   = ctx.host->crypto();
        m_windowMonitor = ctx.host->windowMonitor();
        m_inputMonitor  = ctx.host->inputMonitor();

        if (m_database && m_crypto) {
            m_store = std::make_unique<SessionStore>();
            if (m_store->ensureSchema(*m_database)) {
                ctx.host->logger().info(
                    QStringLiteral("screen_time"),
                    QStringLiteral("schema ready"));
            } else {
                ctx.host->logger().error(
                    QStringLiteral("screen_time"),
                    QStringLiteral("schema failed: %1").arg(m_database->lastError()));
                m_store.reset();  // no store — signals will early-return
            }

            // Category matcher: built-in defaults from the qrc JSON +
            // user overrides from Settings. Bad user regexes are
            // skipped inside loadUserOverrides (warn + continue) so one
            // typo doesn't disable categorization.
            m_matcher = std::make_unique<CategoryMatcher>();
            const int defCount = m_matcher->loadDefaults(
                QStringLiteral(":/screen_time/default_categories.json"));
            const int userCount = m_matcher->loadUserOverrides(ctx.host->settings());
            ctx.host->logger().info(
                QStringLiteral("screen_time"),
                QStringLiteral("category matcher: %1 default + %2 user rule(s)")
                    .arg(defCount).arg(userCount));
        } else {
            ctx.host->logger().warn(
                QStringLiteral("screen_time"),
                QStringLiteral("database (%1) or crypto (%2) missing — tracking disabled")
                    .arg(m_database ? QStringLiteral("ok") : QStringLiteral("null"),
                         m_crypto   ? QStringLiteral("ok") : QStringLiteral("null")));
        }

        wireSignals();

        // Periodic report refresh — QML reads from cached Q_PROPERTYs;
        // a 1-minute cadence is enough for daily totals without
        // hammering SQLite. Manual refreshReport() runs immediately.
        m_reportTimer = new QTimer(this);
        m_reportTimer->setInterval(60 * 1000);
        connect(m_reportTimer, &QTimer::timeout, this, &ScreenTimePlugin::refreshReport);
        m_reportTimer->start();
        refreshReport();

        // M4-C16: keep the tray preview line in sync with the focus total.
        // todayFocusSecondsChanged fires only when the value actually moves
        // (see recalcTodayFocus), so there's no per-second refresh storm.
        connect(this, &ScreenTimePlugin::todayFocusSecondsChanged, this,
            [this]() {
                if (m_ctx.host) {
                    m_ctx.host->tray().refreshPluginMenu(
                        QStringLiteral("screen_time"));
                }
            });

        ctx.host->logger().info(
            QStringLiteral("screen_time"),
            QStringLiteral("loaded (M2-C3 schema wired)"));
    }

    return Result<void, std::string>::ok();
}

QList<TrayMenuContributor::TrayItem> ScreenTimePlugin::contributeTrayItems() {
    // Read-only preview only — Screen Time has no pause semantic.
    TrayMenuContributor::TrayItem info;
    info.id = "preview_focus";
    const qint64 sec = m_cachedTodayFocusSeconds;
    const int h = static_cast<int>(sec / 3600);
    const int m = static_cast<int>((sec % 3600) / 60);
    info.label = QCoreApplication::translate(
        "ScreenTimePlugin", "Today's Focus: %1h %2m")
            .arg(h).arg(m).toStdString();
    info.read_only = true;
    return { info };
}

void ScreenTimePlugin::onUnload() {
    if (m_reportTimer) {
        m_reportTimer->stop();
        m_reportTimer->deleteLater();
        m_reportTimer = nullptr;
    }
    if (m_ctx.host) {
        // Close any in-flight session so it doesn't sit with started_at ==
        // ended_at forever if the user unloads the plugin mid-day.
        closeCurrentSession(QDateTime::currentMSecsSinceEpoch(), false);
        m_ctx.host->logger().info(
            QStringLiteral("screen_time"),
            QStringLiteral("unloaded"));
    }
}

int ScreenTimePlugin::idleThresholdSec() const {
    if (!m_inputMonitor) return 0;
    return m_inputMonitor->idleThresholdMs() / 1000;
}

void ScreenTimePlugin::setViewMode(const QString& mode) {
    // Only accept the three documented modes — ignore anything else
    // so a typo from the caller doesn't put us in a half-state.
    if (mode != QStringLiteral("day") &&
        mode != QStringLiteral("week") &&
        mode != QStringLiteral("month")) {
        return;
    }
    if (m_viewMode == mode) return;
    m_viewMode = mode;
    m_selectedDay = 0; // reset selected day filter on view mode change
    emit viewModeChanged();
    emit selectedDayChanged();
    refreshReport();
}

void ScreenTimePlugin::setSelectedDay(int day) {
    if (m_selectedDay == day) return;
    m_selectedDay = day;
    emit selectedDayChanged();
    refreshReport();
}

void ScreenTimePlugin::setIdleThresholdSec(int sec) {
    if (!m_inputMonitor) return;
    if (sec < 60) sec = 60;       // 1-minute floor — anything shorter is noise
    if (sec > 1800) sec = 1800;   // 30-minute ceiling — beyond that idle feels broken
    m_inputMonitor->setIdleThresholdMs(sec * 1000);
    if (m_ctx.host) {
        m_ctx.host->settings().set(
            QStringLiteral("plugins.screen_time.idle_threshold_sec"), QVariant(sec));
    }
    emit idleThresholdChanged();
}

void ScreenTimePlugin::refreshReport() {
    rebuildReportCache();
    emit reportChanged();
}

void ScreenTimePlugin::wireSignals() {
    if (!m_windowMonitor || !m_inputMonitor || !m_store) return;

    connect(m_windowMonitor, &WindowMonitorService::activeWindowChanged,
            this, &ScreenTimePlugin::onActiveWindowChanged);
    connect(m_inputMonitor, &InputMonitorService::userIdleStateChanged,
            this, &ScreenTimePlugin::onIdleStateChanged);
    // M5 fix for跨休眠计时膨胀 — close in-flight session at suspend so the
    // sleep period doesn't get billed to whatever app was foreground.
    connect(m_inputMonitor, &InputMonitorService::systemSuspendStateChanged,
            this, &ScreenTimePlugin::onSystemSuspendStateChanged);

    // Passive hooks: the host installs them lazily (tracker.isActive() ==
    // false until first startMonitoring). We start them now so signal
    // delivery begins as soon as the plugin loads. Failure is non-fatal
    // — if the hook install fails (e.g. another app has a conflicting LL
    // hook), we still serve data from whatever signals do fire.
    if (!m_windowMonitor->isActive()) {
        if (!m_windowMonitor->startMonitoring()) {
            if (m_ctx.host) m_ctx.host->logger().warn(
                QStringLiteral("screen_time"),
                QStringLiteral("window monitor startMonitoring failed"));
        }
    }
    if (!m_inputMonitor->isActive()) {
        // Default 3-minute idle threshold per docs/11-roadmap.md M2-C4.
        // User-tunable via QML SpinBox; persisted in Settings as
        // plugins.screen_time.idle_threshold_sec.
        int thresholdSec = 3 * 60;
        if (m_ctx.host) {
            const QVariant stored = m_ctx.host->settings().get(
                QStringLiteral("plugins.screen_time.idle_threshold_sec"),
                QVariant(thresholdSec));
            bool ok = false;
            const int parsed = stored.toInt(&ok);
            if (ok && parsed >= 60 && parsed <= 1800) thresholdSec = parsed;
        }
        if (!m_inputMonitor->startMonitoring(thresholdSec * 1000)) {
            if (m_ctx.host) m_ctx.host->logger().warn(
                QStringLiteral("screen_time"),
                QStringLiteral("input monitor startMonitoring failed"));
        }
    }
}

void ScreenTimePlugin::onActiveWindowChanged(qint64 /*pid*/,
                                              const QString& processName,
                                              const QString& processPath,
                                              const QString& windowTitle) {
    if (!m_store || !m_database || !m_crypto) return;

    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    // 1. Close the previous session (idle or window-switch — both end
    //    the interval here).
    closeCurrentSession(now, /*isIdleEnd=*/false);

    // 2. Encrypt the window title. AES-256-GCM via the plugin's HKDF
    //    key. Empty titles produce empty ciphertext; SessionStore stores
    //    BLOB null in that case.
    QByteArray titleEnc;
    if (!windowTitle.isEmpty()) {
        titleEnc = m_crypto->encryptString(windowTitle);
    }

    // 3. Insert the new session row with category resolved by the
    //    matcher. Category is "" if matcher isn't loaded — queries that
    //    GROUP BY category will bucket those as null/empty rather than
    //    fail. processPath is stored as exe_path for icon lookup (PR3).
    const QString category = m_matcher ? m_matcher->match(processName) : QString();
    m_currentRowId = m_store->openSession(*m_database, processName, titleEnc,
                                          category, processPath, now);
    m_currentApp = processName;
    m_currentSessionStartedAt = now;
    emit currentAppChanged();
    // Don't refreshReport() on every switch — that would hammer SQLite
    // under heavy switching. The 1-minute timer will pick up the new
    // totals; users can also hit the refresh button in the UI.
}

void ScreenTimePlugin::onIdleStateChanged(bool idle) {
    if (!m_store || !m_database) return;

    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    if (idle) {
        // Idle edge: close the in-flight session with is_idle_end=true.
        // We DON'T open a new "idle session" — the gap between idle
        // start and the next window switch is intentionally blank (no
        // app was in the foreground during idle, by definition).
        closeCurrentSession(now, /*isIdleEnd=*/true);
        m_idleSinceMs = now;
        m_currentApp.clear();
        m_currentSessionStartedAt = 0;
        emit currentAppChanged();
    } else {
        // Resume edge: write a pickup event. Session stays closed until
        // the next activeWindowChanged naturally opens a new one —
        // writing a synthetic "Margin.exe (idle)" session would mislead
        // the daily total.
        const qint64 prevIdle = m_idleSinceMs ? (now - m_idleSinceMs) : 0;
        m_store->insertPickup(*m_database, now, prevIdle);
        m_idleSinceMs = 0;
        emit idleChanged();
        // New pickup just landed — refresh so the count ticks up.
        refreshReport();
    }
}

void ScreenTimePlugin::onSystemSuspendStateChanged(bool suspended) {
    if (!m_store || !m_database) return;

    if (!suspended) {
        // Resume edge — don't open a synthetic session; let the next
        // activeWindowChanged do it. Refresh so the UI is current if
        // the user just woke the machine and looked at the dashboard.
        refreshReport();
        return;
    }

    // Suspend edge: close the in-flight session immediately so its
    // duration_ms stops accumulating wall-clock during sleep. isIdleEnd
    // is false — this isn't a user-idle transition. Safe no-op if no
    // session is live (idle / already-closed / pre-first-switch).
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    closeCurrentSession(now, /*isIdleEnd=*/false);
    m_currentApp.clear();
    m_currentSessionStartedAt = 0;
    emit currentAppChanged();
}

void ScreenTimePlugin::rebuildReportCache() {
    if (!m_store || !m_database) return;

    const QDate today = QDate::currentDate();
    const int todayLocal = SessionStore::dayLocalFromDate(today);

    if (m_viewMode == QStringLiteral("day")) {
        // Day view: top apps + categories for today; dailyTotals empty.
        const auto top = m_store->topAppsByDay(*m_database, todayLocal, /*limit=*/5);
        m_cachedTopApps.clear();
        m_cachedTopApps.reserve(top.size());
        for (const QVariantMap& r : top) {
            QVariantMap entry;
            entry.insert(QStringLiteral("name"),       r.value(QStringLiteral("app_name")));
            entry.insert(QStringLiteral("durationMs"), r.value(QStringLiteral("duration_ms")));
            entry.insert(QStringLiteral("category"),   r.value(QStringLiteral("category")));
            // PR3 round-2 #2b: exe_path drives MBarChart's image://appicon
            // delegate (PR4). Empty for legacy rows / Mac stub — icon slot
            // stays empty in that case, no broken-image fallback needed.
            entry.insert(QStringLiteral("exePath"),    r.value(QStringLiteral("exe_path")));
            m_cachedTopApps.append(entry);
        }

        const auto cats = m_store->categoriesByDay(*m_database, todayLocal);
        // Compute total to derive percentages AND to expose as
        // todayFocusSeconds (M4-C8) — the total is the same sum either way.
        qint64 total = 0;
        for (const QVariantMap& r : cats) {
            total += r.value(QStringLiteral("duration_ms")).toLongLong();
        }
        m_cachedCategory.clear();
        m_cachedCategory.reserve(cats.size());
        for (const QVariantMap& r : cats) {
            QVariantMap entry;
            entry.insert(QStringLiteral("category"),   r.value(QStringLiteral("category")));
            entry.insert(QStringLiteral("durationMs"), r.value(QStringLiteral("duration_ms")));
            const qint64 dur = r.value(QStringLiteral("duration_ms")).toLongLong();
            entry.insert(QStringLiteral("percentage"),
                         total > 0 ? QVariant(static_cast<double>(dur) * 100.0 / total) : QVariant(0.0));
            m_cachedCategory.append(entry);
        }
        m_cachedDailyTotals.clear();
        // M4-C8: stash the category total as seconds before we lose scope.
        // Compare + emit so the Overview card's binding re-evaluates on
        // every cache rebuild (typically once a minute via m_reportTimer).
        const qint64 newFocusSec = total / 1000;
        if (newFocusSec != m_cachedTodayFocusSeconds) {
            m_cachedTodayFocusSeconds = newFocusSec;
            emit todayFocusSecondsChanged();
        }
    } else {
        // Week / month view: daily totals; clear per-app lists.
        const int days = (m_viewMode == QStringLiteral("week")) ? 7 : 30;
        const int fromDay = SessionStore::dayLocalFromDate(today.addDays(-(days - 1)));
        const auto totals = m_store->dailyTotals(*m_database, fromDay, todayLocal);
        m_cachedDailyTotals.clear();
        m_cachedDailyTotals.reserve(totals.size());
        for (const QVariantMap& r : totals) {
            QVariantMap entry;
            entry.insert(QStringLiteral("day"),        r.value(QStringLiteral("day_local")));
            entry.insert(QStringLiteral("durationMs"), r.value(QStringLiteral("duration_ms")));
            m_cachedDailyTotals.append(entry);
        }

        // Fetch top apps and category breakdown for week/month view (aggregated range or filtered single day)
        QList<QVariantMap> top;
        QList<QVariantMap> cats;
        if (m_selectedDay == 0) {
            top = m_store->topAppsByRange(*m_database, fromDay, todayLocal, /*limit=*/5);
            cats = m_store->categoriesByRange(*m_database, fromDay, todayLocal);
        } else {
            top = m_store->topAppsByDay(*m_database, m_selectedDay, /*limit=*/5);
            cats = m_store->categoriesByDay(*m_database, m_selectedDay);
        }

        m_cachedTopApps.clear();
        m_cachedTopApps.reserve(top.size());
        for (const QVariantMap& r : top) {
            QVariantMap entry;
            entry.insert(QStringLiteral("name"),       r.value(QStringLiteral("app_name")));
            entry.insert(QStringLiteral("durationMs"), r.value(QStringLiteral("duration_ms")));
            entry.insert(QStringLiteral("category"),   r.value(QStringLiteral("category")));
            entry.insert(QStringLiteral("exePath"),    r.value(QStringLiteral("exe_path")));
            m_cachedTopApps.append(entry);
        }

        qint64 total = 0;
        for (const QVariantMap& r : cats) {
            total += r.value(QStringLiteral("duration_ms")).toLongLong();
        }
        m_cachedCategory.clear();
        m_cachedCategory.reserve(cats.size());
        for (const QVariantMap& r : cats) {
            QVariantMap entry;
            entry.insert(QStringLiteral("category"),   r.value(QStringLiteral("category")));
            entry.insert(QStringLiteral("durationMs"), r.value(QStringLiteral("duration_ms")));
            const qint64 dur = r.value(QStringLiteral("duration_ms")).toLongLong();
            entry.insert(QStringLiteral("percentage"),
                         total > 0 ? QVariant(static_cast<double>(dur) * 100.0 / total) : QVariant(0.0));
            m_cachedCategory.append(entry);
        }

        // Day-view-only value — reset to 0 when the user switches to week
        // or month view so the Overview card doesn't show stale focus time.
        if (m_cachedTodayFocusSeconds != 0) {
            m_cachedTodayFocusSeconds = 0;
            emit todayFocusSecondsChanged();
        }
    }

    m_cachedPickupCount = m_store->pickupCountByDay(*m_database, todayLocal);
}

void ScreenTimePlugin::closeCurrentSession(qint64 endedAt, bool isIdleEnd) {
    if (m_currentRowId == 0 || !m_store || !m_database) return;
    m_store->closeSession(*m_database, m_currentRowId, endedAt, isIdleEnd);
    m_currentRowId = 0;
    m_currentApp.clear();
}

QString ScreenTimePlugin::exportJson(const QUrl& fileUrl) {
    if (!m_store || !m_database || !m_crypto) {
        return QStringLiteral("screen_time not fully initialized");
    }
    const QString path = fileUrl.toLocalFile();
    if (path.isEmpty()) return QStringLiteral("invalid file path");

    // Decrypt each window_title on the export path. If decryption fails
    // for a row (key changed / corruption), write null + keep going — a
    // single bad row shouldn't block the whole export.
    const auto sessions = m_store->allSessions(*m_database);
    const auto pickups = m_store->allPickups(*m_database);

    QJsonArray sessionArray;
    for (const QVariantMap& r : sessions) {
        QJsonObject o;
        o[QStringLiteral("id")]           = r.value(QStringLiteral("id")).toLongLong();
        o[QStringLiteral("app_name")]     = r.value(QStringLiteral("app_name")).toString();
        const QByteArray enc = r.value(QStringLiteral("window_title_enc")).toByteArray();
        if (enc.isEmpty()) {
            o[QStringLiteral("window_title")] = QJsonValue::Null;
        } else {
            const QString plain = m_crypto->decryptString(enc);
            o[QStringLiteral("window_title")] = plain.isNull()
                ? QJsonValue::Null
                : QJsonValue(plain);
        }
        o[QStringLiteral("category")]     = r.value(QStringLiteral("category")).toString();
        o[QStringLiteral("exe_path")]     = r.value(QStringLiteral("exe_path")).toString();
        o[QStringLiteral("started_at")]   = r.value(QStringLiteral("started_at")).toLongLong();
        o[QStringLiteral("ended_at")]     = r.value(QStringLiteral("ended_at")).toLongLong();
        o[QStringLiteral("duration_ms")]  = r.value(QStringLiteral("duration_ms")).toLongLong();
        o[QStringLiteral("is_idle_end")]  = r.value(QStringLiteral("is_idle_end")).toInt() != 0;
        o[QStringLiteral("day_local")]    = r.value(QStringLiteral("day_local")).toInt();
        o[QStringLiteral("hour_local")]   = r.value(QStringLiteral("hour_local")).toInt();
        o[QStringLiteral("weekday_local")]= r.value(QStringLiteral("weekday_local")).toInt();
        sessionArray.append(o);
    }

    QJsonArray pickupArray;
    for (const QVariantMap& r : pickups) {
        QJsonObject o;
        o[QStringLiteral("id")]            = r.value(QStringLiteral("id")).toLongLong();
        o[QStringLiteral("event_type")]    = r.value(QStringLiteral("event_type")).toString();
        o[QStringLiteral("occurred_at")]   = r.value(QStringLiteral("occurred_at")).toLongLong();
        o[QStringLiteral("day_local")]     = r.value(QStringLiteral("day_local")).toInt();
        o[QStringLiteral("hour_local")]    = r.value(QStringLiteral("hour_local")).toInt();
        o[QStringLiteral("weekday_local")] = r.value(QStringLiteral("weekday_local")).toInt();
        o[QStringLiteral("prev_idle_ms")]  = r.value(QStringLiteral("prev_idle_ms")).toLongLong();
        pickupArray.append(o);
    }

    QJsonObject root;
    root[QStringLiteral("schema_version")] = 1;
    root[QStringLiteral("exported_at")]    = QDateTime::currentMSecsSinceEpoch();
    root[QStringLiteral("sessions")]       = sessionArray;
    root[QStringLiteral("pickups")]        = pickupArray;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return QStringLiteral("cannot open %1 for writing").arg(path);
    }
    const QJsonDocument doc(root);
    if (f.write(doc.toJson(QJsonDocument::Indented)) < 0) {
        return QStringLiteral("write failed: %1").arg(f.errorString());
    }
    f.close();
    if (m_ctx.host) {
        m_ctx.host->logger().info(
            QStringLiteral("screen_time"),
            QStringLiteral("exported %1 sessions + %2 pickups → %3")
                .arg(sessionArray.size()).arg(pickupArray.size()).arg(QFileInfo(path).fileName()));
    }
    return QString();
}

QString ScreenTimePlugin::exportCsv(const QUrl& fileUrl) {
    if (!m_store || !m_database || !m_crypto) {
        return QStringLiteral("screen_time not fully initialized");
    }
    const QString path = fileUrl.toLocalFile();
    if (path.isEmpty()) return QStringLiteral("invalid file path");

    // CSV: one row per session. Pickups live in a separate table; for
    // the v1 export we ship sessions only — if users ask for pickup CSV
    // we add a second file in a later iteration.
    const auto sessions = m_store->allSessions(*m_database);

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return QStringLiteral("cannot open %1 for writing").arg(path);
    }
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    // RFC 4180: CRLF line endings, fields with comma/quote/newline quoted.
    out << "id,app_name,window_title,category,exe_path,started_at,ended_at,duration_ms,is_idle_end,day_local,hour_local,weekday_local\r\n";
    for (const QVariantMap& r : sessions) {
        const auto writeField = [&out](const QString& s) {
            if (s.contains(QLatin1Char(',')) ||
                s.contains(QLatin1Char('"')) ||
                s.contains(QLatin1Char('\n')) ||
                s.contains(QLatin1Char('\r'))) {
                QString escaped = s;
                escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));
                out << '"' << escaped << '"';
            } else {
                out << s;
            }
        };

        out << r.value(QStringLiteral("id")).toLongLong() << ',';
        writeField(r.value(QStringLiteral("app_name")).toString());
        out << ',';
        const QByteArray enc = r.value(QStringLiteral("window_title_enc")).toByteArray();
        if (!enc.isEmpty()) {
            const QString plain = m_crypto->decryptString(enc);
            writeField(plain.isNull() ? QStringLiteral("<decrypt failed>") : plain);
        }
        out << ',';
        writeField(r.value(QStringLiteral("category")).toString());
        out << ',';
        writeField(r.value(QStringLiteral("exe_path")).toString());
        out << ',';
        out << r.value(QStringLiteral("started_at")).toLongLong()    << ',';
        out << r.value(QStringLiteral("ended_at")).toLongLong()      << ',';
        out << r.value(QStringLiteral("duration_ms")).toLongLong()   << ',';
        out << (r.value(QStringLiteral("is_idle_end")).toInt() != 0 ? "true" : "false") << ',';
        out << r.value(QStringLiteral("day_local")).toInt()     << ',';
        out << r.value(QStringLiteral("hour_local")).toInt()    << ',';
        out << r.value(QStringLiteral("weekday_local")).toInt() << "\r\n";
    }
    f.close();
    if (m_ctx.host) {
        m_ctx.host->logger().info(
            QStringLiteral("screen_time"),
            QStringLiteral("exported %1 sessions (CSV) → %2")
                .arg(sessions.size()).arg(QFileInfo(path).fileName()));
    }
    return QString();
}

int ScreenTimePlugin::sessionCount() {
    if (!m_store || !m_database) return 0;
    return m_store->sessionCount(*m_database);
}

bool ScreenTimePlugin::clearAllData() {
    if (!m_store || !m_database) return false;
    if (!m_store->clearAll(*m_database)) {
        if (m_ctx.host) {
            m_ctx.host->logger().error(
                QStringLiteral("screen_time"),
                QStringLiteral("clearAll failed: %1").arg(m_database->lastError()));
        }
        return false;
    }
    // Reset live state — clearing mid-session would leave m_currentRowId
    // dangling (the row is gone but the plugin still thinks it's open).
    // Close the in-flight session logically (rowId 0) without writing —
    // the row was already deleted by clearAll.
    m_currentRowId = 0;
    m_currentApp.clear();
    m_currentSessionStartedAt = 0;
    m_idleSinceMs = 0;
    emit currentAppChanged();
    refreshReport();
    if (m_ctx.host) {
        m_ctx.host->logger().info(
            QStringLiteral("screen_time"),
            QStringLiteral("cleared all data"));
    }
    return true;
}

DashboardTabContributor::TabInfo ScreenTimePlugin::tabInfo() const {
    return TabInfo{
        "screen_time",
        // PR3 i18n: source switched from Chinese literal "时长" to English
        // "Screen Time" to match Qt's qsTr-source-is-English convention.
        // Chinese rendering now lives in i18n/host_zh_CN.ts under context
        // ScreenTimePlugin.
        QCoreApplication::translate(
            "ScreenTimePlugin", "Screen Time").toStdString(),
        QUrl(QStringLiteral("qrc:/screen_time/icons/screen-time-tab.svg")),
        QUrl(QStringLiteral("qrc:/screen_time/qml/ScreenTimeTab.qml")),
        20,  // order: Overview=10, Screen Time=20, Rhythm=30, Aura=40
    };
}

SettingsPageContributor::PageInfo ScreenTimePlugin::pageInfo() const {
    // M5-C4c: order=50 lands after Rhythm (40) in the "plugins" section.
    // PR3 i18n: previously this diverged from the dashboard tab (tab="时长",
    // sidebar="Screen Time") as a stylistic choice. With i18n, keeping both
    // on the same source "Screen Time" simplifies the .ts — the Chinese
    // catalog can still differentiate via context (DashboardTabs vs
    // ScreenTimePlugin) if we later want to shorten the tab label.
    PageInfo info;
    info.id = "screen_time";
    info.title = QCoreApplication::translate(
        "ScreenTimePlugin", "Screen Time").toStdString();
    info.icon = QUrl(QStringLiteral("qrc:/screen_time/icons/screen-time-tab.svg"));
    info.content_qml = QUrl(QStringLiteral("qrc:/screen_time/qml/ScreenTimeSettingsPage.qml"));
    info.order = 50;
    return info;
}

} // namespace Margin::Plugins::ScreenTime
