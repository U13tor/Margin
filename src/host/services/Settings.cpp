// Settings impl — see header for spec.
// Change handlers fire outside the mutex so callbacks can call set() safely.

#include "Settings.h"

#include "host/security/Keyring.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMutexLocker>
#include <QSaveFile>
#include <QStringLiteral>

#include <cstdio>

namespace Margin {

namespace {
constexpr const char* kEncryptedMarker = "__encrypted__";
constexpr const char* kIvKey = "iv";
constexpr const char* kCtKey = "ct";
} // namespace

SettingsImpl::SettingsImpl(const QString& configDir)
    : m_path(configDir + QLatin1String("/settings.json")) {
    QMutexLocker lock(&m_mutex);
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly)) return;  // missing or unreadable: keep empty cache
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isObject()) m_cache = doc.object();
}

QVariant SettingsImpl::get(const QString& key, const QVariant& defaultValue) const {
    const QStringList parts = key.split(QLatin1Char('.'));
    QMutexLocker lock(&m_mutex);
    const QJsonValue v = lookup(m_cache, parts);
    if (v.isUndefined()) return defaultValue;
    return decryptFromDisk(v);
}

void SettingsImpl::set(const QString& key, const QVariant& value) {
    const QStringList parts = key.split(QLatin1Char('.'));
    QVector<ChangeHandler> toFire;
    {
        QMutexLocker lock(&m_mutex);
        const QJsonValue onDisk = encryptForDisk(key, value);
        assign(m_cache, parts, onDisk);
        save();
        toFire = m_handlers.value(key);
    }
    for (const auto& h : toFire) h(value);
}

void SettingsImpl::onChange(const QString& key, ChangeHandler handler) {
    QMutexLocker lock(&m_mutex);
    m_handlers[key].append(std::move(handler));
}

void SettingsImpl::remove(const QString& key) {
    const QStringList parts = key.split(QLatin1Char('.'));
    QMutexLocker lock(&m_mutex);
    erase(m_cache, parts);
    save();
}

void SettingsImpl::registerEncryptedKeys(const QSet<QString>& keys) {
    QMutexLocker lock(&m_mutex);
    for (const QString& k : keys) m_encryptedKeys.insert(k);
}

const std::vector<uint8_t>& SettingsImpl::masterKey() const {
    if (!m_masterKeyTried) {
        m_masterKeyTried = true;
        try {
            m_masterKey = Keyring::getOrCreateMasterKey();
        } catch (...) {
            // OS keyring unavailable — encryption silently degrades to
            // plaintext. Audit hook: 07-privacy-security.md logs this on
            // bootstrap; we don't double-log here.
            m_masterKey.clear();
        }
    }
    return m_masterKey;
}

QJsonValue SettingsImpl::encryptForDisk(const QString& key, const QVariant& value) const {
    if (!m_encryptedKeys.contains(key)) return QJsonValue::fromVariant(value);

    const auto& mk = masterKey();
    if (mk.empty()) {
        fprintf(stderr,
                "[WARN] Settings: encryption requested for %s but master key unavailable; "
                "falling back to plaintext\n",
                qPrintable(key));
        return QJsonValue::fromVariant(value);
    }

    const QByteArray plaintext = value.toString().toUtf8();
    const QByteArray packed = Keyring::encryptField(plaintext, mk);
    if (packed.size() < 12 + 16) {
        fprintf(stderr, "[WARN] Settings: encryptField produced short output for %s\n",
                qPrintable(key));
        return QJsonValue::fromVariant(value);
    }
    // packed layout = nonce(12) || ciphertext || tag(16). Split for the
    // on-disk envelope so the schema matches docs/05-host-services.md §4.4.
    const QByteArray nonce = packed.left(12);
    const QByteArray ctWithTag = packed.mid(12);

    QJsonObject env;
    env[QString::fromLatin1(kEncryptedMarker)] = true;
    env[QString::fromLatin1(kIvKey)] = QString::fromLatin1(nonce.toBase64());
    env[QString::fromLatin1(kCtKey)]  = QString::fromLatin1(ctWithTag.toBase64());
    return env;
}

QVariant SettingsImpl::decryptFromDisk(const QJsonValue& stored) const {
    if (!stored.isObject()) return stored.toVariant();
    const QJsonObject obj = stored.toObject();
    if (obj.value(QString::fromLatin1(kEncryptedMarker)).toBool() != true) {
        return stored.toVariant();
    }

    const auto& mk = masterKey();
    if (mk.empty()) {
        fprintf(stderr,
                "[WARN] Settings: encrypted envelope present but master key unavailable; "
                "returning raw value\n");
        return stored.toVariant();
    }
    const QByteArray nonce = QByteArray::fromBase64(
        obj.value(QString::fromLatin1(kIvKey)).toString().toLatin1());
    const QByteArray ctWithTag = QByteArray::fromBase64(
        obj.value(QString::fromLatin1(kCtKey)).toString().toLatin1());
    if (nonce.size() != 12 || ctWithTag.size() < 16) {
        fprintf(stderr, "[WARN] Settings: malformed encrypted envelope\n");
        return QVariant();
    }
    QByteArray packed;
    packed.reserve(nonce.size() + ctWithTag.size());
    packed.append(nonce);
    packed.append(ctWithTag);

    const QByteArray plaintext = Keyring::decryptField(packed, mk);
    if (plaintext.isEmpty()) {
        fprintf(stderr, "[WARN] Settings: AES-GCM tag mismatch — refusing to return plaintext\n");
        return QVariant();
    }
    return QString::fromUtf8(plaintext);
}

void SettingsImpl::save() {
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly)) {
        fprintf(stderr, "[WARN] Settings: cannot open %s for write\n", qPrintable(m_path));
        return;
    }
    file.write(QJsonDocument(m_cache).toJson(QJsonDocument::Indented));
    if (!file.commit()) fprintf(stderr, "[WARN] Settings: atomic commit failed for %s\n", qPrintable(m_path));
}

QJsonValue SettingsImpl::lookup(const QJsonObject& obj, const QStringList& parts) {
    QJsonValue current = obj;
    for (const QString& part : parts) {
        if (!current.isObject()) return QJsonValue::Undefined;
        current = current.toObject().value(part);
        if (current.isUndefined()) return QJsonValue::Undefined;
    }
    return current;
}

void SettingsImpl::assign(QJsonObject& obj, const QStringList& parts, const QJsonValue& val) {
    if (parts.isEmpty()) return;
    if (parts.size() == 1) {
        obj.insert(parts[0], val);
        return;
    }
    QJsonObject child = obj.value(parts[0]).toObject();
    const QStringList rest = parts.mid(1);
    assign(child, rest, val);
    obj.insert(parts[0], child);
}

void SettingsImpl::erase(QJsonObject& obj, const QStringList& parts) {
    if (parts.isEmpty()) return;
    if (parts.size() == 1) {
        obj.remove(parts[0]);
        return;
    }
    QJsonObject child = obj.value(parts[0]).toObject();
    const QStringList rest = parts.mid(1);
    erase(child, rest);
    obj.insert(parts[0], child);
}

} // namespace Margin
