# M0 — Manual Acceptance Checklist

> Verify the M0 vertical slice end-to-end. Run after `M0-C7` lands; required for the [roadmap M0 Definition of Done](../../docs/11-roadmap.md) item 6: *"关闭进程 → 任务管理器无残留进程,日志以 `[shutdown complete]` 结束"*.

This file is **human-executed**. M0-C9 will add a Python smoke runner; until then, manual is the gate.

---

## Setup

1. **Clean log state**: delete `%LOCALAPPDATA%\Margin\logs\margin.log` (Win) or `~/Library/Logs/Margin/margin.log` (Mac) so this run is the only content.
2. **Build**: `bash scripts/build-smoke.sh win64-msvc-debug` — all 5 steps must pass.
3. **Locate binary**:
   - Win: `build/win64-msvc-debug/src/host/Margin.exe`
   - Mac: `build/macos-clang-debug/src/host/Margin`

---

## 1. Startup smoke (`--smoke`)

- [ ] `Margin.exe --smoke` exits within ~1 s (500 ms timer + shutdown chain)
- [ ] stdout contains `Margin v0.1.0 starting`
- [ ] `margin.log` contains no `[ERROR]` or `[FATAL]` lines
- [ ] `margin.log` final line is `[YYYY-MM-DD HH:MM:SS] [INFO] [host] shutdown complete`

---

## 2. Tray + Hello plugin

- [ ] Launch `Margin.exe` (no args)
- [ ] System tray shows the Margin SVG icon (not a blank/missing-icon placeholder)
- [ ] Right-click tray → menu shows **Say Hello** above **Quit**
- [ ] Click **Say Hello** → `margin.log` appends a line containing `Hello from HelloPlugin`

---

## 3. Clean shutdown (M0-C7 focus)

- [ ] Right-click tray → **Quit** → process exits within ~2 s
- [ ] **Task Manager / Activity Monitor**: no `Margin.exe` / `Margin` process remains after exit
- [ ] `margin.log` final line: `[YYYY-MM-DD HH:MM:SS] [INFO] [host] shutdown complete`
- [ ] Shutdown order visible in `margin.log` (timestamps strictly increasing):
  1. `[host] shutdown: hiding tray`
  2. `[host] shutdown: unloading plugins`
  3. `[plugin] unloading hello` → `[plugin] unloaded hello` (per loaded plugin)
  4. `[host] shutdown: stopping eventbus`
  5. `[host] shutdown complete`
- [ ] **No `[ERROR]` or `[FATAL]` lines anywhere in the log** (the §5.2 audit pattern doesn't catch these — they're level-tagged, not keyword-tagged)

---

## 4. Re-launch stability

- [ ] After Quit completes, wait 1 s
- [ ] Re-launch `Margin.exe` → starts cleanly, no crash, tray icon reappears
- [ ] Repeat Quit → no residual state, log appended correctly

---

## 5. Final audit (anti-pattern grep)

These are also run by `scripts/build-smoke.sh` step 4 — repeat manually for confidence:

- [ ] `grep -rEIi "placeholder|XOR" src/` returns empty
- [ ] `grep -rEn "QNetworkAccessManager|QNetworkRequest" src/` returns empty

---

## Notes

- **Log path lookup**: don't hard-code the path. Run via [Margin::Paths::logs()](../../src/paths/Paths.cpp) semantics — Windows `%LOCALAPPDATA%\Margin\logs\`, Mac `~/Library/Logs/Margin/`.
- **If shutdown hangs**: check Task Manager for the process, kill it manually, then look for the last log line — that's where shutdown stalled.
- **Failure to tick any item above = M0 not done**. File an issue / start a discussion before moving to M1.
