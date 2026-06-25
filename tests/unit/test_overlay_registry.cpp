// test_overlay_registry — M3-C4: OverlayRegistry polls contributors and
// caches a QVariantList of { overlayQml } for QML consumption. Mirrors
// test_dashboard_registry_cache's pattern: verify the cache identity + that
// pollAll emits activeOverlaysChanged only when the active set actually
// changes (not on every poll).
//
// A fake contributor with settable shouldShow + overlayUrl drives the registry.
// This keeps the test pure-logic — no QML engine, no plugin DLL.

#include <QMetaObject>
#include <QObject>
#include <QSignalSpy>
#include <QString>
#include <QTest>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include "Margin/OverlayContributor.h"
#include "host/core/OverlayRegistry.h"

namespace {

class FakeContributor : public Margin::OverlayContributor {
public:
    bool shouldShow() const override   { return m_show; }
    QUrl overlayUrl() const override   { return m_url; }
    void dismiss() override            { m_dismissed = true; }

    bool m_show     = false;
    bool m_dismissed = false;
    QUrl m_url       = QUrl(QStringLiteral("qrc:/fake/overlay.qml"));
};

} // namespace

class TestOverlayRegistry : public QObject {
    Q_OBJECT

private slots:
    void emptyRegistryHasNoActive();
    void showTrueAddsOverlay();
    void showFalseRemovesOverlay();
    void stablePollDoesNotEmit();
    void multipleContributors();
    void clearDropsEverything();
};

void TestOverlayRegistry::emptyRegistryHasNoActive() {
    Margin::OverlayRegistry r;
    QVERIFY(r.activeOverlays().isEmpty());
    QSignalSpy spy(&r, &Margin::OverlayRegistry::activeOverlaysChanged);
    r.pollAll();
    QCOMPARE(spy.count(), 0);  // no contributors → no change
}

void TestOverlayRegistry::showTrueAddsOverlay() {
    Margin::OverlayRegistry r;
    FakeContributor c;
    r.addContributor(&c);

    QSignalSpy spy(&r, &Margin::OverlayRegistry::activeOverlaysChanged);
    c.m_show = true;
    r.pollAll();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(r.activeOverlays().size(), 1);
    QCOMPARE(r.activeOverlays()[0].toMap().value("overlayQml").toString(),
             QStringLiteral("qrc:/fake/overlay.qml"));
}

void TestOverlayRegistry::showFalseRemovesOverlay() {
    Margin::OverlayRegistry r;
    FakeContributor c;
    r.addContributor(&c);
    c.m_show = true;
    r.pollAll();
    QCOMPARE(r.activeOverlays().size(), 1);

    QSignalSpy spy(&r, &Margin::OverlayRegistry::activeOverlaysChanged);
    c.m_show = false;
    r.pollAll();
    QCOMPARE(spy.count(), 1);
    QVERIFY(r.activeOverlays().isEmpty());
}

void TestOverlayRegistry::stablePollDoesNotEmit() {
    Margin::OverlayRegistry r;
    FakeContributor c;
    r.addContributor(&c);
    c.m_show = true;

    r.pollAll();  // first poll emits
    QSignalSpy spy(&r, &Margin::OverlayRegistry::activeOverlaysChanged);
    r.pollAll();  // state unchanged → no emit
    r.pollAll();  // ditto
    QCOMPARE(spy.count(), 0);
    QCOMPARE(r.activeOverlays().size(), 1);  // still active
}

void TestOverlayRegistry::multipleContributors() {
    Margin::OverlayRegistry r;
    FakeContributor a, b;
    a.m_url = QUrl(QStringLiteral("qrc:/a/overlay.qml"));
    b.m_url = QUrl(QStringLiteral("qrc:/b/overlay.qml"));
    r.addContributor(&a);
    r.addContributor(&b);

    a.m_show = true;
    b.m_show = true;
    r.pollAll();
    QCOMPARE(r.activeOverlays().size(), 2);

    // Drop only b — a stays active.
    b.m_show = false;
    r.pollAll();
    QCOMPARE(r.activeOverlays().size(), 1);
    QCOMPARE(r.activeOverlays()[0].toMap().value("overlayQml").toString(),
             QStringLiteral("qrc:/a/overlay.qml"));
}

void TestOverlayRegistry::clearDropsEverything() {
    Margin::OverlayRegistry r;
    FakeContributor c;
    r.addContributor(&c);
    c.m_show = true;
    r.pollAll();
    QCOMPARE(r.activeOverlays().size(), 1);

    r.clear();
    QVERIFY(r.activeOverlays().isEmpty());
    // Safe to poll after clear (no contributors to dereference).
    r.pollAll();
    QVERIFY(r.activeOverlays().isEmpty());
}

QTEST_MAIN(TestOverlayRegistry)
#include "test_overlay_registry.moc"
