// ActiveWindowTrackerWin impl — see host/platform/windows/ActiveWindowTrackerWin.h.

#include "ActiveWindowTrackerWin.h"

#include <QByteArray>
#include <QString>

#include <algorithm>
#include <atomic>
#include <cstring>

namespace Margin {

namespace {

// Process-static pointer to the single active tracker. Set on
// startMonitoring, cleared on stopMonitoring / dtor. SetWinEventHook's
// callback signature has no user-data slot, so this is the only way to
// route from the static C callback to a C++ instance.
//
// Atomic because the callback fires on the OS message thread (which IS
// the Qt main thread under OUT_OF_CONTEXT, but defensive concurrency is
// cheap).
std::atomic<ActiveWindowTrackerWin*> g_activeInstance{nullptr};

// Returns the basename of a Windows path, e.g. "C:\...\chrome.exe" → "chrome.exe".
// Windows paths use backslash; some processes report forward slashes via
// QueryFullProcessImageNameW so we handle both.
QString basenameOf(const QString& fullPath) {
    int slashIdx = fullPath.lastIndexOf(QLatin1Char('\\'));
    const int fwdIdx = fullPath.lastIndexOf(QLatin1Char('/'));
    slashIdx = std::max(slashIdx, fwdIdx);
    if (slashIdx < 0) return fullPath;
    return fullPath.mid(slashIdx + 1);
}

} // namespace

ActiveWindowTrackerWin::ActiveWindowTrackerWin() = default;

ActiveWindowTrackerWin::~ActiveWindowTrackerWin() {
    stopMonitoring();
}

bool ActiveWindowTrackerWin::startMonitoring() {
    if (m_hook != nullptr) return true;

    // Mark ourselves as the active instance BEFORE installing the hook so
    // any race-ordered callback finds a valid pointer.
    g_activeInstance.store(this);

    m_hook = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
        nullptr,                    // local module — no DLL injection
        &ActiveWindowTrackerWin::winEventProc,
        0,                          // all processes
        0,                          // all threads
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    if (m_hook == nullptr) {
        // Hook install failed — clear the instance pointer so a stale
        // value doesn't survive into the next start attempt.
        g_activeInstance.store(nullptr);
        return false;
    }
    return true;
}

void ActiveWindowTrackerWin::stopMonitoring() {
    if (m_hook != nullptr) {
        UnhookWinEvent(m_hook);
        m_hook = nullptr;
    }
    g_activeInstance.store(nullptr);
}

bool ActiveWindowTrackerWin::isActive() const {
    return m_hook != nullptr;
}

void CALLBACK ActiveWindowTrackerWin::winEventProc(
    HWINEVENTHOOK /*hook*/,
    DWORD event,
    HWND hwnd,
    LONG idObject,
    LONG /*idChild*/,
    DWORD /*dwEventThread*/,
    DWORD /*dwmsEventTime*/) {

    // Filter to the foreground event on the window itself (idObject ==
    // OBJID_WINDOW). Other idObject values fire for sub-objects (menus,
    // scrollbars) and would emit spurious signals.
    if (event != EVENT_SYSTEM_FOREGROUND) return;
    if (idObject != OBJID_WINDOW) return;

    auto* self = g_activeInstance.load();
    if (self == nullptr) return;

    const ForegroundInfo info = resolveForeground(hwnd);
    emit self->activeWindowChanged(info.pid, info.processName,
                                   info.processPath, info.windowTitle);
}

ActiveWindowTrackerWin::ForegroundInfo
ActiveWindowTrackerWin::resolveForeground(HWND hwnd) {
    ForegroundInfo info{0, QString(), QString(), QString()};

    if (hwnd == nullptr) return info;

    // pid via GetWindowThreadProcessId.
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    info.pid = static_cast<qint64>(pid);

    // processName + processPath via QueryFullProcessImageNameW
    // (PROCESS_QUERY_LIMITED_INFORMATION — works without admin token; Vista+).
    // PR3 round-2 #2b: the full path is preserved for AppIconProvider so
    // it can SHGetFileInfoW the per-app icon; basename still feeds the
    // existing processName column + category matcher.
    if (pid != 0) {
        if (HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid)) {
            WCHAR pathBuf[MAX_PATH] = {};
            DWORD pathSize = MAX_PATH;
            if (QueryFullProcessImageNameW(hProc, 0, pathBuf, &pathSize)) {
                const QString fullPath = QString::fromWCharArray(pathBuf);
                info.processPath = fullPath;
                info.processName = basenameOf(fullPath);
            }
            CloseHandle(hProc);
        }
    }

    // windowTitle via GetWindowTextW.
    WCHAR titleBuf[256] = {};
    const int titleLen = GetWindowTextW(hwnd, titleBuf, 256);
    if (titleLen > 0) {
        info.windowTitle = QString::fromWCharArray(titleBuf, titleLen);
    }

    return info;
}

} // namespace Margin
