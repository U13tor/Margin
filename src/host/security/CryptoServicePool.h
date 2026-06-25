// src/host/security/CryptoServicePool.h
//
// Owns the master key copy + a cache of per-plugin CryptoServiceImpl
// instances. Each plugin gets a derived key via HKDF-SHA256(master, salt=
// pluginId, info="margin-plugin-key"). The pool is created in HostCore::
// bootstrap and destroyed in shutdown — destruction zeroizes every cached
// plugin key and the master copy.
//
// Spec: docs/05-host-services.md §8.2 (HKDF isolation).

#pragma once

#include "CryptoServiceImpl.h"

#include <QHash>
#include <QMutex>
#include <QString>

#include <cstdint>
#include <memory>
#include <vector>

namespace Margin {

class CryptoServicePool {
public:
    // Factory: caller passes ownership of the master_key vector in. The
    // pool keeps a private copy and zeroizes it on destruction.
    static std::unique_ptr<CryptoServicePool> create(std::vector<uint8_t> masterKey);

    ~CryptoServicePool();

    CryptoServicePool(const CryptoServicePool&) = delete;
    CryptoServicePool& operator=(const CryptoServicePool&) = delete;

    // Returns the per-plugin CryptoService. Multiple calls with the same
    // pluginId return the same cached instance. Thread-safe.
    CryptoServiceImpl& getOrCreate(const QString& pluginId);

private:
    explicit CryptoServicePool(std::vector<uint8_t> masterKey);

    std::vector<uint8_t>                    m_masterKey;
    QHash<QString, CryptoServiceImpl*>      m_cache;  // pool owns the raw pointers
    QMutex                                  m_mutex;
};

} // namespace Margin
