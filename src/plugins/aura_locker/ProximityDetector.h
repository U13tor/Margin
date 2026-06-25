// ProximityDetector — pure state machine for BLE proximity auto-lock.
// Spec: docs/11-roadmap.md M1 (away → lock → cooldown flow).
//
// Driven by RSSI samples (typically from BluetoothProximityTracker, but
// decoupled so unit tests can feed synthetic data without touching real
// BLE hardware). Emits awayDetected / backDetected / lockRequested for
// the owning plugin to wire to LockService + EventBus.
//
// State machine:
//
//   start()/resume() ─→ Paired ──smoothed<threshold for awayDelay──→ Away
//                          ↑                                          │
//                          │                                          │
//                          └──cooldownTimer elapsed─── Cooldown ←──smoothed≥threshold
//
//   Paired ──no sample for heartbeatSec──→ Away (offline path)
//
// awayDetected fires on Paired→Away; the plugin then calls LockService.
// backDetected fires on Away→Cooldown; cooldownSec gates the lock from
// re-firing if the user comes back and then leaves again immediately
// (e.g. wallet-on-desk false trip).
//
// The heartbeat path covers a real-world BLE failure mode that the
// threshold path misses: many peripherals (smartwatches, once the phone
// connects to them over BT Classic; LE-Privacy phones; battery-saver
// earbuds) stop emitting advertisements entirely when they go "away".
// In that case no sample ever reaches onRssiSample, so the threshold
// check never fires. The heartbeat timer acts as a dead-man's switch —
// every sample resets it; if it expires, the device is presumed gone.

#pragma once

#include "RssiSmoother.h"

#include <QObject>
#include <QTimer>

#include <optional>

namespace Margin::Plugins::Aura {

class ProximityDetector : public QObject {
    Q_OBJECT

public:
    enum class State {
        Inactive,  // not started (or paused)
        Paired,    // device in range, samples flowing
        Away,      // device out of range beyond awayDelaySec → lock requested
        Cooldown,  // just came back, suppressing re-lock for cooldownSec
    };
    Q_ENUM(State)

    explicit ProximityDetector(QObject* parent = nullptr);

    /// Set thresholds before calling start(). Defaults: -65 dBm / 30 s / 60 s.
    void configure(qint16 rssiThresholdDbm, int awayDelaySec, int cooldownSec);

    /// Begin monitoring. Resets the smoother and transitions Inactive→Paired.
    void start();

    /// Stop monitoring. Transitions →Inactive, stops timers, clears smoother.
    void stop();

    /// User toggled pause via tray. Equivalent to stop() but resume() will
    /// restart if start() was previously called.
    void pause();
    void resume();

    /// Feed one RSSI sample. No-op unless state is Paired/Away/Cooldown.
    void onRssiSample(qint16 rssiDbm);

    State state() const { return m_state; }
    qint16 lastSmoothedDbm() const { return m_lastSmoothed; }
    int   sampleCount() const { return m_smoother.sampleCount(); }

signals:
    void stateChanged(Margin::Plugins::Aura::ProximityDetector::State newState);
    void awayDetected(qint16 smoothedRssiDbm);
    void backDetected();
    /// Emitted exactly once per Paired→Away transition. The plugin's slot
    /// is responsible for actually invoking LockService::lockScreen().
    void lockRequested();

private slots:
    void onAwayTimer();
    void onCooldownTimer();
    void onHeartbeatTimer();

private:
    void transitionTo(State next);
    void armAwayTimer();
    void disarmAwayTimer();
    void resetHeartbeat();
    void stopHeartbeat();

    State   m_state             = State::Inactive;
    qint16  m_rssiThresholdDbm  = -65;
    int     m_awayDelaySec      = 30;
    int     m_cooldownSec       = 60;
    qint16  m_lastSmoothed      = 0;
    bool    m_started           = false;  // start() called at least once

    QTimer  m_awayTimer;
    QTimer  m_cooldownTimer;
    QTimer  m_heartbeatTimer;
    RssiSmoother m_smoother;
};

} // namespace Margin::Plugins::Aura
