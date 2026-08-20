#!/bin/sh
#                      D A T U M . S H
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
#
# Exercise the ASCII export/import round-trip for datum reference objects
# (g2asc -> asc2g).  Datum objects carry munition position/orientation
# reference geometry (point/line/plane sub-objects); this test verifies that
# such geometry survives a lossless g -> asc -> g conversion, and that a comb
# carrying a datum with an attribute (the attr->datum->data lookup path)
# round-trips unchanged.

# Ensure /bin/sh
export PATH || (echo "This isn't sh."; sh $0 $*; kill $$)

# source common library functionality, setting ARGS, NAME_OF_THIS,
# PATH_TO_THIS, and THIS.
. "$1/regress/library.sh"

if test "x$LOGFILE" = "x" ; then
    LOGFILE=`pwd`/datum.log
    rm -f $LOGFILE
fi
log "=== TESTING datum asc round-trip ==="

MGED="`ensearch mged`"
if test ! -f "$MGED" ; then
    log "Unable to find mged, aborting"
    exit 1
fi

G2A="`ensearch g2asc`"
if test ! -f "$G2A" ; then
    log "Unable to find g2asc, aborting"
    exit 1
fi

A2G="`ensearch asc2g`"
if test ! -f "$A2G" ; then
    log "Unable to find asc2g, aborting"
    exit 1
fi

GD="`ensearch gdiff`"
if test ! -f "$GD" ; then
    log "Unable to find gdiff, aborting"
    exit 1
fi

# Build the source .g containing datum objects of every sub-type plus a
# multi-sub-object datum and a comb carrying a datum via an attribute.
SRC="datum_src.g"
rm -f "$SRC"
log "creating datum objects in $SRC"

$MGED -c "$SRC" >> $LOGFILE 2>&1 <<EOF
in dpoint.s datum point 10 20 30
in dline.s datum line 0 0 0 1 0 0
in dplane.s datum plane 5 5 5 0 0 1 2
in dmulti.s datum point 1 2 3 line 4 5 6 7 8 9 plane 0 0 0 0 0 1 3
g object.c dmulti.s
attr set object.c datum dmulti.s
EOF

if test ! -f "$SRC" ; then
    log "-> failed to create $SRC, see $LOGFILE"
    cat "$LOGFILE"
    exit 1
fi

# Confirm the datum objects were actually created (mged headless can be
# quiet about failures; make the failure explicit here).
log "verifying datum objects exist in $SRC"
$MGED -c "$SRC" l dmulti.s >> $LOGFILE 2>&1
STATUS=$?
if [ $STATUS -gt 0 ] ; then
    log "-> datum object dmulti.s missing from $SRC, see $LOGFILE"
    cat "$LOGFILE"
    exit 1
fi

# Export to ASCII (g2asc uses rt_datum_get).
ASC="datum.asc"
rm -f "$ASC"
log "$G2A $SRC $ASC"
$G2A "$SRC" "$ASC" >> $LOGFILE 2>&1
STATUS=$?
if [ $STATUS -gt 0 ] ; then
    log "-> g2asc of datum objects FAILED, see $LOGFILE"
    cat "$LOGFILE"
    exit $STATUS
fi

# Guard against the stale-TODO regression: g2asc must NOT emit the
# "not yet been implemented" placeholder for datum objects.
if grep -q "has not yet been implemented" "$ASC" ; then
    log "-> g2asc emitted unimplemented-Tcl-output placeholder for a datum, see $ASC"
    cat "$ASC" >> $LOGFILE
    cat "$LOGFILE"
    exit 1
fi

# Import back (asc2g routes 'put' to ged_put_core -> rt_datum_adjust).
RT="datum_rt.g"
rm -f "$RT"
log "$A2G $ASC $RT"
$A2G "$ASC" "$RT" >> $LOGFILE 2>&1
STATUS=$?
if [ $STATUS -gt 0 ] ; then
    log "-> asc2g of datum objects FAILED, see $LOGFILE"
    cat "$LOGFILE"
    exit $STATUS
fi

# The original and round-tripped .g files should be identical.
log "$GD -v $SRC $RT"
$GD -v "$SRC" "$RT" >> $LOGFILE 2>&1
STATUS=$?
if [ $STATUS -gt 0 ] ; then
    log "-> datum g->asc->g round-trip FAILED, see $LOGFILE"
    cat "$LOGFILE"
    exit $STATUS
fi

# Confirm the comb still carries the datum attribute after the round-trip,
# demonstrating the attr->datum->data lookup path survives conversion.
log "verifying datum attribute survived on object.c"
$MGED -c "$RT" attr get object.c datum >> $LOGFILE 2>&1
STATUS=$?
if [ $STATUS -gt 0 ] ; then
    log "-> datum attribute lost after round-trip, see $LOGFILE"
    cat "$LOGFILE"
    exit $STATUS
fi

log "-> datum.sh succeeded"
exit 0

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
