#!/bin/sh

set -eu

: "${XMIN_TEST_SHELL:?XMIN_TEST_SHELL is required}"
. "$XMIN_TEST_SHELL"

window=$1
geometry=$("$XMIN_CTL" geometry "$window") ||
    xmin_fail "could not query rtwizard window geometry"
set -- $geometry
width=$3
height=$4

# The image-type selector is centered in the upper-left quarter.  Choosing the
# fourth row changes picture type and forces rtwizard to rebuild the page.
selector_x=$((width * 28 / 100))
selector_y=$((height * 30 / 100))
choice_x=$((width * 15 / 100))
choice_y=$((height * 39 / 100))
"$XMIN_CTL" click "$window" "$selector_x" "$selector_y" 1
"$XMIN_CTL" click "$window" "$choice_x" "$choice_y" 1
RTWIZARD_PAGE_SETTLE_SECONDS=${RTWIZARD_PAGE_SETTLE_SECONDS:-1}
sleep "$RTWIZARD_PAGE_SETTLE_SECONDS"
