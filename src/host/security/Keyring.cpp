// src/host/security/Keyring.cpp
//
// Win: DPAPI (CryptProtectData / CryptUnprotectData) wraps the secret with
// the current Windows account key; the encrypted blob is stored on disk
// under Paths::data()/keyring/<service>/<key>.bin (file ACL defaults to the
// current user). Paths::data() resolves to %APPDATA%\Margin\data (Roaming),
// so the master key survives NSIS uninstall (RMDir /r $INSTDIR only clears
// %LOCALAPPDATA%\Margin). Mac: deferred — see docs/12-deferred-items.md C6.
//
// AES-256-GCM field primitive uses Windows CNG (BCrypt). Qt 6.7 has no
// native AES-GCM API — see docs/12-deferred-items.md D5-1. Output layout
// nonce(12) || ciphertext || tag(16) is identical to the prior OpenSSL impl
// shape so callers and on-disk envelope format stay unchanged.

#include "Keyring.h"

#include "paths/Paths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QString>
#include <QtGlobal>

#include <cstring>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <vector>

#if defined(Q_OS_WIN)
#  include <windows.h>
#  include <bcrypt.h>
#  include <dpapi.h>
#endif

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

// RAII guards so early-return paths in AES-GCM helpers do not leak handles.
struct AlgHandleGuard {
    BCRYPT_ALG_HANDLE h = nullptr;
    ~AlgHandleGuard() { if (h) BCryptCloseAlgorithmProvider(h, 0); }
};
struct KeyHandleGuard {
    BCRYPT_KEY_HANDLE h = nullptr;
    ~KeyHandleGuard() { if (h) BCryptDestroyKey(h); }
};

// AES-256-GCM encrypt via CNG. Returns nonce(12) || ct || tag(16), or empty
// on failure. The nonce is random per call (QRandomGenerator::securelySeeded
// is CSPRNG-backed on Windows via RtlGenRandom).
QByteArray cngAesGcmEncrypt(const QByteArray& plaintext,
                             const uint8_t* key32) {
    // 1. Random 12B nonce.
    std::vector<uint8_t> nonce(kGcmNonceLen);
    QRandomGenerator gen = QRandomGenerator::securelySeeded();
    for (int i = 0; i < kGcmNonceLen; ++i) {
        nonce[static_cast<size_t>(i)] = static_cast<uint8_t>(gen.generate() & 0xFFu);
    }

    // 2. Algorithm provider + GCM chaining mode.
    BCRYPT_ALG_HANDLE alg = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_AES_ALGORITHM, nullptr, 0) != 0) {
        return {};
    }
    AlgHandleGuard algGuard{alg};
    if (BCryptSetProperty(alg, BCRYPT_CHAINING_MODE,
                          reinterpret_cast<PUCHAR>(const_cast<PWSTR>(BCRYPT_CHAIN_MODE_GCM)),
                          sizeof(BCRYPT_CHAIN_MODE_GCM), 0) != 0) {
        return {};
    }

    // 3. Import the 32B raw key as a CNG symmetric key handle.
    BCRYPT_KEY_HANDLE kh = nullptr;
    if (BCryptGenerateSymmetricKey(alg, &kh, nullptr, 0,
                                    const_cast<PUCHAR>(key32), kMasterKeyLen, 0) != 0) {
        return {};
    }
    KeyHandleGuard keyGuard{kh};

    // 4. Wire up the GCM authenticated mode info — nonce + 16B tag slot.
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
    BCRYPT_INIT_AUTH_MODE_INFO(info);
    info.pbNonce = nonce.data();
    info.cbNonce = kGcmNonceLen;
    std::vector<uint8_t> tag(kGcmTagLen);
    info.pbTag = tag.data();
    info.cbTag = kGcmTagLen;

    // 5. Encrypt. AES-GCM is a stream cipher — ciphertext length equals
    // plaintext length, no padding. BCryptEncrypt writes the tag at the end
    // of the call via info.pbTag.
    QByteArray ciphertext(plaintext.size(), '\0');
    ULONG ctLen = 0;
    const NTSTATUS rv = BCryptEncrypt(
        kh,
        reinterpret_cast<PUCHAR>(const_cast<char*>(plaintext.constData())),
        static_cast<ULONG>(plaintext.size()),
        &info, nullptr, 0,
        reinterpret_cast<PUCHAR>(ciphertext.data()),
        static_cast<ULONG>(ciphertext.size()),
        &ctLen, 0);
    if (rv != 0) {
        return {};
    }
    ciphertext.resize(static_cast<int>(ctLen));

    // 6. Pack nonce(12) || ct || tag(16).
    QByteArray packed;
    packed.reserve(kGcmNonceLen + static_cast<int>(ctLen) + kGcmTagLen);
    packed.append(reinterpret_cast<const char*>(nonce.data()), kGcmNonceLen);
    packed.append(ciphertext);
    packed.append(reinterpret_cast<const char*>(tag.data()), kGcmTagLen);
    return packed;
}

// AES-256-GCM decrypt via CNG. packed layout: nonce(12) || ct || tag(16).
// Returns empty QByteArray on tag mismatch or malformed input.
QByteArray cngAesGcmDecrypt(const QByteArray& packed, const uint8_t* key32) {
    if (packed.size() < kGcmNonceLen + kGcmTagLen) return {};

    const int ctLen = packed.size() - kGcmNonceLen - kGcmTagLen;
    const uint8_t* nonce = reinterpret_cast<const uint8_t*>(packed.constData());
    const uint8_t* ct    = nonce + kGcmNonceLen;
    const uint8_t* tag   = ct + ctLen;

    BCRYPT_ALG_HANDLE alg = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_AES_ALGORITHM, nullptr, 0) != 0) {
        return {};
    }
    AlgHandleGuard algGuard{alg};
    if (BCryptSetProperty(alg, BCRYPT_CHAINING_MODE,
                          reinterpret_cast<PUCHAR>(const_cast<PWSTR>(BCRYPT_CHAIN_MODE_GCM)),
                          sizeof(BCRYPT_CHAIN_MODE_GCM), 0) != 0) {
        return {};
    }

    BCRYPT_KEY_HANDLE kh = nullptr;
    if (BCryptGenerateSymmetricKey(alg, &kh, nullptr, 0,
                                    const_cast<PUCHAR>(key32), kMasterKeyLen, 0) != 0) {
        return {};
    }
    KeyHandleGuard keyGuard{kh};

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
    BCRYPT_INIT_AUTH_MODE_INFO(info);
    info.pbNonce = const_cast<PUCHAR>(nonce);
    info.cbNonce = kGcmNonceLen;
    info.pbTag = const_cast<PUCHAR>(tag);
    info.cbTag = kGcmTagLen;

    QByteArray plaintext(ctLen, '\0');
    ULONG ptLen = 0;
    const NTSTATUS rv = BCryptDecrypt(
        kh,
        const_cast<PUCHAR>(ct), static_cast<ULONG>(ctLen),
        &info, nullptr, 0,
        reinterpret_cast<PUCHAR>(plaintext.data()),
        static_cast<ULONG>(plaintext.size()),
        &ptLen, 0);
    if (rv != 0) {
        // AUTH_TAG verification failure returns STATUS_AUTH_TAG_MISMATCH —
        // surface as empty QByteArray (caller-visible signal of corruption).
        return {};
    }
    plaintext.resize(static_cast<int>(ptLen));
    return plaintext;
}
#endif // defined(Q_OS_WIN)

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
// Win uses CNG (BCrypt); Mac is gated by C6 — see top-of-file note.

QByteArray Keyring::encryptField(const QByteArray& plaintext,
                                 const std::vector<uint8_t>& key) {
    if (static_cast<int>(key.size()) != kMasterKeyLen) return {};
#if defined(Q_OS_WIN)
    return cngAesGcmEncrypt(plaintext, key.data());
#elif defined(Q_OS_MAC)
    Q_UNUSED(plaintext)
    qFatal("macOS AES-GCM field primitive not yet implemented (C6 deferred)");
    return {};
#else
#  error "Unsupported platform for Keyring::encryptField"
#endif
}

QByteArray Keyring::decryptField(const QByteArray& packed,
                                 const std::vector<uint8_t>& key) {
    if (static_cast<int>(key.size()) != kMasterKeyLen) return {};
#if defined(Q_OS_WIN)
    return cngAesGcmDecrypt(packed, key.data());
#elif defined(Q_OS_MAC)
    Q_UNUSED(packed)
    qFatal("macOS AES-GCM field primitive not yet implemented (C6 deferred)");
    return {};
#else
#  error "Unsupported platform for Keyring::decryptField"
#endif
}

} // namespace Margin
