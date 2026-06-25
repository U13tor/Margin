// PlatformBackend — host-internal abstraction over platform-specific
// OS primitives (lock screen, active-window monitor, input hook).
//
// Composition: PlatformBackend owns LockService capability via IS-A
// (inherits LockService so HostServicesImpl can hold a single pointer)
// and WindowMonitorService / InputMonitorService via HAS-A (returns the
// tracker pointers via windowMonitor() / inputMonitor()). HAS-A is
// preferred for new services because it keeps each tracker as its own
// QObject with its own signal/slot wiring, avoiding the multiple-
// inheritance-from-QObject smell.
//
// Spec: docs/02-source-layout.md §1.1 (host/platform/{windows,macos}/)
// + docs/08-platform-backends.md §4.

#pragma once

#include "Margin/LockService.h"
#include "Margin/WindowMonitorService.h"
#include "Margin/InputMonitorService.h"

#include <memory>

QT_BEGIN_NAMESPACE
class QWindow;
QT_END_NAMESPACE

namespace Margin {

class PlatformBackend : public LockService {
public:
    ~PlatformBackend() override = default;

    // LockService overrides — concrete in PlatformBackendWin / Mac.
    void lockScreen() override = 0;
    bool isSupported() const override = 0;

    // WindowMonitorService accessor — null on platforms without a backend
    // (macOS until v1.1 §A19). Lifetime bound to PlatformBackend.
    virtual WindowMonitorService* windowMonitor() = 0;

    // InputMonitorService accessor — same lifetime / nullability story
    // as windowMonitor().
    virtual InputMonitorService* inputMonitor() = 0;

    // Apply the OS-native dark window chrome to the given QWindow.
    // Win: DwmSetWindowAttribute(DWMWA_USE_IMMERSIVE_DARK_MODE=20) with
    // pre-20H1 fallback (attr 19). Mac: deferred — see docs/12 §A15.
    // Called by HostCore::bootstrap after engine->load() but before show().
    // Defensive: no-op on nullptr or window without a native handle yet.
    virtual void applyDarkTitleBar(QWindow* window) = 0;

    // Launch Margin on user login. Win: writes/removes the value under
    // HKCU\Software\Microsoft\Windows\CurrentVersion\Run (user-level, no
    // UAC). Mac/Linux: deferred — HostCore guards calls so a missing impl
    // here surfaces as a no-op on those platforms.
    virtual bool isAutoStartEnabled() const = 0;
    virtual void setAutoStartEnabled(bool enabled) = 0;

    // Factory: returns a concrete impl on supported platforms, nullptr
    // elsewhere. HostCore owns the unique_ptr; passes the raw pointer
    // down to HostServicesImpl (as LockService*) via PluginManager.
    static std::unique_ptr<PlatformBackend> create();
};

} // namespace Margin
