// src/host/security/CryptoServicePool.cpp
//
// Per-plugin key derivation via HKDF-SHA256 (RFC 5869). Implemented on
// QCryptographicHash::hmac — Qt 6.7 has no native HKDF API, but HMAC-SHA256
// is sufficient. See docs/12-deferred-items.md D5-1 (OpenSSL removal).

#include "CryptoServicePool.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QString>

#include <stdexcept>
#include <vector>

#if defined(Q_OS_WIN)
#  include <windows.h>
#endif

namespace Margin {

namespace {

constexpr char kHkdfInfo[]   = "margin-plugin-key";
constexpr int  kPluginKeyLen = 32;
constexpr int  kSha256Len    = 32;

void secureZero(std::vector<uint8_t>& v) {
    if (v.empty()) return;
#if defined(Q_OS_WIN)
    SecureZeroMemory(v.data(), v.size());
#else
    volatile uint8_t* p = v.data();
    for (size_t i = 0; i < v.size(); ++i) p[i] = 0;
#endif
    v.clear();
}

// HMAC-SHA256(key, message) -> 32-byte digest, wrapped as std::vector.
// QMessageAuthenticationCode is Qt's HMAC primitive (QtCore since 5.1).
std::vector<uint8_t> hmacSha256(const std::vector<uint8_t>& key,
                                const QByteArray& message) {
    const QByteArray keyWrap(reinterpret_cast<const char*>(key.data()),
                              static_cast<int>(key.size()));
    const QByteArray tag = QMessageAuthenticationCode::hash(
        message, keyWrap, QCryptographicHash::Sha256);
    return std::vector<uint8_t>(tag.constBegin(), tag.constEnd());
}

// HKDF-Extract(salt, IKM) -> PRK (32 bytes).
// RFC 5869 §2.2: PRK = HMAC-Hash(salt, IKM). Empty salt is replaced by
// HashLen zeros, but pluginId UTF-8 is always non-empty in our usage.
std::vector<uint8_t> hkdfExtract(const std::vector<uint8_t>& salt,
                                  const std::vector<uint8_t>& ikm) {
    std::vector<uint8_t> effectiveSalt = salt;
    if (effectiveSalt.empty()) effectiveSalt.assign(kSha256Len, 0);

    const QByteArray ikmWrap(reinterpret_cast<const char*>(ikm.data()),
                              static_cast<int>(ikm.size()));
    return hmacSha256(effectiveSalt, ikmWrap);
}

// HKDF-Expand(PRK, info, L) -> OKM (L bytes).
// RFC 5869 §2.3: T(0)=empty; T(i)=HMAC(PRK, T(i-1)|info|i); OKM=first L bytes.
std::vector<uint8_t> hkdfExpand(const std::vector<uint8_t>& prk,
                                 const QByteArray& info,
                                 int length) {
    const int N = (length + kSha256Len - 1) / kSha256Len;
    if (N > 255) throw std::runtime_error("HKDF-Expand length too large");

    std::vector<uint8_t> okm;
    QByteArray prev;  // T(i-1)
    for (int i = 1; i <= N; ++i) {
        QByteArray msg;
        msg.append(prev);
        msg.append(info);
        msg.append(static_cast<char>(i));
        const std::vector<uint8_t> t = hmacSha256(prk, msg);
        prev = QByteArray(reinterpret_cast<const char*>(t.data()),
                          static_cast<int>(t.size()));
        okm.insert(okm.end(), t.begin(), t.end());
    }
    okm.resize(length);
    return okm;
}

// HKDF-SHA256: IKM=master_key (32B), salt=pluginId UTF-8, info=kHkdfInfo,
// output length=32B. Throws std::runtime_error on internal failure.
std::vector<uint8_t> hkdfDerive(const std::vector<uint8_t>& master,
                                const QString& pluginId) {
    const QByteArray saltBytes = pluginId.toUtf8();
    const std::vector<uint8_t> salt(saltBytes.constBegin(),
                                     saltBytes.constEnd());
    const QByteArray info = QByteArray::fromRawData(kHkdfInfo,
                                                    sizeof(kHkdfInfo) - 1);

    const std::vector<uint8_t> prk = hkdfExtract(salt, master);
    return hkdfExpand(prk, info, kPluginKeyLen);
}

} // namespace

std::unique_ptr<CryptoServicePool> CryptoServicePool::create(std::vector<uint8_t> masterKey) {
    return std::unique_ptr<CryptoServicePool>(new CryptoServicePool(std::move(masterKey)));
}

CryptoServicePool::CryptoServicePool(std::vector<uint8_t> masterKey)
    : m_masterKey(std::move(masterKey)) {}

CryptoServicePool::~CryptoServicePool() {
    // Drop all per-plugin wrappers first (their dtors zero their keys), then
    // zeroize the master key copy.
    for (auto it = m_cache.begin(); it != m_cache.end(); ++it) {
        delete it.value();
    }
    m_cache.clear();
    secureZero(m_masterKey);
}

CryptoServiceImpl& CryptoServicePool::getOrCreate(const QString& pluginId) {
    QMutexLocker locker(&m_mutex);
    const auto it = m_cache.constFind(pluginId);
    if (it != m_cache.constEnd()) return *it.value();

    std::vector<uint8_t> pluginKey = hkdfDerive(m_masterKey, pluginId);
    auto* impl = new CryptoServiceImpl(pluginId, std::move(pluginKey));
    m_cache.insert(pluginId, impl);
    return *impl;
}

} // namespace Margin
