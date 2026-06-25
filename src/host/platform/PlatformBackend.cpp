// PlatformBackend factory — picks the concrete impl by platform.
// M1 ships Windows only (user decision: Windows 先行). macOS impl lands
// alongside §A15 (CoreBluetooth BLE backend); Linux is v1.1+.

#include "PlatformBackend.h"

#if defined(_WIN32)
#include "windows/PlatformBackendWin.h"
#endif

namespace Margin {

std::unique_ptr<PlatformBackend> PlatformBackend::create() {
#if defined(_WIN32)
    return std::make_unique<PlatformBackendWin>();
#else
    // macOS / Linux: PlatformBackend not yet implemented. HostCore logs
    // a warning; HostServices::lock() returns nullptr to plugins.
    return nullptr;
#endif
}

} // namespace Margin
