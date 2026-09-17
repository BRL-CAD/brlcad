#!/bin/sh

set -eu

: "${GUI_TEST_SHELL:?GUI_TEST_SHELL is required}"
. "$GUI_TEST_SHELL"

gui_stop_app()
{
    if [ -n "${GUI_APP_PID:-}" ] && kill -0 "$GUI_APP_PID" 2>/dev/null; then
	kill "$GUI_APP_PID" >/dev/null 2>&1 || true
    fi
}

if [ "$#" -lt 2 ]; then
    echo "Usage: $0 expected-window executable [argument ...]" >&2
    exit 2
fi

: "${GUI_TEST_CTL:?GUI_TEST_CTL is required}"
expected_window=$1
shift
application=$1
shift

gui_require_executable "$GUI_TEST_CTL"
gui_require_executable "$application"

GUI_ARTIFACT_DIR=$(mktemp -d "${TMPDIR:-/tmp}/brlcad-gui-smoke.XXXXXX")
export GUI_ARTIFACT_DIR
GUI_APP_LOG="$GUI_ARTIFACT_DIR/application.log"
export GUI_APP_LOG
trap gui_stop_app EXIT INT TERM

"$application" "$@" >"$GUI_APP_LOG" 2>&1 &
GUI_APP_PID=$!
export GUI_APP_PID

if ! window_id=$("$GUI_TEST_CTL" wait-window --timeout "$GUI_WAIT_TIMEOUT_MS" \
    "$expected_window"); then
    gui_fail "window did not appear: $expected_window"
fi
if ! kill -0 "$GUI_APP_PID" 2>/dev/null; then
    "$GUI_TEST_CTL" wait-stable --quiet "$GUI_STABLE_QUIET_MS" \
	--timeout "$GUI_STABLE_TIMEOUT_MS" "$window_id" >/dev/null 2>&1 || true
    gui_fail "application exited before its window could be tested"
fi

"$GUI_TEST_CTL" activate "$window_id"
if ! geometry=$(gui_wait_for_minimum_geometry "$window_id" \
    "$GUI_MIN_WINDOW_WIDTH" "$GUI_MIN_WINDOW_HEIGHT"); then
    gui_fail "window did not reach a usable geometry: $expected_window ($geometry)"
fi

gui_capture_screen "$window_id" "$GUI_ARTIFACT_DIR/before.ppm"

if [ -n "${GUI_SMOKE_ACTION:-}" ]; then
    gui_require_executable "$GUI_SMOKE_ACTION"
    "$GUI_SMOKE_ACTION" "$window_id" "$GUI_ARTIFACT_DIR"
    if [ "${GUI_MIN_CHANGED_BYTES:-0}" -gt 0 ]; then
	gui_wait_for_image_difference "$GUI_ARTIFACT_DIR/before.ppm" \
	    "$GUI_ARTIFACT_DIR/after.ppm" "$GUI_MIN_CHANGED_BYTES" \
	    "$window_id"
    else
	gui_capture_screen "$window_id" "$GUI_ARTIFACT_DIR/after.ppm"
    fi
fi

"$GUI_TEST_CTL" close "$window_id"
wait "$GUI_APP_PID"
GUI_APP_PID=

if [ "${GUI_KEEP_ARTIFACTS:-0}" -eq 1 ]; then
    echo "GUI smoke artifacts: $GUI_ARTIFACT_DIR"
else
    "${CMAKE_COMMAND:-cmake}" -E remove_directory "$GUI_ARTIFACT_DIR"
fi

echo "PASS: $expected_window launched, rendered, and closed"

# Local Variables:
# tab-width: 8
# mode: sh
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8 cino=N-s
