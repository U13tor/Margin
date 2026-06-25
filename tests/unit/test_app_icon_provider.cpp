// tests/unit/test_app_icon_provider.cpp
//
// Validates AppIconProvider — the image://appicon/<exe-path> bridge from
// QML delegates to SHGetFileInfoW on Win (PR3 round-2 #2b/4a).
//
// Coverage:
//   1. Known system exe (notepad.exe) → non-empty QImage + cache populated.
//   2. Second request for same path → served from cache (isCachedForTesting true).
//   3. Empty id → transparent fallback (not null, so QML Image stays quiet).
//   4. Non-existent path → transparent fallback (graceful degradation).
//   5. requestedSize honoured (downsample from 32→16 stays square).
//
// Mac / Linux: provider constructs but extractIcon returns null (deferred
// per docs/12 §A19). Test asserts the transparent-fallback path instead
// of skipping — the fallback contract is platform-independent.

#include <QObject>
#include <QFileInfo>
#include <QImage>
#include <QSize>
#include <QTest>

#include "host/ui/AppIconProvider.h"

using Margin::AppIconProvider;

class TestAppIconProvider : public QObject {
    Q_OBJECT

private slots:
    void testEmptyIdReturnsFallback();
    void testNonExistentPathReturnsFallback();
    void testCachePopulatedOnSecondRequest();

#ifdef Q_OS_WIN
    void testKnownSystemExeReturnsIcon();
    void testRequestedSizeDownsample();
#endif
};

void TestAppIconProvider::testEmptyIdReturnsFallback() {
    AppIconProvider provider;
    QSize reportedSize;
    const QImage img = provider.requestImage(QString(), &reportedSize, QSize());
    QCOMPARE(img.isNull(), false);
    QCOMPARE(img.width() > 0, true);
    QCOMPARE(reportedSize.width(), img.width());
}

void TestAppIconProvider::testNonExistentPathReturnsFallback() {
    AppIconProvider provider;
    QSize reportedSize;
    const QImage img = provider.requestImage(
        QStringLiteral("Z:/no/such/exe.exe"), &reportedSize, QSize());
    QCOMPARE(img.isNull(), false);
    QCOMPARE(img.width() > 0, true);
}

void TestAppIconProvider::testCachePopulatedOnSecondRequest() {
    // Even when the underlying extractor returns nothing (Mac/Linux) or
    // a transparent fallback (Win missing path), the cache should record
    // the lookup so the next request is served without re-entering the
    // extractor. This is the contract MBarChart delegates rely on for
    // scroll-performance.
    AppIconProvider provider;
    const QString bogusPath = QStringLiteral("Z:/no/such/exe.exe");
    QVERIFY(!provider.isCachedForTesting(bogusPath));
    QSize size;
    provider.requestImage(bogusPath, &size, QSize());
    QVERIFY(provider.isCachedForTesting(bogusPath));
}

#ifdef Q_OS_WIN

void TestAppIconProvider::testKnownSystemExeReturnsIcon() {
    // notepad.exe exists on every Windows install back to NT 4.0. Its
    // shell icon is a non-trivial blue-and-white page, so a successful
    // extraction returns a 32x32 non-transparent QImage.
    const QString notepad = QStringLiteral("C:/Windows/System32/notepad.exe");
    if (!QFileInfo::exists(notepad)) {
        QSKIP("notepad.exe not at expected path on this machine");
    }

    AppIconProvider provider;
    QSize reportedSize;
    const QImage img = provider.requestImage(notepad, &reportedSize, QSize());
    QVERIFY(!img.isNull());
    QCOMPARE(img.width() > 0, true);
    QCOMPARE(img.height() > 0, true);
    QCOMPARE(reportedSize.width(), img.width());
    QVERIFY(provider.isCachedForTesting(notepad));
}

void TestAppIconProvider::testRequestedSizeDownsample() {
    // requestedSize=16x16 should return a 16x16 image. We don't assert
    // against the *content* of the downsample — only the dimensions —
    // because Qt's smooth transform is allowed to differ pixel-by-pixel
    // across patch versions of Qt.
    const QString notepad = QStringLiteral("C:/Windows/System32/notepad.exe");
    if (!QFileInfo::exists(notepad)) {
        QSKIP("notepad.exe not at expected path on this machine");
    }

    AppIconProvider provider;
    QSize reportedSize;
    const QImage img = provider.requestImage(notepad, &reportedSize, QSize(16, 16));
    QVERIFY(!img.isNull());
    QCOMPARE(img.width(), 16);
    QCOMPARE(img.height(), 16);
}

#endif // Q_OS_WIN

QTEST_MAIN(TestAppIconProvider)
#include "test_app_icon_provider.moc"
