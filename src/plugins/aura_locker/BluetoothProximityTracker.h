// BluetoothProximityTracker — passive BLE advertisement watcher.
// Spec: docs/08-platform-backends.md §3.2.
//
// Core decision: NEVER use GATT connections (battery drain + iOS LE
// Privacy breaks paired MAC binding). Just listen to advertisement
// packets passively — each carries RSSI + advertised LocalName. The OS
// resolves IRK to a stable device id after pairing; we read that id
// (Windows: BluetoothAddress as hex string; Mac: CBPeripheral.identifier
// UUID string), we don't implement IRK ourselves.
//
// Cross-platform interface lives in the plugin (02-source-layout.md
// §1.1 line 95). Win impl uses C++/WinRT BluetoothLEAdvertisementWatcher;
// Mac impl lands with §A15 (CoreBluetooth CBCentralManager).
//
// Threading: Win callbacks fire on WinRT MTA thread; Qt signal/slot
// Qt::AutoConnection marshals rssiSampled() emission back to the main
// thread where AuraLockerPlugin's state machine lives.

#pragma once

#include <QObject>
#include <QString>

#include <cstdint>
#include <memory>

namespace Margin::Plugins::Aura {

struct RssiSample {
    QString deviceId;        // OS-resolved stable id (hex MAC on Win, UUID on Mac)
    QString advertisedName;  // LocalName from advertisement; may be empty
    QString identHint;       // Best-effort identity hint for the scan UI:
                             //   "iBeacon"           — Apple 0x004C + magic 02 15
                             //   "Apple"/"Microsoft" — known vendor without iBeacon magic
                             //   "Vendor 0x004C"     — ManufacturerData present but unknown
                             //   "Random MAC"        — no manufacturer data, BLE LE Privacy
                             //   "Public MAC"        — no manufacturer data, stable address
                             //   ""                  — nothing useful to surface
    qint16  rssiDbm  = 0;
    qint64  timestampMs = 0; // since epoch, from QDateTime::currentMSecsSinceEpoch
};

enum class RadioState {
    Unknown,  // adapter not yet queried
    Off,      // BT disabled or no adapter present
    On,       // adapter present and powered
};

class BluetoothProximityTracker : public QObject {
    Q_OBJECT

public:
    /// Factory: returns a concrete impl on supported platforms, nullptr
    /// elsewhere. Mac returns nullptr until §A15 lifts.
    static std::unique_ptr<BluetoothProximityTracker> create();

    /// Start continuous passive advertisement watching. Empty filter
    /// string means no LocalName filter (receive all packets — used by
    /// the scan UI in M1-C6; away-detection uses a filtered start).
    virtual void startMonitoring(const QString& filterLocalName) = 0;

    /// Stop watching. Safe to call when not active.
    virtual void stopMonitoring() = 0;

    virtual bool isActive() const = 0;

    /// Last known adapter state. Unknown until first query; updated by
    /// impl-specific radio state change notifications.
    virtual RadioState radioState() const = 0;

signals:
    /// Fired for each advertisement packet received. Cross-thread via
    /// Qt::QueuedConnection when emitter and receiver differ.
    void rssiSampled(const Margin::Plugins::Aura::RssiSample& sample);

    /// Fired when the BT radio transitions (user toggles BT off / on).
    /// Used by AuraLockerPlugin's safety guard to suppress away-locks
    /// while the radio is off (M1-C7).
    void radioStateChanged(Margin::Plugins::Aura::RadioState state);
};

} // namespace Margin::Plugins::Aura

Q_DECLARE_METATYPE(Margin::Plugins::Aura::RssiSample)
Q_DECLARE_METATYPE(Margin::Plugins::Aura::RadioState)
