// src/host/security/CryptoServiceImpl.h
//
// Concrete CryptoService bound to one plugin. Each plugin gets its own
// instance with a distinct derived key (HKDF-SHA256(master, salt=pluginId,
// info="margin-plugin-key")). Constructor stores the key; destructor
// zeroizes it. Spec: docs/05-host-services.md §8, docs/07-privacy-security.md §4.

#pragma once

#include "Margin/CryptoService.h"

#include <QString>

#include <cstdint>
#include <vector>

namespace Margin {

class CryptoServiceImpl final : public CryptoService {
public:
    CryptoServiceImpl(QString pluginId, std::vector<uint8_t> pluginKey);
    ~CryptoServiceImpl() override;

    CryptoServiceImpl(const CryptoServiceImpl&) = delete;
    CryptoServiceImpl& operator=(const CryptoServiceImpl&) = delete;

    QByteArray encryptString(const QString& plaintext) override;
    QString    decryptString(const QByteArray& ciphertext) override;

private:
    QString                 m_pluginId;  // debug logging only
    std::vector<uint8_t>    m_pluginKey; // 32B HKDF-derived
};

} // namespace Margin
