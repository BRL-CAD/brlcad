#!/bin/sh

XMIN_WAIT_TIMEOUT_MS=${XMIN_WAIT_TIMEOUT_MS:-30000}
XMIN_STABLE_QUIET_MS=${XMIN_STABLE_QUIET_MS:-200}
XMIN_STABLE_TIMEOUT_MS=${XMIN_STABLE_TIMEOUT_MS:-5000}
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

xmin_assert_images_differ()
{
    first=$1
    second=$2
    minimum=${3:-1}
    if [ ! -s "$first" ] || [ ! -s "$second" ]; then
	xmin_fail "cannot compare missing or empty Xmin captures"
    fi
    # Stop once the threshold is reached.  Full root captures are large, and
    # their exact total difference is immaterial to this binary assertion.
    changed_bytes=$(cmp -l "$first" "$second" | awk -v minimum="$minimum" \
	'NR >= minimum { print NR; exit }
	 END { if (NR < minimum) print NR }')
    if [ "$changed_bytes" -lt "$minimum" ]; then
	xmin_fail "Xmin captures changed by only $changed_bytes bytes; expected at least $minimum"
    fi
}
