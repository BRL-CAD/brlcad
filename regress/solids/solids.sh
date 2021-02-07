#!/bin/sh
#                       S O L I D S . S H
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
    LOGFILE=`pwd`/solids.log
    rm -f $LOGFILE
fi
log "=== TESTING rendering solids ==="

RT="`ensearch rt`"
if test ! -f "$RT" ; then
    log "Unable to find rt, aborting"
    exit 1
fi
MGED="`ensearch mged`"
if test ! -f "$MGED" ; then
    log "Unable to find mged, aborting"
    exit 1
fi
A2P="`ensearch asc2pix`"
if test ! -f "$A2P" ; then
    log "Unable to find asc2pix, aborting"
    exit 1
fi
GENCOLOR="`ensearch gencolor`"
if test ! -f "$GENCOLOR" ; then
    log "Unable to find gencolor, aborting"
    exit 1
fi
PIXDIFF="`ensearch pixdiff`"
if test ! -f "$PIXDIFF" ; then
    log "Unable to find pixdiff, aborting"
    exit 1
fi


# TEST 1 ==========================================================
# first check a simple TGM
# generate TGM from script
log "... running mged to create simple solids (solids.simple.g)"
rm -f solids.simple.g solids.simple.rt
$MGED -c >> $LOGFILE 2>&1 < "$PATH_TO_THIS/solids.simple.mged"

if [ ! -f solids.simple.rt ] ; then
    log "ERROR: mged failed to create solids.simple.rt script"
    log "-> solids.sh FAILED (test 1 of 2), see $LOGFILE"
    cat "$LOGFILE"
    exit 1
fi
mv solids.simple.rt solids.simple.orig.rt

export RT
sed "s,^rt,$RT -B -P 1 ," < solids.simple.orig.rt > solids.simple.rt
rm -f solids.simple.orig.rt
chmod 775 solids.simple.rt

log '... rendering solids'
./solids.simple.rt >> $LOGFILE 2>&1
if [ ! -f solids.simple.rt.pix ] ; then
	log "ERROR: raytrace failed, see $LOGFILE"
	cat "$LOGFILE"
	exit 1
fi
if [ ! -f "$PATH_TO_THIS/solids.simple.ref.pix" ] ; then
	log "ERROR: No reference file for solids.simple.rt.pix"
	exit 1
fi

rm -f solids.simple.pix.diff
$PIXDIFF solids.simple.rt.pix "$PATH_TO_THIS/solids.simple.ref.pix" > solids.simple.pix.diff 2>> $LOGFILE

NUMBER_WRONG=`tail -n1 "$LOGFILE" | tr , '\012' | awk '/many/ {print $1}'`
log "solids.simple.rt.pix $NUMBER_WRONG off by many"

if [ X$NUMBER_WRONG = X0 ] ; then
    log "-> solids.sh succeeded (test 1 of 2)"
else
    log "-> solids.sh FAILED (test 1 of 2), see $LOGFILE"
    cat "$LOGFILE"
fi


# TEST 2 ==========================================================
# then check a complex TGM

# Trim 1025 byte sequence down to exactly 1024.
log "... running $GENCOLOR -r205 0 16 32 64 128 | dd of=solids.ebm.bw bs=1024 count=1"
rm -f solids.ebm.bw
$GENCOLOR -r205 0 16 32 64 128 | dd of=solids.ebm.bw bs=1024 count=1 > $LOGFILE 2>&1
if [ ! -f solids.ebm.bw ] ; then
    log "ERROR: Failed to create file 'solids.ebm.bw'"
    log "-> solids.sh FAILED, see $LOGFILE"
    cat "$LOGFILE"
    exit 1
fi

# generate required pix file
log "... running $A2P < $PATH_TO_THIS/solids.dsp.dat > solids.dsp.pix"
rm -f solids.dsp.pix
$A2P < "$PATH_TO_THIS/solids.dsp.dat" > solids.dsp.pix
if [ ! -f solids.dsp.pix ] ; then
    log "ERROR: Failed to generate file 'solids.dsp.pix'"
    log "-> solids.sh FAILED (test 2 of 2), see $LOGFILE"
    cat "$LOGFILE"
    exit 1
fi

# generate TGM from script
log "... generating solids geometry file (solids.g)"
rm -f solids.g solids.rt
$MGED -c >> $LOGFILE 2>&1 < "$PATH_TO_THIS/solids.mged"

if [ ! -f solids.rt ] ; then
    log "ERROR: mged failed to create solids.rt script"
    log "-> solids.sh FAILED (test 2 of 2), see $LOGFILE"
    cat "$LOGFILE"
    exit 1
fi
run mv solids.rt solids.orig.rt
sed "s,^rt,$RT -B -P 1 ," < solids.orig.rt > solids.rt
run rm -f solids.orig.rt
run chmod 775 solids.rt

log '... rendering solids'
rm -f solids.rt.pix solids.rt.log
run ./solids.rt
if [ ! -f solids.rt.pix ] ; then
	log "ERROR: solids raytrace failed, see $LOGFILE"
	cat "$LOGFILE"
	exit 1
fi
if [ ! -f "$PATH_TO_THIS/solids.ref.pix" ] ; then
	log "ERROR: No reference file for solids.rt.pix"
	exit 1
fi

log "... running $PIXDIFF solids.rt.pix $PATH_TO_THIS/solids.ref.pix > solids.pix.diff"
rm -f solids.pix.diff
$PIXDIFF solids.rt.pix "$PATH_TO_THIS/solids.ref.pix" > solids.pix.diff 2> $LOGFILE

NUMBER_WRONG=`tail -n1 "$LOGFILE" | tr , '\012' | awk '/many/ {print $1}'`
log "solids.rt.pix $NUMBER_WRONG off by many"


if [ X$NUMBER_WRONG = X0 ] ; then
    log "-> solids.sh succeeded (test 2 of 2)"
else
    log "-> solids.sh FAILED (test 2 of 2), see $LOGFILE"
    cat "$LOGFILE"
fi

exit $NUMBER_WRONG

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
