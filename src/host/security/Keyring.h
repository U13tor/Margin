// src/host/security/Keyring.h
//
// Host-internal OS-keyring wrapper. NOT exported to plugins — the public
// surface for plugins is CryptoService (see plugin_api/Margin/CryptoService.h).
//
// Responsibilities:
//   1. Master-key lifecycle: load from / persist to OS keyring (Win: DPAPI
//      user-scope blob stored under Paths::data()/keyring/; Mac: Keychain
//      Services, see docs/12-deferred-items.md C6).
//   2. Low-level OS-keyring KV primitive (store/load/clear) — Host-internal
//      and exposed for unit tests.
//   3. AES-256-GCM field primitive (encryptField/decryptField) — used by
//      Settings transparent encryption and CryptoService per-plugin wrappers.
//
// Spec: docs/07-privacy-security.md §4 (Keyring 接口签名).

#pragma once

#include <QByteArray>
#include <QString>

#include <cstdint>
#include <optional>
#include <vector>

namespace Margin {

class Keyring {
public:
    // ── Master-key lifecycle ────────────────────────────────────────────
    // Loads persisted master from the OS keyring; generates and persists a
    // fresh 32B secret on first run. Process-cached (std::call_once) after
    // first call. Throws std::runtime_error on OS failure.
    static std::vector<uint8_t> getOrCreateMasterKey();

    // Removes master from OS keyring + clears the in-memory cache.
    static void deleteMasterKey();

    // Zeroizes the in-memory master cache (called from HostCore::shutdown,
    // see docs/07-privacy-security.md §密钥生命周期 step 3).
    static void clearMemory();

    // ── Low-level OS-keyring KV primitive (Host-internal + unit tests) ──
    static bool store(const QString& service, const QString& key, const QByteArray& value);
    static std::optional<QByteArray> load(const QString& service, const QString& key);
    static bool clear(const QString& service, const QString& key);

    // ── AES-256-GCM field primitive (Host-internal; caller supplies key) ──
    // Output layout: nonce(12) || ciphertext || tag(16). decryptField returns
    // an empty QByteArray on tag-mismatch or malformed input — callers MUST
    // check for empty before using.
    static QByteArray encryptField(const QByteArray& plaintext, const std::vector<uint8_t>& key);
    static QByteArray decryptField(const QByteArray& packed, const std::vector<uint8_t>& key);
};

} // namespace Margin
