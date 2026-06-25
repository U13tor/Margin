// InputMonitorService — passive user-idle / activity tracker exposed to
// plugins. Spec: docs/08-platform-backends.md §3.5.
//
// Implementations install a low-level keyboard / mouse hook (Windows
// SetWindowsHookExW with WH_KEYBOARD_LL + WH_MOUSE_LL) that updates an
// internal "last input" timestamp on every event, plus a single-shot
// QTimer that fires after `idleThresholdMs` of no input — emitting
// userIdleStateChanged(true). The next input event emits the false edge.
//
// Edge-triggered: only state transitions fire userIdleStateChanged, so
// plugins don't need to dedupe. userInputDetected fires on every input
// event (off by default; toggle via connect from debug code only — it's
// high-frequency).
//
// ABI note: like WindowMonitorService, this is a QObject so signals
// resolve through the interface pointer directly.
//
// macOS deferred to v1.1 — see docs/12 §A19.

#pragma once

#include <QObject>

namespace Margin {

class InputMonitorService : public QObject {
    Q_OBJECT

public:
    ~InputMonitorService() override = default;

    /// Install the keyboard/mouse hooks + start the idle timer with the
    /// given threshold. Returns false if hook installation fails.
    /// Idempotent; calling again with a new threshold resets the timer.
    virtual bool startMonitoring(int idleThresholdMs) = 0;

    /// Remove hooks + stop the timer. Safe to call when not monitoring.
    virtual void stopMonitoring() = 0;

    /// True iff startMonitoring() succeeded and stopMonitoring() has not
    /// been called since.
    virtual bool isActive() const = 0;

    /// Update the idle threshold. Takes effect immediately: if currently
    /// active, the running single-shot timer is restarted with the new
    /// interval. If currently idle, the next input event clears idle.
    virtual void setIdleThresholdMs(int ms) = 0;

    /// Current idle threshold (ms). 0 until startMonitoring() called.
    virtual int idleThresholdMs() const = 0;

    /// True if no user input has been observed for `idleThresholdMs`.
    /// Useful for plugins to query state on-demand (e.g. on a window
    /// change that happens during idle).
    virtual bool isUserIdle() const = 0;

signals:
    /// Edge-triggered idle state transitions. Fires on the idle→active
    /// and active→idle edges only; subscribers don't dedupe.
    void userIdleStateChanged(bool idle);

    /// Raw input event notification (any keyboard or mouse activity).
    /// High-frequency — only connect from debug code. Used by tests
    /// to assert the hook is firing.
    void userInputDetected();
};

} // namespace Margin
