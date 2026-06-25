// tests/unit/test_keyring.cpp
//
// Round-trip exercise of Keyring low-level KV primitive (store → load →
// clear → load returns nullopt) against the OS keyring. Uses a throwaway
// service="Margin-test" so we never collide with the real master key under
// service="Margin".
//
// Platform note: Win-only at CMake registration. On macOS the Keyring
// branches qFatal (deferred to docs/12-deferred-items.md C6).

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QTest>

#include "host/security/Keyring.h"

using Margin::Keyring;

namespace {
constexpr char kService[] = "Margin-test";

// Pre-clean + post-clean a fixed key name to keep the test idempotent.
struct ScopedSecret {
    QString key;
    explicit ScopedSecret(QString k) : key(std::move(k)) {
        Keyring::clear(QLatin1String(kService), key);
    }
    ~ScopedSecret() {
        Keyring::clear(QLatin1String(kService), key);
    }
    ScopedSecret(const ScopedSecret&) = delete;
    ScopedSecret& operator=(const ScopedSecret&) = delete;
};
} // namespace

class TestKeyring : public QObject {
    Q_OBJECT

private slots:
    void testRoundTrip();
    void testClearIsIdempotent();
    void testLoadMissingReturnsNullopt();
    void testEmptyValueIsTreatedAsAbsent();
};

void TestKeyring::testRoundTrip() {
    ScopedSecret guard(QStringLiteral("rt"));

    const QByteArray payload("hello-margin");
    QVERIFY(Keyring::store(QLatin1String(kService), guard.key, payload));

    const auto loaded = Keyring::load(QLatin1String(kService), guard.key);
    QVERIFY(loaded.has_value());
    QCOMPARE(*loaded, payload);

    QVERIFY(Keyring::clear(QLatin1String(kService), guard.key));
    QVERIFY(!Keyring::load(QLatin1String(kService), guard.key).has_value());
}

void TestKeyring::testClearIsIdempotent() {
    ScopedSecret guard(QStringLiteral("idempotent"));

    QVERIFY(Keyring::store(QLatin1String(kService), guard.key, QByteArray("xyz")));
    QVERIFY(Keyring::clear(QLatin1String(kService), guard.key));
    // Clearing an already-removed entry is still success.
    QVERIFY(Keyring::clear(QLatin1String(kService), guard.key));
}

void TestKeyring::testLoadMissingReturnsNullopt() {
    ScopedSecret guard(QStringLiteral("never-stored"));
    // Never stored — load must return nullopt (we did pre-clean in the ctor).
    const auto v = Keyring::load(QLatin1String(kService), guard.key);
    QVERIFY(!v.has_value());
}

void TestKeyring::testEmptyValueIsTreatedAsAbsent() {
    // Production caller (getOrCreateMasterKey) always passes a 32B value, so
    // empty-value support is degenerate. DPAPI produces an empty blob for an
    // empty input, which load() treats as missing — acceptable semantics.
    ScopedSecret guard(QStringLiteral("empty"));

    QVERIFY(Keyring::store(QLatin1String(kService), guard.key, QByteArray()));
    QVERIFY(!Keyring::load(QLatin1String(kService), guard.key).has_value());
}

QTEST_MAIN(TestKeyring)
#include "test_keyring.moc"
