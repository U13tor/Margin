# M2 — Privacy Audit Checklist (Screen Time)

> Required for [docs/11-roadmap.md M2-C9](../../docs/11-roadmap.md). Run once per M2 release (and any time you touch crypto / DB schema / export paths).
>
> M2 introduces the **first collection of user behavioral data** in Margin (foreground app, window title, idle pickups). The privacy boundary expands beyond M1's "paired device info" into "user daily behavior profile", so the audit gates the milestone.

This is **human-executed**. CI runs the static greps but the dynamic checks (Wireshark, sqlite3 inspection) need hands-on time on a real Windows session.

---

## Pre-flight

1. **Clean state**:
   - Stop Margin if running
   - Delete `%LOCALAPPDATA%\Margin\logs\margin.log`
   - Delete `%APPDATA%\Margin\data\margin.db` (and `-wal`, `-shm`) to start from an empty schema
2. **Build**: `bash scripts/build-smoke.sh win64-msvc-debug` — all 5 steps must pass (configure / build / ctest / audit / startup line). Step 4 already covers static greps 1+2 below, so the per-release audit just re-confirms.

---

## A. Static greps (code never lies)

### A.1 — No placeholder / XOR in crypto path

[CLAUDE.md §5.2](../../CLAUDE.md) invariant. The audit must include `src/plugins/screen_time/` — M2 is the first code path outside Aura that touches crypto.

```bash
grep -rEi --include="*.cpp" --include="*.h" "placeholder|XOR" src/
```

- [ ] Returns **0 lines** (binary PNGs are expected noise; the `--include` filter skips them)

### A.2 — No network classes anywhere

[CLAUDE.md §5.1](../../CLAUDE.md) red line. Screen Time has no reason to phone home — every byte stays local.

```bash
grep -rn "QNetworkAccessManager\|QNetworkRequest\|QTcpSocket\|QUdpSocket\|WinHttp\|WinINet\|libcurl" src/
```

- [ ] Returns **0 hits**

### A.3 — Manifest declares the three required permissions

```bash
cat src/plugins/screen_time/manifest.json | grep permissions
```

- [ ] Output contains `"active-window-monitor"`, `"input-monitor"`, `"database-write"`
- [ ] Output does **not** contain any of: `network`, `bluetooth-gatt`, `system-event`

### A.4 — Per-plugin HKDF key isolation intact

```bash
ctest --test-dir build/win64-msvc-debug -R test_crypto_service --output-on-failure
```

- [ ] Passes — verifies that two plugins deriving keys from the same master cannot decrypt each other's blobs. Screen Time's `window_title_enc` is unreadable by Aura (and vice versa).

---

## B. Dynamic checks (run-time behavior)

### B.1 — Database encryption at rest

1. Start Margin, switch foreground windows 5–10 times (browser / editor / file explorer / etc.), idle for 1 minute then resume.
2. Stop Margin.
3. Open the DB:

   ```bash
   sqlite3 "%APPDATA%\Margin\data\margin.db" \
     "SELECT id, app_name, typeof(window_title_enc), window_title_enc FROM screen_time_app_session LIMIT 3"
   ```

- [ ] `typeof(window_title_enc)` returns `blob` (not `text`)
- [ ] The blob bytes are **not** the plain window title (no readable English / Chinese fragments). It's base64 of `nonce ‖ ct ‖ tag` — looks like random bytes.
- [ ] `app_name` column **is** readable (intentional plaintext — process names are not sensitive)

### B.2 — Zero outbound network traffic

Same protocol as M1 L4.6 (zero-network red line).

1. Start Wireshark on the active network adapter, filter `tcp or udp and margin.exe` (or use the Npcap process filter)
2. Use Margin for ~5 minutes: switch windows, idle/resume, view report, export JSON to disk
3. Stop capture

- [ ] **Zero outbound** packets attributable to Margin.exe (DNS / TCP / UDP all 0)
- [ ] Optional: confirm that whatever packets do appear (mDNS, OS background chatter) are not from `Margin.exe`'s PID

### B.3 — Export path is user-chosen

The export feature decrypts `window_title` to plaintext at write time (documented in [docs/07-privacy-security.md §Screen Time](../../docs/07-privacy-security.md)). The audit verifies:

1. In Margin, click Tab2 "时长" → "数据…" → "导出 JSON"
2. A native `FileDialog` opens prompting the user for a save path
3. Pick a path and confirm

- [ ] The dialog is the OS-native save dialog (Windows: `IFileSaveDialog`)
- [ ] Margin does **not** silently write to a hard-coded path
- [ ] The resulting JSON file contains plaintext titles — this is **expected and documented**, not a leak

### B.4 — Clear-all is destructive + transactional

1. Use Margin enough to have ≥3 sessions + 1 pickup in the DB
2. Click Tab2 → "数据…" → "清除全部数据" → confirm dialog
3. Reload the report (toggle day/week view to force refresh)
4. Open DB:

   ```bash
   sqlite3 "%APPDATA%\Margin\data\margin.db" \
     "SELECT (SELECT COUNT(*) FROM screen_time_app_session), (SELECT COUNT(*) FROM screen_time_activity_event)"
   ```

- [ ] Both counts are `0`
- [ ] Margin's log contains `cleared all data` (no errors)

### B.5 — Hook callback does not consume keystrokes / mouse events

InputMonitorService installs `WH_KEYBOARD_LL` + `WH_MOUSE_LL`. A misbehaving low-level hook that returns non-zero would **swallow** the event. Verify the hook always calls `CallNextHookEx`:

1. Static check:

   ```bash
   grep -A2 "LowLevelInputProc\|LowLevelKeyboardProc\|LowLevelMouseProc" \
     src/host/platform/windows/IdleDetectorWin.cpp | grep -c CallNextHookEx
   ```

   - [ ] Returns ≥1 (every hook path forwards via CallNextHookEx)

2. Dynamic check: with Margin running and idle detection active, type normally in another app for 30 seconds.

   - [ ] No dropped keystrokes (text appears as typed)
   - [ ] No lag (the hook is fast — must not visibly delay input)

---

## C. Sign-off

- [ ] All A.1–A.4 ✓
- [ ] All B.1–B.5 ✓
- [ ] [docs/07-privacy-security.md](../../docs/07-privacy-security.md) §Screen Time reflects the current schema and export path
- [ ] [M2_ACCEPTANCE.md](M2_ACCEPTANCE.md) L4.4 (encryption) + L4.6 (zero network) reference this audit

Auditor: ____________________  Date: ____________
