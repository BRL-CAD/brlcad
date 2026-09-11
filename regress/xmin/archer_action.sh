#!/bin/sh

set -eu

: "${XMIN_TEST_SHELL:?XMIN_TEST_SHELL is required}"
. "$XMIN_TEST_SHELL"

window=$1
geometry=$("$XMIN_CTL" geometry "$window") ||
    xmin_fail "could not query Archer window geometry"
set -- $geometry
width=$3
height=$4

# Archer's command entry spans the bottom of its main window.  Address it
# relative to the live geometry so the action is independent of screen size.
command_x=$((width / 4))
command_y=$((height - 27))
"$XMIN_CTL" click "$window" "$command_x" "$command_y" 1
"$XMIN_CTL" type "draw all.g"
"$XMIN_CTL" key enter
ARCHER_DRAW_SETTLE_SECONDS=${ARCHER_DRAW_SETTLE_SECONDS:-1}
sleep "$ARCHER_DRAW_SETTLE_SECONDS"
