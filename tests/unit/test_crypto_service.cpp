// tests/unit/test_crypto_service.cpp
//
// Validates HKDF per-plugin isolation (docs/05-host-services.md §8.2): a
// ciphertext produced by pluginA's CryptoService cannot be decrypted by
// pluginB's CryptoService, because HKDF derives distinct per-plugin keys
// from the master key. Also verifies GCM nonce randomness (identical
// plaintext yields distinct ciphertexts) and tag-fail on tamper.
//
// Uses a deterministic test master so it does not touch the OS keyring.

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QTest>

#include "host/security/CryptoServicePool.h"

using Margin::CryptoServicePool;
using Margin::CryptoServiceImpl;

namespace {
std::unique_ptr<CryptoServicePool> makePool() {
    // Deterministic 32B master for repeatable test runs. The pool zeroizes
    // its copy on destruction.
    std::vector<uint8_t> master(32, 0x42);
    return CryptoServicePool::create(std::move(master));
}
} // namespace

class TestCryptoService : public QObject {
    Q_OBJECT

private slots:
    void testRoundTrip();
    void testCrossPluginIsolation();
    void testNonceRandomness();
    void testCorruptCiphertextFails();
    void testEmptyPlaintextRoundTrips();
};

void TestCryptoService::testRoundTrip() {
    auto pool = makePool();
    CryptoServiceImpl& a = pool->getOrCreate(QStringLiteral("pluginA"));

    const QString secret = QStringLiteral("hello-margin");
    const QByteArray ct = a.encryptString(secret);
    QVERIFY(!ct.isEmpty());

    const QString pt = a.decryptString(ct);
    QCOMPARE(pt, secret);
}

void TestCryptoService::testCrossPluginIsolation() {
    auto pool = makePool();
    CryptoServiceImpl& a = pool->getOrCreate(QStringLiteral("pluginA"));
    CryptoServiceImpl& b = pool->getOrCreate(QStringLiteral("pluginB"));

    const QByteArray ct = a.encryptString(QStringLiteral("plugin-A-secret"));
    QVERIFY(!ct.isEmpty());

    // Plugin B must NOT be able to decrypt plugin A's ciphertext — HKDF
    // derived distinct keys for the two plugin ids.
    const QString leaked = b.decryptString(ct);
    QVERIFY2(leaked.isNull(), "Plugin B decrypted plugin A's ciphertext — HKDF isolation broken");

    // Plugin A can decrypt its own ciphertext.
    const QString recovered = a.decryptString(ct);
    QCOMPARE(recovered, QStringLiteral("plugin-A-secret"));
}

void TestCryptoService::testNonceRandomness() {
    auto pool = makePool();
    CryptoServiceImpl& a = pool->getOrCreate(QStringLiteral("pluginA"));

    const QByteArray c1 = a.encryptString(QStringLiteral("same"));
    const QByteArray c2 = a.encryptString(QStringLiteral("same"));
    QVERIFY(!c1.isEmpty());
    QVERIFY(!c2.isEmpty());
    QVERIFY2(c1 != c2, "Identical ciphertexts — GCM nonce is not random");

    QCOMPARE(a.decryptString(c1), QStringLiteral("same"));
    QCOMPARE(a.decryptString(c2), QStringLiteral("same"));
}

void TestCryptoService::testCorruptCiphertextFails() {
    auto pool = makePool();
    CryptoServiceImpl& a = pool->getOrCreate(QStringLiteral("pluginA"));

    const QByteArray ct = a.encryptString(QStringLiteral("payload"));
    QVERIFY(!ct.isEmpty());

    // Flip one base64 character near the middle — GCM tag must fail to verify.
    QByteArray corrupted = ct;
    const int idx = corrupted.size() / 2;
    char* data = corrupted.data();
    data[idx] = (data[idx] == 'A') ? 'B' : 'A';
    QVERIFY2(a.decryptString(corrupted).isNull(),
             "Tampered ciphertext decrypted successfully — GCM tag check missing");
}

void TestCryptoService::testEmptyPlaintextRoundTrips() {
    auto pool = makePool();
    CryptoServiceImpl& a = pool->getOrCreate(QStringLiteral("pluginA"));

    const QByteArray ct = a.encryptString(QString());
    QVERIFY(!ct.isEmpty());

    const QString pt = a.decryptString(ct);
    QVERIFY(pt.isNull() || pt.isEmpty());
}

QTEST_MAIN(TestCryptoService)
#include "test_crypto_service.moc"
