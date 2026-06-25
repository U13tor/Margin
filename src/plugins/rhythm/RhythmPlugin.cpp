// RhythmPlugin impl — see RhythmPlugin.h.

#include "RhythmPlugin.h"

#include "Margin/HostServices.h"
#include "Margin/InputMonitorService.h"
#include "Margin/Logger.h"
#include "Margin/QmlService.h"
#include "Margin/Settings.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQuickWindow>
#include <QJsonObject>
#include <QRect>
#include <QScreen>
#include <QSize>
#include <QString>
#include <QTime>
#include <QUrl>

namespace Margin::Plugins::Rhythm {

namespace {
constexpr const char* kId          = "rhythm";
constexpr const char* kVersion     = "0.1.0";
constexpr const char* kTag         = "rhythm";
constexpr const char* kToggleId    = "toggle_pause";
constexpr const char* kQmlName     = "rhythm";  // context property name
constexpr const char* kToastQmlUrl   = "qrc:/rhythm/qml/RhythmToast.qml";
// M4-C13a: standalone break-overlay window. Root is a Window, not Item —
// the file migrated from OverlayContributor/OverlayContainer path to a
// top-level Qt.Tool window shown imperatively from C++.
constexpr const char* kOverlayQmlUrl = "qrc:/rhythm/qml/BreakOverlay.qml";

// Settings keys. None are sensitive (no device ids, no plaintext PII) —
// just user preferences. encrypted_settings stays empty in manifest.
constexpr const char* kKeyWorkMinutes    = "plugins.rhythm.work_minutes";
constexpr const char* kKeyBreakMinutes   = "plugins.rhythm.break_minutes";
constexpr const char* kKeyMaxPostpones   = "plugins.rhythm.max_postpones";
constexpr const char* kKeyTargetRounds   = "plugins.rhythm.target_rounds";
constexpr const char* kKeyAutostart      = "plugins.rhythm.autostart";
constexpr const char* kKeyResumeDelaySec = "plugins.rhythm.resume_delay_sec";
// M4-C8: today-scoped completed-rounds counter + the calendar day it's
// scoped to. Count resets to 0 when stored date != today (cross-machine
// overnight case). Persists across restarts within the same day.
constexpr const char* kKeyTodayRounds    = "plugins.rhythm.today_completed_rounds";
constexpr const char* kKeyTodayDate      = "plugins.rhythm.today_date";
constexpr int kDefaultAuraResumeDelaySec = 60;  // DoD #3 "回座 60s 后恢复"
} // namespace

std::string RhythmPlugin::id() const      { return kId; }
std::string RhythmPlugin::version() const { return kVersion; }

Result<void, std::string> RhythmPlugin::onLoad(const PluginContext& ctx) {
    m_ctx = ctx;

    auto* log = ctx.host ? &ctx.host->logger() : nullptr;
    loadSettings();

    // M3-C2: idle detection. Connect InputMonitorService::userIdleStateChanged
    // straight into PomodoroTimer::setPaused — no EventBus round-trip, the
    // signal is host-side and edge-triggered (no dedupe needed).
    // inputMonitor() is null on macOS until §A19 lifts (CGEventTap not
    // implemented), so on Mac the plugin still works but doesn't pause on
    // idle. Matches Screen Time's behavior on the same platform.
    auto* inputMonitor = ctx.host ? ctx.host->inputMonitor() : nullptr;
    if (inputMonitor) {
        connect(inputMonitor, &InputMonitorService::userIdleStateChanged,
                &m_timer, &PomodoroTimer::setPaused);
        if (log) log->info(QString::fromLatin1(kTag),
            QStringLiteral("idle monitor wired — pausing countdown on idle"));
    } else if (log) {
        log->warn(QString::fromLatin1(kTag),
            QStringLiteral("InputMonitor unavailable on this platform — "
                           "idle-pause disabled (macOS: see §A19)"));
    }

    // Register the timer as `rhythm` context property. QML binds directly
    // to PomodoroTimer's Q_PROPERTY (state, remainingSeconds, etc.) and
    // invokes its Q_INVOKABLE actions (startBreak, skipBreak, ...).
    // The plugin itself is registered as `rhythmHost` so QML can call
    // overlay-management invokables (e.g. dismissBreakOverlay from the
    // done-card close path) without leaking plugin internals through
    // the timer surface.
    if (ctx.host && ctx.host->qml()) {
        ctx.host->qml()->registerContextProperty(
            QString::fromLatin1(kQmlName), &m_timer);
        ctx.host->qml()->registerContextProperty(
            QStringLiteral("rhythmHost"), this);
        createToastWindow();
        createBreakOverlayWindow();
    } else if (log) {
        log->warn(QString::fromLatin1(kTag),
            QStringLiteral("QmlService unavailable — Rhythm UI will be inert"));
    }

    // M3-C5: subscribe to Aura Locker away/back events. When the paired
    // device goes out of range, pause the work countdown (user has stepped
    // away — no point burning the 45-min budget while they're getting
    // coffee). On "back", resume — but only after a presence delay so a brief
    // walk-by doesn't instantly unpause (DoD #3 "回座 60s 后恢复"). Aura's
    // ProximityDetector emits "back" the instant the device re-enters range
    // (its 60s cooldown only suppresses re-lock, NOT the back event), so
    // Rhythm owns the resume delay itself via m_auraResumeTimer.
    //
    // subscriberIdentity is this plugin's QObject — PluginManager calls
    // unsubscribeAll(this) on unload to drop both subscriptions atomically.
    if (ctx.host) {
        const int resumeDelaySec = ctx.host->settings()
            .get(QString::fromLatin1(kKeyResumeDelaySec), kDefaultAuraResumeDelaySec)
            .toInt();
        m_auraResumeTimer.setSingleShot(true);
        m_auraResumeTimer.setInterval(qMax(0, resumeDelaySec) * 1000);
        connect(&m_auraResumeTimer, &QTimer::timeout, this,
            [this]() { m_timer.setPaused(false); });

        auto& bus = ctx.host->eventBus();
        bus.subscribe(QStringLiteral("margin.aura.away"),
            [this](const QJsonObject&) {
                m_auraResumeTimer.stop();  // cancel any pending resume
                m_timer.setPaused(true);
            },
            ctx.subscriberIdentity);
        bus.subscribe(QStringLiteral("margin.aura.back"),
            [this](const QJsonObject&) { m_auraResumeTimer.start(); },
            ctx.subscriberIdentity);
        if (log) log->info(QString::fromLatin1(kTag),
            QStringLiteral("aura away/back wired — pause on away, resume %1s after back")
                .arg(qMax(0, resumeDelaySec)));
    }

    // A3/C2: persist user edits from the Tab3 SpinBoxes (which write the
    // timer's Q_PROPERTYs directly) so they survive a restart. Connected
    // after loadSettings() above, so the initial settings-load doesn't write
    // back. The setters' equality guards keep this from churning.
    if (ctx.host) {
        connect(&m_timer, &PomodoroTimer::workMinutesChanged, this, [this]() {
            if (m_ctx.host) m_ctx.host->settings().set(
                QString::fromLatin1(kKeyWorkMinutes), m_timer.workMinutes());
        });
        connect(&m_timer, &PomodoroTimer::breakMinutesChanged, this, [this]() {
            if (m_ctx.host) m_ctx.host->settings().set(
                QString::fromLatin1(kKeyBreakMinutes), m_timer.breakMinutes());
        });
        connect(&m_timer, &PomodoroTimer::maxPostponesChanged, this, [this]() {
            if (m_ctx.host) m_ctx.host->settings().set(
                QString::fromLatin1(kKeyMaxPostpones), m_timer.maxPostpones());
        });
        // M4-C12: persist targetRounds (daily pomodoro goal for "#N/M" header).
        connect(&m_timer, &PomodoroTimer::targetRoundsChanged, this, [this]() {
            if (m_ctx.host) m_ctx.host->settings().set(
                QString::fromLatin1(kKeyTargetRounds), m_timer.targetRounds());
        });
        // M4-C8: persist today-scoped rounds + the date it's scoped to so
        // a restart within the same day doesn't reset the counter. Both
        // keys are written on every todayCompletedRoundsChanged fire —
        // settings.set is cheap and idempotent.
        connect(&m_timer, &PomodoroTimer::todayCompletedRoundsChanged, this, [this]() {
            if (!m_ctx.host) return;
            m_ctx.host->settings().set(
                QString::fromLatin1(kKeyTodayRounds),
                m_timer.todayCompletedRounds());
            m_ctx.host->settings().set(
                QString::fromLatin1(kKeyTodayDate),
                m_timer.todayDate().toString(Qt::ISODate));
        });
    }

    if (log) {
        log->info(QString::fromLatin1(kTag),
            QStringLiteral("RhythmPlugin onLoad work=%1min break=%2min maxPostpones=%3 targetRounds=%4")
                .arg(m_timer.workMinutes())
                .arg(m_timer.breakMinutes())
                .arg(m_timer.maxPostpones())
                .arg(m_timer.targetRounds()));
    }

    // Auto-start the work countdown unless the user opted out. Default ON
    // — DoD L4.1 expects "00:45 倒计时 visible at startup". Users who want
    // manual-only control set autostart=false.
    const bool autostart = ctx.host
        ? ctx.host->settings().get(QString::fromLatin1(kKeyAutostart), true).toBool()
        : true;
    if (autostart) {
        m_timer.start();
    }
    return Result<void, std::string>::ok();
}

void RhythmPlugin::onUnload() {
    m_timer.stop();
    if (m_toastWindow) {
        m_toastWindow->hide();
        delete m_toastWindow.data();
        m_toastWindow = nullptr;
    }
    if (m_ctx.host) {
        m_ctx.host->logger().info(
            QString::fromLatin1(kTag),
            QStringLiteral("RhythmPlugin onUnload"));
    }
}

void RhythmPlugin::onConfigChange(const QJsonObject&) {
    // External settings.json edits re-clamp + apply live. Idempotent — the
    // PomodoroTimer setters no-op when the value is unchanged. (Note: the host
    // does not yet dispatch onConfigChange; this is the correct hook for when
    // it does, and keeps Tab3 SpinBox edits + file edits on one code path.)
    loadSettings();
}

void RhythmPlugin::loadSettings() {
    auto* settings = m_ctx.host ? &m_ctx.host->settings() : nullptr;
    if (!settings) return;
    auto* log = m_ctx.host ? &m_ctx.host->logger() : nullptr;

    const int rawWork  = settings->get(QString::fromLatin1(kKeyWorkMinutes),
                                       PomodoroTimer::kDefaultWorkMinutes).toInt();
    const int rawBreak = settings->get(QString::fromLatin1(kKeyBreakMinutes),
                                       PomodoroTimer::kDefaultBreakMinutes).toInt();
    const int rawPostp = settings->get(QString::fromLatin1(kKeyMaxPostpones),
                                       PomodoroTimer::kDefaultMaxPostpones).toInt();
    const int rawTarget = settings->get(QString::fromLatin1(kKeyTargetRounds),
                                        PomodoroTimer::kDefaultTargetRounds).toInt();
    const int clampedWork  = PomodoroTimer::clampWorkMinutes(rawWork);
    const int clampedBreak = PomodoroTimer::clampBreakMinutes(rawBreak);
    const int clampedPostp = PomodoroTimer::clampMaxPostpones(rawPostp);
    const int clampedTarget = PomodoroTimer::clampTargetRounds(rawTarget);
    m_timer.setWorkMinutes(clampedWork);
    m_timer.setBreakMinutes(clampedBreak);
    m_timer.setMaxPostpones(clampedPostp);
    m_timer.setTargetRounds(clampedTarget);

    // M4-C8: restore today's completed-rounds counter + the calendar day it's
    // scoped to. Load both BEFORE ensureDailyRollback so the rollback sees
    // the stored date and can decide whether to reset. Order matters: if we
    // called ensureDailyRollback before setTodayDate, the invalid default
    // would always trigger a reset and clobber the stored count.
    const int rawRounds = settings->get(QString::fromLatin1(kKeyTodayRounds), 0).toInt();
    const QString rawDateStr = settings->get(
        QString::fromLatin1(kKeyTodayDate), QString()).toString();
    const QDate rawDate = QDate::fromString(rawDateStr, Qt::ISODate);
    m_timer.setTodayDate(rawDate);
    m_timer.setTodayCompletedRounds(rawRounds);
    m_timer.ensureDailyRollback();  // no-op if rawDate == today; resets otherwise
    if (log) {
        log->info(QString::fromLatin1(kTag),
            QStringLiteral("today completed rounds restored: %1 (date=%2 → %3)")
                .arg(m_timer.todayCompletedRounds())
                .arg(rawDate.isValid() ? rawDate.toString(Qt::ISODate)
                                       : QStringLiteral("<missing>"))
                .arg(m_timer.todayDate().toString(Qt::ISODate)));
    }
    if (log) {
        if (clampedWork != rawWork) log->warn(QString::fromLatin1(kTag),
            QStringLiteral("work_minutes=%1 out of range [%2..%3], clamped to %4")
                .arg(rawWork).arg(PomodoroTimer::kMinWorkMinutes)
                .arg(PomodoroTimer::kMaxWorkMinutes).arg(clampedWork));
        if (clampedBreak != rawBreak) log->warn(QString::fromLatin1(kTag),
            QStringLiteral("break_minutes=%1 out of range [%2..%3], clamped to %4")
                .arg(rawBreak).arg(PomodoroTimer::kMinBreakMinutes)
                .arg(PomodoroTimer::kMaxBreakMinutes).arg(clampedBreak));
        if (clampedPostp != rawPostp) log->warn(QString::fromLatin1(kTag),
            QStringLiteral("max_postpones=%1 out of range [%2..%3], clamped to %4")
                .arg(rawPostp).arg(PomodoroTimer::kMinPostpones)
                .arg(PomodoroTimer::kMaxPostpones).arg(clampedPostp));
        if (clampedTarget != rawTarget) log->warn(QString::fromLatin1(kTag),
            QStringLiteral("target_rounds=%1 out of range [%2..%3], clamped to %4")
                .arg(rawTarget).arg(PomodoroTimer::kMinTargetRounds)
                .arg(PomodoroTimer::kMaxTargetRounds).arg(clampedTarget));
    }
}

QList<TrayMenuContributor::TrayItem> RhythmPlugin::contributeTrayItems() {
    QList<TrayMenuContributor::TrayItem> items;
    // toggle: "X: ON" / "X: OFF" format (docs/06 §4.8).
    TrayMenuContributor::TrayItem toggle;
    toggle.id        = kToggleId;
    const bool paused = m_timer.paused();
    // PR3 i18n: translate both branches so the tray label flips with the
    // active catalog. SystemTray::retranslate re-pulls items on language
    // change so this is evaluated again under the new translator.
    toggle.label     = (paused
        ? QCoreApplication::translate("RhythmPlugin", "Rhythm: OFF")
        : QCoreApplication::translate("RhythmPlugin", "Rhythm: ON")
    ).toStdString();
    toggle.checkable = true;
    toggle.checked   = !paused;
    items.append(toggle);

    // Preview line: only show next-break time when actively working. Other
    // states (Idle / BreakDue / BreakActive) would be misleading or redundant.
    if (m_timer.state() == PomodoroTimer::State::Working) {
        TrayMenuContributor::TrayItem info;
        info.id = "preview_pomodoro";
        const QTime next = QTime::currentTime()
                               .addSecs(m_timer.remainingSeconds());
        info.label = QCoreApplication::translate(
            "RhythmPlugin", "Pomodoro: %1 break")
                .arg(next.toString(QStringLiteral("HH:mm")))
                .toStdString();
        info.read_only = true;
        items.append(info);
    }
    return items;
}

void RhythmPlugin::onTrayItemClicked(const std::string& id) {
    if (id != kToggleId) return;
    // C1: a true pause that preserves the countdown — not stop()/start()
    // (which would reset remaining + restart a fresh work session). Shares the
    // single paused latch with the idle (C2) / Aura (C5) auto-pause sources.
    const bool nowPaused = !m_timer.paused();
    m_timer.setPaused(nowPaused);
    if (m_ctx.host) {
        m_ctx.host->logger().info(
            QString::fromLatin1(kTag),
            nowPaused ? QStringLiteral("Rhythm paused") : QStringLiteral("Rhythm resumed"));
        // Flip "Rhythm: ON" ↔ "Rhythm: OFF".
        m_ctx.host->tray().refreshPluginMenu(QString::fromLatin1(kId));
    }
}

DashboardTabContributor::TabInfo RhythmPlugin::tabInfo() const {
    return TabInfo{
        "rhythm",
        QCoreApplication::translate("RhythmPlugin", "Rhythm").toStdString(),
        QUrl("qrc:/rhythm/icons/rhythm-tab.svg"),
        QUrl("qrc:/rhythm/qml/RhythmTab.qml"),
        30,  // order: Overview=10, Screen Time=20, Rhythm=30, Aura=40
    };
}

SettingsPageContributor::PageInfo RhythmPlugin::pageInfo() const {
    // M5-C4b: id mirrors the dashboard tab. order=40 lands after Aura (30)
    // and before Screen Time (50) in the "plugins" section, matching the
    // dashboard tab order so sidebar + tab strip visually pair.
    PageInfo info;
    info.id = "rhythm";
    info.title = QCoreApplication::translate(
        "RhythmPlugin", "Rhythm").toStdString();
    info.icon = QUrl("qrc:/rhythm/icons/rhythm-tab.svg");
    info.content_qml = QUrl("qrc:/rhythm/qml/RhythmSettingsPage.qml");
    info.order = 40;
    return info;
}

void RhythmPlugin::createBreakOverlayWindow() {
    if (m_overlayWindow) return;
    auto* qml = m_ctx.host ? m_ctx.host->qml() : nullptr;
    auto* engine = qml ? qml->engine() : nullptr;
    if (!engine) return;

    QQmlComponent component(engine, QUrl(QString::fromLatin1(kOverlayQmlUrl)));
    QObject* obj = component.create();
    if (component.isError()) {
        for (const auto& e : component.errors()) {
            if (m_ctx.host) {
                m_ctx.host->logger().warn(
                    QString::fromLatin1(kTag),
                    QStringLiteral("BreakOverlay load error: %1").arg(e.toString()));
            }
        }
        return;
    }
    // BreakOverlay.qml's root is a Window, so the created object is a
    // QQuickWindow. We hold the pointer (QPointer) and toggle visibility
    // from C++ in response to PomodoroTimer lifecycle signals — never
    // via QML bindings — so the trigger logic lives in one place (matching
    // the toast pattern).
    m_overlayWindow = qobject_cast<QQuickWindow*>(obj);
    if (!m_overlayWindow) {
        if (m_ctx.host) {
            m_ctx.host->logger().warn(
                QString::fromLatin1(kTag),
                QStringLiteral("BreakOverlay root is not a QQuickWindow — overlay disabled"));
        }
        delete obj;
        return;
    }
    // Frameless + always-on-top + Tool (no taskbar entry). Window stays
    // invisible until breakStarted — visibility is fully C++-controlled.
    m_overlayWindow->setFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);

    // Drive visibility from the state machine. breakStarted fires on
    // BreakDue → BreakActive; skipped fires on endBreakEarly() (overlay's
    // Esc / 跳过 button). breakEnded is NOT connected here — QML catches
    // the signal via Connections and switches to the "done" card so the
    // user has a 10s buffer (auto-close) instead of the window vanishing.
    // Both done-card close paths call dismissBreakOverlay() which routes
    // back to hideBreakOverlay().
    connect(&m_timer, &PomodoroTimer::breakStarted, this, [this]() {
        showBreakOverlay();
    });
    connect(&m_timer, &PomodoroTimer::skipped, this, [this]() {
        hideBreakOverlay();
    });

    if (m_ctx.host) {
        m_ctx.host->logger().info(
            QString::fromLatin1(kTag),
            QStringLiteral("break overlay window ready (standalone Qt.Tool)"));
    }
}

void RhythmPlugin::showBreakOverlay() {
    if (!m_overlayWindow) return;
    positionOverlayCenter();  // recompute — taskbar / DPI may have moved
    m_overlayWindow->show();
    m_overlayWindow->requestActivate();
    m_overlayVisible = true;
}

void RhythmPlugin::hideBreakOverlay() {
    if (!m_overlayWindow) return;
    m_overlayWindow->hide();
    m_overlayVisible = false;
}

void RhythmPlugin::dismissBreakOverlay() {
    // QML-facing wrapper for the done-card close paths (auto-close
    // countdown + 立即关闭 button). Defers to hideBreakOverlay() so the
    // m_overlayVisible flag stays consistent for the overlayVisible() test
    // seam and any future C++ observers.
    hideBreakOverlay();
}

void RhythmPlugin::positionOverlayCenter() {
    if (!m_overlayWindow) return;
    const auto* screen = QGuiApplication::primaryScreen();
    if (!screen) return;
    const QRect avail = screen->availableGeometry();
    const QSize size  = m_overlayWindow->size();
    // Center on the primary screen's available area (excludes taskbar/Dock).
    m_overlayWindow->setX(avail.center().x() - size.width()  / 2);
    m_overlayWindow->setY(avail.center().y() - size.height() / 2);
}

void RhythmPlugin::createToastWindow() {
    if (m_toastWindow) return;
    auto* qml = m_ctx.host ? m_ctx.host->qml() : nullptr;
    auto* engine = qml ? qml->engine() : nullptr;
    if (!engine) return;

    QQmlComponent component(engine, QUrl(QString::fromLatin1(kToastQmlUrl)));
    QObject* obj = component.create();
    if (component.isError()) {
        for (const auto& e : component.errors()) {
            if (m_ctx.host) {
                m_ctx.host->logger().warn(
                    QString::fromLatin1(kTag),
                    QStringLiteral("RhythmToast load error: %1").arg(e.toString()));
            }
        }
        return;
    }
    // RhythmToast.qml's root is a Window, so the created object is a
    // QQuickWindow. Window stays invisible until breakDue — visibility is
    // controlled from C++ (Window.visible = true) so all trigger logic lives
    // here, not in QML property bindings (per the QML file header comment).
    m_toastWindow = qobject_cast<QQuickWindow*>(obj);
    if (!m_toastWindow) {
        if (m_ctx.host) {
            m_ctx.host->logger().warn(
                QString::fromLatin1(kTag),
                QStringLiteral("RhythmToast root is not a QQuickWindow — toast disabled"));
        }
        delete obj;
        return;
    }

    // Wire PomodoroTimer's lifecycle signals to toast visibility. BreakDue
    // shows the toast; any of {postponed, skipped, breakStarted, breakEnded}
    // hides it. EventBus publish of margin.rhythm.* events is wired alongside
    // — manifest.json declares these in events.publish (per C6 audit) and
    // test_rhythm_manifest_events verifies the set matches the call sites.
    connect(&m_timer, &PomodoroTimer::breakDue, this, [this]() {
        showBreakToast();
        publishEvent(QStringLiteral("margin.rhythm.break_due"));
    });
    connect(&m_timer, &PomodoroTimer::postponed, this, [this]() {
        hideBreakToast();
        publishEvent(QStringLiteral("margin.rhythm.break_dismissed"));
    });
    connect(&m_timer, &PomodoroTimer::skipped, this, [this]() {
        hideBreakToast();
        publishEvent(QStringLiteral("margin.rhythm.break_dismissed"));
    });
    connect(&m_timer, &PomodoroTimer::breakStarted, this, [this]() {
        hideBreakToast();
        publishEvent(QStringLiteral("margin.rhythm.break_started"));
    });
    connect(&m_timer, &PomodoroTimer::breakEnded, this, [this]() { hideBreakToast(); });

    // M4-C16: refresh tray menu on state transitions so the Pomodoro preview
    // (shown only in Working state) appears/disappears as the user moves
    // through the work → break cycle. Per-second remainingChanged is NOT
    // wired (would rebuild the menu every second).
    if (m_ctx.host) {
        connect(&m_timer, &PomodoroTimer::stateChanged, this, [this]() {
            m_ctx.host->tray().refreshPluginMenu(QString::fromLatin1(kId));
        });
    }

    // Closing the window via OS (Alt+F4, taskbar close) is treated as
    // "user dismissed without action" — fall through to postpone. Postpone
    // is preferred over skip because it keeps the work session alive
    // (preserves the user's productivity intent). The m_programmaticHide
    // guard excludes our own hideBreakToast() calls so a postpone/skip/start
    // doesn't re-enter postponeBreak() and double-decrement the budget (B1).
    connect(m_toastWindow, &QQuickWindow::visibleChanged, this,
        [this](bool visible) {
            if (!visible && !m_programmaticHide &&
                m_timer.state() == PomodoroTimer::State::BreakDue) {
                m_timer.postponeBreak();
            }
        });

    positionToastBottomRight();
}

void RhythmPlugin::showBreakToast() {
    if (!m_toastWindow) return;
    positionToastBottomRight();  // recompute — taskbar / DPI may have moved
    m_toastWindow->show();
    m_toastWindow->requestActivate();
}

void RhythmPlugin::hideBreakToast() {
    if (!m_toastWindow) return;
    // Flag the programmatic hide so the visibleChanged handler doesn't treat
    // it as a user OS-close (which maps to postpone). See m_programmaticHide.
    m_programmaticHide = true;
    m_toastWindow->hide();
    m_programmaticHide = false;
}

void RhythmPlugin::positionToastBottomRight() {
    if (!m_toastWindow) return;
    const auto* screen = QGuiApplication::primaryScreen();
    if (!screen) return;
    const QRect avail = screen->availableGeometry();
    const QSize size  = m_toastWindow->size();
    const int margin  = 16;  // px from taskbar / screen edge
    const int x = avail.right()  - size.width()  - margin;
    const int y = avail.bottom() - size.height() - margin;
    m_toastWindow->setX(x);
    m_toastWindow->setY(y);
}

void RhythmPlugin::publishEvent(const QString& topic) {
    if (!m_ctx.host) return;
    QJsonObject p;
    p["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    m_ctx.host->eventBus().publish(topic, p);
}

} // namespace Margin::Plugins::Rhythm
