#!/bin/sh
#                       W E I G H T . S H
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
    LOGFILE=`pwd`/weight.log
    rm -f $LOGFILE
fi
log "=== TESTING rtweight ==="

MGED="`ensearch mged`"
if test ! -f "$MGED" ; then
    echo "Unable to find mged, aborting"
    exit 1
fi
RTWEIGHT="`ensearch rtweight`"
if test ! -f "$RTWEIGHT" ; then
    echo "Unable to find rtweight, aborting"
    exit 1
fi

# FIRST TEST =================================

rm -f weight.mged weight.g
cat > weight.mged <<EOF
opendb weight.g y
units cm
in box rpp 0 1 0 1 0 1
r box.r u box
EOF

log "... creating rtweight geometry database (weight.g)"
$MGED -c > weight.log 2>&1 << EOF
`cat weight.mged`
EOF

rm -f .density
cat > .density <<EOF
1 7.8295        steel
EOF

rm -f weight.out
run $RTWEIGHT -a 25 -e 35 -s128 -o weight.out weight.g box.r

# eliminate the time stamp lines which are obviously different and
# the file path which is not germane to the test
tr -d ' \t' < "$PATH_TO_THIS/weight.ref" | grep -v DensityTableUsed | grep -v TimeStamp > weight.ref_ns
tr -d ' \t' < weight.out | grep -v DensityTableUsed | grep -v TimeStamp > weight.out_ns

run cmp weight.ref_ns weight.out_ns
STATUS=$?

if [ X$STATUS != X0 ] ; then
    echo "ERROR: rtweight results differ $STATUS"
else
    echo "-> weight.sh succeeded (1 of 2)"
fi

# SECOND TEST =================================

# Need to do a slightly more elaborate test
# save first density file in case we fail here

rm -f weight.test2.mged weight.test2.g
cat > weight.test2.mged <<EOF
opendb weight.test2.g y
units cm
in box1 rpp 0 1 0 1 0 1
in box2 rpp 2 3 2 3 2 3
in box3 rpp 4 5 4 5 4 5
r box1.r u box1
r box2.r u box2
r box3.r u box3
attr set box1.r material_id 2
attr set box2.r material_id 7
attr set box3.r material_id 12
attr set box1.r region_id 1000
attr set box2.r region_id 1000
attr set box3.r region_id 1010
g boxes box1.r box2.r box3.r
EOF

log "... creating second geometry database (weight.test2.g)"
$MGED -c > $LOGFILE 2>&1 << EOF
`cat weight.test2.mged`
EOF

# test handling of a more complex density file
if test -f .density ; then
    mv .density .density.weight1
fi
cat > .density <<EOF
#  Test density file with comments and bad input
2    7.82      Carbon Tool Steel
3    2.7       Aluminum, 6061-T6
4    2.74      Aluminum, 7079-T6
#  Incomplete line following
	       Copper, pure
6    19.32     Gold, pure
7    8.03      Stainless, 18Cr-8Ni
#  Comment
8    7.47      Stainless 27Cr

9    7.715     Steel, tool

#  Blank line above
#  Comment following valid data on the line below
10   7.84      Carbon Steel # used for widgets
12   3.00      Gunner
14   10.00     Fuel
#  Material ID too high (MAXMTLS = 32768)
99999 70.84    Kryptonite
EOF

run $RTWEIGHT -a 25 -e 35 -s128 -o weight.test2.out weight.test2.g boxes

# eliminate the time stamp lines which are obviously different and
# the file path which is not germane to the test
tr -d ' \t\r' < "$PATH_TO_THIS/weight.test2.ref" | grep -v DensityTableUsed | grep -v TimeStamp > weight.test2.ref_ns
tr -d ' \t\r' < weight.test2.out | grep -v DensityTableUsed | grep -v TimeStamp > weight.test2.out_ns

run cmp weight.test2.ref_ns weight.test2.out_ns
STATUS=$?

if [ X$STATUS != X0 ] ; then
    log "rtweight results differ $STATUS"
fi


if [ X$STATUS = X0 ] ; then
    log "-> weight.sh succeeded (2 of 2)"
else
    log "-> weight.sh FAILED (2 of 2), see $LOGFILE"
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
