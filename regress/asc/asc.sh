#!/bin/sh
#                        A S C . S H
# BRL-CAD
#
# Copyright (c) 2010-2021 United States Government as represented by
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
    LOGFILE=`pwd`/asc.log
    rm -f $LOGFILE
fi
log "=== TESTING asc format conversion ==="

A2G="`ensearch asc2g`"
if test ! -f "$A2G" ; then
    log "Unable to find asc2g, aborting"
    exit 1
fi

G2A="`ensearch g2asc`"
if test ! -f "$G2A" ; then
    log "Unable to find g2asc, aborting"
    exit 1
fi

GD="`ensearch gdiff`"
if test ! -f "$GD" ; then
    log "Unable to find gdiff, aborting"
    exit 1
fi

DU="`ensearch dbupgrade`"
if test ! -f "$DU" ; then
    log "Unable to find dbupgrade, aborting"
    exit 1
fi

# Start by converting v4 moss.asc to a v5 .g file
log "... convert asc file to v5 .g format"
ASC1="$1/db/moss.asc"
G1="moss_v5.g"
rm -f $G1
$A2G "$ASC1" $G1 2>&1 > $LOGFILE

# Next convert v5 .g file to a v5 asc file
log "convert v5 .g file to v5 asc format"
ASC2="moss_v5.asc"
rm -f $ASC2
log "$G2A $G1 $ASC2"
$G2A $G1 $ASC2 2>&1 > $LOGFILE

# convert v5 .g file to a v5 .g file
log "convert v5 asc file to v5 .g format (simple round trip)"
GRT="moss_v5_basic.g"
rm -f $GRT
log "$A2G $ASC2 $GRT"
$A2G "$ASC2" $GRT 2>&1 > $LOGFILE

# the original v5 .g file and the round-tripped v5 .g file
# should be identical (apparently except for the color table)
log "$GD -v $G1 $GRT -F \"! -attr regionid_colortable\""
$GD -v -F "! -attr regionid_colortable" $G1 $GRT 2>&1 > $LOGFILE
STATUS=$?

# If that didn't work, don't bother going further - we've got a problem.
if [ $STATUS -gt 0 ] ; then
    log "-> basic v5 asc.sh round-trip FAILED, see $LOGFILE"
    cat "$LOGFILE"
    exit $STATUS
fi

# If the basics work, check v4 and dbupgrade

# Use the undocumented dbupgrade -r to downgrade v5 .g file to a v4 .g file.
# Not something the user should be doing, hence it remaining deliberately
# undocumented, but we need it here to get a binary v4 file for testing.
# asc2g can't write out a v4 asc file from a v5 database, so there's no
# other way to exercise that code path.
log "convert v5 .g file to v4 .g format"
G2="moss_v4.g"
rm -f $G2
log "$DU -r $G1 $G2"
$DU -r $G1 $G2 2>&1 > $LOGFILE

# convert v4 .g file to a v4 asc file
log "convert v4 .g file to v4 asc format"
ASC3="moss_v4.asc"
rm -f $ASC3
log "$G2A $G2 $ASC3"
$G2A $G2 $ASC3 2>&1 > $LOGFILE

# convert v4 asc to v5 .g file

log "convert v4 asc file to v5 g format"
G3="moss_v4_asc_v5.g"
rm -f $G3
log "$A2G $ASC3 $G3"
$A2G $ASC3 $G3 2>&1 > $LOGFILE

# Next, do the downgrade/upgrade cycle on the .g file to test dbupgrade's
# behavior on a binary v4 .g.
G4="moss_v4_asc_v4.g"
G5="moss_v4_asc_v4_v5.g"
rm -f $G4 $G5
log "$DU -r $G3 $G4"
$DU -r $G3 $G4 2>&1 > $LOGFILE
log "$DU $G4 $G5"
$DU $G4 $G5 2>&1 > $LOGFILE

# Rather surprisingly, all.g doesn't pass this comparison... not sure
# why yet...
#
#log "$GD -v -t 0.0001 -F \"! -attr regionid_colortable\" $G3 $G5"
#$GD -v -t 0.0001 -F "! -attr regionid_colortable" $G3 $G5 2>&1 > $LOGFILE
#STATUS=$?
# binary dbupdate should't have significantly altered the file.
#if [ $STATUS -gt 0 ] ; then
#    log "-> dbupgrade cycle FAILED, see $LOGFILE"
#    cat "$LOGFILE"
#    exit $STATUS
#fi

# After all that, the original v5 .g file and the file produced at the end of
# all the processing should be identical
#
# (Well, unchanged sans a region color table, region_id=-1 on all.g and
# numerical differences anyway... Accomidate data changes with gdiff
# filtering.)
log "$GD -v -t 0.0001 -F \"! -name _GLOBAL ! -attr region_id=-1\" $G1 $G5"
$GD -v -t 0.0001 -F "! -name _GLOBAL ! -attr region_id=-1" $G1 $G5 2>&1 > $LOGFILE
STATUS=$?

if [ $STATUS -gt 0 ] ; then
    log "-> asc.sh FAILED, see $LOGFILE"
    cat "$LOGFILE"
    exit $STATUS
else
    log "-> asc.sh succeeded"
fi

# Run a test database with better object coverage through g2asc.
# Unfortunately, at the moment it looks like this won't round
# trip.
#
G2ASC1="$1/regress/asc/v4.g"
log "$G2A $G2ASC1 v4.asc"
$G2A "$G2ASC1" v4.asc 2>&1 > $LOGFILE
STATUS=$?
# If something went wrong, bail.
if [ $STATUS -gt 0 ] ; then
    log "-> g2asc broad object coverage check FAILED, see $LOGFILE"
    cat "$LOGFILE"
    exit $STATUS
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
