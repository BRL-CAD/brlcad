#!/bin/sh

GUI_WAIT_TIMEOUT_MS=${GUI_WAIT_TIMEOUT_MS:-30000}
GUI_STABLE_QUIET_MS=${GUI_STABLE_QUIET_MS:-200}
GUI_STABLE_TIMEOUT_MS=${GUI_STABLE_TIMEOUT_MS:-5000}
GUI_POLL_SECONDS=${GUI_POLL_SECONDS:-0.05}
GUI_MIN_WINDOW_WIDTH=${GUI_MIN_WINDOW_WIDTH:-200}
GUI_MIN_WINDOW_HEIGHT=${GUI_MIN_WINDOW_HEIGHT:-150}

gui_fail()
{
    echo "FAIL: $1" >&2
    if [ -n "${GUI_APP_LOG:-}" ] && [ -f "$GUI_APP_LOG" ]; then
	sed -n '1,240p' "$GUI_APP_LOG" >&2
    fi
    exit 1
}

gui_require_executable()
{
    if [ ! -x "$1" ]; then
	gui_fail "required executable is unavailable: $1"
    fi
}

gui_wait_for_minimum_geometry()
{
    window=$1
    minimum_width=$2
    minimum_height=$3
    timeout_seconds=$(((GUI_WAIT_TIMEOUT_MS + 999) / 1000))
    start_time=$(date +%s)

    while :; do
	geometry=$("$GUI_TEST_CTL" geometry "$window") || return 1
	set -- $geometry
	if [ "$#" -ge 4 ]; then
	    width=$3
	    height=$4
	    if [ "$width" -ge "$minimum_width" ] &&
	       [ "$height" -ge "$minimum_height" ]; then
		echo "$geometry"
		return 0
	    fi
	fi

	current_time=$(date +%s)
	if [ $((current_time - start_time)) -ge "$timeout_seconds" ]; then
	    echo "$geometry"
	    return 1
	fi
	sleep "$GUI_POLL_SECONDS"
    done
}

gui_capture_screen()
{
    window=$1
    output=$2
    "$GUI_TEST_CTL" wait-stable --quiet "$GUI_STABLE_QUIET_MS" \
	--timeout "$GUI_STABLE_TIMEOUT_MS" "$window" >/dev/null 2>&1 || true
    # Tk and Qt compose child windows into the root; a top-level-only capture
    # can therefore contain just the parent's cleared backing surface.
    "$GUI_TEST_CTL" capture-screen "$output" ||
	gui_fail "could not capture the GUI test screen"
    if [ "$(sed -n '1p' "$output")" != "P6" ]; then
	gui_fail "screen capture is not a binary PPM image: $output"
    fi
    nonzero_bytes=$(LC_ALL=C tr -d '\000' < "$output" | wc -c | tr -d ' ')
    if [ "$nonzero_bytes" -lt 1024 ]; then
	gui_fail "screen capture contains no meaningful rendered content"
    fi
}

gui_count_changed_bytes()
{
    first=$1
    second=$2
    minimum=$3
    # Stop once the threshold is reached.  Full root captures are large, and
    # their exact total difference is immaterial to this binary assertion.
    changed_bytes=$(cmp -l "$first" "$second" | awk -v minimum="$minimum" \
	'NR >= minimum { print NR; exit }
	 END { if (NR < minimum) print NR }')
    echo "$changed_bytes"
}

gui_wait_for_image_difference()
{
    first=$1
    second=$2
    minimum=$3
    window=$4
    timeout_seconds=$(((GUI_WAIT_TIMEOUT_MS + 999) / 1000))
    start_time=$(date +%s)

    if [ ! -s "$first" ]; then
	gui_fail "cannot compare a missing or empty GUI capture: $first"
    fi

    while :; do
	gui_capture_screen "$window" "$second"
	changed_bytes=$(gui_count_changed_bytes "$first" "$second" "$minimum")
	if [ "$changed_bytes" -ge "$minimum" ]; then
	    return 0
	fi

	current_time=$(date +%s)
	if [ $((current_time - start_time)) -ge "$timeout_seconds" ]; then
	    gui_fail "GUI captures changed by only $changed_bytes bytes; expected at least $minimum"
	fi
	sleep "$GUI_POLL_SECONDS"
    done
}

# Capture a root-coordinate region of the screen as PPM.  The conversion
# commands preserve the image comparison format used by the MGED fixture.
gui_capture_region()
(
    output=$1
    x=$2
    y=$3
    width=$4
    height=$5
    root_capture=${output%.ppm}-root.ppm
    root_pixels=${output%.ppm}-root.pix
    cropped_pixels=${output%.ppm}-cropped.pix
    geometry=$("$GUI_TEST_CTL" screen-geometry) || exit 1
    set -- $geometry
    screen_width=$1
    screen_height=$2
    lower_y=$((screen_height - y - height))
    upper_y=$((lower_y + height - 1))
    right_x=$((x + width - 1))

    "$GUI_TEST_CTL" wait-stable --quiet "$GUI_STABLE_QUIET_MS" \
        --timeout "$GUI_STABLE_TIMEOUT_MS" root >/dev/null 2>&1 || true
    "$GUI_TEST_CTL" capture-screen "$root_capture" || exit 1
    "$ICV_BIN" "$root_capture" "$root_pixels" >/dev/null 2>&1 || exit 1
    "$PIXCROP_BIN" "$root_pixels" "$cropped_pixels" \
        "$screen_width" "$width" "$height" \
        "$x" "$upper_y" "$right_x" "$upper_y" \
        "$right_x" "$lower_y" "$x" "$lower_y" >/dev/null 2>&1 || exit 1
    "$ICV_BIN" -w "$width" -n "$height" \
        "$cropped_pixels" "$output" >/dev/null 2>&1 || exit 1
    rm -f "$root_capture" "$root_pixels" "$cropped_pixels"
)

# Local Variables:
# tab-width: 8
# mode: sh
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8 cino=N-s
