// IdleDetectorWin — passive user-idle / activity tracker.
// Spec: docs/08-platform-backends.md §3.5.
//
// WH_KEYBOARD_LL + WH_MOUSE_LL low-level hooks: any key / mouse motion
// updates m_lastInputMs = now and (if currently idle) emits the false
// edge + restarts the single-shot timer. The timer fires
// userIdleStateChanged(true) after `idleThresholdMs` of inactivity.
//
// Hooks fire on the thread that installed them via the OS message pump.
// We install from the Qt main thread, which runs a message loop, so
// callbacks land there — Qt AutoConnection marshals signals to the
// Qt main thread regardless.
//
// Singleton pattern matches ActiveWindowTrackerWin: SetWindowsHookExW
// takes a raw function pointer with no user-data slot, so a process-
// static atomic routes the static C callback to the live instance.

#pragma once

#include "Margin/InputMonitorService.h"

#include <QAbstractNativeEventFilter>
#include <QTimer>

#include <atomic>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

namespace Margin {

class IdleDetectorWin : public InputMonitorService, public QAbstractNativeEventFilter {
    Q_OBJECT

public:
    IdleDetectorWin();
    ~IdleDetectorWin() override;

    IdleDetectorWin(const IdleDetectorWin&) = delete;
    IdleDetectorWin& operator=(const IdleDetectorWin&) = delete;

    bool startMonitoring(int idleThresholdMs) override;
    void stopMonitoring() override;
    bool isActive() const override;

    void setIdleThresholdMs(int ms) override;
    int  idleThresholdMs() const override;
    bool isUserIdle() const override;

    // QAbstractNativeEventFilter — installed on QCoreApplication inside
    // startMonitoring. Filters WM_POWERBROADCAST so we emit
    // systemSuspendStateChanged on sleep/resume edges. Returns false for
    // all messages so they continue normal dispatch.
    bool nativeEventFilter(const QByteArray& eventType, void* message,
                           qintptr* result) override;

private:
    // Static C callbacks registered with SetWindowsHookExW. Both route
    // to the live instance via g_activeInstance. ALWAYS forward to
    // CallNextHookEx — blocking the event would turn Margin into a
    // keylogger, which is explicitly not what this product does.
    static LRESULT CALLBACK keyboardProc(int code, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK mouseProc(int code, WPARAM wParam, LPARAM lParam);

    // Single entry point called by both static callbacks. Updates
    // m_lastInputMs and drives the idle state machine. Split out so the
    // callbacks can be separately registered (Windows doesn't allow one
    // hook for both keyboard + mouse).
    void onInputEvent();

    // Restart the single-shot timer with the current threshold. Called
    // on each input event and on threshold changes.
    void restartIdleTimer();

    // Connected to m_idleTimer.timeout; flips to idle + emits edge.
    void onIdleTimerFired();

    static std::atomic<IdleDetectorWin*> g_activeInstance;

    HHOOK    m_keyboardHook = nullptr;
    HHOOK    m_mouseHook    = nullptr;
    QTimer   m_idleTimer;
    int      m_thresholdMs  = 0;
    bool     m_isIdle       = false;
    // True between PBT_APMSUSPEND and PBT_APMRESUMEAUTOMATIC. Lets us
    // coalesce duplicate resume broadcasts and avoid double-emitting.
    bool     m_isSuspended  = false;
    std::atomic<qint64> m_lastInputMs {0};
};

} // namespace Margin
