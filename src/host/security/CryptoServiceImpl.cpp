// src/host/security/CryptoServiceImpl.cpp

#include "CryptoServiceImpl.h"
#include "Keyring.h"

#include <QByteArray>
#include <QString>

#if defined(Q_OS_WIN)
#  include <windows.h>
#endif

namespace Margin {

namespace {

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

} // namespace

CryptoServiceImpl::CryptoServiceImpl(QString pluginId, std::vector<uint8_t> pluginKey)
    : m_pluginId(std::move(pluginId))
    , m_pluginKey(std::move(pluginKey)) {}

CryptoServiceImpl::~CryptoServiceImpl() {
    secureZero(m_pluginKey);
}

QByteArray CryptoServiceImpl::encryptString(const QString& plaintext) {
    const QByteArray utf8 = plaintext.toUtf8();
    const QByteArray packed = Keyring::encryptField(utf8, m_pluginKey);
    if (packed.isEmpty()) return {};
    return packed.toBase64();
}

QString CryptoServiceImpl::decryptString(const QByteArray& ciphertext) {
    const QByteArray packed = QByteArray::fromBase64(ciphertext);
    if (packed.isEmpty()) return {};
    const QByteArray utf8 = Keyring::decryptField(packed, m_pluginKey);
    if (utf8.isEmpty()) return {};
    return QString::fromUtf8(utf8);
}

} // namespace Margin
