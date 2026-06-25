// src/host/security/Keyring.cpp
//
// Win: DPAPI (CryptProtectData / CryptUnprotectData) wraps the secret with
// the current Windows account key; the encrypted blob is stored on disk
// under Paths::data()/keyring/<service>/<key>.bin (file ACL defaults to the
// current user). Mac: deferred — see docs/12-deferred-items.md C6.
//
// AES-256-GCM field primitive uses OpenSSL EVP (Apache-2.0, see
// docs/03-build-system.md vcpkg.json increment table).

#include "Keyring.h"

#include "paths/Paths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QString>
#include <QtGlobal>

#if defined(Q_OS_WIN)
#  include <windows.h>
#  include <dpapi.h>
#endif

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <cstring>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <vector>

namespace Margin {

namespace {

constexpr int  kMasterKeyLen   = 32;
constexpr int  kGcmNonceLen    = 12;
constexpr int  kGcmTagLen      = 16;
constexpr char kKeyringService[]  = "Margin";
constexpr char kKeyringMasterKey[] = "master";

// ── Process-local master-key cache (lazy load, secure clear) ─────────────
std::optional<std::vector<uint8_t>>& masterCacheRef() {
    static std::optional<std::vector<uint8_t>> cache;
    return cache;
}

std::once_flag& masterOnceFlag() {
    static std::once_flag flag;
    return flag;
}

void secureZero(std::vector<uint8_t>& v) {
    if (v.empty()) return;
#if defined(Q_OS_WIN)
    SecureZeroMemory(v.data(), v.size());
#else
    // No POSIX explicit_bzero guarantee here; overwrite + compiler fence.
    volatile uint8_t* p = v.data();
    for (size_t i = 0; i < v.size(); ++i) p[i] = 0;
#endif
    v.clear();
}

// ── DPAPI helpers (Win only) ─────────────────────────────────────────────
#if defined(Q_OS_WIN)
QByteArray dpApiProtect(const QByteArray& plaintext) {
    DATA_BLOB in{};
    in.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(plaintext.constData()));
    in.cbData = static_cast<DWORD>(plaintext.size());

    DATA_BLOB out{};
    if (!CryptProtectData(&in, nullptr, nullptr, nullptr, nullptr,
                          CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        return {};
    }
    QByteArray result(reinterpret_cast<const char*>(out.pbData),
                      static_cast<int>(out.cbData));
    if (out.pbData) LocalFree(out.pbData);
    return result;
}

QByteArray dpApiUnprotect(const QByteArray& ciphertext) {
    DATA_BLOB in{};
    in.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(ciphertext.constData()));
    in.cbData = static_cast<DWORD>(ciphertext.size());

    DATA_BLOB out{};
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr,
                            CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        return {};
    }
    QByteArray result(reinterpret_cast<const char*>(out.pbData),
                      static_cast<int>(out.cbData));
    if (out.pbData) LocalFree(out.pbData);
    return result;
}
#endif

// ── On-disk path for (service, key) ──────────────────────────────────────
// Win-only: Mac branch never reaches this (Keychain stores its own data).
QString keyringPath(const QString& service, const QString& key) {
    return Paths::data()
         + QLatin1String("/keyring/")
         + service
         + QLatin1Char('/')
         + key
         + QLatin1String(".bin");
}

} // namespace

// ── Low-level OS-keyring KV primitive ────────────────────────────────────

bool Keyring::store(const QString& service, const QString& key, const QByteArray& value) {
#if defined(Q_OS_WIN)
    const QByteArray encrypted = dpApiProtect(value);
    if (encrypted.isEmpty() && !value.isEmpty()) return false;

    const QString path = keyringPath(service, key);
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    const qint64 written = f.write(encrypted);
    f.close();
    return written == encrypted.size();
#elif defined(Q_OS_MAC)
    Q_UNUSED(service)
    Q_UNUSED(key)
    Q_UNUSED(value)
    qFatal("macOS Keychain not yet implemented (C6 deferred; see docs/12-deferred-items.md)");
    return false;
#else
#  error "Unsupported platform for Keyring::store"
#endif
}

std::optional<QByteArray> Keyring::load(const QString& service, const QString& key) {
#if defined(Q_OS_WIN)
    QFile f(keyringPath(service, key));
    if (!f.exists()) return std::nullopt;
    if (!f.open(QIODevice::ReadOnly)) return std::nullopt;
    const QByteArray encrypted = f.readAll();
    f.close();
    if (encrypted.isEmpty()) return std::nullopt;

    const QByteArray plaintext = dpApiUnprotect(encrypted);
    if (plaintext.isEmpty()) return std::nullopt;
    return plaintext;
#elif defined(Q_OS_MAC)
    Q_UNUSED(service)
    Q_UNUSED(key)
    qFatal("macOS Keychain not yet implemented (C6 deferred; see docs/12-deferred-items.md)");
    return std::nullopt;
#else
#  error "Unsupported platform for Keyring::load"
#endif
}

bool Keyring::clear(const QString& service, const QString& key) {
#if defined(Q_OS_WIN)
    QFile f(keyringPath(service, key));
    if (!f.exists()) return true;  // already gone is success
    return f.remove();
#elif defined(Q_OS_MAC)
    Q_UNUSED(service)
    Q_UNUSED(key)
    qFatal("macOS Keychain not yet implemented (C6 deferred; see docs/12-deferred-items.md)");
    return false;
#else
#  error "Unsupported platform for Keyring::clear"
#endif
}

// ── Master-key lifecycle ────────────────────────────────────────────────

std::vector<uint8_t> Keyring::getOrCreateMasterKey() {
    auto& cache = masterCacheRef();
    std::call_once(masterOnceFlag(), [&]() {
        auto existing = load(QLatin1String(kKeyringService),
                             QLatin1String(kKeyringMasterKey));
        if (existing && static_cast<int>(existing->size()) == kMasterKeyLen) {
            cache = std::vector<uint8_t>(existing->begin(), existing->end());
            return;
        }

        std::vector<uint8_t> fresh(kMasterKeyLen);
        QRandomGenerator gen = QRandomGenerator::securelySeeded();
        for (int i = 0; i < kMasterKeyLen; ++i) {
            fresh[static_cast<size_t>(i)] = static_cast<uint8_t>(gen.generate() & 0xFFu);
        }

        const QByteArray bytes(reinterpret_cast<const char*>(fresh.data()),
                               static_cast<int>(fresh.size()));
        if (!store(QLatin1String(kKeyringService),
                   QLatin1String(kKeyringMasterKey), bytes)) {
            secureZero(fresh);
            throw std::runtime_error("Keyring::store(master) failed");
        }
        cache = std::move(fresh);
    });
    if (!cache) {
        throw std::runtime_error("Keyring::getOrCreateMasterKey cache unavailable");
    }
    return *cache;
}

void Keyring::deleteMasterKey() {
    clear(QLatin1String(kKeyringService), QLatin1String(kKeyringMasterKey));
    auto& cache = masterCacheRef();
    if (cache) secureZero(*cache);
    cache.reset();
}

void Keyring::clearMemory() {
    auto& cache = masterCacheRef();
    if (cache) secureZero(*cache);
    cache.reset();
}

// ── AES-256-GCM field primitive ─────────────────────────────────────────
// Layout: nonce(12) || ciphertext(same length as plaintext) || tag(16).

QByteArray Keyring::encryptField(const QByteArray& plaintext,
                                 const std::vector<uint8_t>& key) {
    if (static_cast<int>(key.size()) != kMasterKeyLen) return {};

    std::vector<uint8_t> nonce(kGcmNonceLen);
    if (RAND_bytes(nonce.data(), kGcmNonceLen) != 1) return {};

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};

    QByteArray ciphertext(plaintext.size(), '\0');
    std::vector<uint8_t> tag(kGcmTagLen);
    int len = 0;
    int total = 0;
    bool ok = true;

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) ok = false;
    if (ok && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, kGcmNonceLen, nullptr) != 1) ok = false;
    if (ok && EVP_EncryptInit_ex(ctx, nullptr, nullptr,
            const_cast<uint8_t*>(key.data()), nonce.data()) != 1) ok = false;
    if (ok && EVP_EncryptUpdate(ctx,
            reinterpret_cast<uint8_t*>(ciphertext.data()), &len,
            reinterpret_cast<const uint8_t*>(plaintext.constData()),
            static_cast<int>(plaintext.size())) != 1) ok = false;
    total = len;
    if (ok && EVP_EncryptFinal_ex(ctx,
            reinterpret_cast<uint8_t*>(ciphertext.data()) + len, &len) != 1) ok = false;
    total += len;
    if (ok && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, kGcmTagLen, tag.data()) != 1) ok = false;

    EVP_CIPHER_CTX_free(ctx);
    if (!ok) return {};
    ciphertext.truncate(total);

    QByteArray packed;
    packed.reserve(kGcmNonceLen + total + kGcmTagLen);
    packed.append(reinterpret_cast<const char*>(nonce.data()), kGcmNonceLen);
    packed.append(ciphertext);
    packed.append(reinterpret_cast<const char*>(tag.data()), kGcmTagLen);
    return packed;
}

QByteArray Keyring::decryptField(const QByteArray& packed,
                                 const std::vector<uint8_t>& key) {
    if (static_cast<int>(key.size()) != kMasterKeyLen) return {};
    if (packed.size() < kGcmNonceLen + kGcmTagLen) return {};

    const int ctLen = packed.size() - kGcmNonceLen - kGcmTagLen;
    const uint8_t* nonce = reinterpret_cast<const uint8_t*>(packed.constData());
    const uint8_t* ct    = nonce + kGcmNonceLen;
    const uint8_t* tag   = ct + ctLen;

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};

    QByteArray plaintext(ctLen, '\0');
    int len = 0;
    int total = 0;
    bool ok = true;

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) ok = false;
    if (ok && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, kGcmNonceLen, nullptr) != 1) ok = false;
    if (ok && EVP_DecryptInit_ex(ctx, nullptr, nullptr,
            const_cast<uint8_t*>(key.data()), nonce) != 1) ok = false;
    if (ok && EVP_DecryptUpdate(ctx,
            reinterpret_cast<uint8_t*>(plaintext.data()), &len,
            ct, ctLen) != 1) ok = false;
    total = len;
    if (ok && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, kGcmTagLen,
            const_cast<uint8_t*>(tag)) != 1) ok = false;
    if (ok && EVP_DecryptFinal_ex(ctx,
            reinterpret_cast<uint8_t*>(plaintext.data()) + len, &len) != 1) ok = false;
    total += len;

    EVP_CIPHER_CTX_free(ctx);
    if (!ok) return {};
    plaintext.truncate(total);
    return plaintext;
}

} // namespace Margin
