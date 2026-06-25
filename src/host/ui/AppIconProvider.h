// AppIconProvider — QQuickImageProvider that resolves per-app icons
// from an executable path (PR3 round-2 #2b/4a).
//
// QML binds `Image { source: "image://appicon/" + encodeURIComponent(exePath) }`.
// The provider extracts the .exe's shell icon via SHGetFileInfoW on Windows
// and returns it as a QImage. Mac deferred per docs/12 §A19 — the provider
// still constructs on Mac but returns a transparent fallback.
//
// Cache: QHash<QString, QImage> keyed on the canonical path. Icon extraction
// is ~1ms per unique .exe but QML may request the same path hundreds of
// times (BarChart delegates recycle) — cache is essential. Thread-safe
// via std::mutex because QQuickImageProvider::requestImage may be called
// concurrently from the image loader thread pool.

#pragma once

#include <QImage>
#include <QQuickImageProvider>

#include <QHash>
#include <QMutex>
#include <QString>

namespace Margin {

class AppIconProvider : public QQuickImageProvider {
public:
    AppIconProvider();
    ~AppIconProvider() override;

    // QQuickImageProvider override. `id` is the URL path component after
    // "image://appicon/" — callers should URL-encode the exe path
    // (encodeURIComponent in QML) to survive special chars. requestedSize
    // is honoured as a scaling hint; 0x0 returns the native size.
    QImage requestImage(const QString& id, QSize* size,
                        const QSize& requestedSize) override;

    // Test surface — returns true iff `path` is in the cache. Const +
    // locking so tests can introspect without racing the loader thread.
    bool isCachedForTesting(const QString& path) const;

private:
    QImage extractIcon(const QString& exePath);

    mutable QMutex m_cacheMutex;
    QHash<QString, QImage> m_cache;
};

} // namespace Margin
