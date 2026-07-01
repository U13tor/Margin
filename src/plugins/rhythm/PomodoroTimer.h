// PomodoroTimer — work/break state machine for Rhythm & Health (M3-C1).
// Spec: docs/11-roadmap.md M3 + docs/06-ui-design.md Tab3.
//
// State machine:
//
//   Idle ──start()──► Working ──(work sec elapsed)──► BreakDue
//      ▲                                              │
//      │                                              ├─ startBreak() ──► BreakActive ──(break sec)──► Idle
//      │                                              ├─ postponeBreak() ► Working (reset, decrement postpones)
//      │                                              └─ skipBreak() ───► Idle (don't count as completed)
//      └─ stop() (from any state)
//
// advance(seconds) is the test hook: production code calls it from the internal
// 1s QTimer; unit tests call it directly without waiting on real time.

#pragma once

#include <QObject>
#include <QDate>
#include <QString>
#include <QTimer>

#include <QtGlobal>  // qBound

namespace Margin::Plugins::Rhythm {

class PomodoroTimer : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString state READ stateString NOTIFY stateChanged)
    Q_PROPERTY(int remainingSeconds READ remainingSeconds NOTIFY remainingChanged)
    Q_PROPERTY(int workMinutes READ workMinutes WRITE setWorkMinutes NOTIFY workMinutesChanged)
    Q_PROPERTY(int breakMinutes READ breakMinutes WRITE setBreakMinutes NOTIFY breakMinutesChanged)
    Q_PROPERTY(int maxPostpones READ maxPostpones WRITE setMaxPostpones NOTIFY maxPostponesChanged)
    // M4-C12: daily pomodoro target. Drives the "#N/M" header and the
    // ProgressDots row in RhythmTab. Default 5 (typical work-day budget).
    // Pure display — does NOT gate transitions or auto-stop the work cycle.
    Q_PROPERTY(int targetRounds READ targetRounds WRITE setTargetRounds NOTIFY targetRoundsChanged)
    // BreakActive natural completion automatically starts the next work round
    // when autoContinueCycle is true (default). False restores the v1.0
    // behavior of returning to Idle after each break, for users who want
    // manual control over session pacing (e.g. autostart-disabled hosts).
    Q_PROPERTY(bool autoContinueCycle READ autoContinueCycle WRITE setAutoContinueCycle NOTIFY autoContinueCycleChanged)
    Q_PROPERTY(int postponesRemaining READ postponesRemaining NOTIFY postponesRemainingChanged)
    Q_PROPERTY(int breaksCompleted READ breaksCompleted NOTIFY breaksCompletedChanged)
    // Today-scoped break counter (rolls over at calendar midnight). Distinct
    // from breaksCompleted which is lifetime — Tab1's "已完成 N / 5 番茄"
    // subtitle needs a daily value, not a career value.
    Q_PROPERTY(int todayCompletedRounds READ todayCompletedRounds
               NOTIFY todayCompletedRoundsChanged)
    Q_PROPERTY(bool paused READ paused NOTIFY pausedChanged STORED false)
    // Highest-priority active pause reason as a localized string for the
    // RhythmTab paused chip. Empty when not paused. Bound to pausedChanged
    // so the chip text flips when the dominant reason changes (e.g. user
    // paused → Aura also triggers away → chip upgrades to "离开中").
    Q_PROPERTY(QString pauseReasonsText READ pauseReasonsText NOTIFY pausedChanged)

public:
    enum class State {
        Idle,
        Working,
        BreakDue,
        BreakActive,
    };
    Q_ENUM(State)

    // PauseReason bitmask — drives the paused latch. Three external sources
    // can pause the countdown (user tray/QML toggle, InputMonitor idle,
    // Aura away); the timer stays paused while ANY bit is set and only
    // resumes when the mask is fully clear. This prevents the historical
    // bug where a brief Aura BLE dropout (false away) would unfreeze a
    // user who had manually paused, or where Aura back would resume
    // despite Idle still being active.
    //
    // 2026-06-26 fix-forward: previously a single `bool m_paused` was
    // flipped by all three sources via setPaused(bool); see
    // docs/11-roadmap.md M3 review §D.
    enum class PauseReason : uint8_t {
        None     = 0,
        User     = 1 << 0,   // tray toggle / QML pause button / legacy setPaused
        Idle     = 1 << 1,   // InputMonitorService::userIdleStateChanged
        AuraAway = 1 << 2,   // margin.aura.away via RhythmPlugin subscription
    };
    Q_ENUM(PauseReason)
    Q_DECLARE_FLAGS(PauseReasonMask, PauseReason)

    explicit PomodoroTimer(QObject* parent = nullptr);

    // ── Configuration ──
    int workMinutes() const    { return m_workMinutes; }
    int breakMinutes() const   { return m_breakMinutes; }
    int maxPostpones() const   { return m_maxPostpones; }
    int targetRounds() const   { return m_targetRounds; }
    bool autoContinueCycle() const { return m_autoContinueCycle; }
    void setWorkMinutes(int minutes);
    void setBreakMinutes(int minutes);
    void setMaxPostpones(int count);
    void setTargetRounds(int rounds);
    void setAutoContinueCycle(bool enabled);

    // ── Live state ──
    State   state() const          { return m_state; }
    QString stateString() const;
    int     remainingSeconds() const { return m_remainingSec; }
    int     postponesRemaining() const { return m_postponesRemaining; }
    int     breaksCompleted() const { return m_breaksCompleted; }
    int     todayCompletedRounds() const { return m_todayCompletedRounds; }
    // The calendar day the todayCompletedRounds counter is currently scoped
    // to. Exposed so RhythmPlugin can persist it alongside the count.
    QDate   todayDate() const { return m_todayDate; }
    bool    paused() const { return m_paused; }
    // Bitmask of all currently-active pause reasons. UI / tests can read it
    // to distinguish "paused by user" from "paused because away". pauseReasonsText
    // is the localized version; pauseMask() is the raw enum for programmatic use.
    PauseReasonMask pauseMask() const { return m_pauseMask; }
    QString pauseReasonsText() const;

    // Push loaded state from RhythmPlugin::loadSettings. Both are raw
    // setters — no rollback logic here; caller is expected to invoke
    // ensureDailyRollback() once after both are set so a stale stored
    // date (machine off overnight) resets the count before first use.
    void    setTodayCompletedRounds(int count);
    void    setTodayDate(QDate date);

    // Roll m_todayDate forward to QDate::currentDate(). If the stored date
    // differs (including invalid → today), reset m_todayCompletedRounds to
    // 0 and emit todayCompletedRoundsChanged. Returns true if rollback
    // fired. Idempotent within the same calendar day. Public so RhythmPlugin
    // can call it from loadSettings; PomodoroTimer also calls it internally
    // before every increment to cover in-session midnight crossings.
    bool    ensureDailyRollback();

    // ── Safe-range floors / ceilings for user-tunable settings ──
    // Single SSOT for the floor / ceiling math. Both onLoad (settings-load)
    // and Q_PROPERTY setters route through these so a hand-edited
    // settings.json with work_minutes=0 hits the same wall as a QML SpinBox
    // that bypassed its floor. Public + static so unit tests can assert
    // against the bounds without instantiating the timer.
    static constexpr int kMinWorkMinutes    = 1;
    static constexpr int kMaxWorkMinutes    = 180;
    static constexpr int kMinBreakMinutes   = 1;
    static constexpr int kMaxBreakMinutes   = 30;
    static constexpr int kMinPostpones      = 0;
    static constexpr int kMaxPostpones      = 10;
    static constexpr int kMinTargetRounds   = 1;
    static constexpr int kMaxTargetRounds   = 12;
    static constexpr int kDefaultWorkMinutes  = 45;
    static constexpr int kDefaultBreakMinutes = 5;
    static constexpr int kDefaultMaxPostpones = 3;
    static constexpr int kDefaultTargetRounds = 5;
    // Postpone 短期倒计时长度。RhythmToast「推迟 5 分钟」按钮的语义承诺:
    // 点击后进入 5 分钟工作倒计时(而非完整工作番茄),到期重新 emit breakDue。
    // 常量与按钮文案硬绑,改这里要同步 i18n 文案。
    static constexpr int kPostponeMinutes     = 5;

    static int clampWorkMinutes(int m)  { return qBound(kMinWorkMinutes,  m, kMaxWorkMinutes); }
    static int clampBreakMinutes(int m) { return qBound(kMinBreakMinutes, m, kMaxBreakMinutes); }
    static int clampMaxPostpones(int n) { return qBound(kMinPostpones,    n, kMaxPostpones); }
    static int clampTargetRounds(int n) { return qBound(kMinTargetRounds, n, kMaxTargetRounds); }

    /// Advance the state as if `seconds` seconds elapsed. Production code
    /// calls this from a 1s QTimer::timeout; unit tests call it directly so
    /// we don't wait 45 minutes to verify the BreakDue transition.
    void advance(int seconds);

public slots:
    /// Idle → Working. No-op if already running. Resets postpones counter
    /// to maxPostpones so a fresh work session has full postpone budget.
    void start();

    /// Any state → Idle. Resets remaining + postpones counter. Does NOT
    /// reset breaksCompleted (that's a session-cumulative metric).
    void stop();

    /// BreakDue → BreakActive. No-op from other states.
    void startBreak();

    /// BreakDue → Working (full work budget reset). Decrements
    /// postponesRemaining. If postponesRemaining == 0, no-op (UI must hide
    /// the postpone button when this hits zero).
    void postponeBreak();

    /// BreakDue → Idle. Does NOT count as a completed break.
    void skipBreak();

    /// BreakActive → Idle. The exit path for the full-screen stretch overlay
    /// (跳过 button / Esc). Does NOT count as a completed break (mirrors
    /// skipBreak's no-credit opt-out). No-op outside BreakActive — skipBreak()
    /// guards BreakDue and is inert during the active break, so the overlay
    /// needs its own dismiss.
    void endBreakEarly();

    /// Pause / resume the countdown without leaving the current state.
    /// Routes through PauseReason::User — the historical entry point for
    /// tray toggle, QML pause button, and existing tests. New external
    /// sources (Idle / Aura) should call setPausedForReason so the mask
    /// tracks which subsystem caused the pause. Idempotent.
    void setPaused(bool paused);

    /// Pause / resume for a specific reason. Sets or clears the matching
    /// bit in m_pauseMask; the timer is paused iff the mask is non-zero.
    /// Multiple sources can therefore overlap: user-paused + Aura-away
    /// stays paused until BOTH clear. Emits pausedChanged on any mask
    /// transition (including same-overall-paused but different dominant
    /// reason — so the UI chip text can update).
    /// Idempotent per reason: set(true, Idle) twice = one signal.
    void setPausedForReason(bool paused, PauseReason reason);

    /// Force-resume the countdown regardless of which pause reasons are
    /// active. Clears the entire pauseMask in one shot. This is the
    /// "user explicit action overrides inference" escape hatch — Aura and
    /// Idle are best-effort inferences about whether the user is available;
    /// a click on the resume button or tray toggle is a fact, and it should
    /// win. Without this, paused-by-Aura-only states cannot be cleared by
    /// the user when the Aura back signal never arrives (Microsoft BLE
    /// peripherals sleep and don't rebroadcast). Idempotent: no-op when
    /// mask is already empty.
    Q_INVOKABLE void forceResume();

signals:
    void stateChanged();
    void remainingChanged();
    void workMinutesChanged();
    void breakMinutesChanged();
    void maxPostponesChanged();
    void postponesRemainingChanged();
    void breaksCompletedChanged();
    void todayCompletedRoundsChanged();
    void targetRoundsChanged();
    void autoContinueCycleChanged();
    void pausedChanged();

    /// Working → BreakDue transition. Plugin listens + shows the toast.
    void breakDue();
    /// BreakDue → BreakActive transition.
    void breakStarted();
    /// BreakActive → Idle (natural completion).
    void breakEnded();
    /// BreakDue → Working via postponeBreak().
    void postponed();
    /// BreakDue → Idle via skipBreak().
    void skipped();

private:
    void setState(State s);
    void enterWorking();
    void enterBreakDue();
    void enterBreakActive();
    void enterIdle();
    void resetPostpones();
    void onTick();

    State m_state              = State::Idle;
    int   m_workMinutes        = kDefaultWorkMinutes;
    int   m_breakMinutes       = kDefaultBreakMinutes;
    int   m_maxPostpones       = kDefaultMaxPostpones;
    int   m_targetRounds       = kDefaultTargetRounds;
    bool  m_autoContinueCycle = true;
    int   m_remainingSec       = 0;
    int   m_postponesRemaining = kDefaultMaxPostpones;
    int   m_breaksCompleted    = 0;
    int   m_todayCompletedRounds = 0;
    QDate m_todayDate;  // invalid until first setTodayDate / rollback fires
    // PauseReason bitmask — see enum above. m_paused is the cached boolean
    // (m_pauseMask != 0) so paused() stays O(1) and the signal path only
    // fires on the actual boolean edge.
    PauseReasonMask m_pauseMask;
    bool  m_paused             = false;

    QTimer m_tickTimer;
};

// Q_DECLARE_OPERATORS_FOR_FLAGS must be at namespace scope (it generates
// free-function binary operators that need 2 args, so they can't be class
// members). Mirrors Qt's own pattern for QFlags.
Q_DECLARE_OPERATORS_FOR_FLAGS(PomodoroTimer::PauseReasonMask)

} // namespace Margin::Plugins::Rhythm
