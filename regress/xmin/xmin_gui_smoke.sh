#!/bin/sh

set -eu

: "${XMIN_TEST_SHELL:?XMIN_TEST_SHELL is required}"
. "$XMIN_TEST_SHELL"

xmin_stop_app()
{
    if [ -n "${XMIN_APP_PID:-}" ] && kill -0 "$XMIN_APP_PID" 2>/dev/null; then
	kill "$XMIN_APP_PID" >/dev/null 2>&1 || true
    fi
}

if [ "$#" -lt 2 ]; then
    echo "Usage: $0 expected-window executable [argument ...]" >&2
    exit 2
fi

: "${XMIN_CTL:?XMIN_CTL is required}"
expected_window=$1
shift
application=$1
shift

xmin_require_executable "$XMIN_CTL"
xmin_require_executable "$application"

XMIN_ARTIFACT_DIR=$(mktemp -d /tmp/brlcad-xmin-smoke.XXXXXX)
export XMIN_ARTIFACT_DIR
XMIN_APP_LOG="$XMIN_ARTIFACT_DIR/application.log"
export XMIN_APP_LOG
trap xmin_stop_app EXIT INT TERM

"$application" "$@" >"$XMIN_APP_LOG" 2>&1 &
XMIN_APP_PID=$!
export XMIN_APP_PID

if ! window_id=$("$XMIN_CTL" wait-window --timeout "$XMIN_WAIT_TIMEOUT_MS" \
    "$expected_window"); then
    xmin_fail "window did not appear: $expected_window"
fi
if ! kill -0 "$XMIN_APP_PID" 2>/dev/null; then
    "$XMIN_CTL" wait-stable --quiet "$XMIN_STABLE_QUIET_MS" \
	--timeout "$XMIN_STABLE_TIMEOUT_MS" "$window_id" >/dev/null 2>&1 || true
    xmin_fail "application exited before its window could be tested"
fi

"$XMIN_CTL" activate "$window_id"
geometry=$("$XMIN_CTL" geometry "$window_id") ||
    xmin_fail "could not query window geometry: $expected_window"
set -- $geometry
if [ "$#" -lt 4 ]; then
    xmin_fail "unexpected window geometry: $geometry"
fi
width=$3
height=$4
if [ "$width" -lt "$XMIN_MIN_WINDOW_WIDTH" ] ||
   [ "$height" -lt "$XMIN_MIN_WINDOW_HEIGHT" ]; then
    xmin_fail "window is unexpectedly small: ${width}x${height}"
fi

xmin_capture_root "$window_id" "$XMIN_ARTIFACT_DIR/before.ppm"

if [ -n "${XMIN_SMOKE_ACTION:-}" ]; then
    xmin_require_executable "$XMIN_SMOKE_ACTION"
    "$XMIN_SMOKE_ACTION" "$window_id" "$XMIN_ARTIFACT_DIR"
    xmin_capture_root "$window_id" "$XMIN_ARTIFACT_DIR/after.ppm"
    if [ "${XMIN_MIN_CHANGED_BYTES:-0}" -gt 0 ]; then
	xmin_assert_images_differ "$XMIN_ARTIFACT_DIR/before.ppm" \
	    "$XMIN_ARTIFACT_DIR/after.ppm" "$XMIN_MIN_CHANGED_BYTES"
    fi
fi

"$XMIN_CTL" close "$window_id"
wait "$XMIN_APP_PID"
XMIN_APP_PID=

if [ "${XMIN_KEEP_ARTIFACTS:-0}" -eq 1 ]; then
    echo "Xmin GUI smoke artifacts: $XMIN_ARTIFACT_DIR"
else
    "${CMAKE_COMMAND:-cmake}" -E remove_directory "$XMIN_ARTIFACT_DIR"
fi

echo "PASS: $expected_window launched, rendered, and closed under Xmin"
