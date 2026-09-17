#!/bin/sh

set -eu

: "${GUI_TEST_SHELL:?GUI_TEST_SHELL is required}"
. "$GUI_TEST_SHELL"

window=$1
geometry=$("$GUI_TEST_CTL" geometry "$window") ||
    gui_fail "could not query qged window geometry"
set -- $geometry
width=$3
height=$4

# The console occupies the bottom of qged's central column.  Focus it away
# from the edge shared with the surrounding docks, then issue one GED command
# that changes the OpenGL view without depending on evolving qged controls.
console_x=$((width / 2))
console_y=$((height * 94 / 100))
"$GUI_TEST_CTL" click "$window" "$console_x" "$console_y" 1
"$GUI_TEST_CTL" type "draw all.g"
"$GUI_TEST_CTL" key enter
QGED_DRAW_SETTLE_SECONDS=${QGED_DRAW_SETTLE_SECONDS:-2}
sleep "$QGED_DRAW_SETTLE_SECONDS"

# Local Variables:
# tab-width: 8
# mode: sh
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8 cino=N-s
