#!/bin/sh
#                       W E I G H T . S H
# BRL-CAD
#
# Copyright (c) 2010-2011 United States Government as represented by
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
. $1/regress/library.sh

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


rm -f weight.log .density weight.g weight.ref weight.out weight.mged

cat > weight.mged <<EOF
opendb weight.g y
units cm
in box rpp 0 1 0 1 0 1
r box.r u box
EOF

$MGED -c > weight.log 2>&1 << EOF
`cat weight.mged`
EOF

cat > .density <<EOF
1 7.8295        steel
EOF

$RTWEIGHT -a 25 -e 35 -s128 -o weight.out weight.g box.r > weight.log 2>&1


cat >> weight.ref <<EOF
RT Weight Program Output:

Database Title: "Untitled BRL-CAD Database"
Time Stamp: Day Mon  0 00:00:00 0000


Density Table Used:/path/to/.density

Material  Density(g/cm^3)  Name
    1         7.8295       steel
Weight by region (in grams, density given in g/cm^3):

  Weight Matl LOS  Material Name  Density Name
 ------- ---- --- --------------- ------- -------------
   7.829    1 100 steel            7.8295 /box.r
Weight by item number (in grams):

Item  Weight  Region Names
---- -------- --------------------
1000    7.829 /box.r
RT Weight Program Output:

Database Title: "Untitled BRL-CAD Database"
Time Stamp: Day Mon  0 00:00:00 0000


Total volume = 0.999991 cm^3

Centroid: X = 0.5 cm
          Y = 0.5 cm
          Z = 0.5 cm

Total mass = 7.82943 grams

EOF

tr -d ' \t' < weight.ref | grep -v DensityTableUsed | grep -v TimeStamp > weight.ref_ns
tr -d ' \t' < weight.out | grep -v DensityTableUsed | grep -v TimeStamp > weight.out_ns

cmp weight.ref_ns weight.out_ns
STATUS=$?
if [ X$STATUS != X0 ] ; then
    echo "rtweight results differ $STATUS"
fi

rm -f weight.out_ns weight.ref_ns

if [ X$STATUS = X0 ] ; then
    echo "-> weight.sh succeeded"
else
    echo "-> weight.sh FAILED"
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
