#include <QTest>
#include <QUrl>
#include "core/LoopbackGuard.h"

using namespace Margin::Plugins::LlamaPet;

class TestLoopbackGuard : public QObject {
    Q_OBJECT

private slots:
    void testValidLoopbackUrls() {
        QVERIFY(LoopbackGuard::isLoopback(QUrl(QStringLiteral("http://127.0.0.1:8080/health"))));
        QVERIFY(LoopbackGuard::isLoopback(QUrl(QStringLiteral("http://127.0.0.1:1802/slots"))));
        QVERIFY(LoopbackGuard::isLoopback(QUrl(QStringLiteral("http://localhost:11434/api/ps"))));
        QVERIFY(LoopbackGuard::isLoopback(QUrl(QStringLiteral("http://[::1]:8080/"))));
        QVERIFY(LoopbackGuard::isLoopback(QUrl(QStringLiteral("http://LOCALHOST:8080"))));
    }

    void testBlockedExternalUrls() {
        QVERIFY(!LoopbackGuard::isLoopback(QUrl(QStringLiteral("http://example.com/api"))));
        QVERIFY(!LoopbackGuard::isLoopback(QUrl(QStringLiteral("https://api.openai.com/v1/models"))));
        QVERIFY(!LoopbackGuard::isLoopback(QUrl(QStringLiteral("http://192.168.1.10:8080"))));
        QVERIFY(!LoopbackGuard::isLoopback(QUrl(QStringLiteral("http://10.0.0.1:8080"))));
        QVERIFY(!LoopbackGuard::isLoopback(QUrl(QStringLiteral("http://0.0.0.0:8080"))));
        QVERIFY(!LoopbackGuard::isLoopback(QUrl(QStringLiteral("invalid_url"))));
        QVERIFY(!LoopbackGuard::isLoopback(QUrl()));
    }
};

QTEST_MAIN(TestLoopbackGuard)
#include "tst_loopback_guard.moc"
