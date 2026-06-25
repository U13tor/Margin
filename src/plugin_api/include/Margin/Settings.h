// Settings service interface — verbatim from docs/05-host-services.md §4.
// Impl: host/services/Settings.cpp. Encrypted fields (§4.4) shipped in M1-C7.
//
// registerEncryptedKeys is host-only in practice (called by PluginManager
// after manifest scan, before any onLoad). Plugins see the method through
// this interface but cannot abuse it — a malicious plugin registering
// another plugin's keys only succeeds in marking those keys for transparent
// encryption, which is the desired behavior anyway. Namespace isolation
// enforcement (§4.5) lives in the loader and rejects out-of-namespace keys.

#pragma once

#include <QSet>
#include <QString>
#include <QVariant>
#include <functional>

namespace Margin {

class Settings {
public:
    virtual QVariant get(const QString& key,
                         const QVariant& defaultValue = {}) const = 0;
    virtual void set(const QString& key, const QVariant& value) = 0;
    virtual void onChange(const QString& key,
                          std::function<void(const QVariant&)> handler) = 0;
    virtual void remove(const QString& key) = 0;

    /// Merge `keys` into the encrypted-key set. Idempotent. See §4.4.
    virtual void registerEncryptedKeys(const QSet<QString>& keys) = 0;

    virtual ~Settings() = default;
};

} // namespace Margin
