#!/bin/sh
#                    B U I L D I N G . S H
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
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
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

if test "x$LOGFILE" = "x" ; then
    LOGFILE="`pwd`/building.log"
    rm -f "$LOGFILE"
fi

MKBUILDING="$2"
MGED="$3"

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
    grep "$pattern" "$file" > /dev/null 2>&1 || fail "expected $pattern in $file"
}

if test ! -x "$MKBUILDING" ; then
    fail "Unable to find mkbuilding executable, aborting"
fi

if test ! -x "$MGED" ; then
    fail "Unable to find mged executable, aborting"
fi

log "=== TESTING procedural building generation ==="

rm -f building-cli.g building-native.g building-osm.g building-effective.json

run "$MKBUILDING" --force --preset house --name regression_house \
    --levels 3 --width 14 --depth 9 --floor-height 3 \
    --roof-shape hipped --roof-height 2 --structure wood_frame \
    --no-auto-openings -o building-cli.g || fail "CLI building generation failed"
run "$MGED" -c building-cli.g "l regression_house_roof.bot" || fail "CLI roof solid missing"
run "$MGED" -c building-cli.g "l regression_house_structure.r" || fail "CLI structure region missing"
run "$MGED" -c building-cli.g "attr get regression_house building::generator" || fail "generator metadata missing"
expect_grep "mkbuilding" "$LOGFILE"
run "$MGED" -c building-cli.g "attr get regression_house building::levels_above_ground" || fail "level metadata missing"
expect_grep "3" "$LOGFILE"
run "$MGED" -c building-cli.g "attr get regression_house building::roof_shape" || fail "roof metadata missing"
expect_grep "hipped" "$LOGFILE"

run "$MKBUILDING" --append --preset garage --name regression_garage \
    --no-auto-openings -o building-cli.g || fail "building append failed"
run "$MGED" -c building-cli.g "tops -n" || fail "appended database tops failed"
expect_grep "regression_house" "$LOGFILE"
expect_grep "regression_garage" "$LOGFILE"
if "$MKBUILDING" --append --preset house --name regression_house \
    -o building-cli.g >> "$LOGFILE" 2>&1 ; then
    fail "duplicate building append unexpectedly succeeded"
fi

run "$MKBUILDING" --force \
    --spec "$1/src/shapes/building/examples/complex_parts.json" \
    --effective-spec building-effective.json -o building-native.g || fail "native specification generation failed"
run "$MGED" -c building-native.g "l civic_complex" || fail "native building group missing"
run "$MGED" -c building-native.g "l clock_tower" || fail "nested tower part missing"
run "$MGED" -c building-native.g "l entrance_canopy" || fail "nested canopy part missing"
run "$MGED" -c building-native.g "attr get civic_complex building::type" || fail "native type metadata missing"
expect_grep "government" "$LOGFILE"
expect_grep "civic_complex" building-effective.json

run "$MKBUILDING" --force \
    --spec "$1/src/shapes/building/examples/osm_feature.geojson" \
    -o building-osm.g || fail "OSM specification generation failed"
run "$MGED" -c building-osm.g "l Mapped_Farm_Building_roof.bot" || fail "OSM roof solid missing"
run "$MGED" -c building-osm.g "attr get Mapped_Farm_Building building::type" || fail "OSM type metadata missing"
expect_grep "barn" "$LOGFILE"
run "$MGED" -c building-osm.g "attr get Mapped_Farm_Building osm::roof:shape" || fail "OSM roof tag missing"
expect_grep "gambrel" "$LOGFILE"

log "-> building.sh succeeded"
exit 0
