// LockService — abstract screen-lock capability exposed to plugins.
// Spec: docs/08-platform-backends.md §3.1 (Win LockWorkStation / Mac
// AppleScript, CGSession deferred).
//
// Impl lives in host/platform/PlatformBackend (host-internal). Plugins
// obtain the LockService via HostServices::lock() — returns nullptr on
// platforms where the platform backend has not landed yet (macOS until
// §A15-equivalent; Linux until v1.1).

#pragma once

namespace Margin {

class LockService {
public:
    virtual ~LockService() = default;

    /// Trigger the platform screen lock. Returns immediately; the lock
    /// itself is asynchronous at the OS level. No-op if isSupported()
    /// is false.
    ///
    /// TODO(abi-0.2): return bool to signal failure. Currently void, so
    /// callers (e.g. AuraLockerPlugin) cannot tell if LockWorkStation()
    /// actually succeeded — failed locks get misreported as success in
    /// the UI trail. See docs/12-deferred-items.md §A18.
    virtual void lockScreen() = 0;

    /// False on platforms where the lock primitive is unavailable
    /// (e.g. macOS AppleScript bridge failed to initialise). True on
    /// Windows where LockWorkStation is a stable user-mode API.
    virtual bool isSupported() const = 0;
};

} // namespace Margin
