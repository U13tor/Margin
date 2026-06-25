// test_aura_encrypted_settings — M1-C7 transparent Settings encryption.
// Drives the real SettingsImpl against a temp config dir to verify:
//   1. A key registered via registerEncryptedKeys gets wrapped on disk as
//      {"__encrypted__": true, "iv": ..., "ct": ...} (not raw plaintext).
//   2. The same value round-trips back through get() as plaintext.
//   3. A non-registered key under the same plugin namespace stays plaintext.
//
// Skipped on non-Win because Keyring::getOrCreateMasterKey is Win-only
// (Mac deferred to docs/12-deferred-items.md §A18 / former C6).

#include "host/services/Settings.h"
#include "host/security/Keyring.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QString>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <cstdio>

class TestAuraEncryptedSettings : public QObject {
    Q_OBJECT

private slots:
    void encryptedKeyRoundTrips();
    void nonRegisteredKeyStaysPlaintext();
    void tagMismatchReturnsEmpty();
};

namespace {
QJsonObject loadRawSettings(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    const auto doc = QJsonDocument::fromJson(f.readAll());
    return doc.object();
}

QJsonValue walkValue(const QJsonObject& obj, const QStringList& parts) {
    QJsonValue cur = obj;
    for (const QString& p : parts) {
        if (!cur.isObject()) return {};
        cur = cur.toObject().value(p);
    }
    return cur;
}

QJsonObject walkObject(const QJsonObject& obj, const QStringList& parts) {
    return walkValue(obj, parts).toObject();
}
} // namespace

void TestAuraEncryptedSettings::encryptedKeyRoundTrips() {
#ifdef Q_OS_WIN
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    Margin::SettingsImpl s(tmp.path());

    const QString key = QStringLiteral("plugins.aura.paired_device_identifier");
    const QString secret = QStringLiteral("AA:BB:CC:DD:EE:FF");

    s.registerEncryptedKeys(QSet<QString>{ key });
    s.set(key, secret);
    fprintf(stderr, "[roundTrips] set done, reading back\n");

    // Read back through the API — should be plaintext.
    const QString got = s.get(key, {}).toString();
    fprintf(stderr, "[roundTrips] got='%s' secret='%s'\n",
            qPrintable(got), qPrintable(secret));
    QCOMPARE(got, secret);

    // Read the raw JSON file — must NOT contain the MAC in plaintext.
    const QJsonObject root = loadRawSettings(tmp.path() + "/settings.json");
    const QJsonObject env = walkObject(root, { QStringLiteral("plugins"),
                                          QStringLiteral("aura"),
                                          QStringLiteral("paired_device_identifier") });
    fprintf(stderr, "[roundTrips] envelope isEmpty=%d\n", env.isEmpty());
    QVERIFY(!env.isEmpty());
    QCOMPARE(env.value(QStringLiteral("__encrypted__")).toBool(), true);
    QVERIFY(!env.value(QStringLiteral("iv")).toString().isEmpty());
    QVERIFY(!env.value(QStringLiteral("ct")).toString().isEmpty());

    QFile rawFile(tmp.path() + "/settings.json");
    QVERIFY(rawFile.open(QIODevice::ReadOnly));
    const QString raw = QString::fromUtf8(rawFile.readAll());
    fprintf(stderr, "[roundTrips] raw contains secret? %d\n",
            int(raw.contains(secret)));
    QVERIFY(!raw.contains(secret));
#else
    QSKIP("Keyring master-key fetch is Win-only (Mac deferred to §A18)");
#endif
}

void TestAuraEncryptedSettings::nonRegisteredKeyStaysPlaintext() {
#ifdef Q_OS_WIN
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    Margin::SettingsImpl s(tmp.path());

    const QString key = QStringLiteral("plugins.aura.rssi_threshold");
    s.set(key, -65);
    QCOMPARE(s.get(key, {}).toInt(), -65);

    const QJsonObject root = loadRawSettings(tmp.path() + "/settings.json");
    const QJsonValue v = walkValue(root, { QStringLiteral("plugins"),
                                      QStringLiteral("aura"),
                                      QStringLiteral("rssi_threshold") });
    fprintf(stderr, "[nonRegistered] raw value=%d\n", int(v.toInt(-999)));
    QCOMPARE(v.toInt(-999), -65);
#else
    QSKIP("Keyring master-key fetch is Win-only (Mac deferred to §A18)");
#endif
}

void TestAuraEncryptedSettings::tagMismatchReturnsEmpty() {
#ifdef Q_OS_WIN
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    Margin::SettingsImpl s(tmp.path());

    const QString key = QStringLiteral("plugins.aura.device_name");
    s.registerEncryptedKeys(QSet<QString>{ key });
    s.set(key, QStringLiteral("Pixel 8"));

    // Tamper: locate the "ct" field in the raw JSON and flip a base64
    // char so the AES-GCM tag check fails on next load.
    QFile f(tmp.path() + "/settings.json");
    QVERIFY(f.open(QIODevice::ReadOnly));
    QByteArray raw = f.readAll();
    f.close();
    fprintf(stderr, "[tagMismatch] raw.size=%lld first16=", (long long)raw.size());
    for (int i = 0; i < qMin(int(raw.size()), 16); ++i) {
        fprintf(stderr, "%02x ", uchar(raw[i]));
    }
    fprintf(stderr, "\n");
    QByteArray flat = raw;
    flat.replace('\n', '|');
    fprintf(stderr, "[tagMismatch] flat: %s\n", flat.constData());
    // Pretty-printed JSON serializes as "ct": "..." (with space). Find
    // the ct field's first base64 char and flip it to break the tag.
    const int ctKeyPos = raw.indexOf("\"ct\":");
    QVERIFY(ctKeyPos >= 0);
    const int colonPos = raw.indexOf(':', ctKeyPos);
    QVERIFY(colonPos >= 0);
    const int firstQuote = raw.indexOf('"', colonPos);
    QVERIFY(firstQuote >= 0);
    const int firstCharPos = firstQuote + 1;  // first base64 char inside ct
    QVERIFY(firstCharPos < raw.size());
    raw[firstCharPos] = (raw[firstCharPos] == 'A') ? 'B' : 'A';
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(raw);
    f.close();

    Margin::SettingsImpl s2(tmp.path());
    const QVariant got = s2.get(key, {});
    fprintf(stderr, "[tagMismatch] got.isValid=%d\n", int(got.isValid()));
    QVERIFY(!got.isValid());
#else
    QSKIP("Keyring master-key fetch is Win-only (Mac deferred to §A18)");
#endif
}

QTEST_MAIN(TestAuraEncryptedSettings)
#include "test_aura_encrypted_settings.moc"
