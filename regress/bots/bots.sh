#!/bin/sh
#                         B O T S . S H
# BRL-CAD
#
# Copyright (c) 2008-2022 United States Government as represented by
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
#
# Basic series of BoT sanity checks
#
###

# Ensure /bin/sh
export PATH || (echo "This isn't sh."; sh $0 $*; kill $$)

# source common library functionality, setting ARGS, NAME_OF_THIS,
# PATH_TO_THIS, and THIS.
. "$1/regress/library.sh"

if test "x$LOGFILE" = "x" ; then
    LOGFILE=`pwd`/bots.log
    rm -f $LOGFILE
fi
log "=== TESTING BoT primitive ==="

MGED="`ensearch mged`"
if test ! -f "$MGED" ; then
    log "Unable to find MGED, aborting"
    exit 1
fi
RT="`ensearch rt`"
if test ! -f "$RT" ; then
    log "Unable to find RT, aborting"
    exit 1
fi
PIXDIFF="`ensearch pixdiff`"
if test ! -f "$PIXDIFF" ; then
    log "Unable to find pixdiff, aborting"
    exit 1
fi

log "creating a geometry database (bots.g) with a BoT of each type"
rm -f bots.g
$MGED -c >> $LOGFILE 2>&1 <<EOF
opendb bots.g y
make sph sph
facetize sph.volume.rh.bot sph
cp sph.volume.rh.bot sph.volume.lh.bot
bot_flip sph.volume.lh.bot
bot_merge sph.volume.no.bot sph.volume.rh.bot
bot_vertex_fuse sph.vertex.fused.volume.no.bot sph.volume.no.bot
bot_face_fuse sph.face.fused.volume.no.bot sph.vertex.fused.volume.no.bot
cp sph.face.fused.volume.no.bot sph.sync.volume.no.bot
bot_sync sph.sync.volume.no.bot
EOF

# begin validation checks

FAILED=0

log "Getting BoT modes"

rh_mode="`$MGED -c bots.g get sph.volume.rh.bot mode 2>&1 | grep -v Using`"
lh_mode="`$MGED -c bots.g get sph.volume.lh.bot mode 2>&1 | grep -v Using`"
no_mode="`$MGED -c bots.g get sph.volume.no.bot mode 2>&1 | grep -v Using`"
sync_mode="`$MGED -c bots.g get sph.sync.volume.no.bot mode 2>&1 | grep -v Using`"

# the echo is to remove a newline that gets stored in the variable
if test "x`echo $rh_mode``echo $lh_mode``echo $no_mode``echo $sync_mode`" != "xvolumevolumevolumevolume" ; then
    log "ERROR: volume BoT mode failure"
    FAILED="`expr $FAILED + 1`"
fi

log "Getting BoT orientations"

rh="`$MGED -c bots.g get sph.volume.rh.bot orient 2>&1 | grep -v Using`"
if test "x`echo $rh`" != "xrh" ; then
    log "ERROR: right-hand BoT orientation (facetize) failure [$rh]"
    FAILED="`expr $FAILED + 1`"
fi
lh="`$MGED -c bots.g get sph.volume.lh.bot orient 2>&1 | grep -v Using`"
if test "x`echo $lh`" != "xlh" ; then
    log "ERROR: left-hand BoT orientation (bot_flip) failure [$lh]"
    FAILED="`expr $FAILED + 1`"
fi
no="`$MGED -c bots.g get sph.volume.no.bot orient 2>&1 | grep -v Using`"
if test "x`echo $no`" != "xno" ; then
    log "ERROR: merged BoT orientation (bot_merge) failure [$no]"
    FAILED="`expr $FAILED + 1`"
fi
sync="`$MGED -c bots.g get sph.sync.volume.no.bot orient 2>&1 | grep -v Using`"
if test "x`echo $no`" != "xno" ; then
    log "ERROR: synced BoT orientation (bot_sync) failure [$no]"
    FAILED="`expr $FAILED + 1`"
fi

# see if the merged/fused/synced bot is the same as the
# original.. this may need to be relaxed if there are floating point
# sensitivity problems.
unfused_vertices="`$MGED -c bots.g get sph.volume.rh.bot V 2>&1`"
refused_vertices="`$MGED -c bots.g get sph.sync.volume.no.bot V 2>&1`"
if test "x$unfused_vertices" != "x$refused_vertices" ; then
    log "ERROR: BoT fuse (bot_vertex_fuse+bot_face_fuse) failure"
    FAILED="`expr $FAILED + 1`"
fi

# exit early if they fail so we don't attempt to render
if test $FAILED -ne 0 ; then
    log "-> BoT check FAILED"
    exit $FAILED
fi

# now test each to make sure they render identical

# BASE REFERENCE
log "... rendering implicit sphere"
rm -f bots.sph.pix
run $RT -s128 -o bots.sph.pix bots.g sph
if [ ! -f bots.sph.pix ] ; then
    log "ERROR: raytrace failure"
    exit 1
fi

# RIGHT-HANDED
log "Rendering right-handed volume BoT sphere"
rm -f bots.rh.pix
run $RT -s128 -o bots.rh.pix bots.g sph.volume.rh.bot
if [ ! -f bots.rh.pix ] ; then
    log "ERROR: raytrace failure"
    exit 1
fi
# compare
rm -f bots.diff.pix
$PIXDIFF bots.sph.pix bots.rh.pix > bots.diff.pix 2>> bots.diff.log
NUMBER_WRONG=`tail -n 1 bots.diff.log | tr , '\012' | awk '/many/ {print $1}'`
if [ $NUMBER_WRONG -eq 0 ] ; then
    # we expect implicit and BoT sphere to be different
    log "ERROR: bots.diff.pix $NUMBER_WRONG off by many (this is WRONG)"
    FAILED="`expr $FAILED + 1`"
fi

# LEFT-HANDED
log "Rendering left-handed volume BoT sphere"
rm -f bots.lh.pix
run $RT -s128 -o bots.lh.pix bots.g sph.volume.lh.bot
if [ ! -f bots.lh.pix ] ; then
    log "ERROR: raytrace failure"
    exit 1
fi
# compare
rm -f bots.rl.diff.pix
$PIXDIFF bots.rh.pix bots.lh.pix > bots.rl.diff.pix 2>> bots.diff.log
NUMBER_WRONG=`tail -n 1 bots.diff.log | tr , '\012' | awk '/many/ {print $1}'`
if [ $NUMBER_WRONG -eq 0 ] ; then
    tail -n 1 bots.diff.log
else
    log "ERROR: bots.rl.diff.pix $NUMBER_WRONG off by many"
    FAILED="`expr $FAILED + 1`"
fi

# UNORIENTED
log "Rendering unoriented volume BoT sphere"
rm -f bots.no.pix
run $RT -s128 -o bots.no.pix bots.g sph.volume.no.bot
if [ ! -f bots.no.pix ] ; then
    log "ERROR: raytrace failure"
    exit 1
fi
# compare
rm -f bots.rn.diff.pix
$PIXDIFF bots.rh.pix bots.no.pix > bots.rn.diff.pix 2>> bots.diff.log
NUMBER_WRONG=`tail -n 1 bots.diff.log | tr , '\012' | awk '/many/ {print $1}'`
if [ $NUMBER_WRONG -eq 0 ] ; then
    tail -n 1 bots.diff.log
else
    log "ERROR: bots.rn.diff.pix $NUMBER_WRONG off by many"
    FAILED="`expr $FAILED + 1`"
fi

# SYNC'D UNORIENTED
log "Rendering synced unoriented volume BoT sphere"
rm -f bots.sync.pix
run $RT -s128 -o bots.sync.pix bots.g sph.sync.volume.no.bot
if [ ! -f bots.sync.pix ] ; then
    log "ERROR: raytrace failure"
    exit 1
fi
# compare
rm -f bots.rs.diff.pix
$PIXDIFF bots.rh.pix bots.sync.pix > bots.rs.diff.pix 2>> bots.diff.log
NUMBER_WRONG=`tail -n 1 bots.diff.log | tr , '\012' | awk '/many/ {print $1}'`
if [ $NUMBER_WRONG -eq 0 ] ; then
    log `tail -n 1 bots.diff.log`
else
    log "ERROR: bots.rs.diff.pix $NUMBER_WRONG off by many"
    FAILED="`expr $FAILED + 1`"
fi


if test $FAILED -eq 0 ; then
    log "-> BoT check succeeded"
else
    log "-> BoT check FAILED, see $LOGFILE"
    cat "$LOGFILE"
fi

exit $FAILED

# Local Variables:
# tab-width: 8
# mode: sh
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
