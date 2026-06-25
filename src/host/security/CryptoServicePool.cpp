// src/host/security/CryptoServicePool.cpp

#include "CryptoServicePool.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/params.h>

#include <stdexcept>

#if defined(Q_OS_WIN)
#  include <windows.h>
#endif

namespace Margin {

namespace {

constexpr char kHkdfInfo[]   = "margin-plugin-key";
constexpr int  kPluginKeyLen = 32;

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

// HKDF-SHA256: IKM=master_key (32B), salt=pluginId UTF-8, info=kHkdfInfo,
// output length=32B. Throws std::runtime_error on OpenSSL failure.
std::vector<uint8_t> hkdfDerive(const std::vector<uint8_t>& master,
                                const QString& pluginId) {
    const QByteArray saltBytes = pluginId.toUtf8();

    EVP_KDF* kdf = EVP_KDF_fetch(nullptr, "HKDF", nullptr);
    if (!kdf) throw std::runtime_error("EVP_KDF_fetch HKDF failed");
    EVP_KDF_CTX* kctx = EVP_KDF_CTX_new(kdf);
    EVP_KDF_free(kdf);
    if (!kctx) throw std::runtime_error("EVP_KDF_CTX_new failed");

    const char* digest = "SHA256";
    std::vector<uint8_t> out(kPluginKeyLen);

    OSSL_PARAM params[5];
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST,
                                                  const_cast<char*>(digest), 0);
    params[1] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY,
                                                  const_cast<uint8_t*>(master.data()),
                                                  master.size());
    params[2] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT,
                                                  const_cast<char*>(saltBytes.constData()),
                                                  static_cast<size_t>(saltBytes.size()));
    params[3] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO,
                                                  const_cast<char*>(kHkdfInfo),
                                                  sizeof(kHkdfInfo) - 1);
    params[4] = OSSL_PARAM_construct_end();

    const int rv = EVP_KDF_derive(kctx, out.data(), out.size(), params);
    EVP_KDF_CTX_free(kctx);
    if (rv <= 0) throw std::runtime_error("EVP_KDF_derive failed");
    return out;
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
