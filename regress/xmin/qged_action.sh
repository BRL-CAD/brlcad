#!/bin/sh

set -eu

: "${XMIN_TEST_SHELL:?XMIN_TEST_SHELL is required}"
. "$XMIN_TEST_SHELL"

window=$1
geometry=$("$XMIN_CTL" geometry "$window") ||
    xmin_fail "could not query qged window geometry"
set -- $geometry
width=$3
height=$4

# The console occupies the bottom of qged's central column.  Focus it away
# from the edge shared with the surrounding docks, then issue one GED command
# that changes the OpenGL view without depending on evolving qged controls.
console_x=$((width / 2))
console_y=$((height * 94 / 100))
"$XMIN_CTL" click "$window" "$console_x" "$console_y" 1
"$XMIN_CTL" type "draw all.g"
"$XMIN_CTL" key enter
QGED_DRAW_SETTLE_SECONDS=${QGED_DRAW_SETTLE_SECONDS:-2}
sleep "$QGED_DRAW_SETTLE_SECONDS"
