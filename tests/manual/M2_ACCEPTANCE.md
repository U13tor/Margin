# M2 — Manual Acceptance Checklist (Screen Time Tracker)

> Verify the Screen Time plugin end-to-end on Windows. Required for [docs/11-roadmap.md M2 Definition of Done](../../docs/11-roadmap.md) items 1–7.
>
> macOS is **deferred to v1.1** ([docs/12-deferred-items.md §A19](../../docs/12-deferred-items.md)); only Windows acceptance is in scope this round. The Mac checklist lives there too — re-run the same L4 cases after §A19 lifts.

This file is **human-executed**. CI cannot tick these — every box below needs a real Windows session with real window switching.

---

## Setup

1. **Clean state**:
   - Stop Margin if running
   - Delete `%LOCALAPPDATA%\Margin\logs\margin.log` so this run is the only content
   - Delete `%APPDATA%\Margin\data\margin.db` (and `-wal`, `-shm`) to start from an empty schema
   - Delete `%APPDATA%\Margin\config\settings.json` to clear the idle threshold / category overrides from any prior runs
2. **Build**: `bash scripts/build-smoke.sh win64-msvc-debug` — all 5 steps must pass (configure / build / ctest / audit / startup line)
3. **Locate binary**: `build/win64-msvc-debug/src/host/Margin.exe`
4. **Recommended**: keep a terminal open with

   ```bash
   tail -F "%LOCALAPPDATA%\Margin\logs\margin.log"
   ```

   so passive-monitor log lines show up live as you switch windows.

---

## L4.1 — Tab + passive monitoring live

- [ ] Launch `Margin.exe`
- [ ] Dashboard shows a third tab **时长** (Screen Time) with the clock icon, positioned after Aura
- [ ] Click **时长** → empty-state hint `还没有数据 — 切换几个窗口让 Margin 记录一些活动`
- [ ] `margin.log` contains `screen_time schema ready`
- [ ] `margin.log` contains `window monitor startMonitoring` / `input monitor startMonitoring` (or both succeed silently — log level is `info`)
- [ ] `margin.log` contains `screen_time loaded (M2-C3 schema wired)`
- [ ] `screen_time.category matcher: N default + 0 user rule(s)` appears once at startup

---

## L4.2 — Passive monitoring (no polling)

Verifies the user's explicit requirement: **no `GetForegroundWindow` polling loop**. Every session row must come from a `SetWinEventHook` callback.

1. Open a few different apps over ~30 seconds: e.g. browser → editor → file explorer → terminal → browser
2. Inspect the DB:

   ```bash
   sqlite3 "%APPDATA%\Margin\data\margin.db" \
     "SELECT id, app_name, started_at, ended_at, duration_ms FROM screen_time_app_session ORDER BY started_at"
   ```

- [ ] Row count matches the number of distinct window switches (off-by-one is fine — the in-flight session hasn't been closed yet)
- [ ] `started_at` values are monotonic and roughly match wall-clock
- [ ] `duration_ms` for closed sessions is plausible (> 0, roughly the time you spent in each app)
- [ ] The in-flight (last) row has `ended_at == started_at` and `duration_ms == 0` — sentinel for "still open"
- [ ] `margin.log` contains **no** `GetForegroundWindow` log lines (the monitoring is event-driven, not polling)

---

## L4.3 — Idle detection + pickup counting

1. With Margin running and at least one app session open, **stop all keyboard / mouse input** for the idle threshold (default 180 s = 3 minutes; temporarily lower via the SpinBox to 60 s to shorten the test)
2. After the threshold elapses:
   - [ ] The Screen Time tab status card flips to `💤 闲置中`
   - [ ] `margin.log` records `user idle: true`
3. Move the mouse / press a key:
   - [ ] Status card returns to `前台: <app>`
   - [ ] `margin.log` records `user idle: false`
4. Click **刷新** (refresh) — the **Pickup count** at the bottom of the day view should have ticked up by 1

Inspect the DB:

```bash
sqlite3 "%APPDATA%\Margin\data\margin.db" \
  "SELECT event_type, occurred_at, prev_idle_ms FROM screen_time_activity_event ORDER BY occurred_at DESC LIMIT 3"
```

- [ ] At least one `pickup` row with `prev_idle_ms` close to the threshold you used
- [ ] The session that was open when idle fired has `is_idle_end = 1`:

  ```bash
  sqlite3 "%APPDATA%\Margin\data\margin.db" \
    "SELECT id, app_name, is_idle_end FROM screen_time_app_session ORDER BY id DESC LIMIT 3"
  ```

---

## L4.4 — Encryption at rest

The `window_title` column is AES-256-GCM ciphertext — must not be readable in `sqlite3`. The audit is shared with [M2_PRIVACY_AUDIT.md §B.1](M2_PRIVACY_AUDIT.md), repeated here as a gate.

```bash
sqlite3 "%APPDATA%\Margin\data\margin.db" \
  "SELECT id, app_name, typeof(window_title_enc), hex(window_title_enc) FROM screen_time_app_session WHERE window_title_enc IS NOT NULL LIMIT 1"
```

- [ ] `typeof(window_title_enc)` = `blob`
- [ ] `hex(...)` looks like random bytes (no recognizable UTF-8 — no app names, no doc titles, no URLs)
- [ ] Compare against `app_name` (plaintext) in the same row — that one **should** be readable. This contrast confirms field-level encryption, not whole-DB encryption.

Optional: from a Qt test harness or a tiny `Margin::CryptoService` consumer, decrypt the blob and confirm it matches the window title you remember.

---

## L4.5 — Report UI: day / week / month views

1. Generate at least 5 minutes of activity across 3+ apps, with one idle gap in between
2. On the **时长** tab:

- [ ] **今日** (day) view: top-apps bar chart populated, longest bar = your most-used app
- [ ] Bars are scaled to the **max** in the set (the longest is full-width; others proportional). Verify by reading the duration labels — they should match the bar widths visually.
- [ ] **分类占比** (category breakdown) shows categories like `Development` / `Browser` with percentages summing to ~100%
- [ ] **本周** (week) view: 7 bars, one per day, today's bar non-zero
- [ ] **本月** (month) view: 30 bars (most may be zero — only today has data unless you've been running M2 across days)
- [ ] Live status card at top updates within ~1 s of every window switch (`currentApp` + `已持续 X s`)
- [ ] Click **刷新** → report re-queries; bars / categories re-render

---

## L4.6 — Category rules (built-in JSON + user override)

1. Use Chrome / Edge / Firefox for ~30 s → switch back to anything else
2. Refresh the report
- [ ] `Browser` category shows up with non-zero duration
3. Use VS Code / IDEA / Visual Studio for ~30 s
- [ ] `Development` category gets duration

Override path:

1. Click **时长** tab → there is currently no full settings UI for categories in M2 (deferred — see [docs/12-deferred-items.md §A20](../../docs/12-deferred-items.md))
2. Manually edit `%APPDATA%\Margin\config\settings.json` and add:

   ```json
   { "plugins.screen_time.category_overrides": "[{\"match\":\"foo\\\\.exe\",\"category\":\"Custom\"}]" }
   ```

3. Restart Margin → `margin.log` should contain `category matcher: 9 default + 1 user rule(s)` (or similar; user rule count = 1)
- [ ] Log reflects the new rule count
- [ ] (Optional) Launch a process named `foo.exe` → its session gets category `Custom`

---

## L4.7 — Export + clear-all

Covers M2-C7. The export path decrypts `window_title` to plaintext — this is documented in [docs/07-privacy-security.md §Screen Time](../../docs/07-privacy-security.md) and audited in [M2_PRIVACY_AUDIT.md §B.3](M2_PRIVACY_AUDIT.md).

1. On **时长** tab, click **数据…** → ExportClearDialog opens
2. **Export JSON**:
   - [ ] OS-native FileDialog prompts for save path
   - [ ] Pick a path → green toast `JSON 已导出`
   - [ ] Open the resulting file — array of session objects, each with `app_name`, **plaintext** `window_title`, `category`, `started_at` (epoch ms), `duration_ms`, `is_idle_end` (bool), time-bucket fields
   - [ ] A separate `pickups` array contains any idle→active events
3. **Export CSV**:
   - [ ] Same FileDialog flow → green toast
   - [ ] File has a header row + one row per session, RFC 4180 compliant (CRLF endings; fields with commas are quoted)
4. **Clear all data**:
   - [ ] Click the red button → confirmation Dialog opens
   - [ ] Confirm → dialog closes, toast `已清除`
   - [ ] Refresh report → all bars / categories empty
   - [ ] `sqlite3 ... "SELECT COUNT(*) FROM screen_time_app_session"` = `0`
   - [ ] `SELECT COUNT(*) FROM screen_time_activity_event` = `0` (both tables wiped)

---

## L4.8 — Zero network traffic

The §1 red line. Same protocol as M1 L4.6.

- [ ] Start Wireshark (Npcap) on the network adapter, filter for `Margin.exe` PID
- [ ] Use Margin for 5 minutes: switch windows, idle/resume, view report, export JSON
- [ ] **Zero outbound** packets attributable to Margin.exe (no DNS, no TCP, no UDP)

If Wireshark shows anything: file a bug. This is the project's hard line.

---

## Sign-off

- [ ] All L4.1–L4.8 ✓
- [ ] [M2_PRIVACY_AUDIT.md](M2_PRIVACY_AUDIT.md) A.1–A.4 + B.1–B.5 ✓
- [ ] [docs/11-roadmap.md](../../docs/11-roadmap.md) M2-C1..C10 all checked
- [ ] [docs/12-deferred-items.md §A19](../../docs/12-deferred-items.md) records the macOS deferral with re-evaluation criteria

Tester: ____________________  Date: ____________
