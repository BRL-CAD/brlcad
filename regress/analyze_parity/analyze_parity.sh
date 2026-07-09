#!/bin/sh
#               A N A L Y Z E _ P A R I T Y . S H
# BRL-CAD
#
# Copyright (c) 2025 United States Government as represented by
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
# disclaimer in the documentation or other materials provided
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
#
# Cross-tool parity test: verify that 'check overlaps' (GED plugin,
# libanalyze path) and 'gqa -Ao' (standalone, libanalyze path) find
# the same overlap pairs for a known geometry.
#
###

# Ensure /bin/sh
export PATH || (echo "This isn't sh."; sh $0 $*; kill $$)

# Source common library functionality.
. "$1/regress/library.sh"

if test "x$LOGFILE" = "x" ; then
    LOGFILE=`pwd`/analyze_parity.log
    rm -f $LOGFILE
fi
log "=== TESTING analyze parity (check vs gqa) ==="

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

# ---- Build geometry ---------------------------------------------------
# Two overlapping boxes: box_outer fully contains box_inner, plus box_clip
# that overlaps box_outer laterally so there is one deterministic overlap
# pair (outer vs clip).

rm -f parity.mged parity.g
cat > parity.mged <<'MGED_EOF'
units mm
in outer.s rpp 0 100 0 100 0 100
in clip.s  rpp 50 150 0 100 0 100
r outer.r u outer.s
r clip.r  u clip.s
g parity outer.r clip.r
q
MGED_EOF

log "... creating parity.g"
$MGED -c parity.g < parity.mged >> $LOGFILE 2>&1
STATUS=$?
if test $STATUS -ne 0 ; then
    log "FAILED: mged exited with status $STATUS while building parity.g"
    exit 1
fi
if test ! -f parity.g ; then
    log "FAILED: parity.g was not created"
    exit 1
fi

# ---- gqa overlap pass -------------------------------------------------
log "... running gqa -Ao parity.g parity"
GQA_OUT=`$GQABIN -Ao -q parity.g parity 2>&1`
log "gqa output:"
log "$GQA_OUT"

# gqa prints pairs like: <outer.r, clip.r>: N overlap ...
GQA_PAIR=`echo "$GQA_OUT" | grep -o 'outer\.r.*clip\.r\|clip\.r.*outer\.r' | head -1`
if test "x$GQA_PAIR" = "x" ; then
    log "FAILED: gqa did not report the expected overlap between outer.r and clip.r"
    log "(gqa output was: $GQA_OUT)"
    exit 1
fi
log "gqa found overlap pair: $GQA_PAIR"

# ---- check overlaps pass ----------------------------------------------
log "... running mged check overlaps on parity.g parity"
CHECK_OUT=`$MGED -c parity.g check overlaps -q parity 2>&1`
log "check output:"
log "$CHECK_OUT"

# 'check overlaps' prints pairs like: outer.r and clip.r overlap
CHECK_PAIR=`echo "$CHECK_OUT" | grep -o 'outer\.r.*clip\.r\|clip\.r.*outer\.r' | head -1`
if test "x$CHECK_PAIR" = "x" ; then
    log "FAILED: check overlaps did not report the expected overlap between outer.r and clip.r"
    log "(check output was: $CHECK_OUT)"
    exit 1
fi
log "check found overlap pair: $CHECK_PAIR"

# ---- parity check -----------------------------------------------------
# Both tools should find the same two region names (order may differ).
GQA_HAS_OUTER=`echo "$GQA_OUT"   | grep -c 'outer\.r'`
GQA_HAS_CLIP=`echo "$GQA_OUT"    | grep -c 'clip\.r'`
CHECK_HAS_OUTER=`echo "$CHECK_OUT" | grep -c 'outer\.r'`
CHECK_HAS_CLIP=`echo "$CHECK_OUT"  | grep -c 'clip\.r'`

if test "$GQA_HAS_OUTER" -eq 0 || test "$GQA_HAS_CLIP" -eq 0 ; then
    log "FAILED: gqa did not mention both outer.r and clip.r in overlap output"
    exit 1
fi
if test "$CHECK_HAS_OUTER" -eq 0 || test "$CHECK_HAS_CLIP" -eq 0 ; then
    log "FAILED: check overlaps did not mention both outer.r and clip.r in overlap output"
    exit 1
fi

log "PASS: both gqa -Ao and check overlaps agree on the overlap pair outer.r / clip.r"

# ---- Cleanup ----------------------------------------------------------
rm -f parity.g parity.mged

if [ $STATUS = 0 ] ; then
    log "-> analyze parity test PASSED"
else
    log "-> analyze parity test FAILED"
fi

exit $STATUS

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# End:
# ex: shiftwidth=4 tabstop=8
