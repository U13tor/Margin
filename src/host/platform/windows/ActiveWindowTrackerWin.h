// ActiveWindowTrackerWin — SetWinEventHook passive listener.
// Spec: docs/08-platform-backends.md §3.4.
//
// WINEVENT_OUTOF_CONTEXT: the OS marshals the callback onto the calling
// thread's message queue. Qt's main thread runs a message loop, so the
// callback fires there and emits activeWindowChanged directly — no extra
// thread marshalling needed. WINEVENT_SKIPOWNPROCESS avoids Margin.exe
// emitting events about its own windows.
//
// The callback is a static C function because SetWinEventHook takes a
// raw function pointer, not a closure. Routing back to the instance goes
// through a process-static pointer — there is exactly one tracker per
// process (host owns it), so a singleton is acceptable.

#pragma once

#include "Margin/WindowMonitorService.h"

// NOMINMAX before Windows.h — prevents the min/max macros from
// polluting std::max / std::min in this translation unit + anyone
// who includes this header. Same idiom as aura_locker's win/ headers.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

namespace Margin {

class ActiveWindowTrackerWin : public WindowMonitorService {
    Q_OBJECT

public:
    ActiveWindowTrackerWin();
    ~ActiveWindowTrackerWin() override;

    ActiveWindowTrackerWin(const ActiveWindowTrackerWin&) = delete;
    ActiveWindowTrackerWin& operator=(const ActiveWindowTrackerWin&) = delete;

    bool startMonitoring() override;
    void stopMonitoring() override;
    bool isActive() const override;

private:
    // Static C callback registered with SetWinEventHook. Routes to the
    // single active instance via g_activeInstance.
    static void CALLBACK winEventProc(HWINEVENTHOOK hook,
                                       DWORD event,
                                       HWND hwnd,
                                       LONG idObject,
                                       LONG idChild,
                                       DWORD dwEventThread,
                                       DWORD dwmsEventTime);

    // Build the {pid, processName, processPath, windowTitle} payload for
    // the given hwnd. Static + pure so the test can call it directly
    // without touching the OS hook machinery.
    struct ForegroundInfo {
        qint64  pid;
        QString processName;
        QString processPath;
        QString windowTitle;
    };
    static ForegroundInfo resolveForeground(HWND hwnd);

    HWINEVENTHOOK m_hook = nullptr;
};

} // namespace Margin
