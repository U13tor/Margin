// ProximityDetector impl — see header.

#include "ProximityDetector.h"

namespace Margin::Plugins::Aura {

ProximityDetector::ProximityDetector(QObject* parent)
    : QObject(parent) {
    m_awayTimer.setSingleShot(true);
    m_cooldownTimer.setSingleShot(true);
    m_heartbeatTimer.setSingleShot(true);
    connect(&m_awayTimer, &QTimer::timeout, this, &ProximityDetector::onAwayTimer);
    connect(&m_cooldownTimer, &QTimer::timeout, this, &ProximityDetector::onCooldownTimer);
    connect(&m_heartbeatTimer, &QTimer::timeout, this, &ProximityDetector::onHeartbeatTimer);
}

void ProximityDetector::configure(qint16 rssiThresholdDbm, int awayDelaySec, int cooldownSec) {
    m_rssiThresholdDbm = rssiThresholdDbm;
    m_awayDelaySec     = awayDelaySec;
    m_cooldownSec      = cooldownSec;
    m_awayTimer.setInterval(m_awayDelaySec * 1000);
    m_cooldownTimer.setInterval(m_cooldownSec * 1000);
    // Dead-man's switch: 2× away delay. Short enough that a truly-away
    // device fires within a reasonable window (60 s at default 30 s
    // awayDelay); long enough that a brief BLE dropout (adapter hiccup,
    // dense 2.4 GHz traffic) doesn't trip a false lock. UI clamps
    // awayDelay to [10, 120] so the heartbeat ranges [20 s, 240 s].
    m_heartbeatTimer.setInterval(m_awayDelaySec * 2 * 1000);
}

void ProximityDetector::start() {
    m_started = true;
    m_smoother.reset();
    disarmAwayTimer();
    m_cooldownTimer.stop();
    transitionTo(State::Paired);
    // Heartbeat starts ticking the moment we enter Paired — if no sample
    // ever arrives (e.g. paired device already offline at boot), the
    // dead-man's switch still fires after heartbeatSec.
    resetHeartbeat();
}

void ProximityDetector::stop() {
    m_started = false;
    disarmAwayTimer();
    m_cooldownTimer.stop();
    stopHeartbeat();
    m_smoother.reset();
    m_lastSmoothed = 0;
    transitionTo(State::Inactive);
}

void ProximityDetector::pause() {
    // Preserve m_started so resume() can restart. Just bring everything
    // down to Inactive — same as stop() without clearing m_started.
    disarmAwayTimer();
    m_cooldownTimer.stop();
    stopHeartbeat();
    m_smoother.reset();
    m_lastSmoothed = 0;
    transitionTo(State::Inactive);
}

void ProximityDetector::resume() {
    if (!m_started) return;
    m_smoother.reset();
    transitionTo(State::Paired);
    resetHeartbeat();
}

void ProximityDetector::onRssiSample(qint16 rssiDbm) {
    if (m_state == State::Inactive) return;

    m_smoother.push(rssiDbm);
    const auto avg = m_smoother.average();
    if (!avg) return;
    m_lastSmoothed = static_cast<qint16>(*avg);

    switch (m_state) {
        case State::Paired:
            // Every live sample rewinds the dead-man's switch. Threshold
            // (weak signal) and heartbeat (no signal) are the two paths
            // into Away; this is what keeps the heartbeat honest while
            // the device is still broadcasting in range.
            resetHeartbeat();
            if (m_lastSmoothed < m_rssiThresholdDbm) {
                armAwayTimer();
            } else {
                disarmAwayTimer();
            }
            break;

        case State::Away:
            if (m_lastSmoothed >= m_rssiThresholdDbm) {
                // Came back into range → enter cooldown, suppress re-lock.
                disarmAwayTimer();
                m_cooldownTimer.start();
                emit backDetected();
                transitionTo(State::Cooldown);
            }
            break;

        case State::Cooldown:
            // Cooldown timer fires regardless of samples; we just consume
            // samples here to keep the smoother warm for the next Paired.
            break;

        case State::Inactive:
            break;
    }
}

void ProximityDetector::onAwayTimer() {
    // Only meaningful if we're still Paired — Cooldown/Away states shouldn't
    // have the awayTimer running, but be defensive.
    if (m_state != State::Paired) return;

    emit awayDetected(m_lastSmoothed);
    emit lockRequested();
    transitionTo(State::Away);
    stopHeartbeat();
}

void ProximityDetector::onCooldownTimer() {
    if (m_state != State::Cooldown) return;
    transitionTo(State::Paired);
    resetHeartbeat();
}

void ProximityDetector::onHeartbeatTimer() {
    // Dead-man's switch tripped — no sample for heartbeatSec. Treat it
    // the same as a threshold trip: emit away + request lock.
    if (m_state != State::Paired) return;

    emit awayDetected(m_lastSmoothed);
    emit lockRequested();
    transitionTo(State::Away);
}

void ProximityDetector::transitionTo(State next) {
    if (m_state == next) return;
    m_state = next;
    emit stateChanged(next);
}

void ProximityDetector::armAwayTimer() {
    if (!m_awayTimer.isActive()) m_awayTimer.start();
}

void ProximityDetector::disarmAwayTimer() {
    if (m_awayTimer.isActive()) m_awayTimer.stop();
}

void ProximityDetector::resetHeartbeat() {
    // start() restarts an already-running single-shot timer; if it's not
    // running, this kicks it off. Either way the deadline rolls forward.
    m_heartbeatTimer.start();
}

void ProximityDetector::stopHeartbeat() {
    if (m_heartbeatTimer.isActive()) m_heartbeatTimer.stop();
}

} // namespace Margin::Plugins::Aura
