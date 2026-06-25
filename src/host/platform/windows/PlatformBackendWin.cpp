// PlatformBackendWin impl — see PlatformBackendWin.h.

#include "PlatformBackendWin.h"

#include <windows.h>
#include <dwmapi.h>

#include <QCoreApplication>
#include <QDebug>
#include <QString>
#include <QWindow>

namespace Margin {

namespace {
// HKCU Run key — Windows' standard user-level launch-on-login hook.
// Writing a REG_SZ value here makes the OS invoke the exe at user login;
// removing it undoes the autostart. No admin rights required because HKCU
// is per-user. Value name "Margin" matches the product/binary name; the
// value data is the full exe path wrapped in double quotes to handle
// spaces (e.g. "C:\Program Files\Margin\Margin.exe").
constexpr const wchar_t* kRunKeyName   = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr const wchar_t* kRunValueName = L"Margin";
}  // namespace

PlatformBackendWin::PlatformBackendWin()
    : m_windowTracker(std::make_unique<ActiveWindowTrackerWin>())
    , m_idleDetector(std::make_unique<IdleDetectorWin>()) {}

void PlatformBackendWin::lockScreen() {
    // LockWorkStation is a user-mode API exported by user32.dll. Returns
    // non-zero on success; zero on failure (e.g. workstation already
    // locked, no interactive session). Failure is non-fatal — Aura
    // retries on the next away transition.
    if (!LockWorkStation()) {
        const DWORD err = GetLastError();
        qWarning() << "PlatformBackendWin: LockWorkStation failed, err=" << err;
    }
}

void PlatformBackendWin::applyDarkTitleBar(QWindow* window) {
    if (!window) {
        qWarning() << "PlatformBackendWin: applyDarkTitleBar called with null window";
        return;
    }
    // winId() lazily creates the native handle. Returns 0 only on truly
    // headless platforms (offscreen) — on Win this is a valid HWND once
    // QQmlApplicationEngine::load() has returned.
    const WId wid = window->winId();
    if (!wid) {
        qWarning() << "PlatformBackendWin: applyDarkTitleBar — window has no native handle yet";
        return;
    }
    HWND hwnd = reinterpret_cast<HWND>(wid);

    // DWMWA_USE_IMMERSIVE_DARK_MODE = 20 on Win10 20H1+ (build 19041+).
    // DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1 = 19 on Win10 1809-1909.
    // Try 20 first; if the build doesn't recognise it, fall back to 19.
    // Builds older than 1809 fail both — title bar stays light, no crash.
    constexpr DWORD kAttrModern  = 20;
    constexpr DWORD kAttrPre20H1 = 19;
    BOOL darkFlag = TRUE;

    HRESULT hr = DwmSetWindowAttribute(hwnd, kAttrModern,
                                       &darkFlag, sizeof(darkFlag));
    if (FAILED(hr)) {
        hr = DwmSetWindowAttribute(hwnd, kAttrPre20H1,
                                   &darkFlag, sizeof(darkFlag));
    }

    if (SUCCEEDED(hr)) {
        // Apply custom title bar and border colors on Windows 11 if supported.
        // DWMWA_CAPTION_COLOR = 35, DWMWA_TEXT_COLOR = 36, DWMWA_BORDER_COLOR = 34
        constexpr DWORD kAttrBorderColor = 34;
        constexpr DWORD kAttrCaptionColor = 35;
        constexpr DWORD kAttrTextColor = 36;

        COLORREF captionColor = RGB(0x0E, 0x0E, 0x10); // #0E0E10 (Theme.bgBase)
        COLORREF textColor = RGB(0xE4, 0xE4, 0xE7);    // #E4E4E7 (Theme.fgPrimary)
        COLORREF borderColor = RGB(0x0E, 0x0E, 0x10);  // #0E0E10 (Theme.bgBase)

        DwmSetWindowAttribute(hwnd, kAttrCaptionColor, &captionColor, sizeof(captionColor));
        DwmSetWindowAttribute(hwnd, kAttrTextColor, &textColor, sizeof(textColor));
        DwmSetWindowAttribute(hwnd, kAttrBorderColor, &borderColor, sizeof(borderColor));
    } else {
        qWarning() << "PlatformBackendWin: DwmSetWindowAttribute failed on both attr 20 and 19"
                   << Qt::hex << hr << Qt::dec << "(last hr) — likely Windows build < 1809";
    }
}

bool PlatformBackendWin::isAutoStartEnabled() const {
    // Open HKCU Run key read-only and probe for the "Margin" value.
    // RegQueryValueExW returns ERROR_FILE_NOT_FOUND when the value is
    // absent — that's our "disabled" signal. We don't validate the path
    // stored in the value: if the user moved Margin.exe the registry
    // entry still exists and Windows will try (and fail) to launch it —
    // surfacing that as "enabled" is correct because the next toggle-off
    // cleans it up.
    HKEY hKey = nullptr;
    LSTATUS ok = RegOpenKeyExW(HKEY_CURRENT_USER, kRunKeyName, 0,
                               KEY_QUERY_VALUE, &hKey);
    if (ok != ERROR_SUCCESS) return false;

    DWORD type = 0, size = 0;
    ok = RegQueryValueExW(hKey, kRunValueName, nullptr, &type, nullptr, &size);
    RegCloseKey(hKey);
    return ok == ERROR_SUCCESS && type == REG_SZ;
}

void PlatformBackendWin::setAutoStartEnabled(bool enabled) {
    // QCoreApplication::applicationFilePath() returns the running exe's
    // full path — Margin.exe in production, test_autostart_win.exe in
    // tests. Wrap in double quotes so paths containing spaces survive
    // the OS's command-line parse at login.
    const QString exePath = QCoreApplication::applicationFilePath();
    const std::wstring value = L"\"" + exePath.toStdWString() + L"\"";

    HKEY hKey = nullptr;
    // RegCreateKeyExW opens existing or creates new — the Run key always
    // exists on a normal Windows install, but create is safer than open
    // for the write path.
    LSTATUS ok = RegCreateKeyExW(HKEY_CURRENT_USER, kRunKeyName, 0, nullptr,
                                 0, KEY_SET_VALUE, nullptr, &hKey, nullptr);
    if (ok != ERROR_SUCCESS) {
        qWarning() << "PlatformBackendWin: autostart RegCreateKeyExW failed, err=" << ok;
        return;
    }

    if (enabled) {
        const BYTE* data = reinterpret_cast<const BYTE*>(value.c_str());
        const DWORD bytes = static_cast<DWORD>(value.size() * sizeof(wchar_t));
        ok = RegSetValueExW(hKey, kRunValueName, 0, REG_SZ, data, bytes);
    } else {
        ok = RegDeleteValueW(hKey, kRunValueName);
        // Already absent = desired state; treat as success.
        if (ok == ERROR_FILE_NOT_FOUND) ok = ERROR_SUCCESS;
    }
    RegCloseKey(hKey);

    if (ok != ERROR_SUCCESS) {
        qWarning() << "PlatformBackendWin: autostart registry op failed, err=" << ok;
    }
}

} // namespace Margin
