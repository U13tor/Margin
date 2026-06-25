#!/usr/bin/env bash
# scripts/build-smoke.sh
#
# CI smoke test — every PR must pass this before merge.
# Verifies:
#   1. CMake configure succeeds
#   2. Build succeeds
#   3. Tests pass (if any)
#   4. No placeholder / XOR / network classes in src/ (anti-pattern from Margin1)
#   5. Binary launches and emits the startup line (CI-verifiable, no human eye)
#
# Usage:
#   ./scripts/build-smoke.sh                    # auto-detect platform
#   ./scripts/build-smoke.sh win64-msvc-debug
#   ./scripts/build-smoke.sh macos-clang-debug

set -euo pipefail

PLATFORM="${1:-auto}"

if [[ "$PLATFORM" == "auto" ]]; then
    case "$(uname -s)" in
        MINGW*|MSYS*|CYGWIN*) PLATFORM="win64-msvc-debug" ;;
        Darwin)               PLATFORM="macos-clang-debug" ;;
        *)
            echo "[build-smoke] Unsupported platform: $(uname -s)"
            echo "[build-smoke] Pass preset name explicitly: $0 <preset>"
            exit 1
            ;;
    esac
fi

# --- Local Windows bootstrap -------------------------------------------------
# CI runners (windows-latest) have MSVC + Qt pre-installed in PATH.
# Local Git Bash does not — re-exec through cmd.exe + vcvars64.bat to inherit
# the full MSVC + Qt environment, then re-enter this script.
if [[ "$(uname -s)" == MINGW* ]] && ! command -v cl.exe >/dev/null 2>&1; then
    VS_ROOT="C:\\Program Files\\Microsoft Visual Studio\\2022\\Community"
    VC_BAT="$VS_ROOT\\VC\\Auxiliary\\Build\\vcvars64.bat"
    VS_NINJA="$VS_ROOT\\Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\Ninja"

    if [ ! -f "$(cygpath -u "$VC_BAT")" ]; then
        echo "[build-smoke] FAIL: vcvars64.bat not found at $VC_BAT"
        echo "[build-smoke] Install Visual Studio 2022 with 'Desktop development with C++' workload"
        exit 1
    fi

    # Qt6_DIR: prefer already-exported, then user-level PowerShell env, then default.
    QT_DIR="${Qt6_DIR:-}"
    if [ -z "$QT_DIR" ]; then
        QT_DIR=$(powershell.exe -NoProfile -Command \
            '[Environment]::GetEnvironmentVariable("Qt6_DIR","User")' 2>/dev/null | tr -d '\r')
    fi
    QT_DIR="${QT_DIR:?Qt6_DIR env var must be set to your Qt 6.5 install path (e.g. D:/Qt/6.5.3/msvc2019_64)}"

    # VCPKG_ROOT: same precedence.
    VCPKG_ROOT_LOCAL="${VCPKG_ROOT:-}"
    if [ -z "$VCPKG_ROOT_LOCAL" ]; then
        VCPKG_ROOT_LOCAL=$(powershell.exe -NoProfile -Command \
            '[Environment]::GetEnvironmentVariable("VCPKG_ROOT","User")' 2>/dev/null | tr -d '\r')
    fi
    VCPKG_ROOT_LOCAL="${VCPKG_ROOT_LOCAL:?VCPKG_ROOT env var must be set to your vcpkg checkout path}"

    echo "[build-smoke] Local Windows: bootstrapping MSVC env (vcvars64 + Qt6_DIR + VCPKG_ROOT)..."
    SCRIPT_PATH=$(cygpath -w "$(readlink -f "$0")")
    BOOTSTRAP_BAT=$(mktemp -t margin-bootstrap.XXXXXX.bat)
    trap 'rm -f "$BOOTSTRAP_BAT"' EXIT
    cat > "$BOOTSTRAP_BAT" <<EOF_BAT
@echo off
call "$VC_BAT"
set "PATH=$VS_NINJA;$QT_DIR\\bin;%PATH%"
set "Qt6_DIR=$QT_DIR"
set "VCPKG_ROOT=$VCPKG_ROOT_LOCAL"
bash "$SCRIPT_PATH" "$PLATFORM"
EOF_BAT
    exec cmd.exe //c "$(cygpath -w "$BOOTSTRAP_BAT")"
fi

echo "[build-smoke] Detected/selected preset: $PLATFORM"

BUILD_DIR="build/$PLATFORM"

echo "[build-smoke] Step 1/5: Configure..."
cmake --preset "$PLATFORM"

echo "[build-smoke] Step 2/5: Build..."
cmake --build "$BUILD_DIR"

echo "[build-smoke] Step 3/5: Test (if any)..."
if ctest --test-dir "$BUILD_DIR" --output-on-failure; then
    echo "[build-smoke]   tests passed"
else
    rc=$?
    if compgen -G "$BUILD_DIR/Testing/*" > /dev/null; then
        echo "[build-smoke]   tests failed (exit $rc)"
        exit $rc
    fi
    echo "[build-smoke]   no tests configured (acceptable through M0-C8)"
fi

echo "[build-smoke] Step 4/5: Audit (no placeholder / XOR / network classes in src/)..."
if [ -d src/ ]; then
    # -I skips binary files (PNG/SVG may have arbitrary byte sequences that
    # look like XOR — those are not code, just data).
    if grep -rEIi "placeholder|XOR" src/; then
        echo "[build-smoke] FAIL: placeholder / XOR found in src/"
        echo "[build-smoke] See docs/13-lessons-learned.md and docs/07-privacy-security.md"
        exit 1
    fi
    if grep -rEn "QNetworkAccessManager\|QNetworkRequest" src/; then
        echo "[build-smoke] FAIL: Qt network classes found in src/ (violates §5.1 zero-network)"
        exit 1
    fi
fi

echo "[build-smoke] Step 5/5: Run binary + verify startup line..."
if [[ "$PLATFORM" == win64-* ]]; then
    BIN="$BUILD_DIR/src/host/Margin.exe"
elif [[ "$PLATFORM" == macos-* ]]; then
    BIN="$BUILD_DIR/src/host/Margin"
else
    echo "[build-smoke]   step 5 skipped (unknown platform: $PLATFORM)"
    echo "[build-smoke] OK — 4 of 5 smoke checks passed."
    exit 0
fi

if [ ! -f "$BIN" ]; then
    echo "[build-smoke] FAIL: binary not found at $BIN"
    exit 1
fi

# offscreen: runs without a display server (CI runners + headless dev loops).
# --smoke: in-process QTimer::singleShot(500) self-quit, so no external kill needed.
SMOKE_LOG=$(mktemp -t margin-smoke.XXXXXX.log)
trap 'rm -f "$SMOKE_LOG"' EXIT
QT_QPA_PLATFORM=offscreen "$BIN" --smoke > "$SMOKE_LOG" 2>&1 || true

if ! grep -q "Margin v0.1.0 starting" "$SMOKE_LOG"; then
    echo "[build-smoke] FAIL: startup line missing in output"
    echo "[build-smoke] --- binary output ---"
    cat "$SMOKE_LOG"
    exit 1
fi

echo "[build-smoke]   startup line detected"
echo "[build-smoke] OK — all 5 smoke checks passed."
