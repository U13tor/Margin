// BluetoothProximityTracker factory — picks concrete impl by platform.
// Win: BluetoothProximityTrackerWin (C++/WinRT). Mac: nullptr until §A15.

#include "BluetoothProximityTracker.h"

#if defined(_WIN32)
#include "win/BluetoothProximityTrackerWin.h"
#endif

namespace Margin::Plugins::Aura {

std::unique_ptr<BluetoothProximityTracker> BluetoothProximityTracker::create() {
#if defined(_WIN32)
    return std::make_unique<BluetoothProximityTrackerWin>();
#else
    // macOS / Linux: BLE backend deferred. AuraLockerPlugin null-checks
    // the returned unique_ptr and shows a "BLE unavailable" message in
    // the UI (commit 5).
    return nullptr;
#endif
}

} // namespace Margin::Plugins::Aura
