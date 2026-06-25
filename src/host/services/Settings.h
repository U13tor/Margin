// Settings impl — JSON + dot-path + QSaveFile atomic write + onChange handlers.
// Spec: docs/05 §4 (transparent encryption §4.4).
//
// Encrypted keys: registerEncryptedKeys() feeds a set of dot-paths that
// PluginManager parsed from manifest `encrypted_settings` arrays. On
// set() for a registered key, the value is wrapped in an envelope
//   { "__encrypted__": true, "iv": <b64 12B nonce>, "ct": <b64 ct+tag> }
// via Keyring::encryptField (AES-256-GCM with the master key from the
// OS keyring). On get(), the envelope is detected and unwrapped back
// to plaintext. Plugin code is fully unaware.

#pragma once

#include "Margin/Settings.h"

#include <QHash>
#include <QJsonObject>
#include <QMutex>
#include <QSet>
#include <QString>
#include <QVariant>
#include <QVector>

#include <cstdint>
#include <functional>
#include <vector>

namespace Margin {

class SettingsImpl : public Settings {
public:
    explicit SettingsImpl(const QString& configDir);

    QVariant get(const QString& key, const QVariant& defaultValue) const override;
    void set(const QString& key, const QVariant& value) override;
    void onChange(const QString& key, std::function<void(const QVariant&)> handler) override;
    void remove(const QString& key) override;

    /// Host calls this after PluginManager scan completes (manifests parsed,
    /// before any onLoad). Idempotent; further calls merge into the set.
    /// See docs/05-host-services.md §4.4 step 2.
    void registerEncryptedKeys(const QSet<QString>& keys);

private:
    using ChangeHandler = std::function<void(const QVariant&)>;

    void save();

    static QJsonValue lookup(const QJsonObject& obj, const QStringList& parts);
    static void assign(QJsonObject& obj, const QStringList& parts, const QJsonValue& val);
    static void erase(QJsonObject& obj, const QStringList& parts);

    /// Lazily fetch the master key on first encrypted op. Returns an
    /// empty vector on Keyring failure (caller falls back to plaintext).
    const std::vector<uint8_t>& masterKey() const;

    /// Wrap a plaintext QVariant into the on-disk envelope object.
    /// Returns the original value if the key is not registered or the
    /// master key is unavailable (logged as a warning on first miss).
    QJsonValue encryptForDisk(const QString& key, const QVariant& value) const;

    /// Reverse of encryptForDisk. Returns the unwrapped QVariant or, if
    /// the input isn't an envelope / decryption fails, the raw value.
    QVariant decryptFromDisk(const QJsonValue& stored) const;

    mutable QMutex m_mutex;
    QString m_path;
    QJsonObject m_cache;
    QHash<QString, QVector<ChangeHandler>> m_handlers;
    QSet<QString> m_encryptedKeys;

    mutable bool m_masterKeyTried = false;
    mutable std::vector<uint8_t> m_masterKey;
};

} // namespace Margin
