// OverlayRegistry impl — see header.

#include "host/core/OverlayRegistry.h"

#include "Margin/OverlayContributor.h"

#include <algorithm>

#include <QVariantMap>

namespace Margin {

void OverlayRegistry::addContributor(OverlayContributor* c) {
    if (!c) return;
    m_contributors.push_back(c);
}

void OverlayRegistry::clear() {
    m_contributors.clear();
    if (!m_activeUrls.empty()) {
        m_activeUrls.clear();
        rebuildCache();
        emit activeOverlaysChanged();
    }
}

void OverlayRegistry::pollAll() {
    std::vector<QUrl> next;
    next.reserve(m_contributors.size());
    for (auto* c : m_contributors) {
        if (c && c->shouldShow()) {
            next.push_back(c->overlayUrl());
        }
    }
    // Sort so two contributors returning the same URL across polls compare
    // equal regardless of address-order — and std::vector::operator== does
    // element-wise compare which is what we want.
    if (next == m_activeUrls) return;
    m_activeUrls = std::move(next);
    rebuildCache();
    emit activeOverlaysChanged();
}

void OverlayRegistry::rebuildCache() {
    QVariantList out;
    out.reserve(static_cast<int>(m_activeUrls.size()));
    for (const auto& url : m_activeUrls) {
        QVariantMap m;
        m.insert(QStringLiteral("overlayQml"), url.toDisplayString());
        out.append(m);
    }
    m_cachedActive = std::move(out);
}

} // namespace Margin
