#!/bin/sh

set -eu

WAIT_LIMIT=800
WAIT_DELAY=0.05
CLICK_DELAY=20
SCREEN_GEOMETRY=1280x960x24
COMMAND_GEOMETRY=680x520+0+0
GRAPHICS_GEOMETRY=512x512+700+0

require_executable()
{
    if [ ! -x "$1" ]; then
	echo "FAIL: required executable is unavailable: $1" >&2
	exit 1
    fi
}

wait_for_file()
{
    count=0
    while [ "$count" -lt "$WAIT_LIMIT" ]; do
	if [ -s "$1" ]; then
	    return 0
	fi
	if [ -s "$MGED_XMIN_TEST_DIR/result" ]; then
	    return 1
	fi
	sleep "$WAIT_DELAY"
	count=$((count + 1))
    done
    return 1
}

wait_for_value()
{
    count=0
    while [ "$count" -lt "$WAIT_LIMIT" ]; do
	if [ -f "$1" ] && [ "$(sed -n '1p' "$1")" = "$2" ]; then
	    return 0
	fi
	if [ -s "$MGED_XMIN_TEST_DIR/result" ]; then
	    return 1
	fi
	sleep "$WAIT_DELAY"
	count=$((count + 1))
    done
    return 1
}

read_point()
{
    point=$(sed -n '1p' "$1")
    POINT_X=${point%% *}
    POINT_Y=${point#* }
}

read_target()
{
    TARGET_WINDOW=$(sed -n '1p' "$MGED_XMIN_TEST_DIR/$1_window")
    read_point "$MGED_XMIN_TEST_DIR/$1"
}

click_target()
{
    read_target "$1"
    "$XMIN_CTL" click --delay "$CLICK_DELAY" "$TARGET_WINDOW" "$POINT_X" "$POINT_Y" "${2:-1}"
}

replace_entry_text()
{
    target=$1
    initial_value=$2
    current_file=$3
    expected_value=$4

    click_target "$target"
    "$XMIN_CTL" key end
    remaining=${#initial_value}
    while [ "$remaining" -gt 0 ]; do
	"$XMIN_CTL" key backspace
	remaining=$((remaining - 1))
    done
    wait_for_value "$current_file" "" || return 1
    "$XMIN_CTL" type "$expected_value"
    wait_for_value "$current_file" "$expected_value"
}

capture_failure()
{
    "$XMIN_CTL" capture-root "$MGED_XMIN_TEST_DIR/failure.ppm" >/dev/null 2>&1 || true
}

fail()
{
    capture_failure
    echo "FAIL: $1" >&2
    if [ -f "$MGED_XMIN_TEST_DIR/result" ]; then
	sed -n '1,20p' "$MGED_XMIN_TEST_DIR/result" >&2
    fi
    for diagnostic in "$MGED_XMIN_TEST_DIR"/*_debug; do
	if [ -f "$diagnostic" ]; then
	    echo "$(basename "$diagnostic"): $(sed -n '1,20p' "$diagnostic")" >&2
	fi
    done
    if [ -f "$MGED_XMIN_TEST_DIR/mged.log" ]; then
	echo "MGED log follows:" >&2
	sed -n '1,240p' "$MGED_XMIN_TEST_DIR/mged.log" >&2
    fi
    exit 1
}

if [ "${MGED_XMIN_INNER:-0}" -eq 0 ]; then
    require_executable "$MGED_BIN"
    require_executable "$XMIN_RUN"
    require_executable "$XMIN_CTL"
    require_executable "$XMIN_SERVER"
    if [ ! -f "$MGED_XMIN_RC" ]; then
	echo "FAIL: MGED Xmin rc file is unavailable: $MGED_XMIN_RC" >&2
	exit 1
    fi

    MGED_XMIN_TEST_DIR=$(mktemp -d /tmp/mged-xmin-gui.XXXXXX)
    export MGED_XMIN_TEST_DIR
    cleanup()
    {
	if [ -n "${MGED_PID:-}" ]; then
	    kill "$MGED_PID" >/dev/null 2>&1 || true
	fi
	if [ "${MGED_XMIN_KEEP:-0}" -eq 1 ]; then
	    echo "MGED Xmin test artifacts: $MGED_XMIN_TEST_DIR" >&2
	else
	    "$CMAKE_COMMAND" -E remove_directory "$MGED_XMIN_TEST_DIR"
	fi
    }
    trap cleanup EXIT INT TERM

    test_db="$MGED_XMIN_TEST_DIR/gui-edit.g"
    "$MGED_BIN" -c "$test_db" "in gui.s sph 0 0 0 10" >"$MGED_XMIN_TEST_DIR/create.log" 2>&1

    MGED_XMIN_INNER=1
    export MGED_XMIN_INNER
    "$XMIN_RUN" --server "$XMIN_SERVER" --screen "$SCREEN_GEOMETRY" -- "$0"
    exit $?
fi

test_db="$MGED_XMIN_TEST_DIR/gui-edit.g"
inner_cleanup()
{
    if [ -n "${MGED_PID:-}" ]; then
	kill "$MGED_PID" >/dev/null 2>&1 || true
    fi
}
trap inner_cleanup EXIT INT TERM

"$MGED_BIN" --gui --dm-type tkswrast --rcfile "$MGED_XMIN_RC" \
    --geom "$COMMAND_GEOMETRY" --ggeom "$GRAPHICS_GEOMETRY" "$test_db" \
    >"$MGED_XMIN_TEST_DIR/mged.log" 2>&1 &
MGED_PID=$!

if ! wait_for_file "$MGED_XMIN_TEST_DIR/main_window"; then
    fail "MGED main window did not become ready"
fi

main_window=$(sed -n '1p' "$MGED_XMIN_TEST_DIR/main_window")
"$XMIN_CTL" activate "$main_window"

if ! wait_for_file "$MGED_XMIN_TEST_DIR/main_edit_menu" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/main_edit_menu_window"; then
    fail "MGED Edit menu did not become ready"
fi
click_target main_edit_menu

if ! wait_for_file "$MGED_XMIN_TEST_DIR/primitive_editor_menu" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/primitive_editor_menu_window"; then
    fail "MGED Primitive Editor menu entry did not become ready"
fi
click_target primitive_editor_menu

if ! wait_for_file "$MGED_XMIN_TEST_DIR/editor_window"; then
    fail "Primitive Editor did not open"
fi

editor_window=$(sed -n '1p' "$MGED_XMIN_TEST_DIR/editor_window")
"$XMIN_CTL" activate "$editor_window"

if ! wait_for_file "$MGED_XMIN_TEST_DIR/editor_name" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/editor_name_window" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/editor_name_initial" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/editor_reset" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/editor_reset_window"; then
    fail "Primitive Editor controls did not become ready"
fi
editor_name=$(sed -n '1p' "$MGED_XMIN_TEST_DIR/editor_name_initial")
if ! replace_entry_text editor_name "$editor_name" \
    "$MGED_XMIN_TEST_DIR/editor_name_current" gui.s; then
    fail "Primitive Editor name entry did not receive gui.s"
fi
click_target editor_reset
"$CMAKE_COMMAND" -E touch "$MGED_XMIN_TEST_DIR/editor_loaded"

if ! wait_for_file "$MGED_XMIN_TEST_DIR/sphere_vx" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/sphere_vx_window" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/sphere_vx_initial" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/apply_button_window" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/apply_button"; then
    fail "Primitive Editor did not load gui.s"
fi

initial_value=$(sed -n '1p' "$MGED_XMIN_TEST_DIR/sphere_vx_initial")
if [ "$initial_value" != "0.0000" ]; then
    fail "unexpected initial sphere V-x value '$initial_value'"
fi
if ! replace_entry_text sphere_vx "$initial_value" \
    "$MGED_XMIN_TEST_DIR/sphere_vx_current" 5; then
    fail "sphere V-x entry did not receive the expected value"
fi
click_target apply_button

if ! wait_for_file "$MGED_XMIN_TEST_DIR/primitive_result"; then
    fail "MGED did not report a primitive edit result"
fi
primitive_result=$(sed -n '1p' "$MGED_XMIN_TEST_DIR/primitive_result")

"$CMAKE_COMMAND" -E touch "$MGED_XMIN_TEST_DIR/begin_matrix"
if ! wait_for_file "$MGED_XMIN_TEST_DIR/matrix_dm_window" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/matrix_dm"; then
    fail "MGED did not enter matrix edit mode"
fi
click_target matrix_dm 2
"$CMAKE_COMMAND" -E touch "$MGED_XMIN_TEST_DIR/matrix_clicked"

if ! wait_for_file "$MGED_XMIN_TEST_DIR/result"; then
    fail "MGED did not report a matrix edit result"
fi

result=$(sed -n '1p' "$MGED_XMIN_TEST_DIR/result")
case "$result" in
    PASS:*) ;;
    *)
	fail "MGED reported: $result"
	;;
esac

wait "$MGED_PID"
MGED_PID=""
echo "$primitive_result"
echo "$result"
