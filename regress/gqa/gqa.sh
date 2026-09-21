#!/bin/sh
#                          G Q A . S H
# BRL-CAD
#
# Copyright (c) 2010-2026 United States Government as represented by
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
#
###

# Ensure /bin/sh
export PATH || (echo "This isn't sh."; sh $0 $*; kill $$)

# source common library functionality, setting ARGS, NAME_OF_THIS,
# PATH_TO_THIS, and THIS.
. "$1/regress/library.sh"

if test "x$LOGFILE" = "x" ; then
    LOGFILE=`pwd`/gqa.log
    rm -f $LOGFILE
fi
log "=== TESTING 'gqa' ==="

run_capture ( ) {
    outfile="$1"
    shift
    log "... running $@"
    "$@" > "$outfile" 2>&1
    ret=$?
    cat "$outfile" >> "$LOGFILE"
    case "x$STATUS" in
	'x'|*[!0-9]*)
	    :;;
	*)
	    if test $ret -ne 0 ; then
		STATUS="`expr $STATUS + 1`"
	    fi
	    ;;
    esac
    return $ret
}

extract_last_number ( ) {
    label="$1"
    file="$2"
    awk -v label="$label" '
	index($0, label) {
	    val = $(NF-1)
	}
	END {
	    if (val == "") exit 1
	    print val
	}
    ' "$file"
}

assert_close ( ) {
    expected="$1"
    actual="$2"
    tolerance="$3"
    desc="$4"

    if awk -v expected="$expected" -v actual="$actual" -v tol="$tolerance" '
	BEGIN {
	    diff = actual - expected
	    if (diff < 0) diff = -diff
	    exit(diff <= tol ? 0 : 1)
	}
    '; then
	log "PASS: $desc (expected=$expected actual=$actual tol=$tolerance)"
    else
	log "FAIL: $desc (expected=$expected actual=$actual tol=$tolerance)"
	STATUS="`expr $STATUS + 1`"
    fi
}

MGED="`ensearch mged`"
if test ! -f "$MGED" ; then
    log "Unable to find mged, aborting"
    exit 1
fi

GQABIN="`ensearch gqa`"
if test ! -f "$GQABIN" ; then
    log "Unable to find gqa, aborting"
    exit 1
fi
NIRT="`ensearch nirt`"
if test ! -f "$NIRT" ; then
    log "Unable to find nirt, aborting"
    exit 1
fi

rm -f density_table.txt
echo "5 1 stuff" > density_table.txt
echo "2 0.001 gas" >> density_table.txt

rm -f gqa.mged
cat > gqa.mged <<EOF
units m
bo -i u c _DENSITIES density_table.txt

in box1.s rpp 0 10 0 10 0 10
in box2.s rpp 1  9 1  9 1  9
in box3.s rpp 1 10 1  9 1  9
in box4.s rpp 0.5  9.5 0.5  5 0.5  9.5

r solid_box.r u box1.s
adjust solid_box.r GIFTmater 5
mater solid_box.r "plastic tr=0.5 di=0.5 sp=0.5" 128 128 128 0

r closed_box.r u box1.s - box2.s
adjust closed_box.r GIFTmater 5
mater closed_box.r "plastic tr=0.5 di=0.5 sp=0.5" 128 128 128 0

r open_box.r u box1.s - box3.s
mater open_box.r "plastic tr=0.5 di=0.5 sp=0.5" 128 128 128 0

r exposed_air.r u box3.s
adjust exposed_air.r air 2
mater exposed_air.r  "plastic tr=0.8 di=0.1 sp=0.1" 255 255 128 0
g exposed_air.g exposed_air.r open_box.r

r adj_air1.r u box2.s + box4.s
r adj_air2.r u box2.s - box4.s

adjust adj_air1.r air 3
adjust adj_air1.r GIFTmater 2

adjust adj_air2.r air 4
adjust adj_air2.r GIFTmater 2

mater adj_air1.r  "plastic tr=0.5 di=0.1 sp=0.1" 255 128 128  0
mater adj_air2.r  "plastic tr=0.5 di=0.1 sp=0.1" 128 128 255 0

g adj_air.g closed_box.r adj_air1.r adj_air2.r

g gap.g closed_box.r adj_air2.r

r overlap_obj.r u box3.s
adjust overlap_obj.r GIFTMater 5
g overlaps closed_box.r overlap_obj.r

g pure_air.g adj_air1.r

in mass_solid.s rpp 0 1 0 1 0 1
r mass_solid.r u mass_solid.s
adjust mass_solid.r GIFTmater 5

# A 0.01 mm solid verifies that automatic stability scales below
# BN_TOL_DIST rather than treating sub-tolerance geometry as ordinary size.
in tiny.s rpp 0 0.00001 0 0.00001 0 0.00001
r tiny.r u tiny.s

in mass_air.s rpp 1 2 0 1 0 1
r mass_air.r u mass_air.s
adjust mass_air.r air 2
adjust mass_air.r GIFTmater 2

g mass_air_only.g mass_air.r
g mass_air_mix.g mass_solid.r mass_air.r

in sphere.s sph 0 0 0 1
r sphere.r u sphere.s
adjust sphere.r GIFTmater 5

q
EOF


log "... running mged to create a geometry database (gqa.g)"
rm -f gqa.g
$MGED -c gqa.g <<EOF >> $LOGFILE 2>&1
`cat gqa.mged`
EOF

cat > gqa.matrix.mged <<EOF
units m
in box.s rpp 0 1 0 1 0 1
r left.r u box.s
r right.r u box.s
comb moved.c u right.r
arced moved.c/right.r matrix rarc xlate 0.5 0 0
g model left.r moved.c
q
EOF
rm -f gqa.matrix.g
$MGED -c gqa.matrix.g < gqa.matrix.mged >> $LOGFILE 2>&1

GQA="$GQABIN -u m,m^3,kg -g 250mm-50mm -p gqa."
STATUS=0

#
# now that the inputs have been built, run the tests
#
# box1.s = 10x10x10     = 1000 m^3
# box2.s = 8x8x8        =  512 m^3
# box3.s = 8x8x9        =  576 m^3
# adj_air1.r = 512-256  = 256 m^3
# adj_air2.r = 512-256  = 256 m^3
# closed_box.r =1000-512= 488 m^3
# exposed_air.r         = 576 m^3
# open_box.r = 1000-576 = 424 m^3

rm -f gqa.overlaps.plot3
run $GQA -Ao gqa.g overlaps

rm -f gqa.exp_air.plot3
run $GQA -Ae gqa.g exposed_air.g

rm -f gqa.adj_air.plot3
run $GQA -Aa gqa.g adj_air.g

rm -f gqa.gaps.plot3
run $GQA -Ag gqa.g gap.g

rm -f gqa.volume.plot3
run $GQA -Av -v gqa.g closed_box.r

run $GQA -r -Aw -v gqa.g closed_box.r

rm -f gqa.volume.plot3
run $GQA -r -Avw gqa.g solid_box.r

rm -f gqa.volume.plot3
run $GQA -r -Avw gqa.g adj_air.g

rm -f gqa.overlaps.plot3
run $GQA -r -v -g 0.25m-25mm -Awo gqa.g closed_box.r

rm -f gqa.overlaps.plot3
run $GQA -g 50mm -Ao gqa.g closed_box.r

run $GQA -Am gqa.g closed_box.r

run_capture gqa.mass_air.aw.out $GQABIN -u m,m^3,kg -g 250mm-50mm -Aw gqa.g mass_air_only.g
run_capture gqa.mass_air.am.out $GQABIN -u m,m^3,kg -g 250mm-50mm -Am gqa.g mass_air_only.g
run_capture gqa.mass_mix.aw.out $GQABIN -u m,m^3,kg -g 250mm-50mm -Aw gqa.g mass_air_mix.g
run_capture gqa.mass_mix.am.out $GQABIN -u m,m^3,kg -g 250mm-50mm -Am gqa.g mass_air_mix.g
run_capture gqa.mass_mix.u0.out $GQABIN -u m,m^3,kg -g 250mm-50mm -U 0 -Aw gqa.g mass_air_mix.g
run_capture gqa.sphere_adaptive.out $GQABIN -u m,m^3,kg -q -Av -g 0.05m gqa.g sphere.r

MASS_AIR_AW="`extract_last_number 'Average total weight:' gqa.mass_air.aw.out`"
MASS_AIR_AM="`extract_last_number 'Average total weight:' gqa.mass_air.am.out`"
MASS_MIX_AW="`extract_last_number 'Average total weight:' gqa.mass_mix.aw.out`"
MASS_MIX_AM="`extract_last_number 'Average total weight:' gqa.mass_mix.am.out`"
MASS_MIX_U0="`extract_last_number 'Average total weight:' gqa.mass_mix.u0.out`"
SPHERE_VOLUME="`extract_last_number 'Average total volume:' gqa.sphere_adaptive.out`"

assert_close 1 "$MASS_AIR_AW" 0.001 "pure modeled air weight with -Aw"
assert_close "$MASS_AIR_AW" "$MASS_AIR_AM" 0.001 "-Aw and -Am agree on pure modeled air"
assert_close 1001 "$MASS_MIX_AW" 0.001 "mixed solid+air modeled weight with -Aw"
assert_close "$MASS_MIX_AW" "$MASS_MIX_AM" 0.001 "-Aw and -Am agree on mixed modeled air"
assert_close 1000 "$MASS_MIX_U0" 0.001 "-U 0 excludes modeled air from weight"
assert_close 4.18879 "$SPHERE_VOLUME" 0.01 "adaptive sphere volume refines beyond symmetric first pass"


# The experimental interface must remain separate from the legacy options.
run_capture gqa.modern.grid.out $GQABIN --analyze --measure volume,mass,area --spacing 50 --refine 0 --json gqa.modern.grid.json gqa.g mass_solid.r
MODERN_GRID_VOLUME="`extract_last_number 'Volume:' gqa.modern.grid.out`"
MODERN_GRID_MASS="`extract_last_number 'Mass:' gqa.modern.grid.out`"
assert_close 1000000000 "$MODERN_GRID_VOLUME" 1000 "experimental grid cube volume"
assert_close 1000000 "$MODERN_GRID_MASS" 1 "experimental grid cube mass"
if test ! -s gqa.modern.grid.json || ! grep -q '"schema_version": 2' gqa.modern.grid.json || ! grep -q '"path": "/mass_solid.r"' gqa.modern.grid.json ; then
    log "FAIL: experimental JSON region record"
    STATUS="`expr $STATUS + 1`"
fi

GQA_DB_CHECKSUM_BEFORE="`cksum gqa.g`"
if $GQABIN --analyze --measure volume --json gqa.g gqa.g mass_solid.r > gqa.modern.collision.out 2>&1 ; then
    log "FAIL: analysis accepted the database as its JSON output"
    STATUS="`expr $STATUS + 1`"
fi
GQA_DB_CHECKSUM_AFTER="`cksum gqa.g`"
if test "x$GQA_DB_CHECKSUM_BEFORE" != "x$GQA_DB_CHECKSUM_AFTER" ; then
    log "FAIL: rejected output collision modified the database"
    STATUS="`expr $STATUS + 1`"
fi

if $GQABIN --analyze --measure volume --json gqa.alias.json --invalid-rays ./gqa.alias.json gqa.g mass_solid.r > gqa.modern.alias_collision.out 2>&1 ; then
    log "FAIL: analysis accepted aliased JSON and invalid-ray output paths"
    STATUS="`expr $STATUS + 1`"
fi
if test -e gqa.alias.json ; then
    log "FAIL: rejected aliased output paths created an output file"
    STATUS="`expr $STATUS + 1`"
fi

# The shared bu_opt parser must recognize assignment syntax and options on
# either side of positional database and object arguments.
run_capture gqa.modern.options.out $GQABIN --analyze --measure=volume gqa.g --sampler=crofton --sequence=random mass_solid.r --rays=1000
if ! grep -q '^gqa analysis (crofton, random), 1000 rays$' gqa.modern.options.out ||
   ! grep -q '^Stopping condition: ray_limit$' gqa.modern.options.out ; then
    log "FAIL: experimental bu_opt syntax and positional argument handling"
    STATUS="`expr $STATUS + 1`"
fi

# Exercise QMC when this build provides it.  A failed probe is a valid skip
# only when gqa explicitly reports that QMC support is unavailable.
log "... checking for QMC support"
QMC_STATUS=0
$GQABIN --analyze --measure=volume --sampler=crofton --sequence=qmc --rays=1000 gqa.g mass_solid.r > gqa.modern.qmc.out 2>&1 || QMC_STATUS=$?
cat gqa.modern.qmc.out >> "$LOGFILE"
if test $QMC_STATUS -eq 0 ; then
    if ! grep -q '^gqa analysis (crofton, qmc), 1000 rays$' gqa.modern.qmc.out ||
       ! grep -q '^Stopping condition: ray_limit$' gqa.modern.qmc.out ; then
	log "FAIL: experimental QMC analysis"
	STATUS="`expr $STATUS + 1`"
    fi
else
    if ! grep -q '^QMC is unavailable in this build\.$' gqa.modern.qmc.out ; then
	log "FAIL: experimental QMC availability check"
	STATUS="`expr $STATUS + 1`"
    fi
fi

if $GQABIN --analyze --measure=invalid gqa.g mass_solid.r > gqa.modern.invalid_value.out 2>&1 ; then
    log "FAIL: analysis accepted an invalid bu_opt value"
    STATUS="`expr $STATUS + 1`"
fi
if $GQABIN --analyze --unknown-option gqa.g mass_solid.r > gqa.modern.unknown_option.out 2>&1 ; then
    log "FAIL: analysis accepted an unknown bu_opt option"
    STATUS="`expr $STATUS + 1`"
fi

run_capture gqa.modern.grid_edge.out $GQABIN --analyze --measure volume,area --spacing 600 --refine 0 gqa.g mass_solid.r
MODERN_GRID_EDGE_VOLUME="`extract_last_number 'Volume:' gqa.modern.grid_edge.out`"
MODERN_GRID_EDGE_AREA="`extract_last_number 'Area:' gqa.modern.grid_edge.out`"
assert_close 1000000000 "$MODERN_GRID_EDGE_VOLUME" 1000 "experimental clipped grid-cell volume"
assert_close 6000000 "$MODERN_GRID_EDGE_AREA" 1 "experimental clipped grid-cell area"

run_capture gqa.modern.refine.out $GQABIN --analyze --measure volume,area --spacing 600 --refine 1 --json gqa.modern.refine.json gqa.g mass_solid.r
if ! grep -q '^Final refinement volume change:' gqa.modern.refine.out ||
   ! grep -q '"level": 1' gqa.modern.refine.json ; then
    log "FAIL: experimental grid refinement evidence"
    STATUS="`expr $STATUS + 1`"
fi

run_capture gqa.modern.rotated.out $GQABIN --analyze --measure volume,mass,area,centroid,moments --density density_table.txt --sampler grid-rotated --spacing 50 --refine 2 --azimuth 35 --elevation 25 --json gqa.modern.rotated.json gqa.g mass_solid.r
MODERN_ROTATED_VOLUME="`extract_last_number 'Volume:' gqa.modern.rotated.out`"
MODERN_ROTATED_MASS="`extract_last_number 'Mass:' gqa.modern.rotated.out`"
MODERN_ROTATED_AREA="`extract_last_number 'Area:' gqa.modern.rotated.out`"
assert_close 1000000000 "$MODERN_ROTATED_VOLUME" 1000000 "experimental rotated-grid cube volume"
assert_close 1000000 "$MODERN_ROTATED_MASS" 1000 "experimental rotated-grid cube mass"
assert_close 6000000 "$MODERN_ROTATED_AREA" 10000 "experimental rotated-grid cube area"
if ! grep -q '^gqa analysis (grid-rotated),' gqa.modern.rotated.out ||
   ! grep -q '^Centroid:' gqa.modern.rotated.out ||
   ! grep -q '^Inertia tensor' gqa.modern.rotated.out ||
   ! grep -q '^Grid directional spread:' gqa.modern.rotated.out ||
   ! grep -q '"sampler": "grid-rotated"' gqa.modern.rotated.json ||
   ! grep -q '"centroid_mm":' gqa.modern.rotated.json ||
   ! grep -q '"inertia_tensor_g_mm2":' gqa.modern.rotated.json ||
   ! grep -q '"grid_view_directions":' gqa.modern.rotated.json ||
   ! grep -q '"phase_u":' gqa.modern.rotated.json ; then
    log "FAIL: experimental rotated-grid evidence"
    STATUS="`expr $STATUS + 1`"
fi

run_capture gqa.modern.grid_defaults.out $GQABIN --analyze --measure volume --json gqa.modern.grid_defaults.json gqa.g mass_solid.r
if ! grep -q '^Stopping condition: stable$' gqa.modern.grid_defaults.out ||
   ! grep -q '"sampler": "grid"' gqa.modern.grid_defaults.json ||
   ! grep -q '"spacing_defaulted": true' gqa.modern.grid_defaults.json ||
   ! grep -q '"stability_defaulted": true' gqa.modern.grid_defaults.json ||
   ! grep -q '"time_defaulted": true' gqa.modern.grid_defaults.json ||
   ! grep -q '"time_ms": 60000.0' gqa.modern.grid_defaults.json ; then
    log "FAIL: grid time-first automatic stability defaults"
    STATUS="`expr $STATUS + 1`"
fi

if $GQABIN --analyze --measure volume --sampler grid --azimuth 35 --refine 0 gqa.g mass_solid.r > gqa.modern.axis_angle.out 2>&1 ; then
    log "FAIL: axis-aligned grid accepted a rotated-grid orientation"
    STATUS="`expr $STATUS + 1`"
fi

run_capture gqa.modern.air_excluded.out $GQABIN --analyze --measure volume,mass --air exclude --spacing 50 --refine 0 gqa.g mass_air_mix.g
MODERN_SOLID_VOLUME="`extract_last_number 'Volume:' gqa.modern.air_excluded.out`"
MODERN_SOLID_MASS="`extract_last_number 'Mass:' gqa.modern.air_excluded.out`"
assert_close 1000000000 "$MODERN_SOLID_VOLUME" 1000 "experimental air-excluded volume"
assert_close 1000000 "$MODERN_SOLID_MASS" 1 "experimental air-excluded mass"

run_capture gqa.modern.random.out $GQABIN --analyze --measure volume,area --sampler crofton --sequence random --rays 20000 gqa.g mass_solid.r
MODERN_RANDOM_VOLUME="`extract_last_number 'Volume:' gqa.modern.random.out`"
assert_close 1000000000 "$MODERN_RANDOM_VOLUME" 50000000 "experimental random Crofton cube volume"

run_capture gqa.modern.uncertainty.out $GQABIN --analyze --measure volume,area --sampler crofton --sequence random --rays 3200 --uncertainty --accuracy-scope all --json gqa.modern.uncertainty.json gqa.g mass_solid.r
if ! grep -q '^Approximate 95% sampling intervals (16 independent replicates):' gqa.modern.uncertainty.out ||
   ! grep -q '"replicates": 16' gqa.modern.uncertainty.json ||
   ! grep -q '"metric": "object:mass_solid.r.volume_mm3"' gqa.modern.uncertainty.json ||
   ! grep -q '"metric": "region:/mass_solid.r#' gqa.modern.uncertainty.json ||
   ! grep -q '"half_width":' gqa.modern.uncertainty.json ; then
    log "FAIL: Crofton independent-replicate uncertainty"
    STATUS="`expr $STATUS + 1`"
fi

run_capture gqa.modern.centroid.out $GQABIN --analyze --measure mass,centroid --density density_table.txt --sampler crofton --sequence random --rays 3200 --uncertainty --json gqa.modern.centroid.json gqa.g mass_solid.r
if ! grep -q '^Centroid:' gqa.modern.centroid.out ||
   ! grep -q '"metric": "model.centroid_x_mm"' gqa.modern.centroid.json ||
   ! grep -q '"centroid_mm":' gqa.modern.centroid.json ; then
    log "FAIL: Crofton paired-ratio centroid uncertainty"
    STATUS="`expr $STATUS + 1`"
fi

run_capture gqa.modern.absolute.out $GQABIN --analyze --measure volume --sampler crofton --sequence random --rays 6400 --absolute-error volume=100000000 --json gqa.modern.absolute.json gqa.g mass_solid.r
if ! grep -q '^Accuracy target:' gqa.modern.absolute.out ||
   ! grep -q '"volume": 100000000' gqa.modern.absolute.json ||
   ! grep -q '"target_tolerance": 100000000' gqa.modern.absolute.json ; then
    log "FAIL: Crofton absolute accuracy target"
    STATUS="`expr $STATUS + 1`"
fi

run_capture gqa.modern.seed_a.out $GQABIN --analyze --measure volume --sampler crofton --sequence random --seed 42 --rays 1000 gqa.g mass_solid.r
run_capture gqa.modern.seed_b.out $GQABIN --analyze --measure volume --sampler crofton --sequence random --seed 42 --rays 1000 gqa.g mass_solid.r
if ! cmp -s gqa.modern.seed_a.out gqa.modern.seed_b.out ; then
    log "FAIL: seeded Crofton sampling is not reproducible"
    STATUS="`expr $STATUS + 1`"
fi

run_capture gqa.modern.defaults.out $GQABIN --analyze --measure volume --sampler crofton --sequence random --seed 42 --json gqa.modern.defaults.json gqa.g tiny.r
if ! grep -q '^Stopping condition: stable' gqa.modern.defaults.out ||
   ! grep -q '"rays": null' gqa.modern.defaults.json ||
   ! grep -q '"stability_defaulted": true' gqa.modern.defaults.json ||
   ! grep -q '"stability_mm": 1e-05' gqa.modern.defaults.json ||
   ! grep -q '"time_defaulted": true' gqa.modern.defaults.json ||
   ! grep -q '"time_ms": 60000.0' gqa.modern.defaults.json ; then
    log "FAIL: Crofton time-first automatic stability defaults"
    STATUS="`expr $STATUS + 1`"
fi

run_capture gqa.modern.accuracy.out $GQABIN --analyze --measure volume,area --sampler crofton --sequence random --rays 6400 --uncertainty --relative-error 0.1 --json gqa.modern.accuracy.json gqa.g mass_solid.r
if ! grep -q '^Accuracy target:' gqa.modern.accuracy.out ||
   ! grep -q '"pilot_rays":' gqa.modern.accuracy.json ||
   ! grep -q '"production_rays":' gqa.modern.accuracy.json ; then
    log "FAIL: Crofton pilot and production accuracy evaluation"
    STATUS="`expr $STATUS + 1`"
fi

run_capture gqa.modern.deadline.out $GQABIN --analyze --measure volume --sampler crofton --sequence random --uncertainty --relative-error 0.1 --time 1000 --json gqa.modern.deadline.json gqa.g mass_solid.r
if ! grep -q '^Accuracy target: target_met' gqa.modern.deadline.out ||
   ! grep -q '^Adaptive plan:' gqa.modern.deadline.out ||
   ! grep -q '^Deadline:' gqa.modern.deadline.out ||
   ! grep -q '"rays": null' gqa.modern.deadline.json ||
   ! grep -q '"forecast_production_rays":' gqa.modern.deadline.json ||
   ! grep -q '"planning_safety_factor": 2' gqa.modern.deadline.json ||
   ! grep -q '"deadline_hit": false' gqa.modern.deadline.json ; then
    log "FAIL: Crofton deadline-driven adaptive accuracy"
    STATUS="`expr $STATUS + 1`"
fi

run_capture gqa.modern.time_limit.out $GQABIN --analyze --measure volume --sampler crofton --sequence random --uncertainty --relative-error 0.000000001 --time 500 --json gqa.modern.time_limit.json gqa.g mass_solid.r
if ! grep -q '^Accuracy target: time_limit' gqa.modern.time_limit.out ||
   ! grep -q '"accuracy_status": "time_limit"' gqa.modern.time_limit.json ||
   ! grep -q '"deadline_limited": true' gqa.modern.time_limit.json ; then
    log "FAIL: Crofton adaptive deadline status"
    STATUS="`expr $STATUS + 1`"
fi

run_capture gqa.modern.issues.out $GQABIN --analyze --measure volume --check overlaps --spacing 250 --json gqa.modern.issues.json gqa.g overlaps
if ! grep -q '^overlap: ' gqa.modern.issues.out ||
   ! grep -q '"type": "overlap"' gqa.modern.issues.json ||
   ! grep -q '"start_point_mm"' gqa.modern.issues.json ||
   ! grep -q '"end_point_mm"' gqa.modern.issues.json ; then
    log "FAIL: experimental overlap report"
    STATUS="`expr $STATUS + 1`"
fi

run_capture gqa.modern.matrix.out $GQABIN --analyze --measure volume --check overlaps --spacing 100 gqa.matrix.g model
if ! grep -q '/model/moved.c/right.r' gqa.modern.matrix.out ||
   grep -q '^Area:' gqa.modern.matrix.out ; then
    log "FAIL: transformed overlap path must remain complete"
    STATUS="`expr $STATUS + 1`"
fi

# Crofton diagnostics use complete ray partitions and must report the same
# intentionally constructed overlap, gap, and air cases as the grid path.
run_capture gqa.modern.crofton.out $GQABIN --analyze --sampler crofton --sequence random --rays 12000 --check overlaps --json gqa.modern.crofton.json gqa.g overlaps
if ! grep -q 'Overlap candidate boxes: ' gqa.modern.crofton.out ||
   ! grep -q '^overlap: ' gqa.modern.crofton.out ||
   ! grep -q '"targeted_overlap": true' gqa.modern.crofton.json ||
   ! grep -q '"event_probability_upper_bound_95"' gqa.modern.crofton.json ||
   ! grep -q '"type": "overlap"' gqa.modern.crofton.json ; then
    log "FAIL: Crofton targeted overlap report"
    STATUS="`expr $STATUS + 1`"
fi

CROFTON_EVENT_COUNT="`grep -c '"ray_id":' gqa.modern.crofton.json`"
CROFTON_RAY_COUNT="`grep '"ray_id":' gqa.modern.crofton.json | sort -u | wc -l | tr -d ' '`"
if test "$CROFTON_EVENT_COUNT" -ne "$CROFTON_RAY_COUNT" ||
   test "$CROFTON_RAY_COUNT" -le 1 ; then
    log "FAIL: Crofton overlap events must be delivered once with their own ray IDs"
    STATUS="`expr $STATUS + 1`"
fi

run_capture gqa.modern.tolerance.out $GQABIN --analyze --sampler crofton --sequence random --rays 1000 --check overlaps --tolerance 1000000000 gqa.g overlaps
if grep -q '^overlap: ' gqa.modern.tolerance.out ; then
    log "FAIL: Crofton overlap tolerance"
    STATUS="`expr $STATUS + 1`"
fi

run_capture gqa.modern.stability_cap.out $GQABIN --analyze --measure volume --sampler crofton --sequence random --rays 100 --stability 1 gqa.g mass_solid.r
if ! grep -q '), 100 rays$' gqa.modern.stability_cap.out ; then
    log "FAIL: Crofton stability must honor the ray limit"
    STATUS="`expr $STATUS + 1`"
fi

run_capture gqa.modern.gaps.out $GQABIN --analyze --measure volume --sampler crofton --sequence random --rays 5000 --check gaps --json gqa.modern.gaps.json gqa.g gap.g
if ! grep -q '^gap: ' gqa.modern.gaps.out ||
   ! grep -q '"type": "gap"' gqa.modern.gaps.json ; then
    log "FAIL: Crofton gap report"
    STATUS="`expr $STATUS + 1`"
fi

run_capture gqa.modern.air.out $GQABIN --analyze --measure volume --sampler crofton --sequence random --rays 5000 --check adjacent-air,exposed-air --json gqa.modern.air.json gqa.g adj_air.g exposed_air.g
if ! grep -q '"sampler": "crofton"' gqa.modern.air.json ||
   ! grep -q '"ray_count": 5000' gqa.modern.air.json ; then
    log "FAIL: Crofton air diagnostic run"
    STATUS="`expr $STATUS + 1`"
fi

cat > gqa.replay.json <<EOF
{"schema_version":2,"model":["mass_solid.r"],"rays":[{"ray_id":7,"origin_mm":[1500,500,500],"direction":[-1,0,0]}]}
EOF
run_capture gqa.replay.out $NIRT --replay gqa.replay.json --ray-id 7 gqa.g mass_solid.r
if ! grep -q 'Replaying ray 7' gqa.replay.out ||
   ! grep -q 'mass_solid.r' gqa.replay.out ; then
    log "FAIL: nirt replay of analysis ray record"
    STATUS="`expr $STATUS + 1`"
fi

if [ $STATUS = 0 ] ; then
    log "-> gqa.sh succeeded"
else
    log "-> gqa.sh FAILED, see $LOGFILE"
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
