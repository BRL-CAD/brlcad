#!/bin/sh

XMIN_WAIT_TIMEOUT_MS=${XMIN_WAIT_TIMEOUT_MS:-30000}
XMIN_STABLE_QUIET_MS=${XMIN_STABLE_QUIET_MS:-200}
XMIN_STABLE_TIMEOUT_MS=${XMIN_STABLE_TIMEOUT_MS:-5000}
XMIN_POLL_SECONDS=${XMIN_POLL_SECONDS:-0.05}
XMIN_MIN_WINDOW_WIDTH=${XMIN_MIN_WINDOW_WIDTH:-200}
XMIN_MIN_WINDOW_HEIGHT=${XMIN_MIN_WINDOW_HEIGHT:-150}

xmin_fail()
{
    echo "FAIL: $1" >&2
    if [ -n "${XMIN_APP_LOG:-}" ] && [ -f "$XMIN_APP_LOG" ]; then
	sed -n '1,240p' "$XMIN_APP_LOG" >&2
    fi
    exit 1
}

xmin_require_executable()
{
    if [ ! -x "$1" ]; then
	xmin_fail "required executable is unavailable: $1"
    fi
}

xmin_wait_for_minimum_geometry()
{
    window=$1
    minimum_width=$2
    minimum_height=$3
    timeout_seconds=$(((XMIN_WAIT_TIMEOUT_MS + 999) / 1000))
    start_time=$(date +%s)

    while :; do
	geometry=$("$XMIN_CTL" geometry "$window") || return 1
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
	sleep "$XMIN_POLL_SECONDS"
    done
}

xmin_capture_root()
{
    window=$1
    output=$2
    "$XMIN_CTL" wait-stable --quiet "$XMIN_STABLE_QUIET_MS" \
	--timeout "$XMIN_STABLE_TIMEOUT_MS" "$window" >/dev/null 2>&1 || true
    # Tk and Qt compose child windows into the root; a top-level-only capture
    # can therefore contain just the parent's cleared backing surface.
    "$XMIN_CTL" capture-root "$output" ||
	xmin_fail "could not capture the Xmin root window"
    if [ "$(sed -n '1p' "$output")" != "P6" ]; then
	xmin_fail "window capture is not a binary PPM image: $output"
    fi
    nonzero_bytes=$(LC_ALL=C tr -d '\000' < "$output" | wc -c | tr -d ' ')
    if [ "$nonzero_bytes" -lt 1024 ]; then
	xmin_fail "window capture contains no meaningful rendered content"
    fi
}

xmin_count_changed_bytes()
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

xmin_wait_for_image_difference()
{
    first=$1
    second=$2
    minimum=$3
    window=$4
    timeout_seconds=$(((XMIN_WAIT_TIMEOUT_MS + 999) / 1000))
    start_time=$(date +%s)

    if [ ! -s "$first" ]; then
	xmin_fail "cannot compare a missing or empty Xmin capture: $first"
    fi

    while :; do
	xmin_capture_root "$window" "$second"
	changed_bytes=$(xmin_count_changed_bytes "$first" "$second" "$minimum")
	if [ "$changed_bytes" -ge "$minimum" ]; then
	    return 0
	fi

	current_time=$(date +%s)
	if [ $((current_time - start_time)) -ge "$timeout_seconds" ]; then
	    xmin_fail "Xmin captures changed by only $changed_bytes bytes; expected at least $minimum"
	fi
	sleep "$XMIN_POLL_SECONDS"
    done
}

# Local Variables:
# tab-width: 8
# mode: sh
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8 cino=N-s
