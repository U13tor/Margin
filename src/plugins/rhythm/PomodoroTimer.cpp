// PomodoroTimer impl — see PomodoroTimer.h.

#include "PomodoroTimer.h"

#include <QCoreApplication>

namespace Margin::Plugins::Rhythm {

namespace {
constexpr int kTickIntervalMs = 1000;
} // namespace

PomodoroTimer::PomodoroTimer(QObject* parent)
    : QObject(parent) {
    m_tickTimer.setInterval(kTickIntervalMs);
    connect(&m_tickTimer, &QTimer::timeout, this, &PomodoroTimer::onTick);
}

QString PomodoroTimer::stateString() const {
    switch (m_state) {
        case State::Idle:        return QStringLiteral("idle");
        case State::Working:     return QStringLiteral("working");
        case State::BreakDue:    return QStringLiteral("break_due");
        case State::BreakActive: return QStringLiteral("break_active");
    }
    return QStringLiteral("unknown");
}

void PomodoroTimer::setWorkMinutes(int minutes) {
    const int clamped = clampWorkMinutes(minutes);
    if (m_workMinutes == clamped) return;
    m_workMinutes = clamped;
    // If we're currently in Working state with a longer countdown, snap
    // remaining to the new cap. A user shortening the work timer mid-run
    // expects the new value immediately, not after the old countdown lapses.
    if (m_state == State::Working && m_remainingSec > m_workMinutes * 60) {
        m_remainingSec = m_workMinutes * 60;
        emit remainingChanged();
    }
    emit workMinutesChanged();
}

void PomodoroTimer::setBreakMinutes(int minutes) {
    const int clamped = clampBreakMinutes(minutes);
    if (m_breakMinutes == clamped) return;
    m_breakMinutes = clamped;
    if (m_state == State::BreakActive && m_remainingSec > m_breakMinutes * 60) {
        m_remainingSec = m_breakMinutes * 60;
        emit remainingChanged();
    }
    emit breakMinutesChanged();
}

void PomodoroTimer::setMaxPostpones(int count) {
    const int clamped = clampMaxPostpones(count);
    if (m_maxPostpones == clamped) return;
    m_maxPostpones = clamped;
    // Refuse to leave postponesRemaining above the new ceiling.
    if (m_postponesRemaining > m_maxPostpones) {
        m_postponesRemaining = m_maxPostpones;
        emit postponesRemainingChanged();
    }
    emit maxPostponesChanged();
}

void PomodoroTimer::setTargetRounds(int rounds) {
    // Pure display setting — does NOT touch the state machine. Lowering the
    // target below todayCompletedRounds is allowed; the Tab3 header just
    // renders "#(N+1)/M" with N+1 > M when the user has already exceeded
    // the goal, which is the honest signal.
    const int clamped = clampTargetRounds(rounds);
    if (m_targetRounds == clamped) return;
    m_targetRounds = clamped;
    emit targetRoundsChanged();
}

void PomodoroTimer::setAutoContinueCycle(bool enabled) {
    if (m_autoContinueCycle == enabled) return;
    m_autoContinueCycle = enabled;
    emit autoContinueCycleChanged();
}

void PomodoroTimer::setTodayCompletedRounds(int count) {
    if (m_todayCompletedRounds == count) return;
    m_todayCompletedRounds = count;
    emit todayCompletedRoundsChanged();
}

void PomodoroTimer::setTodayDate(QDate date) {
    m_todayDate = date;
    // No emit — the date itself isn't observed by QML. Only count changes
    // fire todayCompletedRoundsChanged; ensureDailyRollback handles the
    // stale-date reset and emits once when it actually resets.
}

bool PomodoroTimer::ensureDailyRollback() {
    const QDate today = QDate::currentDate();
    if (m_todayDate == today) return false;
    m_todayDate = today;
    if (m_todayCompletedRounds != 0) {
        m_todayCompletedRounds = 0;
        emit todayCompletedRoundsChanged();
        return true;
    }
    // Count was already 0 but date was missing/stale — still return true
    // so callers know the date moved (matters for settings persistence).
    return true;
}

void PomodoroTimer::start() {
    if (m_state == State::Working || m_state == State::BreakDue ||
        m_state == State::BreakActive) {
        return;
    }
    // start() is a strong user intent ("I want to begin work now"). Clear
    // any leftover pause bits from a prior session so we don't enter Working
    // with m_paused=true — that would freeze the countdown immediately and
    // leave the user looking at a stuck timer. The bitmask invariant says
    // paused = (mask != 0); entering Working with non-empty mask violates
    // it. See docs/11-roadmap.md M3 review §F.
    if (m_pauseMask != 0) {
        m_pauseMask = PauseReason::None;
        m_paused = false;
        emit pausedChanged();
    }
    resetPostpones();
    enterWorking();
}

void PomodoroTimer::stop() {
    if (m_state == State::Idle) return;
    enterIdle();
}

void PomodoroTimer::startBreak() {
    if (m_state != State::BreakDue) return;
    enterBreakActive();
}

void PomodoroTimer::postponeBreak() {
    if (m_state != State::BreakDue) return;
    if (m_postponesRemaining <= 0) return;
    --m_postponesRemaining;
    emit postponesRemainingChanged();
    emit postponed();
    // 真正的「推迟 5 分钟」:进入 Working 但 m_remainingSec 只设 kPostponeMinutes,
    // 自然走 Working → onTick → enterBreakDue 路径,5 分钟后重新弹 toast。
    // 不复用 enterWorking() —— 那是「开始完整工作番茄」的入口(start() /
    // autoContinueCycle 共用),会把 remaining 重置为 m_workMinutes*60(默认 45
    // 分钟),与按钮文案「推迟 5 分钟」不符。
    setState(State::Working);
    m_remainingSec = kPostponeMinutes * 60;
    emit remainingChanged();
    m_tickTimer.start();
}

void PomodoroTimer::skipBreak() {
    if (m_state != State::BreakDue) return;
    emit skipped();
    enterIdle();
}

void PomodoroTimer::endBreakEarly() {
    if (m_state != State::BreakActive) return;
    emit skipped();
    enterIdle();
}

void PomodoroTimer::setPaused(bool paused) {
    // Legacy entry point — routes through the User bit so the mask tracks
    // who actually caused the pause. Existing QML / tray / tests keep working
    // without API churn.
    setPausedForReason(paused, PauseReason::User);
}

void PomodoroTimer::setPausedForReason(bool paused, PauseReason reason) {
    if (reason == PauseReason::None) return;

    const PauseReasonMask before = m_pauseMask;
    if (paused) {
        m_pauseMask |= reason;
    } else {
        m_pauseMask &= ~PauseReasonMask(reason);
    }
    if (m_pauseMask == before) return;

    // m_paused is the cached boolean for paused() / external observers.
    // Re-derive from the mask; emit + tickTimer side-effects fire on the
    // boolean edge only, but pausedChanged also flushes the pauseReasonsText
    // binding (which depends on the dominant reason, not just the bool).
    const bool nowPaused = (m_pauseMask != 0);
    const bool edgeChanged = (nowPaused != m_paused);
    m_paused = nowPaused;

    if (edgeChanged) {
        if (nowPaused) {
            m_tickTimer.stop();
        } else if (m_state == State::Working || m_state == State::BreakActive) {
            m_tickTimer.start();
        }
    }
    emit pausedChanged();
}

void PomodoroTimer::forceResume() {
    // Strong user intent: clear the entire pause mask. Aura/Idle are
    // inferences; a click on "继续" or tray toggle is a fact, and the fact
    // wins. This is the only path that clears bits the owning subsystem
    // didn't explicitly clear itself — without it, paused-by-Aura-only
    // states get stuck when the BLE back signal never arrives (Microsoft
    // peripherals sleep + don't rebroadcast after reconnection).
    if (m_pauseMask == 0) return;
    m_pauseMask = PauseReason::None;
    m_paused = false;
    if (m_state == State::Working || m_state == State::BreakActive) {
        m_tickTimer.start();
    }
    emit pausedChanged();
}

QString PomodoroTimer::pauseReasonsText() const {
    // Highest-priority active reason — matches what the user most likely
    // wants to see in the chip. Order: AuraAway > Idle > User so an active
    // away-session chip says "离开中" even if the user also hit pause.
    // Returns empty string when not paused so the QML chip can `visible:
    // rhythm.paused && text !== ""`.
    if (!m_paused) return QString();
    if (m_pauseMask & PauseReason::AuraAway) {
        return QCoreApplication::translate("PomodoroTimer", "离开中");
    }
    if (m_pauseMask & PauseReason::Idle) {
        return QCoreApplication::translate("PomodoroTimer", "闲置");
    }
    if (m_pauseMask & PauseReason::User) {
        return QCoreApplication::translate("PomodoroTimer", "用户暂停");
    }
    return QString();
}

void PomodoroTimer::advance(int seconds) {
    if (seconds <= 0) return;
    // Tick one second at a time so the state machine sees each transition
    // in order. Tests calling advance(60) on a 45-minute timer want the
    // 1-second granularity so the BreakDue transition fires at the right
    // tick, not after a 60-second lump sum.
    for (int i = 0; i < seconds; ++i) {
        onTick();
    }
}

void PomodoroTimer::onTick() {
    if (m_state != State::Working && m_state != State::BreakActive) return;
    if (m_paused) return;  // C2 idle / C5 away — don't burn the countdown
    if (m_remainingSec <= 0) return;
    --m_remainingSec;
    emit remainingChanged();
    if (m_remainingSec == 0) {
        if (m_state == State::Working) {
            enterBreakDue();
        } else {
            // BreakActive natural completion — count it, then either auto-
            // continue into the next work round (default) or fall to Idle
            // for manual-pacing users (autostart-disabled hosts). Either
            // way breakEnded fires BEFORE the state transition so the
            // RhythmPlugin hideBreakToast + overlay done-card paths see
            // the same event regardless of where we go next.
            ++m_breaksCompleted;
            emit breaksCompletedChanged();
            // Roll the today-scoped counter forward if we crossed midnight
            // during the break (long break, user away). ensureDailyRollback
            // returns true if it reset, but we always ++ and emit after —
            // even on rollback the count goes from 0 → 1, not 0 → 0.
            ensureDailyRollback();
            ++m_todayCompletedRounds;
            emit todayCompletedRoundsChanged();
            emit breakEnded();
            if (m_autoContinueCycle) {
                resetPostpones();
                enterWorking();
            } else {
                enterIdle();
            }
        }
    }
}

void PomodoroTimer::setState(State s) {
    if (m_state == s) return;
    m_state = s;
    emit stateChanged();
}

void PomodoroTimer::enterWorking() {
    setState(State::Working);
    m_remainingSec = m_workMinutes * 60;
    emit remainingChanged();
    m_tickTimer.start();
}

void PomodoroTimer::enterBreakDue() {
    setState(State::BreakDue);
    m_remainingSec = 0;
    emit remainingChanged();
    m_tickTimer.stop();
    emit breakDue();
}

void PomodoroTimer::enterBreakActive() {
    setState(State::BreakActive);
    m_remainingSec = m_breakMinutes * 60;
    emit remainingChanged();
    m_tickTimer.start();
    emit breakStarted();
}

void PomodoroTimer::enterIdle() {
    setState(State::Idle);
    m_remainingSec = 0;
    emit remainingChanged();
    m_tickTimer.stop();
}

void PomodoroTimer::resetPostpones() {
    if (m_postponesRemaining == m_maxPostpones) return;
    m_postponesRemaining = m_maxPostpones;
    emit postponesRemainingChanged();
}

} // namespace Margin::Plugins::Rhythm
