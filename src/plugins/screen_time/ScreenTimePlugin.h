// ScreenTimePlugin — passive foreground-app duration tracker (M2).
// Spec: docs/04-plugin-spec.md §8.3 + docs/11-roadmap.md M2.
//
// Composition (M2-C8 ships skeleton only; rest wired in M2-C3 / C5 / C6 / C7):
//
//   WindowMonitorService ──activeWindowChanged──→ closePrevSession + openNew
//   InputMonitorService  ──userIdleStateChanged──→ closePrevSession(is_idle_end)
//                                                       │
//                                                       ▼
//                                       SessionStore → Database (margin.db)
//                                                       │
//                                                       ▼
//                                   CategoryMatcher → category column (M2-C5)
//                                                       │
//                                                       ▼
//                                       ScreenTimeTab.qml (M2-C6) → report UI
//
// Q_PROPERTY surface (registered as `screen_time` context property in onLoad):
//   (M2-C8 ships no Q_PROPERTY — bare skeleton. M2-C3/C5/C6/C7 will add
//    currentApp / topApps / categoryBreakdown / viewMode / etc.)
//
// Q_INVOKABLE surface: same — none in M2-C8 skeleton.

#pragma once

#include "CategoryMatcher.h"
#include "SessionStore.h"

#include "Margin/CryptoService.h"
#include "Margin/DashboardTabContributor.h"
#include "Margin/Database.h"
#include "Margin/InputMonitorService.h"
#include "Margin/PluginContext.h"
#include "Margin/PluginInterface.h"
#include "Margin/Result.h"
#include "Margin/SettingsPageContributor.h"
#include "Margin/TrayMenuContributor.h"
#include "Margin/WindowMonitorService.h"

#include <QObject>
#include <QString>
#include <QVariantList>

#include <memory>

namespace Margin::Plugins::ScreenTime {

class ScreenTimePlugin : public QObject,
                        public PluginInterface,
                        public TrayMenuContributor,
                        public DashboardTabContributor,
                        public SettingsPageContributor {
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
    Q_PROPERTY(int     selectedDay           READ selectedDay WRITE setSelectedDay NOTIFY selectedDayChanged)
    // Sum of categoryBreakdown durations in day view, 0 otherwise. Daily-
    // scoped total screen time as seconds — Tab1 Overview card formats it
    // via the formatFocus helper. Not derivable from dailyTotals (which is
    // empty in day view per rebuildReportCache week/month branch).
    Q_PROPERTY(qint64  todayFocusSeconds     READ todayFocusSeconds
               NOTIFY todayFocusSecondsChanged)
    Q_PROPERTY(int     idleThresholdSec      READ idleThresholdSec WRITE setIdleThresholdSec
               NOTIFY idleThresholdChanged)

public:
    ScreenTimePlugin() = default;

    std::string id() const override;
    std::string version() const override;
    Result<void, std::string> onLoad(const PluginContext& ctx) override;
    void onConfigChange(const QJsonObject&) override {}
    void onUnload() override;

    // TrayMenuContributor (M4-C16): contributes a read-only "Today's Focus:
    // Hh Mm" preview line. No toggle — Screen Time has no pause semantic.
    TrayMenuContributor* asTrayMenu() override { return this; }
    QList<TrayItem> contributeTrayItems() override;
    void onTrayItemClicked(const std::string&) override {}

    // DashboardTabContributor — Tab2 "时长" entry. Loaded from screen_time.qrc.
    DashboardTabContributor* asDashboardTab() override { return this; }
    TabInfo tabInfo() const override;

    // SettingsPageContributor (M5-C4c) — sidebar entry under "plugins".
    SettingsPageContributor* asSettingsPage() override { return this; }
    PageInfo pageInfo() const override;

    // ── Q_PROPERTY reads ────────────────────────────────────────────
    QString currentApp() const { return m_currentApp; }
    qint64  currentSessionStartedAt() const { return m_currentSessionStartedAt; }
    bool    isUserIdle() const { return m_idleSinceMs != 0; }
    QString viewMode() const { return m_viewMode; }
    QVariantList topApps() const { return m_cachedTopApps; }
    QVariantList categoryBreakdown() const { return m_cachedCategory; }
    QVariantList dailyTotals() const { return m_cachedDailyTotals; }
    int     pickupCount() const { return m_cachedPickupCount; }
    qint64  todayFocusSeconds() const { return m_cachedTodayFocusSeconds; }
    int     idleThresholdSec() const;
    int     selectedDay() const { return m_selectedDay; }

    // ── Q_PROPERTY writes ───────────────────────────────────────────
    void setViewMode(const QString& mode);
    void setIdleThresholdSec(int sec);
    void setSelectedDay(int day);

    // ── Q_INVOKABLE ─────────────────────────────────────────────────
    // Force a re-query of all report data. Called from the UI after the
    // user toggles view mode or wants to refresh manually. Auto-called
    // once per minute by m_reportTimer (started in onLoad).
    Q_INVOKABLE void refreshReport();

    // Export every session as JSON / CSV. window_title is decrypted at
    // export time — the user owns this data and exporting is an explicit
    // user action (FileDialog picks the destination), so plaintext-on-disk
    // is the documented behavior (docs/07-privacy-security.md §M2).
    // Returns empty string on success; an error message otherwise.
    Q_INVOKABLE QString exportJson(const QUrl& fileUrl);
    Q_INVOKABLE QString exportCsv(const QUrl& fileUrl);

    // Total session count — drives the "delete N sessions" confirm text.
    Q_INVOKABLE int sessionCount();

    // DELETE both tables. Caller (QML) is responsible for the confirm
    // dialog — this method executes immediately.
    Q_INVOKABLE bool clearAllData();

signals:
    void currentAppChanged();
    void idleChanged();
    void viewModeChanged();
    void reportChanged();
    void todayFocusSecondsChanged();
    void idleThresholdChanged();
    void selectedDayChanged();

private:
    // Wire WindowMonitorService / InputMonitorService signals to
    // SessionStore calls. Called once at onLoad.
    void wireSignals();

    // Slots — see ScreenTimePlugin.cpp for the state machine.
    void onActiveWindowChanged(qint64 pid,
                               const QString& processName,
                               const QString& processPath,
                               const QString& windowTitle);
    void onIdleStateChanged(bool idle);

    // Close the in-flight session row (if any). Pure helper — callers
    // already hold m_currentRowId / m_currentApp + the ended_at + isIdleEnd.
    void closeCurrentSession(qint64 endedAt, bool isIdleEnd);

    // Rebuild the cached QVariantLists exposed via Q_PROPERTY. Picked
    // up by refreshReport() + a periodic QTimer.
    void rebuildReportCache();

    PluginContext m_ctx;

    // The two platform services are kept as raw pointers for connect();
    // they're owned by PlatformBackend / HostCore and outlive this plugin
    // because HostCore unloads plugins before tearing down PlatformBackend.
    WindowMonitorService* m_windowMonitor = nullptr;
    InputMonitorService*  m_inputMonitor  = nullptr;

    // Store + crypto (per-plugin HKDF key from CryptoService). Crypto is
    // raw-pointer because HostServices owns the CryptoService& pool.
    std::unique_ptr<SessionStore>    m_store;
    std::unique_ptr<CategoryMatcher> m_matcher;
    CryptoService* m_crypto = nullptr;
    Database*      m_database = nullptr;

    // ── Live session state ──────────────────────────────────────────
    // The currently-open app_session row id; 0 when no session is live
    // (after idle, after stopMonitoring, before the first switch).
    long long m_currentRowId = 0;
    QString   m_currentApp;
    qint64    m_currentSessionStartedAt = 0;

    // Idle tracking. m_idleSinceMs = 0 means not currently idle. When
    // idle ends we compute prev_idle_ms = now - m_idleSinceMs for the
    // pickup event.
    qint64 m_idleSinceMs = 0;

    // ── Report cache (M2-C6) ────────────────────────────────────────
    // Pre-built QVariantLists refreshed by rebuildReportCache(). QML
    // reads go through these caches directly — no per-frame SQL.
    QString       m_viewMode = QStringLiteral("day");
    QVariantList  m_cachedTopApps;
    QVariantList  m_cachedCategory;
    QVariantList  m_cachedDailyTotals;
    int           m_cachedPickupCount = 0;
    // M4-C8: derived sum of categoryBreakdown durations (day view). 0 in
    // week/month view — Overview card shows "—" in that case, which is fine
    // because the default viewMode is "day".
    qint64        m_cachedTodayFocusSeconds = 0;
    class QTimer* m_reportTimer = nullptr;
    int           m_selectedDay = 0;
};

} // namespace Margin::Plugins::ScreenTime


