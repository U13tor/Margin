// tests/unit/test_lock_service.cpp
//
// M1-C4: PlatformBackend factory + LockService contract. Verifies:
//   - PlatformBackend::create() returns a non-null impl on Win
//   - returned backend IS-A LockService (inheritance wiring)
//   - isSupported() reports true on Win
//   - custom LockService subclass (mock) is callable for plugin-side tests
//
// Does NOT call PlatformBackendWin::lockScreen() — that would invoke the
// real LockWorkStation and lock the dev/CI machine. Lock trigger is
// exercised end-to-end in commit 5 (state-machine test uses a fake
// tracker + fake lock service to avoid hardware/OS side effects).

#include <QObject>
#include <QTest>

#include <memory>

#include "Margin/LockService.h"
#include "host/platform/PlatformBackend.h"

using Margin::LockService;
using Margin::PlatformBackend;

namespace {

class FakeLockService : public LockService {
public:
    int calls = 0;
    void lockScreen() override { ++calls; }
    bool isSupported() const override { return true; }
};

} // namespace

class TestLockService : public QObject {
    Q_OBJECT

private slots:
    void fakeLockServiceHonoursContract();
    void platformBackendFactoryReturnsImplOnWin();
    void platformBackendIsLockService();
    void platformBackendReportsSupported();
};

void TestLockService::fakeLockServiceHonoursContract() {
    FakeLockService s;
    QCOMPARE(s.calls, 0);
    s.lockScreen();
    s.lockScreen();
    QCOMPARE(s.calls, 2);
    QVERIFY(s.isSupported());
}

void TestLockService::platformBackendFactoryReturnsImplOnWin() {
    auto backend = PlatformBackend::create();
#ifdef _WIN32
    QVERIFY(backend != nullptr);
#else
    QVERIFY(backend == nullptr);
#endif
}

void TestLockService::platformBackendIsLockService() {
    auto backend = PlatformBackend::create();
#ifdef _WIN32
    QVERIFY(backend);
    // Implicit upcast — PlatformBackend inherits LockService (is-a) so
    // HostServicesImpl can store a single pointer that satisfies both.
    LockService* asService = backend.get();
    QVERIFY(asService != nullptr);
#else
    QVERIFY(!backend);
#endif
}

void TestLockService::platformBackendReportsSupported() {
    auto backend = PlatformBackend::create();
#ifdef _WIN32
    QVERIFY(backend);
    QVERIFY(backend->isSupported());
#else
    QVERIFY(!backend);
#endif
}

QTEST_MAIN(TestLockService)
#include "test_lock_service.moc"
