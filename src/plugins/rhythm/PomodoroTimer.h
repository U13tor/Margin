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
    Q_PROPERTY(int postponesRemaining READ postponesRemaining NOTIFY postponesRemainingChanged)
    Q_PROPERTY(int breaksCompleted READ breaksCompleted NOTIFY breaksCompletedChanged)
    // Today-scoped break counter (rolls over at calendar midnight). Distinct
    // from breaksCompleted which is lifetime — Tab1's "已完成 N / 5 番茄"
    // subtitle needs a daily value, not a career value.
    Q_PROPERTY(int todayCompletedRounds READ todayCompletedRounds
               NOTIFY todayCompletedRoundsChanged)
    Q_PROPERTY(bool paused READ paused NOTIFY pausedChanged STORED false)

public:
    enum class State {
        Idle,
        Working,
        BreakDue,
        BreakActive,
    };
    Q_ENUM(State)

    explicit PomodoroTimer(QObject* parent = nullptr);

    // ── Configuration ──
    int workMinutes() const    { return m_workMinutes; }
    int breakMinutes() const   { return m_breakMinutes; }
    int maxPostpones() const   { return m_maxPostpones; }
    int targetRounds() const   { return m_targetRounds; }
    void setWorkMinutes(int minutes);
    void setBreakMinutes(int minutes);
    void setMaxPostpones(int count);
    void setTargetRounds(int rounds);

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
    /// Used by InputMonitorService::userIdleStateChanged (C2) and Aura's
    /// away/back events (C5). Idempotent — calling with the same value
    /// twice is a no-op. Pausing during BreakActive is also honored:
    /// if the user is away from the keyboard during a stretch break,
    /// the break should pause too (they're not at the desk doing it).
    void setPaused(bool paused);

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
    int   m_remainingSec       = 0;
    int   m_postponesRemaining = kDefaultMaxPostpones;
    int   m_breaksCompleted    = 0;
    int   m_todayCompletedRounds = 0;
    QDate m_todayDate;  // invalid until first setTodayDate / rollback fires
    bool  m_paused             = false;

    QTimer m_tickTimer;
};

} // namespace Margin::Plugins::Rhythm
