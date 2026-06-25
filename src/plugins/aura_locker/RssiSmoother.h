// src/plugins/aura_locker/RssiSmoother.h
//
// Sliding-window RSSI averager for BLE proximity tracking. Smooths out
// per-packet jitter so a single weak advertisement doesn't trip the
// away-detector. Default window N=5 per docs/11-roadmap.md M1-C3.

#pragma once

#include <optional>
#include <deque>

#include <QtGlobal>

namespace Margin::Plugins::Aura {

class RssiSmoother {
public:
    explicit RssiSmoother(int windowSize = kDefaultWindowSize);

    void push(qint16 rssiDbm);
    std::optional<double> average() const;
    void reset();
    void setWindowSize(int windowSize);

    int windowSize() const { return m_windowSize; }
    int sampleCount() const { return static_cast<int>(m_samples.size()); }

    static constexpr int kDefaultWindowSize = 5;

private:
    int m_windowSize;
    std::deque<qint16> m_samples;
};

} // namespace Margin::Plugins::Aura
