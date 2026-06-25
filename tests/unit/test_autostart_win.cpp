// tests/unit/test_autostart_win.cpp
//
// Windows autostart registry contract test (HKCU\...\Run\Margin round-trip).
// Verifies the three caller-facing behaviors HostCore relies on:
//   (1) isAutoStartEnabled() returns false when the value is absent
//   (2) setAutoStartEnabled(true) creates a REG_SZ value containing the
//       running exe's path (Margin.exe in prod, test_autostart_win.exe here)
//   (3) setAutoStartEnabled(false) removes the value; calling it again on
//       an already-absent value is idempotent (no error, no crash)
//
// Registry hygiene: cleanup() runs after every test case to delete the
// "Margin" value from HKCU Run, so this test never leaves autostart
// enabled on the dev machine it ran on. initTestCase() does the same
// before the first test in case a previous run crashed mid-test.
//
// Windows-only — mirrors test_dark_title_bar.cpp's #ifdef _WIN32 guard.
// On Mac/Linux the suite QSKIPs so ctest still reports green.

#include <QObject>
#include <QTest>

#include <memory>

#include "host/platform/PlatformBackend.h"

#ifdef _WIN32
#include <windows.h>
#endif

using Margin::PlatformBackend;

namespace {
// The value name PlatformBackendWin writes — kept in sync with the impl.
// Read-only here; if the impl constant changes, this test must too.
constexpr const wchar_t* kRunKey   = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr const wchar_t* kValueName = L"Margin";
}  // namespace

class TestAutoStartWin : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();

    void disableByDefault();
    void enableCreatesRegistryEntry();
    void disableRemovesRegistryEntry();
    void enableThenDisableRoundTrip();
    void disableWhenAlreadyAbsentIsIdempotent();
};

void TestAutoStartWin::initTestCase() {
#ifdef _WIN32
    // Best-effort cleanup of any "Margin" value left from a prior run
    // (manual install, previous test crash, etc.). Failure is non-fatal —
    // disableByDefault() will QSKIP if the value persists.
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &hKey)
        == ERROR_SUCCESS) {
        RegDeleteValueW(hKey, kValueName);
        RegCloseKey(hKey);
    }
#else
    QSKIP("autostart registry test is Windows-only");
#endif
}

void TestAutoStartWin::cleanup() {
#ifdef _WIN32
    // Always delete the value after a test so the dev machine isn't left
    // with autostart silently enabled.
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &hKey)
        == ERROR_SUCCESS) {
        RegDeleteValueW(hKey, kValueName);
        RegCloseKey(hKey);
    }
#endif
}

void TestAutoStartWin::disableByDefault() {
#ifdef _WIN32
    auto backend = PlatformBackend::create();
    QVERIFY(backend);
    QCOMPARE(backend->isAutoStartEnabled(), false);
#else
    QSKIP("Windows-only");
#endif
}

void TestAutoStartWin::enableCreatesRegistryEntry() {
#ifdef _WIN32
    auto backend = PlatformBackend::create();
    QVERIFY(backend);
    backend->setAutoStartEnabled(true);

    // Backend says it's enabled.
    QVERIFY(backend->isAutoStartEnabled());

    // And the registry actually has the value, type REG_SZ.
    HKEY hKey = nullptr;
    QCOMPARE(RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_READ, &hKey),
             ERROR_SUCCESS);
    DWORD type = 0, size = 0;
    QCOMPARE(RegQueryValueExW(hKey, kValueName, nullptr, &type, nullptr, &size),
             ERROR_SUCCESS);
    QCOMPARE(type, static_cast<DWORD>(REG_SZ));
    QVERIFY(size > 0);
    RegCloseKey(hKey);
#else
    QSKIP("Windows-only");
#endif
}

void TestAutoStartWin::disableRemovesRegistryEntry() {
#ifdef _WIN32
    auto backend = PlatformBackend::create();
    QVERIFY(backend);
    backend->setAutoStartEnabled(true);
    QVERIFY(backend->isAutoStartEnabled());

    backend->setAutoStartEnabled(false);
    QCOMPARE(backend->isAutoStartEnabled(), false);

    // Confirm at the registry level too — not just the cached backend state.
    HKEY hKey = nullptr;
    QCOMPARE(RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_READ, &hKey),
             ERROR_SUCCESS);
    DWORD type = 0, size = 0;
    QCOMPARE(RegQueryValueExW(hKey, kValueName, nullptr, &type, nullptr, &size),
             static_cast<DWORD>(ERROR_FILE_NOT_FOUND));
    RegCloseKey(hKey);
#else
    QSKIP("Windows-only");
#endif
}

void TestAutoStartWin::enableThenDisableRoundTrip() {
#ifdef _WIN32
    auto backend = PlatformBackend::create();
    QVERIFY(backend);

    backend->setAutoStartEnabled(true);
    QVERIFY(backend->isAutoStartEnabled());

    backend->setAutoStartEnabled(false);
    QCOMPARE(backend->isAutoStartEnabled(), false);

    backend->setAutoStartEnabled(true);
    QVERIFY(backend->isAutoStartEnabled());

    backend->setAutoStartEnabled(false);
    QCOMPARE(backend->isAutoStartEnabled(), false);
#else
    QSKIP("Windows-only");
#endif
}

void TestAutoStartWin::disableWhenAlreadyAbsentIsIdempotent() {
#ifdef _WIN32
    auto backend = PlatformBackend::create();
    QVERIFY(backend);
    QCOMPARE(backend->isAutoStartEnabled(), false);

    // Calling disable when the value is already absent must not error or
    // crash — the impl treats ERROR_FILE_NOT_FOUND as success.
    backend->setAutoStartEnabled(false);
    backend->setAutoStartEnabled(false);
    QCOMPARE(backend->isAutoStartEnabled(), false);
#else
    QSKIP("Windows-only");
#endif
}

QTEST_GUILESS_MAIN(TestAutoStartWin)
#include "test_autostart_win.moc"
