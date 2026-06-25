// tests/unit/test_dark_title_bar.cpp
//
// M4-C10 / M0-C10c: PlatformBackend::applyDarkTitleBar defensive contract.
// Verifies the call is safe across the three caller scenarios HostCore
// could realistically hit:
//   (1) nullptr window — early return, no crash
//   (2) QWindow that has never been shown — winId() may lazily create a
//       handle; the call must still return without throwing
//   (3) shown QWindow — exercises the real DwmSetWindowAttribute path
//
// Does NOT assert the DWM attribute actually flipped (reading it back via
// DwmGetWindowAttribute is itself flaky on Win10 1809). Visual confirmation
// is an L4 manual step (docs/11-roadmap.md M4-C10 完成标志).
//
// Windows-only — Mac impl deferred per docs/12 §A15.

#include <QObject>
#include <QTest>
#include <QWindow>

#include <memory>

#include "host/platform/PlatformBackend.h"

using Margin::PlatformBackend;

class TestDarkTitleBar : public QObject {
    Q_OBJECT

private slots:
    void nullWindowIsNoOp();
    void unshownWindowIsNoOp();
    void shownWindowDoesNotCrash();
};

void TestDarkTitleBar::nullWindowIsNoOp() {
    auto backend = PlatformBackend::create();
#ifdef _WIN32
    QVERIFY(backend);
    // No QCOMPARE — the contract is "returns without throwing / crashing".
    backend->applyDarkTitleBar(nullptr);
#else
    QSKIP("applyDarkTitleBar is Windows-only");
#endif
}

void TestDarkTitleBar::unshownWindowIsNoOp() {
    auto backend = PlatformBackend::create();
#ifdef _WIN32
    QVERIFY(backend);
    QWindow w;
    // winId() may lazy-create a handle even without show(); the call
    // must not crash either way.
    backend->applyDarkTitleBar(&w);
#else
    QSKIP("applyDarkTitleBar is Windows-only");
#endif
}

void TestDarkTitleBar::shownWindowDoesNotCrash() {
    auto backend = PlatformBackend::create();
#ifdef _WIN32
    QVERIFY(backend);
    QWindow w;
    w.setWidth(100);
    w.setHeight(100);
    w.show();
    backend->applyDarkTitleBar(&w);
    w.hide();
#else
    QSKIP("applyDarkTitleBar is Windows-only");
#endif
}

QTEST_MAIN(TestDarkTitleBar)
#include "test_dark_title_bar.moc"
