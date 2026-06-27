#!/usr/bin/env python3
"""Layer-4 smoke test (M0-C9): launch the real Margin binary headless via
``--smoke`` and verify it starts, runs the clean-shutdown chain, and exits 0
with no error lines.

Usage:
    smoke_runner.py <path-to-Margin-binary>

Registered via CTest with LABEL "smoke" (see tests/smoke/CMakeLists.txt), so
``ctest -L smoke`` runs it on Windows + macOS CI runners. Mirrors the bash
check in scripts/build-smoke.sh step 5, but additionally asserts the clean
shutdown line so CI catches a shutdown regression without a human.
"""

import os
import subprocess
import sys

STARTUP_LINE = "Margin v0.1.0 starting"
SHUTDOWN_LINE = "shutdown complete"
TIMEOUT_S = 30


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <path-to-Margin-binary>", file=sys.stderr)
        return 2

    binary = sys.argv[1]
    if not os.path.isfile(binary):
        print(f"[smoke] FAIL: binary not found: {binary}", file=sys.stderr)
        return 1

    # offscreen: no display server needed (CI runners are headless).
    # --smoke: in-process 500ms self-quit, so the process ends on its own.
    env = dict(os.environ, QT_QPA_PLATFORM="offscreen")
    try:
        proc = subprocess.run(
            [binary, "--smoke"],
            env=env,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=TIMEOUT_S,
        )
    except subprocess.TimeoutExpired:
        print(f"[smoke] FAIL: did not exit within {TIMEOUT_S}s (shutdown hang?)",
              file=sys.stderr)
        return 1

    out = proc.stdout + proc.stderr

    failures = []
    if proc.returncode != 0:
        failures.append(f"exit code {proc.returncode} (expected 0)")
    if STARTUP_LINE not in out:
        failures.append(f"missing startup line '{STARTUP_LINE}'")
    if SHUTDOWN_LINE not in out:
        failures.append(f"missing clean-shutdown line '{SHUTDOWN_LINE}'")
    for marker in ("[ERROR]", "[FATAL]"):
        if marker in out:
            failures.append(f"unexpected {marker} line in output")

    if failures:
        print("[smoke] FAIL:", file=sys.stderr)
        for f in failures:
            print(f"  - {f}", file=sys.stderr)
        print("[smoke] --- captured output ---", file=sys.stderr)
        print(out, file=sys.stderr)
        return 1

    print(f"[smoke] OK: startup + clean shutdown verified ({binary})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
