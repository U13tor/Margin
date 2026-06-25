// PlatformBackendWin — Windows impl of PlatformBackend.
// M1 ships lockScreen() via LockWorkStation (user-mode, no admin).
// M2-C1 adds ActiveWindowTrackerWin (SetWinEventHook passive listener).
// M2-C4 adds IdleDetectorWin (WH_KEYBOARD_LL + WH_MOUSE_LL + idle QTimer).
// M4-C10 adds applyDarkTitleBar (DwmSetWindowAttribute immersive dark).
// Spec: docs/08-platform-backends.md §3.1 + §3.4 + §3.5 + §3.6.

#pragma once

#include "host/platform/PlatformBackend.h"
#include "host/platform/windows/ActiveWindowTrackerWin.h"
#include "host/platform/windows/IdleDetectorWin.h"

#include <memory>

QT_BEGIN_NAMESPACE
class QWindow;
QT_END_NAMESPACE

namespace Margin {

class PlatformBackendWin : public PlatformBackend {
public:
    PlatformBackendWin();

    void lockScreen() override;
    bool isSupported() const override { return true; }
    void applyDarkTitleBar(QWindow* window) override;
    bool isAutoStartEnabled() const override;
    void setAutoStartEnabled(bool enabled) override;

    WindowMonitorService* windowMonitor() override { return m_windowTracker.get(); }
    InputMonitorService*  inputMonitor()  override { return m_idleDetector.get(); }

private:
    // Owned. ActiveWindowTrackerWin / IdleDetectorWin are QObjects; their
    // signals wire directly to plugin slots via HostServices → plugin connect().
    std::unique_ptr<ActiveWindowTrackerWin> m_windowTracker;
    std::unique_ptr<IdleDetectorWin>        m_idleDetector;
};

} // namespace Margin
