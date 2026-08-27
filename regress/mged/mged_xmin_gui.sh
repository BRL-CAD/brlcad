#!/bin/sh

set -eu

WAIT_LIMIT=200
WAIT_DELAY=0.05
DATABASE_QUERY_LIMIT=100
PRIMITIVE_LOAD_RETRY_LIMIT=100
CLICK_DELAY=20
STABLE_QUIET=100
STABLE_TIMEOUT=3000
APNG_FPS=1
RAYTRACE_SIZE=128
MGED_RAYTRACE_SIZE=$RAYTRACE_SIZE
export MGED_RAYTRACE_SIZE
MIN_RENDER_CHANGED_CHANNELS=100
MIN_REFERENCE_EXACT_PERCENT=90
MAX_RAYTRACE_ELAPSED_MS=5000
RGB_CHANNELS=3
SCREEN_GEOMETRY=1280x960x24
SCREEN_WIDTH=${SCREEN_GEOMETRY%%x*}
screen_height_depth=${SCREEN_GEOMETRY#*x}
SCREEN_HEIGHT=${screen_height_depth%%x*}
COMMAND_GEOMETRY=680x520+0+0
GRAPHICS_GEOMETRY=512x512+700+0
TOP_LEVEL_MENUS="file:File edit:Edit create:Create view:View viewring:ViewRing settings:Settings modes:Modes misc:Misc tools:Tools help:Help"
MGED_DM_TYPES=${MGED_DM_TYPES:-tkswrast}
MGED_GUI_APNG_DIR=${MGED_GUI_APNG_DIR:-.}
MGED_PIPE_ORACLE=${MGED_PIPE_ORACLE:-${MGED_XMIN_RC%/*}/xmin_gui_pipe_oracle.tcl}

require_executable()
{
    if [ ! -x "$1" ]; then
	echo "FAIL: required executable is unavailable: $1" >&2
	exit 1
    fi
}

wait_for_file()
{
    wait_for_file_with_limit "$1" "$WAIT_LIMIT"
}

wait_for_file_with_limit()
{
    path=$1
    limit=$2
    count=0
    while [ "$count" -lt "$limit" ]; do
	if [ -s "$path" ]; then
	    return 0
	fi
	if [ -s "$MGED_XMIN_TEST_DIR/result" ]; then
	    return 1
	fi
	if [ -n "${MGED_PID:-}" ] && ! kill -0 "$MGED_PID" 2>/dev/null; then
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

wait_for_not_value()
{
    count=0
    while [ "$count" -lt "$WAIT_LIMIT" ]; do
	if [ -s "$1" ] && [ "$(sed -n '1p' "$1")" != "$2" ]; then
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

query_database()
{
    database=$1
    query=$2
    error_file="$MGED_XMIN_TEST_DIR/database_query_error"
    count=0
    while [ "$count" -lt "$DATABASE_QUERY_LIMIT" ]; do
	if DATABASE_VALUE=$("$MGED_BIN" -c "$database" "$query" 2>"$error_file"); then
	    if [ -z "$DATABASE_VALUE" ] && [ -s "$error_file" ]; then
		DATABASE_VALUE=$(sed -n '1p' "$error_file")
	    fi
	    if [ -n "$DATABASE_VALUE" ]; then
		return 0
	    fi
	fi
	sleep "$WAIT_DELAY"
	count=$((count + 1))
    done
    DATABASE_VALUE=
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

activate_menu_target()
{
    read_target "$1"
    "$XMIN_CTL" mouse-move "$TARGET_WINDOW" "$POINT_X" "$POINT_Y"
    "$XMIN_CTL" wait-stable --quiet "$STABLE_QUIET" \
	--timeout "$STABLE_TIMEOUT" root >/dev/null 2>&1 || true
    "$XMIN_CTL" button 1 down
    "$XMIN_CTL" button 1 up
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
    capture_frame "failure" >/dev/null 2>&1 || true
}

capture_frame()
{
    frames_dir="$MGED_XMIN_RUN_DIR/frames"
    counter_file="$MGED_XMIN_RUN_DIR/frame_counter"
    "$CMAKE_COMMAND" -E make_directory "$frames_dir"

    frame_number=0
    if [ -s "$counter_file" ]; then
	frame_number=$(sed -n '1p' "$counter_file")
    fi
    frame_number=$((frame_number + 1))
    frame_name=$(printf '%06d.ppm' "$frame_number")

    "$XMIN_CTL" wait-stable --quiet "$STABLE_QUIET" \
	--timeout "$STABLE_TIMEOUT" root >/dev/null 2>&1 || true
    if ! "$XMIN_CTL" capture-root "$frames_dir/$frame_name" >/dev/null 2>&1; then
	return 1
    fi
    printf '%s\n' "$frame_number" > "$counter_file"
    printf '%06d %s %s\n' "$frame_number" "$MGED_DM_TYPE" "$1" \
	>> "$MGED_XMIN_RUN_DIR/frames.log"
}

record_state()
{
    if ! capture_frame "$1"; then
	fail "could not record GUI state '$1'"
    fi
}

capture_dm_snapshot()
{
    snapshot="$MGED_XMIN_TEST_DIR/$1.ppm"
    root_snapshot="$MGED_XMIN_TEST_DIR/$1-root.ppm"
    root_pixels="$MGED_XMIN_TEST_DIR/$1-root.pix"
    cropped_pixels="$MGED_XMIN_TEST_DIR/$1-cropped.pix"
    read -r dm_x dm_y dm_width dm_height \
	< "$MGED_XMIN_TEST_DIR/active_dm_geometry"
    lower_y=$((SCREEN_HEIGHT - dm_y - dm_height))
    upper_y=$((lower_y + dm_height - 1))
    right_x=$((dm_x + dm_width - 1))

    "$XMIN_CTL" wait-stable --quiet "$STABLE_QUIET" \
	--timeout "$STABLE_TIMEOUT" root >/dev/null 2>&1 || true
    if ! "$XMIN_CTL" capture-root "$root_snapshot" >/dev/null 2>&1 ||
       ! "$ICV_BIN" "$root_snapshot" "$root_pixels" >/dev/null 2>&1 ||
       ! "$PIXCROP_BIN" "$root_pixels" "$cropped_pixels" \
	"$SCREEN_WIDTH" "$dm_width" "$dm_height" \
	"$dm_x" "$upper_y" "$right_x" "$upper_y" \
	"$right_x" "$lower_y" "$dm_x" "$lower_y" >/dev/null 2>&1 ||
       ! "$ICV_BIN" -w "$dm_width" -n "$dm_height" \
	"$cropped_pixels" "$snapshot" >/dev/null 2>&1; then
	fail "could not capture display-manager snapshot '$1'"
    fi
    "$CMAKE_COMMAND" -E rm -f "$root_snapshot" "$root_pixels" "$cropped_pixels"
}

assert_raw_images_equal()
{
    description=$1
    first=$2
    second=$3
    expected_channels=$((RAYTRACE_SIZE * RAYTRACE_SIZE * RGB_CHANNELS))
    first_bytes=$(wc -c < "$first" | tr -d ' ')
    second_bytes=$(wc -c < "$second" | tr -d ' ')
    if [ "$first_bytes" -ne "$expected_channels" ] ||
       [ "$second_bytes" -ne "$expected_channels" ]; then
	fail "$description produced $first_bytes and $second_bytes bytes; expected $expected_channels each"
    fi
    if ! cmp -s "$first" "$second"; then
	set +e
	diff_report=$("$ICV_BIN" diff --format-img1 pix --format-img2 pix \
	    --width-img1 "$RAYTRACE_SIZE" --height-img1 "$RAYTRACE_SIZE" \
	    --width-img2 "$RAYTRACE_SIZE" --height-img2 "$RAYTRACE_SIZE" \
	    "$first" "$second" 2>&1 >/dev/null)
	diff_status=$?
	set -e
	matching=$(printf '%s\n' "$diff_report" | \
	    sed -n 's/.*channels: \([0-9][0-9]*\) matching,.*/\1/p')
	off_many=$(printf '%s\n' "$diff_report" | \
	    sed -n 's/.* \([0-9][0-9]*\) off by many.*/\1/p')
	if [ "$diff_status" -ne 1 ] || [ -z "$matching" ] ||
	   [ -z "$off_many" ] || [ "$off_many" -ne 0 ] ||
	   [ $((matching * 100)) -lt \
	       $((expected_channels * MIN_REFERENCE_EXACT_PERCENT)) ]; then
	    fail "$description differed from the standalone rt reference: $diff_report"
	fi
    fi
}

assert_images_differ()
{
    description=$1
    first=$2
    second=$3
    set +e
    diff_report=$("$ICV_BIN" diff "$first" "$second" 2>&1 >/dev/null)
    diff_status=$?
    set -e
    if [ "$diff_status" -eq 0 ]; then
	fail "$description did not visibly change the display-manager image"
    fi
    off_one=$(printf '%s\n' "$diff_report" | \
	sed -n 's/.* \([0-9][0-9]*\) off by 1,.*/\1/p')
    off_many=$(printf '%s\n' "$diff_report" | \
	sed -n 's/.* \([0-9][0-9]*\) off by many.*/\1/p')
    if [ "$diff_status" -ne 1 ] || [ -z "$off_one" ] || [ -z "$off_many" ]; then
	fail "could not compare images for $description: $diff_report"
    fi
    changed_channels=$((off_one + off_many))
    if [ "$changed_channels" -lt "$MIN_RENDER_CHANGED_CHANNELS" ]; then
	fail "$description changed only $changed_channels image channels"
    fi
}

assert_raytrace_duration()
{
    request=$1
    description=$2
    elapsed_file="$MGED_XMIN_TEST_DIR/raytrace_elapsed_${request}_ms"
    if ! wait_for_file "$elapsed_file"; then
	fail "$description did not report its elapsed time"
    fi
    elapsed_ms=$(sed -n '1p' "$elapsed_file")
    case "$elapsed_ms" in
	''|*[!0-9]*) fail "$description reported an invalid elapsed time: $elapsed_ms" ;;
    esac
    if [ "$elapsed_ms" -gt "$MAX_RAYTRACE_ELAPSED_MS" ]; then
	fail "$description took ${elapsed_ms}ms; expected no more than ${MAX_RAYTRACE_ELAPSED_MS}ms"
    fi
    echo "PASS: $description completed in ${elapsed_ms}ms"
}

toggle_renderer_setting()
{
    setting=$1
    initial=$(sed -n '1p' "$MGED_XMIN_TEST_DIR/renderer_${setting}_initial")
    mutable=$(sed -n '1p' "$MGED_XMIN_TEST_DIR/renderer_${setting}_mutable")
    if [ "$initial" -eq 0 ]; then
	expected=1
    else
	expected=0
    fi

    click_target main_misc_menu
    if ! wait_for_file "$MGED_XMIN_TEST_DIR/renderer_${setting}" ||
       ! wait_for_file "$MGED_XMIN_TEST_DIR/renderer_${setting}_window"; then
	fail "Misc menu did not expose $setting"
    fi
    record_state "Misc menu $setting"
    activate_menu_target "renderer_${setting}"
    if ! wait_for_value "$MGED_XMIN_TEST_DIR/renderer_${setting}_gui_current" "$expected"; then
	fail "$setting menu state did not change to $expected"
    fi
    backend_expected=$expected
    state_description="enabled state $expected"
    if [ "$mutable" -eq 0 ]; then
	backend_expected=$initial
	state_description="requested state $expected (backend unavailable)"
    fi
    if ! wait_for_value "$MGED_XMIN_TEST_DIR/renderer_${setting}_current" \
	"$backend_expected"; then
	fail "$setting backend state did not become $backend_expected"
    fi
    record_state "$setting $state_description"

    click_target main_misc_menu
    record_state "Misc menu $setting restore"
    activate_menu_target "renderer_${setting}"
    if ! wait_for_value "$MGED_XMIN_TEST_DIR/renderer_${setting}_gui_current" "$initial"; then
	fail "$setting menu state did not return to $initial"
    fi
    if ! wait_for_value "$MGED_XMIN_TEST_DIR/renderer_${setting}_current" "$initial"; then
	fail "$setting did not return to $initial"
    fi
}

create_recording()
{
    frames_dir="$MGED_XMIN_RUN_DIR/frames"
    set -- "$frames_dir"/*.ppm
    if [ ! -f "$1" ]; then
	echo "FAIL: MGED GUI run did not produce recording frames" >&2
	return 1
    fi

    "$CMAKE_COMMAND" -E make_directory "$MGED_GUI_APNG_DIR"
    datestamp=$(date -u +%Y%m%dT%H%M%SZ)
    apng="$MGED_GUI_APNG_DIR/${datestamp}_MGED_GUI_test_run.apng"
    if ! "$ICV_BIN" anim add "$apng" "$@" >/dev/null 2>&1 ||
       ! "$ICV_BIN" anim set-fps "$apng" "$APNG_FPS" >/dev/null 2>&1; then
	echo "FAIL: could not encode MGED GUI recording $apng" >&2
	return 1
    fi

    validation_dir="$MGED_XMIN_RUN_DIR/apng_validation"
    "$CMAKE_COMMAND" -E make_directory "$validation_dir"
    if ! "$ICV_BIN" anim extract "$apng" "$validation_dir/frame_" \
	    >/dev/null 2>&1; then
	echo "FAIL: could not reopen MGED GUI recording $apng" >&2
	return 1
    fi
    expected_frames=$(sed -n '1p' "$MGED_XMIN_RUN_DIR/frame_counter")
    actual_frames=$(find "$validation_dir" -type f -name '*.png' | wc -l | tr -d ' ')
    if [ "$actual_frames" -ne "$expected_frames" ]; then
	echo "FAIL: APNG has $actual_frames frames; expected $expected_frames" >&2
	return 1
    fi

    "$CMAKE_COMMAND" -E remove_directory "$validation_dir"
    "$CMAKE_COMMAND" -E remove_directory "$frames_dir"
    echo "MGED GUI recording ($expected_frames frames at ${APNG_FPS} fps): $apng"
}

fail()
{
    capture_failure
    echo "FAIL: $1" >&2
    if [ -n "${MGED_PID:-}" ] && ! kill -0 "$MGED_PID" 2>/dev/null; then
	set +e
	wait "$MGED_PID"
	mged_status=$?
	set -e
	MGED_PID=""
	echo "MGED exited with status $mged_status" >&2
    fi
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
    require_executable "$ICV_BIN"
    require_executable "$PIXCROP_BIN"
    if [ ! -f "$MGED_XMIN_RC" ]; then
	echo "FAIL: MGED Xmin rc file is unavailable: $MGED_XMIN_RC" >&2
	exit 1
    fi
    if [ ! -f "$MGED_PIPE_ORACLE" ]; then
	echo "FAIL: MGED pipe oracle is unavailable: $MGED_PIPE_ORACLE" >&2
	exit 1
    fi

    MGED_XMIN_RUN_DIR=$(mktemp -d /tmp/mged-xmin-gui.XXXXXX)
    export MGED_XMIN_RUN_DIR
    run_status=0
    cleanup()
    {
	if [ -n "${MGED_PID:-}" ]; then
	    kill "$MGED_PID" >/dev/null 2>&1 || true
	fi
	if [ "${MGED_XMIN_KEEP:-0}" -eq 1 ]; then
	    echo "MGED Xmin test artifacts: $MGED_XMIN_RUN_DIR" >&2
	else
	    "$CMAKE_COMMAND" -E remove_directory "$MGED_XMIN_RUN_DIR"
	fi
    }
    trap cleanup EXIT INT TERM

    MGED_XMIN_INNER=1
    export MGED_XMIN_INNER
    for MGED_DM_TYPE in $MGED_DM_TYPES; do
	export MGED_DM_TYPE
	MGED_XMIN_TEST_DIR="$MGED_XMIN_RUN_DIR/cases/$MGED_DM_TYPE"
	export MGED_XMIN_TEST_DIR
	"$CMAKE_COMMAND" -E make_directory "$MGED_XMIN_TEST_DIR"
	test_db="$MGED_XMIN_TEST_DIR/gui-edit.g"
	"$MGED_BIN" -c "$test_db" "in gui.s sph 0 0 0 10" \
	    >"$MGED_XMIN_TEST_DIR/create.log" 2>&1
	set +e
	"$XMIN_RUN" --server "$XMIN_SERVER" --screen "$SCREEN_GEOMETRY" -- "$0"
	dm_status=$?
	set -e
	if [ "$dm_status" -ne 0 ]; then
	    run_status=$dm_status
	    break
	fi
	if query_database "$test_db" "get gui.s V"; then
	    sphere_vertex=$DATABASE_VALUE
	else
	    sphere_vertex=
	fi
	if [ "$sphere_vertex" != "5 0 0" ]; then
	    echo "FAIL: $MGED_DM_TYPE primitive edit stored V {$sphere_vertex}, expected {5 0 0}" >&2
	    sed -n '1,20p' "$MGED_XMIN_TEST_DIR/database_query_error" >&2
	    run_status=1
	    break
	fi
	if ! query_database "$test_db" "source {$MGED_PIPE_ORACLE}" ||
	   [ "$DATABASE_VALUE" != \
	   "PASS: pipe split stored the expected five-point geometry" ]; then
	    query_database "$test_db" "get gui.pipe" || true
	    echo "FAIL: $MGED_DM_TYPE pipe split stored unexpected geometry: $DATABASE_VALUE" >&2
	    run_status=1
	    break
	fi
	read -r matrix_x matrix_y < "$MGED_XMIN_TEST_DIR/matrix_expected"
	if query_database "$test_db" "get matrix.c tree"; then
	    matrix_tree=$DATABASE_VALUE
	else
	    matrix_tree=
	fi
	expected_matrix="l matrix.s {1 0 0 $matrix_x  0 1 0 $matrix_y  0 0 1 0  0 0 0 1}"
	if [ "$matrix_tree" != "$expected_matrix" ]; then
	    echo "FAIL: $MGED_DM_TYPE matrix edit stored '$matrix_tree', expected '$expected_matrix'" >&2
	    run_status=1
	    break
	fi
    done

    set +e
    create_recording
    recording_status=$?
    set -e
    if [ "$run_status" -ne 0 ] || [ "$recording_status" -ne 0 ]; then
	MGED_XMIN_KEEP=1
	exit 1
    fi
    exit 0
fi

test_db="$MGED_XMIN_TEST_DIR/gui-edit.g"
inner_cleanup()
{
    if [ -n "${MGED_PID:-}" ]; then
	kill "$MGED_PID" >/dev/null 2>&1 || true
    fi
}
trap inner_cleanup EXIT INT TERM

"$MGED_BIN" --gui --dm-type "$MGED_DM_TYPE" --rcfile "$MGED_XMIN_RC" \
    --geom "$COMMAND_GEOMETRY" --ggeom "$GRAPHICS_GEOMETRY" "$test_db" \
    >"$MGED_XMIN_TEST_DIR/mged.log" 2>&1 &
MGED_PID=$!

if ! wait_for_file "$MGED_XMIN_TEST_DIR/main_window"; then
    fail "MGED main window did not become ready"
fi

main_window=$(sed -n '1p' "$MGED_XMIN_TEST_DIR/main_window")
"$XMIN_CTL" activate "$main_window"
record_state "MGED ready"

for menu in $TOP_LEVEL_MENUS; do
    menu_name=${menu%%:*}
    menu_label=${menu#*:}
    if ! wait_for_file "$MGED_XMIN_TEST_DIR/main_${menu_name}_menu" ||
       ! wait_for_file "$MGED_XMIN_TEST_DIR/main_${menu_name}_menu_window"; then
	fail "MGED $menu_label menu target did not become ready"
    fi
    click_target "main_${menu_name}_menu"
    record_state "$menu_label menu"
    if [ "$menu_name" = misc ]; then
	if ! wait_for_file "$MGED_XMIN_TEST_DIR/faceplate_toggle" ||
	   ! wait_for_file "$MGED_XMIN_TEST_DIR/faceplate_toggle_window" ||
	   ! wait_for_file "$MGED_XMIN_TEST_DIR/orig_gui_toggle" ||
	   ! wait_for_file "$MGED_XMIN_TEST_DIR/orig_gui_toggle_window"; then
	    fail "Misc menu Faceplate targets did not become ready"
	fi
    fi
    "$XMIN_CTL" key escape
done
"$XMIN_CTL" click --delay "$CLICK_DELAY" "$main_window" 100 100 1
"$XMIN_CTL" activate "$main_window"

if [ "$MGED_DM_TYPE" = "tkswrast" ] && [ -n "${MGED_GUI_MANIFEST:-}" ]; then
    if ! wait_for_file "$MGED_XMIN_TEST_DIR/menu_inventory"; then
	fail "MGED menu inventory did not become ready"
    fi
    if ! "$CMAKE_COMMAND" -E compare_files \
	"$MGED_XMIN_TEST_DIR/menu_inventory" "$MGED_GUI_MANIFEST"; then
	fail "MGED menu inventory differs from $MGED_GUI_MANIFEST"
    fi
fi

for setting in faceplate orig_gui; do
    if ! wait_for_file "$MGED_XMIN_TEST_DIR/${setting}_current"; then
	fail "MGED did not report the current $setting state"
    fi
    if [ "$(sed -n '1p' "$MGED_XMIN_TEST_DIR/${setting}_current")" -ne 1 ]; then
	click_target main_misc_menu
	record_state "Misc menu $setting"
	if ! wait_for_file "$MGED_XMIN_TEST_DIR/${setting}_toggle" ||
	   ! wait_for_file "$MGED_XMIN_TEST_DIR/${setting}_toggle_window"; then
	    fail "Misc menu did not expose $setting"
	fi
	activate_menu_target "${setting}_toggle"
	if ! wait_for_value "$MGED_XMIN_TEST_DIR/${setting}_current" 1; then
	    fail "$setting did not enable the in-scene Faceplate"
	fi
	record_state "$setting enabled"
    fi
done

if ! wait_for_file "$MGED_XMIN_TEST_DIR/faceplate_header" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/faceplate_header_window" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/faceplate_3525" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/faceplate_3525_window" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/faceplate_view_initial"; then
    fail "Faceplate targets did not become ready"
fi
faceplate_view_initial=$(sed -n '1p' "$MGED_XMIN_TEST_DIR/faceplate_view_initial")
click_target faceplate_header 2
if ! wait_for_file "$MGED_XMIN_TEST_DIR/faceplate_general_ready"; then
    fail "Faceplate general menu did not expand"
fi
record_state "Faceplate general menu"
click_target faceplate_3525 2
if ! wait_for_not_value "$MGED_XMIN_TEST_DIR/faceplate_view_current" \
    "$faceplate_view_initial"; then
    fail "Faceplate 35,25 did not change the view"
fi
record_state "Faceplate 35,25 view"
click_target faceplate_header 2

if ! wait_for_file "$MGED_XMIN_TEST_DIR/renderer_capabilities" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/main_misc_menu" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/main_misc_menu_window"; then
    fail "renderer capability inventory did not become ready"
fi
renderer_capabilities=$(sed -n '1p' "$MGED_XMIN_TEST_DIR/renderer_capabilities")
if [ "$renderer_capabilities" != "none" ]; then
    for setting in $renderer_capabilities; do
	if ! wait_for_file "$MGED_XMIN_TEST_DIR/renderer_${setting}_initial"; then
	    fail "renderer did not report initial $setting state"
	fi
	if ! wait_for_file "$MGED_XMIN_TEST_DIR/renderer_${setting}_mutable"; then
	    fail "renderer did not report $setting mutability"
	fi
	toggle_renderer_setting "$setting"
    done
fi

"$CMAKE_COMMAND" -E touch "$MGED_XMIN_TEST_DIR/stop_general_monitors"
if ! wait_for_file "$MGED_XMIN_TEST_DIR/renderer_monitor_stopped" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/faceplate_monitor_stopped"; then
    fail "MGED general-interface monitors did not stop"
fi

if ! wait_for_file "$MGED_XMIN_TEST_DIR/main_edit_menu" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/main_edit_menu_window"; then
    fail "MGED Edit menu did not become ready"
fi
click_target main_edit_menu
record_state "Edit menu"

if ! wait_for_file "$MGED_XMIN_TEST_DIR/primitive_editor_menu" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/primitive_editor_menu_window"; then
    fail "MGED Primitive Editor menu entry did not become ready"
fi
activate_menu_target primitive_editor_menu

if ! wait_for_file "$MGED_XMIN_TEST_DIR/editor_window"; then
    fail "Primitive Editor did not open"
fi
record_state "Primitive Editor"

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
record_state "primitive name entered"
click_target editor_reset
record_state "primitive load dispatched"
"$CMAKE_COMMAND" -E touch "$MGED_XMIN_TEST_DIR/editor_loaded"
if ! wait_for_file_with_limit "$MGED_XMIN_TEST_DIR/sphere_vx" \
    "$PRIMITIVE_LOAD_RETRY_LIMIT"; then
    click_target editor_name
    "$XMIN_CTL" key enter
    record_state "primitive load keyboard retry"
fi

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
record_state "sphere loaded"
if ! replace_entry_text sphere_vx "$initial_value" \
    "$MGED_XMIN_TEST_DIR/sphere_vx_current" 5; then
    fail "sphere V-x entry did not receive the expected value"
fi
record_state "sphere vertex edited"
click_target apply_button
record_state "sphere Apply dispatched"
"$CMAKE_COMMAND" -E touch "$MGED_XMIN_TEST_DIR/primitive_applied"

if ! wait_for_file "$MGED_XMIN_TEST_DIR/primitive_result"; then
    fail "MGED did not report a primitive edit result"
fi
primitive_result=$(sed -n '1p' "$MGED_XMIN_TEST_DIR/primitive_result")
record_state "sphere edit applied"

"$CMAKE_COMMAND" -E touch "$MGED_XMIN_TEST_DIR/begin_pipe"
if ! wait_for_file "$MGED_XMIN_TEST_DIR/pipe_ready" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/pipe_select" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/pipe_select_window" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/pipe_next" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/pipe_next_window" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/pipe_split" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/pipe_split_window" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/pipe_split_point" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/pipe_split_point_window" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/pipe_point" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/pipe_point_window"; then
    fail "pipe Faceplate edit did not become ready"
fi
record_state "pipe contextual Faceplate"
click_target pipe_select 2
record_state "pipe Select Point mode"
click_target pipe_point 2
"$CMAKE_COMMAND" -E touch "$MGED_XMIN_TEST_DIR/pipe_point_clicked"
if ! wait_for_file "$MGED_XMIN_TEST_DIR/pipe_selected"; then
    fail "pipe point was not selected through the viewport"
fi
pipe_keypoint=$(sed -n '1p' "$MGED_XMIN_TEST_DIR/pipe_keypoint_current")
record_state "pipe point selected"
click_target pipe_next 2
if ! wait_for_not_value "$MGED_XMIN_TEST_DIR/pipe_keypoint_current" \
    "$pipe_keypoint"; then
    fail "pipe Next Point did not change the edit keypoint"
fi
record_state "pipe next point selected"
click_target pipe_split 2
record_state "pipe Split Segment mode"
click_target pipe_split_point 2
record_state "pipe segment split"
"$CMAKE_COMMAND" -E touch "$MGED_XMIN_TEST_DIR/pipe_finish"
if ! wait_for_file "$MGED_XMIN_TEST_DIR/pipe_done"; then
    fail "pipe edit did not accept cleanly"
fi

"$CMAKE_COMMAND" -E touch "$MGED_XMIN_TEST_DIR/begin_sketch"
if ! wait_for_file "$MGED_XMIN_TEST_DIR/sketch_ready" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/sketch_create_line" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/sketch_create_line_window" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/sketch_zoom_in" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/sketch_zoom_in_window" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/sketch_reset" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/sketch_reset_window" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/sketch_dismiss" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/sketch_dismiss_window" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/sketch_canvas_start" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/sketch_canvas_start_window" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/sketch_canvas_end" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/sketch_canvas_end_window" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/sketch_scale_current" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/sketch_vertex_count_current"; then
    fail "Sketch Editor controls did not become ready"
fi
record_state "Sketch Editor ready"
sketch_scale=$(sed -n '1p' "$MGED_XMIN_TEST_DIR/sketch_scale_current")
click_target sketch_zoom_in
if ! wait_for_not_value "$MGED_XMIN_TEST_DIR/sketch_scale_current" \
    "$sketch_scale"; then
    fail "Sketch Editor Zoom In did not change the canvas scale"
fi
record_state "Sketch Editor zoomed"
sketch_vertex_count=$(sed -n '1p' \
    "$MGED_XMIN_TEST_DIR/sketch_vertex_count_current")
click_target sketch_create_line
record_state "Sketch Editor Create Line mode"
click_target sketch_canvas_start
record_state "Sketch Editor line start"
click_target sketch_canvas_end
if ! wait_for_not_value "$MGED_XMIN_TEST_DIR/sketch_vertex_count_current" \
    "$sketch_vertex_count"; then
    fail "Sketch Editor did not add vertices for the new line"
fi
record_state "Sketch Editor line created"
click_target sketch_reset
if ! wait_for_value "$MGED_XMIN_TEST_DIR/sketch_vertex_count_current" \
    "$sketch_vertex_count"; then
    fail "Sketch Editor Reset Sketch did not restore the fixture"
fi
record_state "Sketch Editor reset"
click_target sketch_dismiss
if ! wait_for_file "$MGED_XMIN_TEST_DIR/sketch_closed"; then
    fail "Sketch Editor did not dismiss cleanly"
fi

"$CMAKE_COMMAND" -E touch "$MGED_XMIN_TEST_DIR/begin_matrix"
if ! wait_for_file "$MGED_XMIN_TEST_DIR/matrix_dm_window" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/matrix_dm"; then
    fail "MGED did not enter matrix edit mode"
fi
record_state "matrix edit ready"
click_target matrix_dm 2
record_state "matrix edit moved"
"$CMAKE_COMMAND" -E touch "$MGED_XMIN_TEST_DIR/matrix_clicked"

if ! wait_for_file "$MGED_XMIN_TEST_DIR/matrix_result"; then
    fail "MGED did not report a matrix edit result"
fi

matrix_result=$(sed -n '1p' "$MGED_XMIN_TEST_DIR/matrix_result")
case "$matrix_result" in
    PASS:*) ;;
    *)
	fail "MGED reported: $matrix_result"
	;;
esac

record_state "matrix edit accepted"
capture_dm_snapshot raytrace_baseline

click_target main_tools_menu
record_state "Tools menu embedded raytrace"
if ! wait_for_file "$MGED_XMIN_TEST_DIR/raytrace_control_menu" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/raytrace_control_menu_window"; then
    fail "Tools menu did not expose the Raytrace Control Panel"
fi
activate_menu_target raytrace_control_menu
if ! wait_for_file "$MGED_XMIN_TEST_DIR/raytrace_ready" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/raytrace_window" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/raytrace_size" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/raytrace_size_window" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/raytrace_destination" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/raytrace_destination_window" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/raytrace_button" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/raytrace_button_window" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/raytrace_active" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/raytrace_active_window" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/raytrace_dismiss" ||
   ! wait_for_file "$MGED_XMIN_TEST_DIR/raytrace_dismiss_window"; then
    fail "Raytrace Control Panel controls did not become ready"
fi
record_state "Raytrace Control Panel"

raytrace_size=$(sed -n '1p' "$MGED_XMIN_TEST_DIR/raytrace_size_initial")
if ! replace_entry_text raytrace_size "$raytrace_size" \
    "$MGED_XMIN_TEST_DIR/raytrace_size_current" "$RAYTRACE_SIZE"; then
    fail "Raytrace Control Panel size entry did not accept $RAYTRACE_SIZE"
fi
record_state "embedded raytrace size $RAYTRACE_SIZE"
"$CMAKE_COMMAND" -E touch "$MGED_XMIN_TEST_DIR/raytrace_prepare_reference"
if ! wait_for_file "$MGED_XMIN_TEST_DIR/raytrace_reference_ready"; then
    fail "MGED did not prepare the standalone raytrace reference"
fi
reference_script=$(sed -n '1p' "$MGED_XMIN_TEST_DIR/raytrace_reference_script")
reference_image=$(sed -n '1p' "$MGED_XMIN_TEST_DIR/raytrace_reference_image")
if ! /bin/sh "$reference_script"; then
    fail "standalone reference raytrace failed"
fi
if [ ! -s "$reference_image" ]; then
    fail "standalone reference raytrace did not create an image"
fi
record_state "standalone raytrace reference complete"
click_target raytrace_button
printf '%s\n' 1 > "$MGED_XMIN_TEST_DIR/raytrace_request"
record_state "embedded raytrace started"
if ! wait_for_value "$MGED_XMIN_TEST_DIR/raytrace_complete" 1; then
    fail "embedded raytrace did not complete"
fi
assert_raytrace_duration 1 "embedded raytrace"
record_state "embedded raytrace complete"

for mode in overlay interlay underlay; do
    case "$mode" in
	overlay) layer=2 ;;
	interlay) layer=1 ;;
	underlay) layer=0 ;;
    esac
    click_target raytrace_framebuffer_menu
    record_state "Raytrace Framebuffer menu $mode"
    activate_menu_target "raytrace_$mode"
    if ! wait_for_value "$MGED_XMIN_TEST_DIR/raytrace_overlay_current" "$layer"; then
	fail "embedded framebuffer did not enter $mode mode"
    fi
    record_state "embedded framebuffer $mode"
    capture_dm_snapshot "raytrace_$mode"
done

assert_images_differ "embedded raytrace overlay" \
    "$MGED_XMIN_TEST_DIR/raytrace_baseline.ppm" \
    "$MGED_XMIN_TEST_DIR/raytrace_overlay.ppm"

"$CMAKE_COMMAND" -E touch "$MGED_XMIN_TEST_DIR/raytrace_export_requested"
if ! wait_for_file "$MGED_XMIN_TEST_DIR/raytrace_export_ready"; then
    fail "MGED did not export the embedded framebuffer"
fi
embedded_image=$(sed -n '1p' "$MGED_XMIN_TEST_DIR/raytrace_export_image")
if [ ! -s "$embedded_image" ]; then
    fail "MGED exported an empty embedded framebuffer image"
fi
assert_raw_images_equal "embedded framebuffer readback" \
    "$reference_image" "$embedded_image"
record_state "embedded framebuffer matches standalone rt"

if ! wait_for_value "$MGED_XMIN_TEST_DIR/raytrace_fb_current" 1; then
    fail "embedded framebuffer was not active after raytracing"
fi
click_target raytrace_active
if ! wait_for_value "$MGED_XMIN_TEST_DIR/raytrace_fb_current" 0; then
    fail "Raytrace Control Panel did not deactivate the framebuffer"
fi
record_state "embedded framebuffer inactive"
capture_dm_snapshot raytrace_inactive
assert_images_differ "framebuffer active toggle" \
    "$MGED_XMIN_TEST_DIR/raytrace_underlay.ppm" \
    "$MGED_XMIN_TEST_DIR/raytrace_inactive.ppm"
click_target raytrace_active
if ! wait_for_value "$MGED_XMIN_TEST_DIR/raytrace_fb_current" 1; then
    fail "Raytrace Control Panel did not reactivate the framebuffer"
fi
record_state "embedded framebuffer reactivated"

file_destination=$(sed -n '1p' "$MGED_XMIN_TEST_DIR/raytrace_file_destination")
embedded_destination=$(sed -n '1p' "$MGED_XMIN_TEST_DIR/raytrace_destination_initial")
if ! replace_entry_text raytrace_destination "$embedded_destination" \
    "$MGED_XMIN_TEST_DIR/raytrace_destination_current" "$file_destination"; then
    fail "Raytrace Control Panel did not accept a file destination"
fi
record_state "raytrace file destination"
file_render_size=$(sed -n '1p' "$MGED_XMIN_TEST_DIR/raytrace_size_current")
if ! replace_entry_text raytrace_size "$file_render_size" \
    "$MGED_XMIN_TEST_DIR/raytrace_size_current" "$RAYTRACE_SIZE"; then
    fail "Raytrace Control Panel did not restore size $RAYTRACE_SIZE for the file render"
fi
record_state "raytrace file size $RAYTRACE_SIZE"
click_target raytrace_button
printf '%s\n' 2 > "$MGED_XMIN_TEST_DIR/raytrace_request"
record_state "GUI file raytrace started"
if ! wait_for_value "$MGED_XMIN_TEST_DIR/raytrace_complete" 2; then
    fail "GUI file raytrace did not complete"
fi
assert_raytrace_duration 2 "GUI file raytrace"
if [ ! -s "$file_destination" ]; then
    fail "GUI file raytrace did not create an image"
fi
assert_raw_images_equal "GUI file raytrace" "$reference_image" "$file_destination"
record_state "GUI raytrace matches standalone rt"
if ! replace_entry_text raytrace_destination "$file_destination" \
    "$MGED_XMIN_TEST_DIR/raytrace_destination_current" "$embedded_destination"; then
    fail "Raytrace Control Panel did not restore the embedded destination"
fi
record_state "embedded raytrace destination restored"

click_target raytrace_dismiss
if ! wait_for_file "$MGED_XMIN_TEST_DIR/raytrace_closed"; then
    fail "Raytrace Control Panel did not dismiss cleanly"
fi
"$XMIN_CTL" activate "$main_window"
click_target main_view_menu
record_state "post-raytrace View menu responsiveness"
"$XMIN_CTL" key escape
"$CMAKE_COMMAND" -E touch "$MGED_XMIN_TEST_DIR/raytrace_responsive"

if ! wait_for_file "$MGED_XMIN_TEST_DIR/result"; then
    fail "MGED did not remain responsive after embedded raytrace"
fi
result=$(sed -n '1p' "$MGED_XMIN_TEST_DIR/result")
case "$result" in
    PASS:*) ;;
    *)
	fail "MGED reported: $result"
	;;
esac

"$CMAKE_COMMAND" -E touch "$MGED_XMIN_TEST_DIR/finish"

wait "$MGED_PID"
MGED_PID=""
echo "$primitive_result"
echo "$matrix_result"
echo "$result"
