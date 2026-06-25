# M1 — Manual Acceptance Checklist (Aura Locker)

> Verify the Aura Locker plugin end-to-end on Windows. Required for the [roadmap M1 Definition of Done](../../docs/11-roadmap.md) items 1–4 + 6.
>
> macOS is **deferred to v1.1** ([docs/12-deferred-items.md §A15](../../docs/12-deferred-items.md)); only Windows acceptance is in scope this round. The Mac checklist lives there too — re-run the same L4 cases after §A15 lifts.

This file is **human-executed**. CI cannot tick these — every box below needs a real BLE device + a real Windows session.

---

## Setup

1. **Hardware prep**: a paired BLE device (phone / headphones / smartwatch). Avoid devices you actively depend on for the next 30 minutes — the test will trigger real screen locks.
2. **Clean state**:
   - Delete `%LOCALAPPDATA%\Margin\logs\margin.log` so this run is the only content
   - Delete `%APPDATA%\Margin\config\settings.json` to clear any prior paired-device record (so we exercise the scan + pair path from scratch)
3. **Build**: `bash scripts/build-smoke.sh win64-msvc-debug` — all 5 steps must pass (configure / build / ctest / audit / startup line)
4. **Locate binary**: `build/win64-msvc-debug/src/host/Margin.exe`
5. **Disable Windows auto-lock** (Settings → Sign-in options → "Dynamic lock" off) so the OS doesn't race the Aura lock trigger and confuse the test
6. **Optional but recommended**: open Wireshark on the Bluetooth adapter (Npcap + Bluetooth Monitor filter) to verify L4.6 (zero outbound traffic from Margin.exe)

---

## L4.1 — Tab + status card visible

- [ ] Launch `Margin.exe`
- [ ] System tray shows the Margin icon (same as M0)
- [ ] Dashboard shows a second tab **Aura** with the Bluetooth icon
- [ ] Click **Aura** tab → status card reads `Not paired`
- [ ] `margin.log` contains `AuraLockerPlugin onLoad paired=no thresh=-65dBm away=30s cooldown=60s`

---

## L4.2 — Scan + pair

- [ ] Click **Settings** on the Aura tab → Settings page opens (StackView push)
- [ ] Click **Scan for devices** → button label changes to `Scanning…` (disabled)
- [ ] Wait ~10 s → button returns to `Scan for devices`, **device list** populates
- [ ] At least one visible device (your paired phone/headphones) is listed with name + `deviceId` + RSSI in dBm
- [ ] Click **Pair** next to your device → list collapses, returns to Aura tab
- [ ] Aura tab status card now reads `Paired: <your device name>`
- [ ] `margin.log` contains `BLE scan started (10000 ms)` then `BLE scan finished: N device(s)` then `AuraLockerPlugin onLoad paired=yes` on next launch
- [ ] Open `%APPDATA%\Margin\config\settings.json` — the `plugins.aura.paired_device_identifier` value MUST be an envelope `{"__encrypted__": true, "iv": "...", "ct": "..."}` (NOT raw plaintext MAC). Use `type %APPDATA%\Margin\config\settings.json` in cmd.

---

## L4.3 — Away detection triggers screen lock

This is the headline KPI of M1. Allow ~3 minutes.

- [ ] Make sure the paired device is **close to the PC** (≤1 m) so RSSI is healthy
- [ ] Wait 30 s after pairing — the detector needs ≥5 smoothed samples before arming the away timer (sanity buffer)
- [ ] Move the paired device **>5 m away** from the PC (next room, with the door closed if possible)
- [ ] Within ~30 s, `margin.log` should contain `Aura triggered screen lock (away)`
- [ ] The screen locks (you see the Windows lock screen). Enter your password to unlock.
- [ ] After unlock, `margin.log` should contain a `margin.aura.state` event publish with `"state": "cooldown"`

---

## L4.4 — Cooldown suppresses re-lock

- [ ] From the unlocked state, **immediately** move the paired device away again (>5 m)
- [ ] Wait 30 s — screen should **NOT** lock this time
- [ ] `margin.log` contains no new `Aura triggered screen lock (away)` line in this window
- [ ] After the 60 s cooldown elapses (visible in log: state transitions back to `paired`), move the device away one more time
- [ ] Screen locks again on the 30 s away window — confirms cooldown is one-shot, not sticky

---

## L4.5 — BT module off → warning, no lock

- [ ] Pair a device, confirm L4.3 works (sanity)
- [ ] Open Windows Settings → Bluetooth & devices → toggle Bluetooth **off**
- [ ] Within ~5 s, Aura tab should update its state (or at least `margin.log` should record `BT radio off — away-locks suppressed until radio returns`)
- [ ] A `margin.aura.warning` event publishes in EventBus (check log) with `reason: "bt_disabled"`
- [ ] Move the paired device away for 60+ s — screen must NOT lock
- [ ] Toggle Bluetooth **on** again
- [ ] Within ~5 s, `margin.log` records `BT radio on — away-locks re-enabled`
- [ ] Move device away again — L4.3 should fire normally

---

## L4.6 — Zero network traffic (data localization)

The §1 invariant: Margin never opens a socket. Aura's BLE traffic is OS-internal (advertisement packets the kernel handles for us), not Margin-initiated.

- [ ] Open Wireshark with Npcap + monitor the Bluetooth adapter (or use `pktmon` if Wireshark isn't available)
- [ ] Pair a device, leave it in range for 2 minutes, then move it away and trigger a lock
- [ ] Filter Wireshark for any TCP/UDP packet from the `Margin.exe` process
- [ ] **Zero outbound packets** from Margin.exe across the entire test (not even DNS, not even on first launch — there's no telemetry)

If Wireshark shows anything: file a bug. This is the project's hard line.

---

## L4.7 — iOS LE Privacy / IRK stability

Mac-only verification path (skip on Windows if no iOS device is available).

- [ ] Pair an iPhone running iOS 14+ (LE Privacy enabled by default)
- [ ] Keep Margin running + iPhone paired for **15+ minutes** (one full MAC rotation cycle)
- [ ] Across the rotation boundary, away/back detection should still recognize the same paired device (Windows resolves IRK → stable id at the BLE stack level)
- [ ] `margin.log` should NOT show a "device disappeared / new device appeared" pair of events around the rotation timestamp
- [ ] Trigger an away event at the 16-minute mark — should still fire lockScreen

If the iPhone appears as a "new" device after 15 min on Windows, that's a Windows BLE stack limitation, not an Aura bug — document it but don't block M1 sign-off. Mac (§A15) is the SSOT for IRK interop validation.

---

## Final audit (re-run before signing off)

These are also enforced by `scripts/build-smoke.sh` step 4, but tick them manually one more time:

- [ ] `grep -rEIi "placeholder|XOR" src/` returns empty
- [ ] `grep -rEn "QNetworkAccessManager|QNetworkRequest" src/` returns empty
- [ ] `grep -rn "QBluetooth" src/` returns empty (we use cppwinrt, not Qt's BLE module)

---

## Notes

- **Log path**: `%LOCALAPPDATA%\Margin\logs\margin.log` (Windows). `tail -f` in Git Bash works fine.
- **Settings path**: `%APPDATA%\Margin\config\settings.json`. Encrypted values look like `{"__encrypted__": true, "iv": "...", "ct": "..."}`.
- **If away lock doesn't fire**: check (1) is the device actually moving out of range? (2) is `plugins.aura.rssi_threshold` reasonable for the device's BLE class? (-65 dBm default may be too aggressive for some headsets — try -55 via Settings slider) (3) did the 5-sample warmup complete?
- **If BT-off warning doesn't fire**: the Win watcher's Stopped event needs Status=Aborted. Some OEM Bluetooth stacks report Stopped instead of Aborted — we treat both as Off. If your radio never reports either, the away-detection path still runs (lock can fire) and we're missing the safety net. File an issue with the adapter model.
- **Failure to tick any L4.x item above = M1 not done**. Don't mark the milestone complete in [docs/11-roadmap.md](../../docs/11-roadmap.md) until every box here is checked.
