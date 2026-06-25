// TrayService — host-side system tray abstraction. Verbatim from
// docs/05-host-services.md §6. Impl: host/core/SystemTray.cpp.

#pragma once

#include <QString>

namespace Margin {

class TrayService {
public:
    /// Tray icon state. Drives which MSG-00x SVG is shown.
    /// Normal = idle; Locked = Aura lock-screen active; Stretching = Rhythm stretch active.
    enum class IconState { Normal, Locked, Stretching };

    /// Show a toast notification (OS-native balloon / notification center).
    virtual void showToast(const QString& title,
                           const QString& message,
                           int timeoutMs = 5000) = 0;

    /// Swap between the three tray SVGs (MSG-001 / MSG-002 / MSG-003).
    virtual void setIconState(IconState state) = 0;

    /// Update the tray tooltip.
    virtual void setTooltip(const QString& text) = 0;

    /// Rebuild the menu items contributed by `pluginId`. Plugins call this
    /// after their state changes so dynamic labels (e.g. "Aura: ON" ↔
    /// "Aura: OFF") and read-only previews reflect current state.
    /// M4-C16 — see docs/05-host-services.md §6.
    virtual void refreshPluginMenu(const QString& pluginId) = 0;

    virtual ~TrayService() = default;
};

} // namespace Margin
