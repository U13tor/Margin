// BluetoothProximityTrackerWin impl — see header.

#include "BluetoothProximityTrackerWin.h"

#include <winrt/windows.foundation.collections.h>
#include <winrt/windows.storage.streams.h>
#include <winrt/base.h>

#include <QByteArray>
#include <QDateTime>
#include <QString>

namespace Margin::Plugins::Aura {

namespace winrt_ble = winrt::Windows::Devices::Bluetooth::Advertisement;

// Bluetooth SIG-assigned Company Identifiers (a subset — full registry at
// bluetooth.com). Picked the ones most likely to show up in a desk-side
// BLE scan: phones, headphones, wearables, input devices, beacons.
// https://bitbucket.org/bluetooth-SIG/public/src/main/assigned_numbers/company_identifiers/company_identifiers.yaml
QString companyFromId(uint16_t id) {
    switch (id) {
        case 0x004C: return QStringLiteral("Apple");
        case 0x0006: return QStringLiteral("Microsoft");
        case 0x000E: return QStringLiteral("Google");
        case 0x0075: return QStringLiteral("Samsung");
        case 0x0591: return QStringLiteral("Xiaomi");
        case 0x000F: return QStringLiteral("Qualcomm");
        case 0x0087: return QStringLiteral("Garmin");
        case 0x013A: return QStringLiteral("Logitech");
        case 0x0059: return QStringLiteral("Nordic Semiconductor");
        case 0x0D00: return QStringLiteral("Fitbit");
        case 0x0001: return QStringLiteral("Ericsson");
        case 0x0024: return QStringLiteral("Sony");
        case 0x04C9: return QStringLiteral("Huawei");
        default:     return {};
    }
}

// Read raw bytes from a WinRT IBuffer into a QByteArray via DataReader.
// IBuffer doesn't expose its bytes directly; DataReader::FromBuffer wraps
// it with a byte stream we can pull from synchronously.
QByteArray manufacturerPayload(const winrt_ble::BluetoothLEManufacturerData& md) {
    QByteArray out;
    auto buf = md.Data();
    if (!buf) return out;
    const uint32_t len = buf.Length();
    out.resize(static_cast<int>(len));
    using namespace winrt::Windows::Storage::Streams;
    DataReader reader{ DataReader::FromBuffer(buf) };
    reader.ReadBytes(winrt::array_view<uint8_t>(
        reinterpret_cast<uint8_t*>(out.data()),
        reinterpret_cast<uint8_t*>(out.data()) + len));
    return out;
}

// Apple's iBeacon prefix: CompanyId 0x004C followed by the magic bytes
// 0x02 (SubType) and 0x15 (SubTypeDataLength = 21 = 16 UUID + 2 major +
// 2 minor + 1 txPower). Ref: Apple "Getting Started with iBeacon" (2014).
bool isIBeaconPayload(uint16_t companyId, const QByteArray& payload) {
    if (companyId != 0x004C) return false;
    if (payload.size() < 2) return false;
    const auto u = reinterpret_cast<const uint8_t*>(payload.data());
    return u[0] == 0x02 && u[1] == 0x15;
}

QString BluetoothProximityTrackerWin::buildIdentHint(
    const winrt_ble::BluetoothLEAdvertisementReceivedEventArgs& args) {
    try {
        // Tier 1: ManufacturerData — iBeacon magic wins over plain vendor.
        // Address-type identification (Random/Public MAC) would also be
        // useful, but cppwinrt's BluetoothLEAdvertisementReceivedEventArgs
        // projection doesn't expose the BluetoothLEAddressType enum in a
        // header we currently include — deferring it until we pull in a
        // fuller Bluetooth LE projection (e.g. when GATT work lands).
        auto md = args.Advertisement().ManufacturerData();
        const uint32_t size = md ? md.Size() : 0;
        if (size > 0) {
            const auto& first = md.GetAt(0);
            const uint16_t cid = first.CompanyId();
            const QByteArray payload = manufacturerPayload(first);
            if (isIBeaconPayload(cid, payload)) {
                return QStringLiteral("iBeacon");
            }
            const QString vendor = companyFromId(cid);
            if (!vendor.isEmpty()) return vendor;
            return QStringLiteral("Vendor 0x%1")
                .arg(cid, 4, 16, QLatin1Char('0')).toUpper();
        }
        // Nothing else to surface — caller falls back to "(unnamed)".
        return {};
    } catch (...) {
        return {};
    }
}

BluetoothProximityTrackerWin::BluetoothProximityTrackerWin() {
    // MTA — WinRT watchers fire callbacks on the WinRT thread pool, not
    // on the Qt main thread. Qt::AutoConnection (default) marshals the
    // emit rssiSampled() back to the receiver's thread.
    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
    } catch (...) {
        // init_apartment can throw if called twice on the same thread with
        // incompatible types; safe to swallow — the second call's apartment
        // type already wins.
    }
}

BluetoothProximityTrackerWin::~BluetoothProximityTrackerWin() {
    stopMonitoring();
}

void BluetoothProximityTrackerWin::startMonitoring(const QString& filterLocalName) {
    if (m_active) return;

    m_watcher = winrt_ble::BluetoothLEAdvertisementWatcher();
    m_watcher.ScanningMode(winrt_ble::BluetoothLEScanningMode::Active);

    if (!filterLocalName.isEmpty()) {
        winrt_ble::BluetoothLEAdvertisementFilter f;
        // winrt::hstring accepts std::wstring_view implicitly; QString's
        // toStdWString() returns std::wstring which converts cleanly.
        f.Advertisement().LocalName(winrt::hstring{ filterLocalName.toStdWString() });
        m_watcher.AdvertisementFilter(f);
    }

    m_watcher.Received({ this, &BluetoothProximityTrackerWin::onReceived });

    // Stopped fires when the OS tears the watcher down — most common cause
    // is the user toggling Bluetooth off. Status == Aborted means the radio
    // is gone; plain Stopped (user-initiated via stopMonitoring) we ignore
    // because we cleared m_watcher first and the token revokes.
    m_watcher.Stopped({ this, &BluetoothProximityTrackerWin::onStopped });

    m_watcher.Start();
    m_active = true;
    setRadioState(RadioState::On);
}

void BluetoothProximityTrackerWin::stopMonitoring() {
    if (!m_active) return;
    m_active = false;  // set first so onStopped knows it was user-initiated
    if (m_watcher) {
        try {
            m_watcher.Stop();
        } catch (...) {
            // Stop is async; failure here is non-fatal — the watcher is
            // being torn down anyway and the unique_ptr release will
            // drop the COM ref count.
        }
        m_watcher = nullptr;
    }
}

void BluetoothProximityTrackerWin::onStopped(
    const winrt_ble::BluetoothLEAdvertisementWatcher&,
    const winrt_ble::BluetoothLEAdvertisementWatcherStoppedEventArgs&) {
    if (!m_active) return;  // user-initiated via stopMonitoring
    // Query the watcher's own Status — the args struct only carries an
    // Error code (BluetoothError enum). Aborted indicates the radio is
    // gone (user toggled BT off, adapter removed, etc.). Plain Stopped
    // happens when the watcher reaches its max advertisement count.
    using Status = winrt_ble::BluetoothLEAdvertisementWatcherStatus;
    const Status status = m_watcher ? m_watcher.Status() : Status::Stopped;
    if (status == Status::Aborted || status == Status::Stopped) {
        m_active = false;
        setRadioState(RadioState::Off);
    }
}

void BluetoothProximityTrackerWin::onReceived(
    const winrt_ble::BluetoothLEAdvertisementWatcher&,
    const winrt_ble::BluetoothLEAdvertisementReceivedEventArgs& args) {

    RssiSample sample;
    // BluetoothAddress is the 64-bit MAC. On iOS this rotates every
    // ~15 min due to LE Privacy; for paired devices Windows resolves
    // IRK back to a stable id at the stack level. We surface the raw
    // BluetoothAddress as hex; the state machine keys on advertisedName
    // when present (more stable for unpaired scan UI).
    sample.deviceId = QStringLiteral("%1")
                          .arg(args.BluetoothAddress(), 12, 16, QLatin1Char('0'))
                          .toUpper();
    auto localName = args.Advertisement().LocalName();
    if (!localName.empty()) {
        sample.advertisedName = QString::fromStdWString(std::wstring(localName));
    }
    sample.identHint = buildIdentHint(args);
    sample.rssiDbm = args.RawSignalStrengthInDBm();
    sample.timestampMs = QDateTime::currentMSecsSinceEpoch();

    emit rssiSampled(sample);
}

void BluetoothProximityTrackerWin::setRadioState(RadioState state) {
    if (m_radioState == state) return;
    m_radioState = state;
    emit radioStateChanged(state);
}

} // namespace Margin::Plugins::Aura
