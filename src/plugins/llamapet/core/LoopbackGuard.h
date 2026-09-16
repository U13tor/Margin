#pragma once

#include <QUrl>
#include <QString>
#include <QtGlobal>

namespace Margin::Plugins::LlamaPet {

/**
 * @brief LoopbackGuard enforces the zero-external-network policy established
 * by the localhost-http ADR (2026-09-15).
 *
 * Requests can ONLY target 127.0.0.1, localhost, or ::1. Any attempt to access
 * non-loopback hosts or external networks will fail validation and assert.
 */
class LoopbackGuard {
public:
    static bool isLoopback(const QUrl& url) {
        if (!url.isValid()) {
            return false;
        }

        const QString host = url.host().trimmed().toLower();
        if (host.isEmpty()) {
            return false;
        }

        if (host == QStringLiteral("127.0.0.1") ||
            host == QStringLiteral("localhost") ||
            host == QStringLiteral("::1") ||
            host == QStringLiteral("[::1]")) {
            return true;
        }

        return false;
    }

    static bool validate(const QUrl& url) {
        if (!isLoopback(url)) {
            Q_ASSERT_X(false, "LoopbackGuard", "Network access outside local loopback is strictly prohibited!");
            return false;
        }
        return true;
    }
};

} // namespace Margin::Plugins::LlamaPet
