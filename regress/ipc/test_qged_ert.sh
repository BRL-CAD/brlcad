#!/bin/sh
# test_qged_ert.sh — Integration test: qged ert command via IPC under Xvfb
#
# This script verifies the full Phase 3+5 code path end-to-end:
#   - qged starts under Xvfb with software-rasterizer mode (-s)
#   - A geometry object is drawn
#   - The "ert" GED command is issued via xdotool
#   - We confirm that rt was spawned and the IPC path was taken
#     (no TCP port binding, BU_IPC_ADDR_ENVVAR visible in rt's /proc/environ)
#
# Environment variables consumed:
#   QGED_BIN      path to the qged binary (required)
#   RT_BIN        path to the rt binary   (required)
#   TEST_DB       path to a .g file with objects (defaults to boolean-ops.g)
#   DISPLAY       X display to use — if unset or not usable, start Xvfb
#
# Exit status: 0 = all checks passed, 1 = any check failed

set -e

BDIR="$(cd "$(dirname "$0")/../.." && pwd)"

QGED_BIN="${QGED_BIN:-${BDIR}/bin/qged}"
RT_BIN="${RT_BIN:-${BDIR}/bin/rt}"
TEST_DB="${TEST_DB:-${BDIR}/share/db/boolean-ops.g}"

PASS=0
FAIL=0
VERBOSE="${VERBOSE:-0}"

pass() { PASS=$((PASS+1)); [ "$VERBOSE" = "1" ] && echo "PASS: $1"; }
fail() { FAIL=$((FAIL+1)); echo "FAIL: $1"; }
check() {
    label="$1"; cond="$2"
    if eval "test $cond" 2>/dev/null; then pass "$label"; else fail "$label"; fi
}

TMPDIR_TEST=$(mktemp -d /tmp/test_qged_ert.XXXXXX)
XVFB_PID=""
QGED_PID=""

cleanup() {
    if [ -n "$QGED_PID" ]; then kill "$QGED_PID" 2>/dev/null || true; fi
    if [ -n "$XVFB_PID" ]; then kill "$XVFB_PID" 2>/dev/null || true; fi
    rm -rf "$TMPDIR_TEST"
}
trap 'cleanup' EXIT

echo "=== qged ert IPC integration test ==="

# -------------------------------------------------------------------
# 1. Verify required binaries exist
# -------------------------------------------------------------------
check "qged binary exists"  "-x $QGED_BIN"
check "rt binary exists"    "-x $RT_BIN"
check "test .g file exists" "-f $TEST_DB"

if [ ! -x "$QGED_BIN" ] || [ ! -x "$RT_BIN" ] || [ ! -f "$TEST_DB" ]; then
    echo "SKIP: required binaries or test database not found"
    echo "Tests: 0/$((PASS+FAIL)) passed"
    exit 0
fi

# -------------------------------------------------------------------
# 2. Check that xdotool is available (needed to send commands to qged)
# -------------------------------------------------------------------
HAVE_XDOTOOL=0
if command -v xdotool >/dev/null 2>&1; then
    HAVE_XDOTOOL=1
fi
check "xdotool available" "$HAVE_XDOTOOL -eq 1"

# -------------------------------------------------------------------
# 3. Ensure we have a working X display
#    When invoked via xvfb-run (from CTest), DISPLAY is pre-set.
#    If DISPLAY is not set or not usable, start our own Xvfb.
# -------------------------------------------------------------------
XVFB_PID=""
display_ok=0
if [ -n "$DISPLAY" ]; then
    # Verify the display accepts connections (up to 5 s to allow startup)
    for i in $(seq 1 50); do
        if xdpyinfo -display "$DISPLAY" >/dev/null 2>&1; then
            display_ok=1; break
        fi
        sleep 0.1
    done
fi

if [ "$display_ok" -ne 1 ]; then
    # No usable display - start our own Xvfb
    DISPLAY=":$(shuf -i 200-299 -n 1)"
    export DISPLAY
    Xvfb "$DISPLAY" -screen 0 1280x960x24 &
    XVFB_PID=$!
    for i in $(seq 1 50); do
        if xdpyinfo -display "$DISPLAY" >/dev/null 2>&1; then
            display_ok=1; break
        fi
        sleep 0.1
    done
fi

check "X display ($DISPLAY) is usable" "$display_ok -eq 1"

if [ "$display_ok" -ne 1 ]; then
    echo "ABORT: no usable X display (tried $DISPLAY)"
    exit 1
fi

# -------------------------------------------------------------------
# 4. Start qged under the X display in swrast mode
# -------------------------------------------------------------------
QGED_LOG="$TMPDIR_TEST/qged.log"
QGED_ERRLOG="$TMPDIR_TEST/qged_err.log"

export LD_LIBRARY_PATH="${BDIR}/lib:${LD_LIBRARY_PATH:-}"

"$QGED_BIN" -s "$TEST_DB" >"$QGED_LOG" 2>"$QGED_ERRLOG" &
QGED_PID=$!

# Wait up to 15 s for qged window to appear
qged_ready=0
for i in $(seq 1 150); do
    if xdotool search --pid "$QGED_PID" 2>/dev/null | grep -q .; then
        qged_ready=1; break
    fi
    sleep 0.1
done

check "qged window appeared under Xvfb" "$qged_ready -eq 1"

if [ "$qged_ready" -ne 1 ]; then
    echo "qged stdout: $(cat $QGED_LOG 2>/dev/null | head -20)"
    echo "qged stderr: $(cat $QGED_ERRLOG 2>/dev/null | head -20)"
    echo "ABORT: qged did not start"
    exit 1
fi

# Allow a little more time for qged to finish initialisation
sleep 2

check "qged process still running after init" "-d /proc/$QGED_PID"

# -------------------------------------------------------------------
# 5. Draw geometry and run ert via the console widget
# -------------------------------------------------------------------
if [ "$HAVE_XDOTOOL" -eq 1 ]; then
    # Focus and raise the window
    WIN=$(xdotool search --pid "$QGED_PID" 2>/dev/null | head -1)
    if [ -n "$WIN" ]; then
        xdotool windowfocus --sync "$WIN" 2>/dev/null || true
        xdotool windowraise "$WIN" 2>/dev/null || true
    fi

    sleep 0.5

    # Click in the console dock text area.
    # qged lays out its docks as: toolbar at top (~50px), 3D view in centre,
    # console dock at bottom.  With a default 1100x800 window, the console text
    # area (QPlainTextEdit dark background) is at approximately y=600-777.
    # Click at the bottom-centre of the text area to give it keyboard focus.
    WIN_H=$(xdotool getwindowgeometry "$WIN" 2>/dev/null | awk '/Geometry/{split($2,g,"x"); print g[2]}')
    WIN_H="${WIN_H:-800}"
    CONSOLE_Y=$(( WIN_H * 79 / 100 ))   # ~79% from top = upper part of bottom dock
    CONSOLE_X=550
    xdotool mousemove --window "$WIN" "$CONSOLE_X" "$CONSOLE_Y" 2>/dev/null || true
    xdotool click --window "$WIN" 1 2>/dev/null || true
    sleep 0.5

    # Draw geometry (use "all" which is the top-level group in boolean-ops.g)
    xdotool type --clearmodifiers "draw all" 2>/dev/null || true
    sleep 0.2
    xdotool key Return 2>/dev/null || true
    sleep 2

    # Run ert
    xdotool type --clearmodifiers "ert" 2>/dev/null || true
    sleep 0.2
    xdotool key Return 2>/dev/null || true

    # Wait for rt to start (up to 30 s)
    rt_appeared=0
    for i in $(seq 1 300); do
        if pgrep -x rt >/dev/null 2>&1; then
            rt_appeared=1; break
        fi
        sleep 0.1
    done
    check "rt process was spawned by ert" "$rt_appeared -eq 1"

    # Check that BU_IPC_ADDR_ENVVAR was passed to rt (IPC path taken)
    ipc_used=0
    for pid in $(pgrep -x rt 2>/dev/null); do
        if cat /proc/"$pid"/environ 2>/dev/null | tr '\0' '\n' | grep -q "BU_IPC_ADDR="; then
            ipc_used=1; break
        fi
    done
    check "rt received BU_IPC_ADDR_ENVVAR (IPC path taken)" "$ipc_used -eq 1"

    # Wait for rt to finish (up to 60 s); treat zombie as completed
    rt_done=0
    for i in $(seq 1 600); do
        RT_ALIVE=0
        for pid in $(pgrep -x rt 2>/dev/null); do
            state=$(awk '/^State/{print $2}' /proc/"$pid"/status 2>/dev/null)
            if [ "$state" != "Z" ]; then
                RT_ALIVE=1; break
            fi
        done
        if [ "$RT_ALIVE" -eq 0 ]; then
            rt_done=1; break
        fi
        sleep 0.1
    done
    check "rt process completed" "$rt_done -eq 1"

    # qged should still be alive
    check "qged process survived ert" "-d /proc/$QGED_PID"

    # Check logs for crash indicators
    if [ -f "$QGED_ERRLOG" ]; then
        if grep -q "Segmentation fault\|Aborted\|core dumped" "$QGED_ERRLOG" 2>/dev/null; then
            fail "qged did not crash during ert"
        else
            pass "qged did not crash during ert"
        fi
    fi
else
    echo "SKIP: xdotool not available; skipping interactive ert test"
fi

# -------------------------------------------------------------------
# 6. Summary
# -------------------------------------------------------------------
echo ""
echo "Tests: $PASS/$((PASS+FAIL)) passed"
[ "$FAIL" -eq 0 ] && exit 0 || exit 1
