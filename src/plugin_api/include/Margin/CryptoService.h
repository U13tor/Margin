// src/plugin_api/include/Margin/CryptoService.h
//
// Per-plugin crypto service. The Host hands each plugin a CryptoService*
// already initialized with a per-plugin key derived from the master key
// via HKDF (see docs/05-host-services.md §8.2). Plugins cannot reach the
// master key — they only see this interface.
//
// Spec: docs/05-host-services.md §8.1.

#pragma once

#include <QByteArray>
#include <QString>

namespace Margin {

class CryptoService {
public:
    virtual ~CryptoService() = default;

    // Encrypt a UTF-8 string with this plugin's derived key. Returns a
    // base64-encoded blob (nonce || ciphertext || tag). Returns an empty
    // QByteArray on internal failure (caller should treat as fatal-ish).
    virtual QByteArray encryptString(const QString& plaintext) = 0;

    // Decrypt a blob produced by encryptString. Returns the recovered
    // plaintext on success, or a null QString() if the blob is malformed
    // or was encrypted by a different plugin's key (HKDF isolation).
    virtual QString decryptString(const QByteArray& ciphertext) = 0;
};

} // namespace Margin
