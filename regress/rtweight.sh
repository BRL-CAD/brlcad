#!/bin/sh
#                         R T W E I G H T . S H
# BRL-CAD
#
# Copyright (c) 2021-2022 United States Government as represented by
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
    LOGFILE=`pwd`/rtweight_output.log
    LOGFILE2=`pwd`/rtweight_output.log
    rm -f $LOGFILE
    rm -f $LOGFILE2
fi
log "=== TESTING 'rtweight' ==="

MGED="`ensearch mged`"
if test ! -f "$MGED" ; then
    log "Unable to find mged, aborting"
    exit 1
fi

RTWEIGHT="`ensearch rtweight`"
if test ! -f "$RTWEIGHT" ; then
    log "Unable to find rtweight, aborting"
    exit 1
fi

rm -f `pwd`/test.g
# manually set status before running commands
STATUS=0

# Test 1: Make Sure RTWEIGHT imports and uses material objects
log "... making sure RTWEIGHT imports and uses material objects"
$MGED -c test.g >> $LOGFILE 2>&1 << EOF
make rpp rpp
r rpp.r u rpp
material import --name ../misc/GQA_SAMPLE_DENSITIES
material assign rpp.r "Carbon Tool Steel"
EOF

$RTWEIGHT -o $LOGFILE test.g rpp.r

if ! grep -q 'Total mass = 7819.98 kg' $LOGFILE; then STATUS=1; fi

rm -f `pwd`/test.g

# Test 2: rainy day, mged fails
log "... rainy day - mged fails"
$MGED -c test.g >> $LOGFILE 2>&1 << EOF
make rpp rpp
r rpp.r u rpp
material import --name .density55
material assign rpp.r "Carbon Tool Steel"
EOF

$RTWEIGHT -o $LOGFILE2 test.g rpp.r

if grep -q 'Total mass = 7819.98 kg' $LOGFILE2; then STATUS=1; fi
cat $LOGFILE2 >> $LOGFILE

if [ X$STATUS = X0 ] ; then
    log "-> rt-weight.sh succeeded"
else
    log "-> rt-weight.sh FAILED, see $LOGFILE"
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
