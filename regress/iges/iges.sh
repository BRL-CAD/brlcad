#!/bin/sh
#                         I G E S . S H
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

# Tests should use a local cache
BU_DIR_CACHE="`pwd`/cache"
rm -rf $BU_DIR_CACHE && mkdir $BU_DIR_CACHE
export BU_DIR_CACHE
LIBRT_CACHE="`pwd`/rtcache"
rm -rf $LIBRT_CACHE && mkdir $LIBRT_CACHE
export LIBRT_CACHE

if test "x$LOGFILE" = "x" ; then
    LOGFILE=`pwd`/iges.log
    rm -f "$LOGFILE"
fi
log "=== TESTING iges conversion ==="

MGED="`ensearch mged`"
if test ! -f "$MGED" ; then
    log "Unable to find mged, aborting"
    exit 1
fi

GIGES="`ensearch g-iges`"
if test ! -f "$MGED" ; then
    log "Unable to find g-iges, aborting"
    exit 1
fi

IGESG="`ensearch iges-g`"
if test ! -f "$MGED" ; then
    log "Unable to find iges-g, aborting"
    exit 1
fi


STATUS=0

# CREATE G

output=iges.g
rm -f "$output"
log "... running mged to create facetized geometry ($output)"
$MGED -c >> "$LOGFILE" 2>&1 <<EOF
opendb $output y

units mm
size 1000
make box.s arb8
facetize -n box.nmg box.s
kill box.s
q
EOF
if [ ! -f "$output" ] ; then
    log "ERROR: mged failed to create $output"
    log "-> iges.sh FAILED, see $LOGFILE"
    cat "$LOGFILE"
    exit 1
fi

# G TO IGES

# test G -> IGES via -o
output="iges.export.iges"
rm -f "$output"
run $GIGES -o "$output" iges.g box.nmg
if [ ! -f "$output" ] ; then
    log "ERROR: g-iges failed to create $output"
    log "-> iges.sh FAILED, see $LOGFILE"
    cat "$LOGFILE"
    exit 1
fi

# test G -> IGES via stdout (can't use 'run')
output="iges.export.stdout.iges"
norm_output="iges.export.stdout.norm.iges"
rm -f "$output"
log "... running $GIGES iges.g box.nmg > $output"
$GIGES iges.g box.nmg > $output 2>> "$LOGFILE"
if [ ! -f "$output" ] ; then
    log "ERROR: g-iges failed to create $output"
    log "-> iges.sh FAILED, see $LOGFILE"
    cat "$LOGFILE"
    exit 1
fi

# test that the first g-iges -o output matches the stdout output
tr -d '\r' < "$output" > "$norm_output"
files_match iges.export.iges "$norm_output" -I 'G'
if test $? -ne 0 ; then
    STATUS="`expr $STATUS + 1`"
    export STATUS
fi

# G TO IGES TO G

# test IGES -> G
output="iges.import.g"
rm -f "$output"
run $IGESG -o "$output" iges.export.iges
if [ ! -f "$output" ] ; then
    log "ERROR: iges-g failed to create $output"
    log "-> iges.sh FAILED, see $LOGFILE"
    cat "$LOGFILE"
    exit 1
fi

# test IGES -> G (with -p)
output="iges.import2.g"
rm -f "$output"
run $IGESG -o "$output" -p iges.export.iges
if [ ! -f "$output" ] ; then
    log "ERROR: iges-g failed to create $output"
    log "-> iges.sh FAILED, see $LOGFILE"
    cat "$LOGFILE"
    exit 1
fi

# test IGES -> G (with -p -N name)
output="iges.import3.g"
rm -f "$output"
run $IGESG -o "$output" -p -N box.nmg iges.export.iges
if [ ! -f "$output" ] ; then
    log "ERROR: iges-g failed to create $output"
    log "-> iges.sh FAILED, see $LOGFILE"
    cat "$LOGFILE"
    exit 1
fi

# test that these produced different output
files_differ iges.import.g iges.import2.g
if test $? -ne 0 ; then
    STATUS="`expr $STATUS + 1`"
    export STATUS
fi
# FIXME: these should match but -N is creating 'box.nmgA'
# files_match iges.import2.g iges.import3.g
# if test $? -ne 0 ; then
#     STATUS="`expr $STATUS + 1`"
#     export STATUS
# fi
# FIXME: these should match but -N is creating 'box.nmgA'
# files_match iges.import.g iges.import3.g
# if test $? -ne 0 ; then
#     STATUS="`expr $STATUS + 1`"
#     export STATUS
# fi

# G TO IGES TO G TO IGES (ROUND TRIP)

# All boundary-representation output modes use the shared name sanitizer.
direct_brep_name=box_nmg

# make sure we don't permute vertices or introduce some other
# unintended change.

# test G -> IGES #2a via -o
output="iges.import.export.iges"
rm -f "$output"
run $GIGES -o "$output" iges.import.g "$direct_brep_name"
if [ ! -f "$output" ] ; then
    log "ERROR: g-iges failed to create $output"
    log "-> iges.sh FAILED, see $LOGFILE"
    cat "$LOGFILE"
    exit 1
fi

# test G -> IGES #2b via -o
output="iges.import2.export.iges"
rm -f "$output"
run $GIGES -o "$output" iges.import2.g "$direct_brep_name"
if [ ! -f "$output" ] ; then
    log "ERROR: g-iges failed to create $output"
    log "-> iges.sh FAILED, see $LOGFILE"
    cat "$LOGFILE"
    exit 1
fi

# test G -> IGES #2c via -o
output="iges.import3.export.iges"
rm -f "$output"
run $GIGES -o "$output" iges.import3.g "$direct_brep_name"
if [ ! -f "$output" ] ; then
    log "ERROR: g-iges failed to create $output"
    log "-> iges.sh FAILED, see $LOGFILE"
    cat "$LOGFILE"
    exit 1
fi

# COMPARE RESULTS

# test that initial g-iges output does NOT match final BoT output
files_differ iges.export.iges iges.import.export.iges -I 'G'
if test $? -ne 0 ; then
    STATUS="`expr $STATUS + 1`"
    export STATUS
fi

# FIXME: these should match but vertices are permuted!
# test that initial g-iges output DOES match final NMG output
# files_match iges.export.iges iges.import2.export.iges -I 'G'
# if test $? -ne 0 ; then
#     STATUS="`expr $STATUS + 1`"
#     export STATUS
# fi

# test that initial g-iges output DOES match final named NMG output
# FIXME: these should match but iges-g -N is creating 'box.nmgA'
# files_match iges.export.iges iges.import3.export.iges -I 'G'
# if test $? -ne 0 ; then
#     STATUS="`expr $STATUS + 1`"
#     export STATUS
# fi

# BREP CONVERSION

# The default IGES import now preserves boundary representations as rt_brep
# (OpenNURBS) solids.  Verify the box (a type 186 Manifold BREP in
# iges.export.iges) imports as a valid brep, that -m/-p still yield mesh/NMG,
# and that a brep round-trips back out to IGES and in again.

output="iges.brep.g"
rm -f "$output"
run $IGESG -o "$output" iges.export.iges
if [ ! -f "$output" ] ; then
    log "ERROR: iges-g (brep) failed to create $output"
    STATUS="`expr $STATUS + 1`"
    export STATUS
fi

btype=`$MGED -c "$output" "db get $direct_brep_name" 2>&1 | tr -d '\r' | grep -oE '^(brep|bot|nmg)' | head -1`
log "... default import type for $direct_brep_name: [$btype]"
if test "x$btype" != "xbrep" ; then
    log "ERROR: default IGES import did not produce a brep (got '$btype')"
    STATUS="`expr $STATUS + 1`"
    export STATUS
fi

valid=`$MGED -c "$output" "brep $direct_brep_name valid" 2>&1 | tr -d '\r'`
log "... brep validity: $valid"
case "x$valid" in
    *valid*) : ;;
    *) log "ERROR: imported brep is not valid" ; STATUS="`expr $STATUS + 1`" ; export STATUS ;;
esac

# -m should produce a BoT (mesh)
$IGESG -m -o iges.brep.mesh.g iges.export.iges >> "$LOGFILE" 2>&1
mtype=`$MGED -c iges.brep.mesh.g "db get $direct_brep_name" 2>&1 | tr -d '\r' | grep -oE '^(brep|bot|nmg)' | head -1`
if test "x$mtype" != "xbot" ; then
    log "ERROR: -m did not produce a BoT (got '$mtype')"
    STATUS="`expr $STATUS + 1`"
    export STATUS
fi

# -p should produce an NMG
$IGESG -p -o iges.brep.nmg.g iges.export.iges >> "$LOGFILE" 2>&1
ptype=`$MGED -c iges.brep.nmg.g "db get $direct_brep_name" 2>&1 | tr -d '\r' | grep -oE '^(brep|bot|nmg)' | head -1`
if test "x$ptype" != "xnmg" ; then
    log "ERROR: -p did not produce an NMG (got '$ptype')"
    STATUS="`expr $STATUS + 1`"
    export STATUS
fi

# Round-trip the BRep through native IGES 5.3 topology.  The default exporter
# must use type 186 and the modern importer must preserve the box topology.
run $GIGES -o iges.brep.export.iges iges.brep.g "$direct_brep_name"
native_solids=`awk 'substr($0,73,1)=="D" && (substr($0,74,7)+0)%2==1 && (substr($0,1,8)+0)==186 {n++} END {print n+0}' iges.brep.export.iges`
if test "x$native_solids" != "x1" ; then
    log "ERROR: native BRep export wrote $native_solids type 186 entities"
    STATUS="`expr $STATUS + 1`"
    export STATUS
fi
run $IGESG --strict --repair none -o iges.brep.roundtrip.g iges.brep.export.iges
# Native exports contain NURBS faces, which must tessellate without passing
# curved edge geometry into the legacy straight-edge NMG assembler.
for mode in m p ; do
    run $IGESG -$mode --report iges.native-$mode.json -o iges.native-$mode.g iges.brep.export.iges
    case "$mode" in m) expected=bot ;; p) expected=nmg ;; esac
    actual=`$MGED -c iges.native-$mode.g "db get $direct_brep_name" 2>&1 | tr -d '\r' | grep -oE '^(brep|bot|nmg)' | head -1`
    if test "x$actual" != "x$expected" ; then
        log "ERROR: native NURBS -$mode import produced '$actual', expected '$expected'"
        STATUS="`expr $STATUS + 1`"
        export STATUS
    fi
done

# Reject path aliases before opening either destination, and preserve an
# existing database when parsing or strict conversion fails.
cp iges.brep.export.iges iges.protected.iges
cp iges.brep.g iges.protected.g
for alias in iges.protected.iges ./iges.protected.iges ; do
    $IGESG -o "$alias" iges.protected.iges >> "$LOGFILE" 2>&1
    if test $? -eq 0 || ! cmp -s iges.protected.iges iges.brep.export.iges ; then
        log "ERROR: input/output alias was accepted or modified the input"
        STATUS="`expr $STATUS + 1`"
        export STATUS
    fi
done
$IGESG --report iges.protected.g -o ./iges.protected.g iges.protected.iges >> "$LOGFILE" 2>&1
if test $? -eq 0 || ! cmp -s iges.protected.g iges.brep.g ; then
    log "ERROR: output/report alias was accepted or modified the database"
    STATUS="`expr $STATUS + 1`"
    export STATUS
fi
rm -f iges.alias-hard.iges iges.alias-symbolic.iges
for link_kind in hard symbolic ; do
    link_option=
    if test "x$link_kind" = "xsymbolic" ; then link_option=-s ; fi
    if ln $link_option iges.protected.iges iges.alias-$link_kind.iges ; then
        $IGESG -o iges.alias-$link_kind.iges iges.protected.iges >> "$LOGFILE" 2>&1
        if test $? -eq 0 || ! cmp -s iges.protected.iges iges.brep.export.iges ; then
            log "ERROR: $link_kind input/output alias was accepted or modified the input"
            STATUS="`expr $STATUS + 1`"
            export STATUS
        fi
    fi
done
$IGESG --strict --report iges.failed.json -o iges.protected.g iges.brep.g >> "$LOGFILE" 2>&1
if test $? -eq 0 || ! cmp -s iges.protected.g iges.brep.g || test ! -s iges.failed.json ; then
    log "ERROR: failed conversion did not preserve the database and report failure"
    STATUS="`expr $STATUS + 1`"
    export STATUS
fi
if [ ! -f iges.brep.roundtrip.g ] ; then
    log "ERROR: brep round-trip failed to produce iges.brep.roundtrip.g"
    STATUS="`expr $STATUS + 1`"
    export STATUS
fi

# the round-tripped object should again be a valid brep
rtobj=`$MGED -c iges.brep.roundtrip.g "ls" 2>&1 | tr -d '\r' | awk '{print $1}' | head -1`
rttype=`$MGED -c iges.brep.roundtrip.g "db get $rtobj" 2>&1 | tr -d '\r' | grep -oE '^(brep|bot|nmg)' | head -1`
log "... brep round-trip produced object [$rtobj] of type [$rttype]"
if test "x$rttype" != "xbrep" ; then
    log "ERROR: brep round-trip did not reproduce a brep (got '$rttype')"
    STATUS="`expr $STATUS + 1`"
    export STATUS
fi

rtinfo=`$MGED -c iges.brep.roundtrip.g "brep $rtobj info" 2>&1 | tr -d '\r'`
case "x$rtinfo" in
    *"Valid: YES, Solid: YES"*"faces:     6"*"edges:     12"*"vertices:  8"*) : ;;
    *) log "ERROR: native BRep round-trip did not preserve box topology: $rtinfo" ; STATUS="`expr $STATUS + 1`" ; export STATUS ;;
esac

# Verify native CSG reports and mixed assemblies.  The dotted surface label
# exercises source-to-database name reconciliation.
for fixture in native-csg mixed-assembly name-collision mixed-subfigure hollerith-boundary numeric-boundary ; do
    run $IGESG --strict --report iges.$fixture.json -o iges.$fixture.g "$1/src/conv/iges/tests/$fixture.igs"
    if ! grep -q '"success": true' iges.$fixture.json ||
       ! grep -q '"unresolved_output_references": 0' iges.$fixture.json ; then
        log "ERROR: $fixture did not produce a complete consolidated report"
        STATUS="`expr $STATUS + 1`"
        export STATUS
    fi
done

# Numeric parameters may span the last data column without losing a digit.
numeric_sphere=`$MGED -c iges.numeric-boundary.g "db get sphere.0" 2>&1 | tr -d '\r'`
case "$numeric_sphere" in
    *"A {120 0 0}"*) : ;;
    *) log "ERROR: record-boundary numeric parsing changed the radius: $numeric_sphere" ; STATUS="`expr $STATUS + 1`" ;;
esac

# Compatibility surface switches must still complete mixed hierarchy.
for mode in n t ; do
    run $IGESG -$mode --strict -N selected_root --report iges.mixed-$mode.json \
        -o iges.mixed-$mode.g "$1/src/conv/iges/tests/mixed-subfigure.igs"
    definition=`$MGED -c iges.mixed-$mode.g "db get MIXED" 2>&1 | tr -d '\r'`
    instance=`$MGED -c iges.mixed-$mode.g "db get INST" 2>&1 | tr -d '\r'`
    root=`$MGED -c iges.mixed-$mode.g "db get selected_root" 2>&1 | tr -d '\r'`
    case "$definition/$instance/$root" in
        *sphere.0*FACE_X*MIXED*100*INST*) : ;;
        *) log "ERROR: -$mode lost mixed hierarchy: $definition/$instance/$root" ; STATUS="`expr $STATUS + 1`" ;;
    esac
    if ! grep -q '"groups_written": 3' iges.mixed-$mode.json ||
       ! grep -q '"unresolved_members": 0' iges.mixed-$mode.json ||
       ! grep -q '"omitted": 0' iges.mixed-$mode.json ; then
        log "ERROR: -$mode did not finalize its mixed import report"
        STATUS="`expr $STATUS + 1`"
    fi
    run $IGESG -$mode --strict -N selected_root -o iges.native-$mode.g \
        "$1/src/conv/iges/tests/native-csg.igs"
    root=`$MGED -c iges.native-$mode.g "db get selected_root" 2>&1 | tr -d '\r'`
    case "$root" in
        *sphere.0*) : ;;
        *) log "ERROR: -$mode did not complete a native-only import: $root" ; STATUS="`expr $STATUS + 1`" ;;
    esac
done

# String prefixes may end exactly at the last data column of either section.
boundary_sphere=`$MGED -c iges.hollerith-boundary.g "db get boundary" 2>&1 | tr -d '\r'`
case "$boundary_sphere" in
    *"A {1 0 0}"*) : ;;
    *) log "ERROR: record-boundary string parsing changed the name or units: $boundary_sphere" ; STATUS="`expr $STATUS + 1`" ;;
esac

# A native name must not overwrite a modern BRep with the same sanitized name.
collision_brep=`$MGED -c iges.name-collision.g "db get FACE_X" 2>&1 | grep -oE '^brep' | head -1`
collision_sphere=`$MGED -c iges.name-collision.g "db get FACE_X_1" 2>&1 | grep -oE '^ell' | head -1`
collision_types="$collision_brep $collision_sphere"
case "$collision_types" in
    *brep*ell*) : ;;
    *) log "ERROR: mixed-path name collision lost geometry: $collision_types" ; STATUS="`expr $STATUS + 1`" ;;
esac
definition=`$MGED -c iges.mixed-subfigure.g "db get MIXED" 2>&1 | tr -d '\r'`
instance=`$MGED -c iges.mixed-subfigure.g "db get INST" 2>&1 | tr -d '\r'`
case "$definition/$instance" in
    *sphere.0*FACE_X*MIXED*100*) : ;;
    *) log "ERROR: mixed subfigure lost a member or its placement: $definition/$instance" ; STATUS="`expr $STATUS + 1`" ;;
esac

# Invalid optional metadata is recoverable in default mode, but strict mode
# must reject it without crashing or replacing an existing database.
run $IGESG --report iges.bad-property.json -o iges.bad-property.g "$1/src/conv/iges/tests/bad-property.igs"
$IGESG --strict -o iges.protected.g "$1/src/conv/iges/tests/bad-property.igs" >> "$LOGFILE" 2>&1
if test $? -ne 1 || ! cmp -s iges.protected.g iges.brep.g ||
   ! grep -q 'invalid_property_reference' iges.bad-property.json ; then
    log "ERROR: invalid name-property reference was not handled safely"
    STATUS="`expr $STATUS + 1`"
fi

# In-place repair/edit commands must not silently turn a marked solid into
# an ordinary sheet by replacing the primitive without its attributes.
cp iges.brep.g iges.marked.g
run $MGED -c iges.marked.g "attr set $direct_brep_name _brep_invalid_solid 1 review_note retained"
for edit in "shrink_surfaces" "flip" "geo c2_create_line 0 0 1 1" "topo f_rev 0" ; do
    run $MGED -c iges.marked.g "brep $direct_brep_name $edit"
    marker=`$MGED -c iges.marked.g "attr get $direct_brep_name _brep_invalid_solid review_note" 2>&1 | tr -d '\r'`
    case "$marker" in
        *1*retained*) : ;;
        *) log "ERROR: BRep edit '$edit' lost attributes: $marker" ; STATUS="`expr $STATUS + 1`" ;;
    esac
done
export STATUS

# Unsupported source geometry must make export fail explicitly, while the
# successfully written subset remains a readable IGES file.
run $MGED -c iges.g "in unsupported.half half 0 0 1 0"
run $MGED -c iges.g "put unsupported.heart hrt V {0 0 0} X {10 0 0} Y {0 10 0} Z {0 0 10} d 1"
$GIGES -o iges.partial.iges iges.g box.nmg unsupported.half unsupported.heart >> "$LOGFILE" 2>&1
if test $? -ne 1 ; then
    log "ERROR: partial CSG export did not return a failure status"
    STATUS="`expr $STATUS + 1`"
    export STATUS
fi
run $IGESG --strict -o iges.partial.g iges.partial.iges

# Missing selections are export failures, even when no selected object exists.
for selection in "missing.object" "box.nmg missing.object" ; do
    $GIGES -o iges.missing.iges iges.g $selection >> "$LOGFILE" 2>&1
    if test $? -ne 1 ; then
        log "ERROR: missing export selection '$selection' returned success"
        STATUS="`expr $STATUS + 1`"
    fi
done
run $IGESG --strict -o iges.missing.g iges.missing.iges

# A selected combination with a missing child must also fail explicitly.
run $MGED -c iges.g "put broken.group comb tree {l missing.member}"
$GIGES -o iges.missing.iges iges.g box.nmg broken.group >> "$LOGFILE" 2>&1
if test $? -eq 0 ; then
    log "ERROR: export with a missing combination member returned success"
    STATUS="`expr $STATUS + 1`"
    export STATUS
fi
run $IGESG --strict -o iges.missing.g iges.missing.iges
export STATUS

# Compatibility mode deliberately flattens the same BRep to independent type
# 144 faces.  The modern importer must stitch them back into the same manifold
# without routing through NMG.
run $GIGES --flatten-brep -o iges.brep.flat.iges iges.brep.g "$direct_brep_name"
flat_faces=`awk 'substr($0,73,1)=="D" && (substr($0,74,7)+0)%2==1 && (substr($0,1,8)+0)==144 {n++} END {print n+0}' iges.brep.flat.iges`
flat_solids=`awk 'substr($0,73,1)=="D" && (substr($0,74,7)+0)%2==1 && (substr($0,1,8)+0)==186 {n++} END {print n+0}' iges.brep.flat.iges`
if test "x$flat_faces" != "x6" -o "x$flat_solids" != "x0" ; then
    log "ERROR: flattened BRep export wrote $flat_faces type 144 and $flat_solids type 186 entities"
    STATUS="`expr $STATUS + 1`"
    export STATUS
fi
run $IGESG --strict --repair none -o iges.brep.flat.g iges.brep.flat.iges
flatobj=`$MGED -c iges.brep.flat.g "search -type brep" 2>&1 | tr -d '\r' | awk '{print $1}' | head -1`
flatinfo=`$MGED -c iges.brep.flat.g "brep $flatobj info" 2>&1 | tr -d '\r'`
case "x$flatinfo" in
    *"Valid: YES, Solid: YES"*"faces:     6"*"edges:     12"*"vertices:  8"*) : ;;
    *) log "ERROR: type 144 assembly did not recover box topology: $flatinfo" ; STATUS="`expr $STATUS + 1`" ; export STATUS ;;
esac

# A one-face BRep is an OpenNURBS plate-mode object.  Verify that import keeps
# its zero-thickness default unless the user explicitly supplies a thickness,
# and that the selected policy is visible in the structured report.
plate_source="iges.brep.plate-source.g"
cp iges.brep.g "$plate_source"
if test $? -ne 0 ; then
    log "ERROR: could not prepare the plate-mode test database"
    STATUS="`expr $STATUS + 1`"
    export STATUS
else
    $MGED -c "$plate_source" "brep $direct_brep_name split -O -o box.plates 0" >> "$LOGFILE" 2>&1
    if test $? -ne 0 ; then
	log "ERROR: could not extract a one-face BRep for the plate-mode test"
	STATUS="`expr $STATUS + 1`"
	export STATUS
    else
	run $GIGES --flatten-brep -o iges.brep.plate.iges "$plate_source" "$direct_brep_name.0"
	run $IGESG --strict --repair none -o iges.brep.plate-default.g iges.brep.plate.iges
	plate_obj=`$MGED -c iges.brep.plate-default.g "search -type brep" 2>&1 | tr -d '\r' | awk '{print $1}' | head -1`
	plate_info=`$MGED -c iges.brep.plate-default.g "brep $plate_obj info" 2>&1 | tr -d '\r'`
	case "x$plate_info" in
	    *"Valid: YES, Solid: NO, Plate mode: YES[0.000000 (COS)]"*) : ;;
	    *) log "ERROR: one-face BRep did not retain zero-thickness plate mode: $plate_info" ; STATUS="`expr $STATUS + 1`" ; export STATUS ;;
	esac

	run $IGESG --strict --repair none --default-plate-thickness 1 \
	    --report iges.brep.plate-thick.json \
	    -o iges.brep.plate-thick.g iges.brep.plate.iges
	plate_obj=`$MGED -c iges.brep.plate-thick.g "search -type brep" 2>&1 | tr -d '\r' | awk '{print $1}' | head -1`
	plate_info=`$MGED -c iges.brep.plate-thick.g "brep $plate_obj info" 2>&1 | tr -d '\r'`
	case "x$plate_info" in
	    *"Valid: YES, Solid: NO, Plate mode: YES[1.000000 (COS)]"*) : ;;
	    *) log "ERROR: --default-plate-thickness was not applied: $plate_info" ; STATUS="`expr $STATUS + 1`" ; export STATUS ;;
	esac
	plate_count=`sed -n 's/.*"plate_mode_objects_thickened": \([0-9][0-9]*\).*/\1/p' iges.brep.plate-thick.json`
	if test "x$plate_count" != "x1" ; then
	    log "ERROR: plate-mode report count is '$plate_count', expected 1"
	    STATUS="`expr $STATUS + 1`"
	    export STATUS
	fi
    fi
fi

# COMPLEX TEST

# check another TGM known to have a conversion failure which should be graceful
ASC2G="`ensearch asc2g`"
if test ! -f "$ASC2G" ; then
    log "Unable to find asc2g, aborting"
    exit 1
fi
GZIP="`which gzip`"
if test ! -f "$GZIP" ; then
    log "Unable to find gzip, aborting"
    exit 1
fi

# make our starting database
$GZIP -d -c "$PATH_TO_THIS/m35.asc.gz" > iges.m35.asc
$ASC2G iges.m35.asc iges.m35.g

# and test it (note it should work with the '-f' option, but fail
# without any options)
run $GIGES -f -o iges.m35.r516.iges iges.m35.g r516
if test $? -ne 0 ; then
    STATUS="`expr $STATUS + 1`"
    export STATUS
fi

# TODO: add full m35 conversion test
# run $GIGES -f -o iges.m35.iges iges.m35.g component
# if test $? -ne 0 ; then
#     STATUS="`expr $STATUS + 1`"
#     export STATUS
# fi

if [ X$STATUS = X0 ] ; then
    log "-> iges.sh succeeded"
else
    log "-> iges.sh FAILED, see $LOGFILE"
    cat "$LOGFILE"
fi

# Cleanup
rm -rf "$BU_DIR_CACHE"
rm -rf "$LIBRT_CACHE"

exit $STATUS

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
