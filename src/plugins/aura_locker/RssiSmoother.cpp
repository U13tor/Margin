// src/plugins/aura_locker/RssiSmoother.cpp

#include "RssiSmoother.h"

namespace Margin::Plugins::Aura {

RssiSmoother::RssiSmoother(int windowSize)
    : m_windowSize(windowSize > 0 ? windowSize : kDefaultWindowSize) {}

void RssiSmoother::push(qint16 rssiDbm) {
    m_samples.push_back(rssiDbm);
    while (static_cast<int>(m_samples.size()) > m_windowSize) {
        m_samples.pop_front();
    }
}

std::optional<double> RssiSmoother::average() const {
    if (m_samples.empty()) {
        return std::nullopt;
    }
    qint64 sum = 0;
    for (auto v : m_samples) {
        sum += v;
    }
    return static_cast<double>(sum) / static_cast<double>(m_samples.size());
}

void RssiSmoother::reset() {
    m_samples.clear();
}

void RssiSmoother::setWindowSize(int windowSize) {
    if (windowSize <= 0) {
        return;
    }
    m_windowSize = windowSize;
    while (static_cast<int>(m_samples.size()) > m_windowSize) {
        m_samples.pop_front();
    }
}

} // namespace Margin::Plugins::Aura
