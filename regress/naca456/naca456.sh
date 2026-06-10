#!/bin/sh
#                       N A C A 4 5 6 . S H
# BRL-CAD
#
# Copyright (c) 2026 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following
# disclaimer in the documentation and/or other materials provided
# with the distribution.
#
# 3. The name of the author may not be used to endorse or promote
# products derived from this software without specific prior written
# permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

. "$1/regress/library.sh"

STATUS=0
if test "x$LOGFILE" = "x" ; then
    LOGFILE="`pwd`/naca456.log"
    rm -f "$LOGFILE"
fi

fail ( ) {
    log "ERROR: $*"
    if test -f "$LOGFILE" ; then
	cat "$LOGFILE"
    fi
    exit 1
}

expect_grep ( ) {
    pattern="$1"
    file="$2"
    grep "$pattern" "$file" > /dev/null 2>&1 || fail "expected '$pattern' in $file"
}

compare_sections ( ) {
    ref="$1"
    got="$2"
    tol="$3"

    awk -v tol="$tol" '
    function abs(x) { return x < 0 ? -x : x }
    /^#/ { next }
    NR == FNR {
	n++
	case_name[n] = $1
	for (i = 2; i <= 5; i++)
	    refv[n, i] = $i
	next
    }
    /^#/ { next }
    {
	m++
	if (m > n) {
	    printf("extra generated row %d: %s\n", m, $0)
	    bad = 1
	    exit
	}
	if ($1 != case_name[m]) {
	    printf("case mismatch row %d: expected %s got %s\n", m, case_name[m], $1)
	    bad = 1
	    exit
	}
	for (i = 2; i <= 5; i++) {
	    if (abs($i - refv[m, i]) > tol) {
		printf("numeric mismatch row %d col %d for %s: expected %.6f got %.6f\n", m, i, $1, refv[m, i], $i)
		bad = 1
		exit
	    }
	}
    }
    END {
	if (bad)
	    exit 1
	if (m != n) {
	    printf("row count mismatch: expected %d got %d\n", n, m)
	    exit 1
	}
    }' "$ref" "$got" >> "$LOGFILE" 2>&1 || fail "generated section coordinates differ from PDAS NACA456 references"
}

run_brep_matrix_case ( ) {
    matrix_db="$1"
    matrix_surface="$2"
    matrix_write="$3"
    matrix_name="$4"
    shift 4

    if test "x$matrix_write" = "xforce" ; then
	matrix_db_mode="--force"
    else
	matrix_db_mode="--append"
    fi

    run "$NACA456" --mode brep --brep-surface "$matrix_surface" "$matrix_db_mode" --output "$matrix_db" --name "$matrix_name" "$@" || fail "BREP matrix case $matrix_name ($matrix_surface) failed to generate"
    run "$MGED" -c "$matrix_db" "l $matrix_name.brep" || fail "BREP matrix case $matrix_name ($matrix_surface) failed MGED listing"
    expect_grep "Boundary Representation" "$LOGFILE"
    run "$MGED" -c "$matrix_db" "attr get $matrix_name.r naca456::brep_surface" || fail "BREP matrix case $matrix_name ($matrix_surface) missing surface metadata"
    expect_grep "$matrix_surface" "$LOGFILE"
}

run_brep_matrix_surface ( ) {
    matrix_surface="$1"
    matrix_db="naca456-brep-matrix-$matrix_surface.g"

    log "=== BREP robustness matrix ($matrix_surface) ==="

    run_brep_matrix_case "$matrix_db" "$matrix_surface" force "brep_${matrix_surface}_sharp_te_0012" --airfoil 0012 --sharp-te --semi-span 540 --root-chord 180 --tip-chord 120 --stations 6 --samples 21
    run_brep_matrix_case "$matrix_db" "$matrix_surface" append "brep_${matrix_surface}_finite_te_2412" --airfoil 2412 --semi-span 640 --root-chord 210 --tip-chord 130 --sweep-offset 40 --stations 6 --samples 25
    run_brep_matrix_case "$matrix_db" "$matrix_surface" append "brep_${matrix_surface}_mixed_te_4_to_6" --airfoil 2412 --tip-airfoil 63-210 --semi-span 700 --root-chord 220 --tip-chord 110 --sweep-offset 70 --dihedral 2 --tip-twist -2 --stations 7 --samples 25
    run_brep_matrix_case "$matrix_db" "$matrix_surface" append "brep_${matrix_surface}_very_thin_0006" --airfoil 0006 --semi-span 620 --root-chord 220 --tip-chord 80 --sweep-offset 50 --stations 7 --samples 31
    run_brep_matrix_case "$matrix_db" "$matrix_surface" append "brep_${matrix_surface}_thick_0024" --airfoil 0024 --full-span 1120 --root-chord 240 --tip-chord 170 --dihedral -3 --stations 7 --samples 31
    run_brep_matrix_case "$matrix_db" "$matrix_surface" append "brep_${matrix_surface}_high_taper" --airfoil 2412 --semi-span 850 --root-chord 280 --tip-chord 28 --sweep-offset 180 --dihedral 3 --stations 8 --samples 29
    run_brep_matrix_case "$matrix_db" "$matrix_surface" append "brep_${matrix_surface}_forward_sweep" --airfoil 4412 --semi-span 720 --root-chord 230 --tip-chord 120 --sweep-offset -170 --dihedral 2 --stations 7 --samples 25
    run_brep_matrix_case "$matrix_db" "$matrix_surface" append "brep_${matrix_surface}_negative_dihedral" --airfoil 2415 --semi-span 700 --root-chord 230 --tip-chord 130 --sweep-offset 60 --dihedral -9 --tip-twist 2 --stations 7 --samples 25
    run_brep_matrix_case "$matrix_db" "$matrix_surface" append "brep_${matrix_surface}_strong_twist" --airfoil 2412 --semi-span 680 --root-chord 230 --tip-chord 120 --sweep-offset 60 --tip-twist -14 --stations 8 --samples 29
    run_brep_matrix_case "$matrix_db" "$matrix_surface" append "brep_${matrix_surface}_mixed_4_to_5" --airfoil 2412 --tip-airfoil 23012 --semi-span 720 --root-chord 230 --tip-chord 120 --sweep-angle 5.555 --dihedral 3 --tip-twist -3 --stations 7 --samples 25
    run_brep_matrix_case "$matrix_db" "$matrix_surface" append "brep_${matrix_surface}_mixed_4_to_6" --airfoil 0012 --tip-airfoil 65-210 --semi-span 720 --root-chord 230 --tip-chord 120 --sweep-offset 90 --dihedral 3 --tip-twist -3 --stations 7 --samples 25
    run_brep_matrix_case "$matrix_db" "$matrix_surface" append "brep_${matrix_surface}_mixed_5_to_6" --airfoil 23012 --tip-airfoil 63-212 --semi-span 720 --root-chord 230 --tip-chord 120 --sweep-offset 90 --dihedral 3 --tip-twist -3 --stations 7 --samples 25

    run "$MGED" -c "$matrix_db" "tops -n" || fail "BREP matrix $matrix_surface failed MGED tops"
    expect_grep "brep_${matrix_surface}_sharp_te_0012.r" "$LOGFILE"
    expect_grep "brep_${matrix_surface}_mixed_5_to_6.r" "$LOGFILE"
}

if test "x$2" != "x" ; then
    NACA456="$2"
else
    NACA456="`ensearch naca456`"
fi
if test ! -f "$NACA456" ; then
    fail "Unable to find naca456, aborting"
fi

if test "x$3" != "x" ; then
    MGED="$3"
else
    MGED="`ensearch mged`"
fi
if test ! -f "$MGED" ; then
    fail "Unable to find mged, aborting"
fi

log "=== TESTING naca456 procedural wing generation ==="

rm -f naca456-bot.g naca456-five-bot.g naca456-sharp-brep.g naca456-smooth-brep.g
rm -f naca456-five-reflex-brep.g naca456-six-bot.g naca456-sixa-brep.g
rm -f naca456-append.g naca456-demo.g naca456-demo-smooth.g
rm -f naca456-brep-matrix-ruled.g naca456-brep-matrix-smooth.g
rm -f naca456-sections.tsv naca456-section-2412.tsv naca456-section-23012.tsv
rm -f naca456-section-23112.tsv naca456-section-63-206.tsv naca456-section-63a210.tsv

run "$NACA456" --mode bot --force --output naca456-bot.g --name naca_bot_2412 --airfoil 2412 --semi-span 900 --root-chord 240 --tip-chord 120 --sweep-offset 80 --dihedral 4 --tip-twist -2 --stations 7 --samples 33
run "$MGED" -c naca456-bot.g "l naca_bot_2412.bot"
expect_grep "vertices" "$LOGFILE"
expect_grep "counter-clockwise" "$LOGFILE"
run "$MGED" -c naca456-bot.g "attr get naca_bot_2412.r naca456::root_airfoil"
expect_grep "2412" "$LOGFILE"
run "$MGED" -c naca456-bot.g "attr get naca_bot_2412.r naca456::geometry_mode"
expect_grep "bot" "$LOGFILE"
run "$MGED" -c naca456-bot.g "attr get naca_bot_2412.r naca456::semi_span"
expect_grep "900" "$LOGFILE"
run "$MGED" -c naca456-bot.g "attr get naca_bot_2412.r naca456::sweep_offset"
expect_grep "80" "$LOGFILE"

run "$NACA456" --mode bot --force --output naca456-five-bot.g --name naca_five_23012 --airfoil 23012 --semi-span 650 --root-chord 200 --tip-chord 110 --sweep-offset 60 --dihedral 3 --stations 6 --samples 31
run "$MGED" -c naca456-five-bot.g "l naca_five_23012.bot"
expect_grep "naca_five_23012.bot" "$LOGFILE"

run "$NACA456" --mode brep --sharp-te --force --output naca456-sharp-brep.g --name naca_sharp_0012 --airfoil 0012 --semi-span 500 --root-chord 180 --tip-chord 120 --stations 5 --samples 17
run "$MGED" -c naca456-sharp-brep.g "l naca_sharp_0012.brep"
expect_grep "Boundary Representation" "$LOGFILE"

run "$NACA456" --mode brep --brep-surface smooth --force --output naca456-smooth-brep.g --name naca_smooth_2412 --airfoil 2412 --semi-span 650 --root-chord 200 --tip-chord 100 --sweep-offset 60 --dihedral 3 --tip-twist -2 --stations 6 --samples 21
run "$MGED" -c naca456-smooth-brep.g "l naca_smooth_2412.brep"
expect_grep "Boundary Representation" "$LOGFILE"
run "$MGED" -c naca456-smooth-brep.g "attr get naca_smooth_2412.r naca456::brep_surface"
expect_grep "smooth" "$LOGFILE"

run "$NACA456" --mode brep --force --output naca456-five-reflex-brep.g --name naca_five_reflex_23112 --airfoil 23112 --semi-span 560 --root-chord 190 --tip-chord 115 --sweep-offset 40 --stations 5 --samples 21
run "$MGED" -c naca456-five-reflex-brep.g "l naca_five_reflex_23112.brep"
expect_grep "Boundary Representation" "$LOGFILE"

run "$NACA456" --mode bot --force --output naca456-six-bot.g --name naca_six_63215 --airfoil 63-215 --six-a 0.8 --semi-span 700 --root-chord 210 --tip-chord 105 --sweep-offset 70 --tip-twist -2 --stations 6 --samples 31
run "$MGED" -c naca456-six-bot.g "l naca_six_63215.bot"
expect_grep "naca_six_63215.bot" "$LOGFILE"

run "$NACA456" --mode brep --force --output naca456-sixa-brep.g --name naca_sixa_64a210 --airfoil 64A210 --semi-span 560 --root-chord 190 --tip-chord 120 --dihedral 4 --stations 5 --samples 21
run "$MGED" -c naca456-sixa-brep.g "l naca_sixa_64a210.brep"
expect_grep "Boundary Representation" "$LOGFILE"

run "$NACA456" --mode bot --force --output naca456-append.g --name naca_append_bot --airfoil 4415 --semi-span 750 --root-chord 220 --tip-chord 150 --sweep-offset -40 --dihedral -2 --tip-twist 3 --stations 6 --samples 29
run "$NACA456" --mode brep --append --output naca456-append.g --name naca_append_brep --airfoil 2412 --tip-airfoil 65-210 --semi-span 700 --root-chord 220 --tip-chord 130 --sweep-offset 90 --dihedral 6 --tip-twist -4 --x-offset 550 --stations 6 --samples 21
run "$MGED" -c naca456-append.g "tops -n"
expect_grep "naca_append_bot.r" "$LOGFILE"
expect_grep "naca_append_brep.r" "$LOGFILE"

if "$NACA456" --mode bot --append --output naca456-append.g --name naca_append_bot --airfoil 0012 --semi-span 200 --root-chord 80 --tip-chord 50 --stations 3 --samples 9 >> "$LOGFILE" 2>&1 ; then
    fail "append accepted an existing object name"
fi

run "$NACA456" --demo-file naca456-demo.g
run "$MGED" -c naca456-demo.g "tops -n"
expect_grep "bot_00_baseline_2412.r" "$LOGFILE"
expect_grep "bot_23_compound.r" "$LOGFILE"
expect_grep "brep_24_baseline_2412.r" "$LOGFILE"
expect_grep "brep_35_high_resolution.r" "$LOGFILE"
run "$MGED" -c naca456-demo.g "attr get brep_24_baseline_2412.r naca456::brep_surface"
expect_grep "ruled" "$LOGFILE"

run "$NACA456" --brep-surface smooth --demo-file naca456-demo-smooth.g
run "$MGED" -c naca456-demo-smooth.g "attr get brep_24_baseline_2412.r naca456::brep_surface"
expect_grep "smooth" "$LOGFILE"
run "$MGED" -c naca456-demo-smooth.g "attr get bot_00_baseline_2412.r naca456::brep_surface"
expect_grep "ruled" "$LOGFILE"

run_brep_matrix_surface ruled
run_brep_matrix_surface smooth

run "$NACA456" --force --airfoil 2412 --section-file naca456-section-2412.tsv
run "$NACA456" --force --airfoil 23012 --section-file naca456-section-23012.tsv
run "$NACA456" --force --airfoil 23112 --section-file naca456-section-23112.tsv
run "$NACA456" --force --airfoil 63-206 --section-file naca456-section-63-206.tsv
run "$NACA456" --force --airfoil 63a210 --section-file naca456-section-63a210.tsv
cat naca456-section-2412.tsv naca456-section-23012.tsv naca456-section-23112.tsv naca456-section-63-206.tsv naca456-section-63a210.tsv > naca456-sections.tsv
compare_sections "$1/regress/naca456/pdas_sections.tsv" naca456-sections.tsv 0.0001

if test $STATUS -eq 0 ; then
    log "-> naca456.sh succeeded"
else
    log "-> naca456.sh FAILED, see $LOGFILE"
    cat "$LOGFILE"
fi

exit $STATUS

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
