// BluetoothProximityTrackerWin — C++/WinRT impl.
// Spec: docs/08-platform-backends.md §3.2.
//
// Wraps Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementWatcher
// in Active scanning mode (sends SCAN_REQ to get SCAN_RSP with device
// name + more frequent samples). Filter by LocalName when supplied —
// more stable than MAC filtering because iOS LE Privacy rotates MAC
// every ~15 minutes.

#pragma once

#include "../BluetoothProximityTracker.h"

#include <winrt/windows.devices.bluetooth.advertisement.h>

#include <QObject>
#include <QString>

namespace Margin::Plugins::Aura {

class BluetoothProximityTrackerWin : public BluetoothProximityTracker {
    Q_OBJECT

public:
    BluetoothProximityTrackerWin();
    ~BluetoothProximityTrackerWin() override;

    void startMonitoring(const QString& filterLocalName) override;
    void stopMonitoring() override;
    bool isActive() const override { return m_active; }
    RadioState radioState() const override { return m_radioState; }

private:
    void onReceived(
        const winrt::Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementWatcher&,
        const winrt::Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementReceivedEventArgs& args);

    void onStopped(
        const winrt::Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementWatcher&,
        const winrt::Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementWatcherStoppedEventArgs& args);

    void setRadioState(RadioState state);

    // Build a human-readable identity hint for the scan UI.
    // Inspects ManufacturerData first (iBeacon magic wins), then the BLE
    // address type. Returns "" when nothing useful is available — caller
    // lets the QML fall back to "(unnamed)".
    static QString buildIdentHint(
        const winrt::Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementReceivedEventArgs& args);

    using Watcher = winrt::Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementWatcher;
    Watcher m_watcher{ nullptr };

    RadioState m_radioState = RadioState::Unknown;
    bool m_active = false;
};

} // namespace Margin::Plugins::Aura
