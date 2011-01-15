#                        B O T S . S H
# BRL-CAD
#
# Copyright (c) 2008-2011 United States Government as represented by
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
. $1/regress/library.sh

MGED="`ensearch mged`"
if test ! -f "$MGED" ; then
    echo "Unable to find MGED, aborting"
    exit 1
fi
RT="`ensearch rt`"
if test ! -f "$RT" ; then
    echo "Unable to find RT, aborting"
    exit 1
fi
PIXDIFF="`ensearch pixdiff`"
if test ! -f "$PIXDIFF" ; then
    echo "Unable to find pixdiff, aborting"
    exit 1
fi

echo "testing BoT primitive..."

rm -f bots.g bots.log bots.sph.pix bots.rh.pix bots.lh.pix bots.no.pix bots.sync.pix bots.diff.pix bots.rl.diff.pix bots.rn.diff.pix bots.rs.diff.pix bots.diff.log

# create a geometry database with a BoT of each type

$MGED -c > bots.log 2>&1 <<EOF
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

echo "Getting BoT modes"

rh_mode="`$MGED -c bots.g get sph.volume.rh.bot mode 2>&1 | grep -v Using`"
lh_mode="`$MGED -c bots.g get sph.volume.lh.bot mode 2>&1 | grep -v Using`"
no_mode="`$MGED -c bots.g get sph.volume.no.bot mode 2>&1 | grep -v Using`"
sync_mode="`$MGED -c bots.g get sph.sync.volume.no.bot mode 2>&1 | grep -v Using`"

# the echo is to remove a newline that gets stored in the variable
if test "x`echo $rh_mode``echo $lh_mode``echo $no_mode``echo $sync_mode`" != "xvolumevolumevolumevolume" ; then
    echo "ERROR: volume BoT mode failure"
    FAILED="`expr $FAILED + 1`"
fi

echo "Getting BoT orientations"

rh="`$MGED -c bots.g get sph.volume.rh.bot orient 2>&1 | grep -v Using`"
if test "x`echo $rh`" != "xrh" ; then
    echo "ERROR: right-hand BoT orientation (faceitize) failure [$rh]"
    FAILED="`expr $FAILED + 1`"
fi
lh="`$MGED -c bots.g get sph.volume.lh.bot orient 2>&1 | grep -v Using`"
if test "x`echo $lh`" != "xlh" ; then
    echo "ERROR: left-hand BoT orientation (bot_flip) failure [$lh]"
    FAILED="`expr $FAILED + 1`"
fi
no="`$MGED -c bots.g get sph.volume.no.bot orient 2>&1 | grep -v Using`"
if test "x`echo $no`" != "xno" ; then
    echo "ERROR: merged BoT orientation (bot_merge) failure [$no]"
    FAILED="`expr $FAILED + 1`"
fi
sync="`$MGED -c bots.g get sph.sync.volume.no.bot orient 2>&1 | grep -v Using`"
if test "x`echo $no`" != "xno" ; then
    echo "ERROR: synced BoT orientation (bot_sync) failure [$no]"
    FAILED="`expr $FAILED + 1`"
fi

# see if the merged/fused/synced bot is the same as the
# original.. this may need to be relaxed if there are floating point
# sensitivity problems.
unfused_vertices="`$MGED -c bots.g get sph.volume.rh.bot V 2>&1`"
refused_vertices="`$MGED -c bots.g get sph.sync.volume.no.bot V 2>&1`"
if test "x$unfused_vertices" != "x$refused_vertices" ; then
    echo "ERROR: BoT fuse (bot_vertex_fuse+bot_face_fuse) failure"
    FAILED="`expr $FAILED + 1`"
fi

# exit early if they fail so we don't attempt to render
if test $FAILED -ne 0 ; then
    echo "-> BoT check FAILED"
    exit $FAILED
fi

# now make sure they render identical

echo "Rendering implicit sphere"
$RT -s128 -o bots.sph.pix bots.g sph >> bots.log 2>&1
if [ ! -f bots.sph.pix ] ; then
    echo "ERROR: raytrace failure"
    exit 1
fi

echo "Rendering right-handed volume BoT sphere"
$RT -s128 -o bots.rh.pix bots.g sph.volume.rh.bot >> bots.log 2>&1
if [ ! -f bots.rh.pix ] ; then
    echo "ERROR: raytrace failure"
    exit 1
fi

echo "Rendering left-handed volume BoT sphere"
$RT -s128 -o bots.lh.pix bots.g sph.volume.lh.bot >> bots.log 2>&1
if [ ! -f bots.lh.pix ] ; then
    echo "ERROR: raytrace failure"
    exit 1
fi

echo "Rendering unoriented volume BoT sphere"
$RT -s128 -o bots.no.pix bots.g sph.volume.no.bot >> bots.log 2>&1
if [ ! -f bots.no.pix ] ; then
    echo "ERROR: raytrace failure"
    exit 1
fi

echo "Rendering synced unoriented volume BoT sphere"
$RT -s128 -o bots.sync.pix bots.g sph.sync.volume.no.bot >> bots.log 2>&1
if [ ! -f bots.sync.pix ] ; then
    echo "ERROR: raytrace failure"
    exit 1
fi

# compare

$PIXDIFF bots.sph.pix bots.rh.pix > bots.diff.pix 2>> bots.diff.log
NUMBER_WRONG=`tail -n 1 bots.diff.log | tr , '\012' | awk '/many/ {print $1}'`
if [ $NUMBER_WRONG -eq 0 ] ; then
    # we expect implicit and BoT sphere to be different
    echo "ERROR: bots.diff.pix $NUMBER_WRONG off by many (this is WRONG)"
    FAILED="`expr $FAILED + 1`"
fi

$PIXDIFF bots.rh.pix bots.lh.pix > bots.rl.diff.pix 2>> bots.diff.log
NUMBER_WRONG=`tail -n 1 bots.diff.log | tr , '\012' | awk '/many/ {print $1}'`
if [ $NUMBER_WRONG -eq 0 ] ; then
    tail -n 1 bots.diff.log
else
    echo "ERROR: bots.rl.diff.pix $NUMBER_WRONG off by many"
    FAILED="`expr $FAILED + 1`"
fi

$PIXDIFF bots.rh.pix bots.no.pix > bots.rn.diff.pix 2>> bots.diff.log
NUMBER_WRONG=`tail -n 1 bots.diff.log | tr , '\012' | awk '/many/ {print $1}'`
if [ $NUMBER_WRONG -eq 0 ] ; then
    tail -n 1 bots.diff.log
else
    echo "ERROR: bots.rn.diff.pix $NUMBER_WRONG off by many"
    FAILED="`expr $FAILED + 1`"
fi

$PIXDIFF bots.rh.pix bots.sync.pix > bots.rs.diff.pix 2>> bots.diff.log
NUMBER_WRONG=`tail -n 1 bots.diff.log | tr , '\012' | awk '/many/ {print $1}'`
if [ $NUMBER_WRONG -eq 0 ] ; then
    tail -n 1 bots.diff.log
else
    echo "ERROR: bots.rs.diff.pix $NUMBER_WRONG off by many"
    FAILED="`expr $FAILED + 1`"
fi


if test $FAILED -eq 0 ; then
    echo "-> BoT check succeeded"
else
    echo "-> BoT check FAILED"
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
