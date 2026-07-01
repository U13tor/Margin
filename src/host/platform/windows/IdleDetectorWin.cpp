// IdleDetectorWin impl — see host/platform/windows/IdleDetectorWin.h.

#include "IdleDetectorWin.h"

#include <QCoreApplication>
#include <QDateTime>

namespace Margin {

std::atomic<IdleDetectorWin*> IdleDetectorWin::g_activeInstance{nullptr};

IdleDetectorWin::IdleDetectorWin() {
    m_idleTimer.setSingleShot(true);
    connect(&m_idleTimer, &QTimer::timeout, this, &IdleDetectorWin::onIdleTimerFired);
}

IdleDetectorWin::~IdleDetectorWin() {
    stopMonitoring();
}

bool IdleDetectorWin::startMonitoring(int idleThresholdMs) {
    if (m_keyboardHook != nullptr && m_mouseHook != nullptr) {
        // Already running — just update the threshold + reset timer.
        setIdleThresholdMs(idleThresholdMs);
        return true;
    }

    if (idleThresholdMs <= 0) return false;

    g_activeInstance.store(this);
    m_thresholdMs = idleThresholdMs;
    m_lastInputMs.store(QDateTime::currentMSecsSinceEpoch());
    m_isIdle = false;
    m_isSuspended = false;

    // HMODULE must be non-NULL when the callback is in a DLL — but for
    // an EXE-hosted callback, GetModuleHandle(nullptr) returns the host
    // EXE module, which is what WH_KEYBOARD_LL requires.
    const HINSTANCE hInst = GetModuleHandleW(nullptr);
    m_keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, &IdleDetectorWin::keyboardProc, hInst, 0);
    if (m_keyboardHook == nullptr) {
        g_activeInstance.store(nullptr);
        return false;
    }
    m_mouseHook = SetWindowsHookExW(WH_MOUSE_LL, &IdleDetectorWin::mouseProc, hInst, 0);
    if (m_mouseHook == nullptr) {
        UnhookWindowsHookEx(m_keyboardHook);
        m_keyboardHook = nullptr;
        g_activeInstance.store(nullptr);
        return false;
    }

    // Native event filter is process-global; install once and keep for
    // the lifetime of this singleton (PlatformBackendWin owns the only
    // instance, which itself lives as long as HostCore). Installed here
    // rather than in the ctor so QCoreApplication is guaranteed to exist.
    // Cast is unambiguous via the QAbstractNativeEventFilter base — both
    // base classes of IdleDetectorWin are non-QObject siblings under the
    // QObject grandparent (InputMonitorService derives QObject), so the
    // static_cast to the filter base is well-formed.
    if (auto* app = QCoreApplication::instance()) {
        app->installNativeEventFilter(
            static_cast<QAbstractNativeEventFilter*>(this));
    }

    restartIdleTimer();
    return true;
}

void IdleDetectorWin::stopMonitoring() {
    if (m_keyboardHook != nullptr) {
        UnhookWindowsHookEx(m_keyboardHook);
        m_keyboardHook = nullptr;
    }
    if (m_mouseHook != nullptr) {
        UnhookWindowsHookEx(m_mouseHook);
        m_mouseHook = nullptr;
    }
    if (auto* app = QCoreApplication::instance()) {
        app->removeNativeEventFilter(
            static_cast<QAbstractNativeEventFilter*>(this));
    }
    m_idleTimer.stop();
    m_isIdle = false;
    m_isSuspended = false;
    g_activeInstance.store(nullptr);
}

bool IdleDetectorWin::nativeEventFilter(const QByteArray& eventType, void* message,
                                        qintptr* /*result*/) {
    // Qt dispatches Windows messages as eventType "windows_generic_MSG"
    // with message pointing to a native MSG struct. Anything else (e.g.
    // "windows_dispatcher") is passed through untouched.
    if (eventType != "windows_generic_MSG" || message == nullptr) {
        return false;
    }
    const MSG* msg = static_cast<const MSG*>(message);
    if (msg->message != WM_POWERBROADCAST) {
        return false;
    }
    // PBT_APMSUSPEND: OS is entering S3/S4. PBT_APMRESUMEAUTOMATIC:
    // OS resumed (PBT_APMRESUMESUSPEND fires only after a user-triggered
    // resume event like keyboard — we want the automatic edge so a
    // network-triggered wake from sleep also clears the flag). Both can
    // broadcast multiple times per physical event — m_isSuspended
    // dedupes the edge so plugins see one transition each way.
    switch (msg->wParam) {
    case PBT_APMSUSPEND:
        if (!m_isSuspended) {
            m_isSuspended = true;
            emit systemSuspendStateChanged(true);
        }
        break;
    case PBT_APMRESUMEAUTOMATIC:
        if (m_isSuspended) {
            m_isSuspended = false;
            emit systemSuspendStateChanged(false);
        }
        break;
    default:
        break;
    }
    return false;
}

bool IdleDetectorWin::isActive() const {
    return m_keyboardHook != nullptr && m_mouseHook != nullptr;
}

void IdleDetectorWin::setIdleThresholdMs(int ms) {
    if (ms <= 0) return;
    m_thresholdMs = ms;
    if (isActive()) restartIdleTimer();
}

int IdleDetectorWin::idleThresholdMs() const {
    return m_thresholdMs;
}

bool IdleDetectorWin::isUserIdle() const {
    return m_isIdle;
}

LRESULT CALLBACK IdleDetectorWin::keyboardProc(int code, WPARAM wParam, LPARAM lParam) {
    // HC_ACTION means wParam/lParam carry the actual event. Other codes
    // (HC_NOREMOVE etc.) are pass-through bookkeeping.
    if (code == HC_ACTION) {
        // wParam is the key message (WM_KEYDOWN, WM_SYSKEYDOWN, WM_KEYUP,
        // WM_SYSKEYUP). We treat any of them as "user is active" — the
        // direction doesn't matter.
        if (auto* self = g_activeInstance.load()) self->onInputEvent();
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

LRESULT CALLBACK IdleDetectorWin::mouseProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION) {
        // Any mouse message (move, button, wheel) counts. LL hooks fire
        // VERY frequently on mouse move — we accept the cost; the
        // callback body is O(1).
        if (auto* self = g_activeInstance.load()) self->onInputEvent();
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

void IdleDetectorWin::onInputEvent() {
    m_lastInputMs.store(QDateTime::currentMSecsSinceEpoch());
    if (m_isIdle) {
        m_isIdle = false;
        emit userIdleStateChanged(false);
    }
    emit userInputDetected();
    restartIdleTimer();
}

void IdleDetectorWin::restartIdleTimer() {
    if (m_thresholdMs > 0 && isActive()) {
        m_idleTimer.start(m_thresholdMs);
    }
}

void IdleDetectorWin::onIdleTimerFired() {
    if (m_isIdle) return;  // already idle; timer shouldn't be running but be safe
    m_isIdle = true;
    emit userIdleStateChanged(true);
}

} // namespace Margin
