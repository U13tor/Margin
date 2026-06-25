// WindowMonitorService — passive foreground-window listener exposed to
// plugins. Spec: docs/08-platform-backends.md §3.4.
//
// Implementations listen for OS foreground-window change events (Windows
// SetWinEventHook(EVENT_SYSTEM_FOREGROUND) / macOS NSWorkspace notifications)
// and emit activeWindowChanged on every switch. NO POLLING — the OS pushes
// the event; we don't query GetForegroundWindow on a timer.
//
// Impl lives in host/platform/windows/ActiveWindowTrackerWin (Win). macOS
// deferred to v1.1 alongside the BLE §A15-equivalent — see docs/12 §A19.
// Plugins obtain the service via HostServices::windowMonitor(); nullptr on
// platforms without a backend or until startMonitoring() succeeds.

#pragma once

#include <QObject>
#include <QString>

#include <cstdint>

namespace Margin {

class WindowMonitorService : public QObject {
    Q_OBJECT

public:
    ~WindowMonitorService() override = default;

    /// Install the OS hook. Returns false if hook installation fails
    /// (e.g. desktop locked down, OS API unavailable). Idempotent.
    virtual bool startMonitoring() = 0;

    /// Remove the hook. Safe to call when not monitoring.
    virtual void stopMonitoring() = 0;

    /// True iff startMonitoring() succeeded and stopMonitoring() has not
    /// been called since.
    virtual bool isActive() const = 0;

signals:
    /// Emitted on every foreground-window change. processName is the
    /// image name (e.g. "chrome.exe"); processPath is the full executable
    /// path (e.g. "C:\Program Files\...\chrome.exe") — used by AppIconProvider
    /// to resolve per-app icons (PR3 round-2 #2b/4a). Mac backend (when it
    /// lands per §A19) will pass empty string until a counterpart extractor
    /// exists. windowTitle is the visible window text at the moment of the
    /// switch. Any field may be empty if the OS couldn't resolve it at
    /// callback time.
    void activeWindowChanged(qint64 pid,
                             const QString& processName,
                             const QString& processPath,
                             const QString& windowTitle);
};

} // namespace Margin
