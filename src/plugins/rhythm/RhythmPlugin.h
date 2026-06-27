// RhythmPlugin — Pomodoro work/break conductor (M3).
// Spec: docs/04-plugin-spec.md §8.4 + docs/11-roadmap.md M3 + M4-C13a.
//
// Composition:
//   PomodoroTimer (state machine) ──breakDue──→ plugin ──showToast (M3-C3)
//                                          └──publish margin.rhythm.break_due
//                                  ──breakStarted──→ plugin ──showBreakOverlay
//                                  ──breakEnded/skipped──→ hideBreakOverlay
//
// M4-C13a: BreakOverlay is no longer an OverlayContributor. It is now a
// standalone Qt.Tool top-level Window shown/hidden imperatively from C++
// in response to PomodoroTimer lifecycle signals, mirroring the toast
// window pattern. The old OverlayContributor / OverlayContainer.qml path
// stays in host API for future fullscreen overlays but rhythm no longer
// participates.
//
// Q_PROPERTY surface is the timer itself, registered as the `rhythm` context
// property. The plugin class owns: lifecycle (onLoad/onConfigChange/
// onUnload), tray menu items, DashboardTabContributor info, EventBus
// subscriptions (Aura away/back), and the break overlay + toast windows.

#pragma once

#include "PomodoroTimer.h"

#include "Margin/DashboardTabContributor.h"
#include "Margin/PluginContext.h"
#include "Margin/PluginInterface.h"
#include "Margin/Result.h"
#include "Margin/SettingsPageContributor.h"
#include "Margin/TrayMenuContributor.h"

#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>

QT_BEGIN_NAMESPACE
class QQuickWindow;
QT_END_NAMESPACE

namespace Margin::Plugins::Rhythm {

class RhythmPlugin : public QObject,
                    public PluginInterface,
                    public TrayMenuContributor,
                    public DashboardTabContributor,
                    public SettingsPageContributor {
    Q_OBJECT

public:
    RhythmPlugin() = default;

    std::string id() const override;
    std::string version() const override;
    Result<void, std::string> onLoad(const PluginContext& ctx) override;
    void onConfigChange(const QJsonObject& cfg) override;
    void onUnload() override;

    // TrayMenuContributor
    TrayMenuContributor* asTrayMenu() override { return this; }
    QList<TrayItem> contributeTrayItems() override;
    void onTrayItemClicked(const std::string& id) override;

    // DashboardTabContributor
    DashboardTabContributor* asDashboardTab() override { return this; }
    TabInfo tabInfo() const override;

    // SettingsPageContributor (M5-C4b)
    SettingsPageContributor* asSettingsPage() override { return this; }
    PageInfo pageInfo() const override;

    // Test seam — exposes the internal timer for unit tests that load the
    // plugin manually (e.g. through the integration test harness).
    PomodoroTimer* timer() { return &m_timer; }
    // M4-C13a test seam — tracks visibility via a flag instead of
    // QWindow::isVisible() (which is unreliable on offscreen CI).
    bool overlayVisible() const { return m_overlayVisible; }

    // QML-invokable: hide the break overlay from the done-card auto-close
    // + close button. Exposed publicly so QML can dismiss the window
    // without touching the C++-controlled visibility flag directly.
    Q_INVOKABLE void dismissBreakOverlay();
    Q_INVOKABLE void restoreBreakOverlay();

private:
    void loadSettings();
    void createToastWindow();
    void showBreakToast();
    void hideBreakToast();
    void positionToastBottomRight();
    // M4-C13a: standalone break-overlay window management. Mirrors the
    // toast window pattern — lazily created in onLoad, kept hidden until
    // breakStarted, shown centered on the primary screen.
    void createBreakOverlayWindow();
    void showBreakOverlay();
    void hideBreakOverlay();
    void positionOverlayCenter();
    /// Publish a margin.rhythm.* event to the EventBus. Null-safe: no-op if
    /// HostServices is unavailable (matches the C3 toast pattern). Called
    /// from PomodoroTimer signal handlers — never from outside the main thread.
    void publishEvent(const QString& topic);

    PluginContext m_ctx;
    PomodoroTimer m_timer;

    // M3-C3: top-level toast window. Created lazily in onLoad (we need the
    // QQmlEngine from QmlService) and kept hidden until breakDue fires. Uses
    // QPointer because QML owns the QObject lifetime once Window.visible=true
    // is set from C++ side (we never delete it explicitly; the engine tears
    // down on shutdown).
    QPointer<QQuickWindow> m_toastWindow;

    // B1: guards the toast's visibleChanged handler so a programmatic hide()
    // isn't mistaken for a user OS-close (Alt+F4 / taskbar), which maps to
    // postpone. Without it, hideBreakToast() during postponeBreak() re-enters
    // postponeBreak() and double-decrements the budget.
    bool m_programmaticHide = false;

    // M4-C13a: standalone break overlay window. Same lifecycle shape as
    // m_toastWindow — created in onLoad, C++ controls visibility. The card
    // visual lives in BreakOverlay.qml (root is a Window, not Item).
    QPointer<QQuickWindow> m_overlayWindow;
    // Mirrors m_overlayWindow->isVisible() but reliable on offscreen CI.
    // Set in showBreakOverlay/hideBreakOverlay; read by overlayVisible().
    bool m_overlayVisible = false;

    // M3-C5: single-shot delay before resuming after Aura "back" so a brief
    // walk-by doesn't instantly unpause the countdown (DoD #3 "回座 60s 后恢复").
    QTimer m_auraResumeTimer;
};

} // namespace Margin::Plugins::Rhythm
