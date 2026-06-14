#!/bin/sh
#                         T R E E . S H
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

if test "x$LOGFILE" = "x" ; then
    LOGFILE="`pwd`/tree.log"
    rm -f "$LOGFILE"
fi

TREE="$2"
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
    grep "$pattern" "$file" > /dev/null 2>&1 || fail "expected '$pattern' in $file"
}

if test ! -x "$TREE" ; then
    fail "Unable to find tree executable, aborting"
fi

if test ! -x "$MGED" ; then
    fail "Unable to find mged executable, aborting"
fi

log "=== TESTING procedural volumetric tree generation ==="

rm -f tree-mesh.g tree-json.g tree-csg.g tree-append.g tree-demo.g

run "$TREE" --force --output tree-mesh.g --name test_mesh --preset broadleaf --seed 10 --height 4200 --trunk-radius 150 --leaf-mode simple --detail low || fail "mesh tree generation failed"
run "$MGED" -c tree-mesh.g "l test_mesh_wood.bot" || fail "mesh wood BOT missing"
run "$MGED" -c tree-mesh.g "l test_mesh_leaves.bot" || fail "mesh leaves BOT missing"
run "$MGED" -c tree-mesh.g "l test_mesh_leaves.r" || fail "mesh leaves region missing"
expect_grep "Shader 'stack {{leaf" "$LOGFILE"
run "$MGED" -c tree-mesh.g "attr get test_mesh tree::geometry_mode" || fail "mesh metadata missing"
expect_grep "mesh" "$LOGFILE"
run "$MGED" -c tree-mesh.g "attr get test_mesh tree::solid_volumetric" || fail "solid metadata missing"
expect_grep "1" "$LOGFILE"

run "$TREE" --settings "$1/regress/tree/broadleaf.json" --force --output tree-json.g --name test_json || fail "JSON mesh tree generation failed"
run "$MGED" -c tree-json.g "l test_json_wood.r" || fail "JSON wood region missing"
run "$MGED" -c tree-json.g "attr get test_json tree::settings_json" || fail "settings JSON metadata missing"
expect_grep "broadleaf" "$LOGFILE"

run "$TREE" --settings "$1/regress/tree/conifer.json" --force --output tree-csg.g --name test_csg || fail "CSG tree generation failed"
run "$MGED" -c tree-csg.g "l test_csg_wood.c" || fail "CSG wood comb missing"
run "$MGED" -c tree-csg.g "l test_csg_canopy.s" || fail "CSG canopy solid missing"
run "$MGED" -c tree-csg.g "l test_csg_leaves.r" || fail "CSG leaves region missing"
expect_grep "Shader 'stack {{tree" "$LOGFILE"
run "$MGED" -c tree-csg.g "attr get test_csg tree::geometry_mode" || fail "CSG metadata missing"
expect_grep "csg" "$LOGFILE"

run "$TREE" --force --output tree-append.g --name append_mesh --preset shrub --height 2200 --trunk-radius 90 --leaf-mode simple --detail low || fail "append seed mesh generation failed"
run "$TREE" --append --output tree-append.g --name append_csg --preset dead --mode csg --height 3200 --trunk-radius 120 --leaf-mode none || fail "append CSG generation failed"
run "$MGED" -c tree-append.g "tops -n" || fail "append tops failed"
expect_grep "append_mesh" "$LOGFILE"
expect_grep "append_csg" "$LOGFILE"

if "$TREE" --append --output tree-append.g --name append_mesh --preset broadleaf >> "$LOGFILE" 2>&1 ; then
    fail "duplicate object name in append mode unexpectedly succeeded"
fi

run "$TREE" --demo-file tree-demo.g || fail "demo file generation failed"
run "$MGED" -c tree-demo.g "l all" || fail "demo all comb missing"
run "$MGED" -c tree-demo.g "attr get all tree::demo_tree_count" || fail "demo metadata missing"
expect_grep "10" "$LOGFILE"

log "-> tree.sh succeeded"
exit 0
