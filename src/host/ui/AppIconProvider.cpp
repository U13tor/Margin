// AppIconProvider impl — see AppIconProvider.h.

#include "AppIconProvider.h"

#include <QFileInfo>
#include <QImage>
#include <QPixmap>
#include <QString>
#include <QUrl>

#include <cmath>

namespace Margin {

namespace {

// Transparent 16x16 fallback returned when extraction fails or the
// platform has no icon backend. We avoid returning a null QImage because
// QML Image treats null as an error and spams "Failed to get image from
// provider: appicon" — a transparent image is silent.
QImage fallbackImage() {
    QImage img(16, 16, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    return img;
}

} // namespace

AppIconProvider::AppIconProvider()
    // QImage — the provider returns raster data. QQmlImageProviderBase::Image
    // forces QML to go through the async image loader (off the GUI thread
    // for non-trivial sizes).
    : QQuickImageProvider(QQmlImageProviderBase::Image)
{
}

AppIconProvider::~AppIconProvider() = default;

QImage AppIconProvider::requestImage(const QString& id, QSize* size,
                                     const QSize& requestedSize) {
    if (id.isEmpty()) {
        if (size) *size = QSize(16, 16);
        return fallbackImage();
    }

    // Percent-decode the id back to the real exe path. QML calls
    // `encodeURIComponent(exePath)` to keep Windows path separators (`\`)
    // and drive letters (`C:`) from confusing Qt's URL parser — a bare
    // `:` would be misread as a scheme separator and `%5C` is needed so
    // the backslash survives QUrl's normalization. Qt 6's
    // QQuickImageProvider does NOT auto-decode the id — it hands us the
    // raw encoded form, so we must QUrl::fromPercentEncoding here.
    // Earlier code assumed auto-decode; the result was every exePath
    // arriving literally as `C%3A%5CUsers%5C...%5CCode.exe`, which
    // QFileInfo::exists (and SHGetFileInfoW) treated as a non-existent
    // file and every icon fell back to transparent. See
    // docs/15-dev-gotchas.md §A11.
    const QString exePath = QUrl::fromPercentEncoding(id.toUtf8());

    QImage img;
    {
        QMutexLocker lock(&m_cacheMutex);
        const auto it = m_cache.constFind(exePath);
        if (it != m_cache.cend()) {
            img = it.value();
        }
    }
    if (img.isNull()) {
        img = extractIcon(exePath);
        if (img.isNull()) img = fallbackImage();
        QMutexLocker lock(&m_cacheMutex);
        m_cache.insert(exePath, img);
    }

    if (size) *size = img.size();

    // Honour requestedSize — QML delegates ask for 16x16 typically; the
    // shell icon is usually 32x32. Scaled smooth on the loader thread,
    // not the GUI thread. Qt's smooth transform is good enough for the
    // 32→16 downsample; no need for a manual high-quality resampler.
    if (requestedSize.isValid() && !requestedSize.isEmpty() &&
        requestedSize != img.size()) {
        return img.scaled(requestedSize, Qt::KeepAspectRatio,
                          Qt::SmoothTransformation);
    }
    return img;
}

bool AppIconProvider::isCachedForTesting(const QString& path) const {
    QMutexLocker lock(&m_cacheMutex);
    return m_cache.contains(path);
}

} // namespace Margin

// ── Platform extraction ─────────────────────────────────────────────────
// Win: SHGetFileInfoW with SHGFI_ICON | SHGFI_LARGEICON extracts the
// .exe's shell icon (usually the application's main icon resource).
// Non-Win: deferred per docs/12 §A19. Mac will use NSWorkspace.iconForFile
// when a backend lands.

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <ShellAPI.h>
#endif

namespace Margin {

QImage AppIconProvider::extractIcon(const QString& exePath) {
#ifdef Q_OS_WIN
    if (exePath.isEmpty()) return QImage();
    if (!QFileInfo::exists(exePath)) return QImage();

    SHFILEINFOW sfi = {};
    // SHGFI_ICON | SHGFI_LARGEICON → 32x32 shell icon. The caller
    // (requestImage) handles downscaling. We don't request SHGFI_SMALLICON
    // (16x16) because the large variant scales down cleaner than the
    // small variant scales up.
    const DWORD flags = SHGFI_ICON | SHGFI_LARGEICON;
    if (!SHGetFileInfoW(reinterpret_cast<LPCWSTR>(exePath.utf16()),
                        0, &sfi, sizeof(sfi), flags)) {
        return QImage();
    }
    if (sfi.hIcon == nullptr) return QImage();

    // HICON → QImage via Qt 6's public QtGui API. (Qt 5 had QPixmap::
    // fromWinHICON; Qt 6.0 moved it to QImage::fromHICON — the per-pixel
    // alpha + mask flatten happens inside Qt.)
    const QImage img = QImage::fromHICON(sfi.hIcon);
    DestroyIcon(sfi.hIcon);
    return img;
#else
    // Mac / Linux deferred — see docs/12 §A19 (Mac ActiveWindowTracker +
    // icon backend). Returning null here makes requestImage substitute
    // the transparent fallback.
    Q_UNUSED(exePath);
    return QImage();
#endif
}

} // namespace Margin
