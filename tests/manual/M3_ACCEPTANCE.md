# M3 — Manual Acceptance Checklist (Rhythm & Health)

> Verify the Rhythm plugin end-to-end on Windows. Required for [docs/11-roadmap.md M3 Definition of Done](../../docs/11-roadmap.md) items 1–5.
>
> macOS is **deferred to v1.1** ([docs/12-deferred-items.md §A21](../../docs/12-deferred-items.md)); only Windows acceptance is in scope this round. The Mac checklist lives there too — re-run the same L4 cases after §A21 lifts.

This file is **human-executed**. CI cannot tick these — every box below needs a real Windows session with the work/break loop running for real-time minutes.

---

## Setup

1. **Clean state**:
   - Stop Margin if running
   - Delete `%LOCALAPPDATA%\Margin\logs\margin.log` so this run is the only content
   - Delete `%APPDATA%\Margin\config\settings.json` to clear the work/break length / autostart flags from prior runs
2. **Build**: `bash scripts/build-smoke.sh win64-msvc-debug` — all 5 steps must pass (configure / build / ctest / audit / startup line)
3. **Locate binary**: `build/win64-msvc-debug/src/host/Margin.exe`
4. **Recommended**: keep a terminal open with

   ```bash
   tail -F "%LOCALAPPDATA%\Margin\logs\margin.log"
   ```

   so PomodoroTimer state transitions + Aura subscriptions show up live.
5. **Acceleration tip**: most L4 cases take 45 min real-time at defaults. To shorten, add to `settings.json`:

   ```json
   { "plugins.rhythm.work_minutes": 1, "plugins.rhythm.break_minutes": 1 }
   ```

   → 60 s work + 60 s break. The state machine is identical; only the tick budget changes.

---

## L4.1 — Tab + countdown visible

- [ ] Launch `Margin.exe`
- [ ] Dashboard shows a fourth tab **Rhythm** with the rhythm-tab icon, positioned after Screen Time
- [ ] Click **Rhythm** → status card shows `working` state with a `mm:ss` countdown starting at 45:00 (or 01:00 if you set `work_minutes=1`)
- [ ] `margin.log` contains `RhythmPlugin onLoad work=45min break=5min maxPostpones=3` (or your override values)
- [ ] `margin.log` contains `idle monitor wired — pausing countdown on idle`
- [ ] `margin.log` contains `aura away/back wired — pause on away, resume 60s after back` (or your `resume_delay_sec` value)
- [ ] `margin.log` contains `overlay contributors: 1` (just Rhythm; the host's own Overview has no overlay)

---

## L4.2 — Work countdown → toast

After the work budget elapses:

- [ ] Bottom-right of the **primary screen** shows the RhythmToast window (frameless, stays on top)
- [ ] Title `该起身活动一下了`
- [ ] Subtitle `已连续工作 X 分钟` with X = your work_minutes
- [ ] `还可推迟 3 次` visible (maxPostpones=3 default)
- [ ] Two buttons: `推迟 5 分钟` (enabled) + `开始做操` (highlighted)
- [ ] `margin.log` records a `margin.rhythm.break_due` publish (`[eventbus] dispatching` line, if debug logging enabled)
- [ ] The Rhythm tab status card flips from `working` → `break_due`

If you want to verify the geometry: `Margin.exe` should still be in the foreground for the toast to render. Alt-Tab away then back to confirm it stays on top.

---

## L4.3 — "开始做操" → break overlay window (M4-C13a/c/d)

1. With the toast visible, click **开始做操**
2. Toast hides (immediately)
3. **Standalone centered floating window** appears (not full-screen — the
   underlying desktop / VS Code / browser remains visible around it):
   - [ ] Window is frameless, always-on-top, centered on the primary screen
   - [ ] Title `颈椎放松操`
   - [ ] NeckStretchAnimator shows the current pose frame from
     `qrc:/rhythm/icons/neck-stretch-{1..8}.png` — one pose per
     `breakMinutes * 60 / 8` seconds (e.g. 5-min break ≈ 37.5 s/pose,
     entire 8-pose round fills the break)
   - [ ] Step counter `动作 N/8 · <name>` updates as the animator advances
   - [ ] ProgressDots row of 8 dots, with `currentStep` dots filled
   - [ ] Live `mm:ss` countdown starting at your break_minutes
   - [ ] **跳过** button (always enabled)
   - [ ] **推迟 5 分钟** button (enabled only when `postponesRemaining > 0`)

- [ ] `margin.log` records a `margin.rhythm.break_started` publish
- [ ] The Rhythm tab status card flips from `break_due` → `break_active`

### L4.3a — Skip path (user-initiated)

Press **Esc** or click **跳过**:

- [ ] Overlay dismisses immediately → returns to dashboard
- [ ] `margin.log` records a `margin.rhythm.break_dismissed` publish

### L4.3b — Natural end path (M4-C13d done card)

Let the break countdown reach `mm:ss = 00:00` without user input:

- [ ] Pose cycle reaches step 8/8 by the time the countdown ends
- [ ] Overlay does NOT vanish — it switches to a **completion card**:
  - [ ] 🎉 emoji + `本节休息已完成` title
  - [ ] `颈椎八式一轮完成。下一节工作番茄 N 分钟后开始。` subtitle
  - [ ] `10 秒后自动关闭` countdown text ticks down each second
  - [ ] **立即关闭** button calls `rhythmHost.dismissBreakOverlay()`
- [ ] After 10 s the overlay auto-dismisses
- [ ] Pressing **立即关闭** dismisses immediately
- [ ] Pressing **Esc** during done card also dismisses

---

## L4.4 — Idle 3 minutes → countdown pauses

1. With countdown running, **stop all keyboard / mouse input** for the idle threshold (default 180 s; lower via Screen Time tab SpinBox to 60 s to shorten)
2. After the threshold elapses:
   - [ ] The Rhythm tab countdown freezes (no longer ticks down)
   - [ ] `margin.log` records `user idle: true`
3. Move the mouse / press a key:
   - [ ] Countdown resumes ticking
   - [ ] `margin.log` records `user idle: false`

The pause is via PomodoroTimer::setPaused — same path as the Aura subscription in L4.5.

---

## L4.5 — Aura away/back integration

Requires a paired Aura device (BLE phone in range). If no device is paired, skip this case and note in the sign-off — the integration is unit-tested by [test_rhythm_aura_integration](../unit/test_rhythm_aura_integration.cpp).

1. Pair a phone in the Aura tab; wait for state `Connected` / `在岗`
2. With Rhythm countdown running, walk away from the desk (phone leaves BLE range)
   - [ ] Aura emits `margin.aura.away` → Rhythm tab countdown pauses
   - [ ] `margin.log` records both events: `aura ... awayDetected` followed shortly by `rhythm` activity pause (no explicit log line, but the countdown freeze is visible)
3. Walk back into BLE range
   - [ ] Aura emits `margin.aura.back` **immediately** on re-entry — `margin.log` records `aura ... backDetected` (Aura's 60 s cooldown only suppresses re-lock; it does not delay the `back` event)
   - [ ] Rhythm does **not** resume instantly — it arms a `resume_delay_sec` timer (default 60 s, DoD #3) before the Rhythm tab countdown resumes. To shorten, set `plugins.rhythm.resume_delay_sec` to a small value (e.g. `5`)
   - [ ] After the delay elapses, the Rhythm tab countdown resumes; stepping away again before it fires cancels the pending resume

If you don't have BLE hardware: load both plugins in a synthetic test harness and publish `margin.aura.away` / `margin.aura.back` directly via the EventBus. The unit test in C5 already does this.

---

## L4.6 — Postpone cap (3 → no more)

1. With toast visible (accelerate: `work_minutes=1` so each work session is 60 s)
2. Click **推迟 5 分钟** → toast hides, countdown restarts at 5:00 (or your `work_minutes` value)
   - [ ] Subtitle text `还可推迟 X 次` decrements (3 → 2 → 1 → 0)
3. Repeat until the counter reaches 0
4. Wait for the next break → toast appears
   - [ ] Subtitle text now reads `本次不能再推迟`
   - [ ] **推迟 5 分钟** button is disabled (greyed out, not clickable)
   - [ ] Only `开始做操` remains clickable

`margin.log` records a `margin.rhythm.break_dismissed` publish on each postpone. Stopping and restarting via the tray menu resets the postpone budget to maxPostpones (default 3).

---

## L4.7 — Skip → overlay closes + countdown reset

1. Trigger a break (work countdown elapses OR set work_minutes=1 and wait 60 s)
2. From the toast, click **开始做操** → overlay appears
3. Press **Esc** or click **跳过**
   - [ ] Overlay closes
   - [ ] Rhythm tab status card flips `break_active` → `idle`
   - [ ] **`breaksCompleted` counter does NOT increment** (skip is opt-out — only natural completion counts)
   - [ ] Start button re-enables; clicking it begins a fresh 45:00 (or your work_minutes) countdown

`margin.log` records `margin.rhythm.break_dismissed` for the skip path. Distinguish from postpone by inspecting whether the state went to `Idle` (skip) vs `Working` (postpone).

---

## L4.8 — Shutdown cleanliness

1. With Margin running (mid-countdown is fine), click the tray icon → Quit
2. Wait 5 s for shutdown to complete
3. `margin.log` should end with:
   - [ ] `shutdown: hiding tray`
   - [ ] `shutdown: unloading plugins` followed by `RhythmPlugin onUnload`
   - [ ] `shutdown: closing dashboard` (the OverlayContainer.qml Loader is destroyed here)
   - [ ] `shutdown complete`
4. Task Manager:
   - [ ] No `Margin.exe` process remains
   - [ ] No `margin-rhythm.dll` lingering in any process's loaded modules (it's unloaded with the host)

---

## L4.9 — Zero network + local-only assets

The §1 red line + the Lottie fallback audit (§A22).

- [ ] Start Wireshark (Npcap) on the network adapter, filter for `Margin.exe` PID
- [ ] Use Margin for 5 minutes: run a full work→break→overlay→skip cycle, switch apps, sit idle
- [ ] **Zero outbound** packets attributable to Margin.exe (no DNS, no TCP, no UDP)
- [ ] The break overlay renders from `qrc:/rhythm/icons/neck-stretch-{1..8}.png` (local, compiled into the DLL) — verify by deleting one PNG from disk and rebuilding; the overlay still renders that frame because it's in the qrc
- [ ] No `.json` Lottie assets are loaded at runtime (Lottie deferred per §A22; the 8-frame PNG sequence via NeckStretchAnimator is what's running)

If Wireshark shows anything: file a bug. This is the project's hard line.

---

## Sign-off

- [ ] All L4.1–L4.9 ✓ (or noted as skipped with reason — e.g. no BLE hardware for L4.5)
- [ ] [docs/11-roadmap.md](../../docs/11-roadmap.md) M3-C1..C7 all checked
- [ ] [docs/12-deferred-items.md §A21](../../docs/12-deferred-items.md) records the macOS deferral with re-evaluation criteria
- [ ] [docs/12-deferred-items.md §A22](../../docs/12-deferred-items.md) records the Lottie deferral with re-evaluation criteria

Tester: ____________________  Date: ____________
