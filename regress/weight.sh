#!/bin/sh
#                       W E I G H T . S H
# BRL-CAD
#
# Copyright (c) 2010-2014 United States Government as represented by
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
rm -f .density .density0 weight.log weight.g weight.ref weight.out weight.mged

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
Weight by region name (in grams, density given in g/cm^3):

 Weight   Matl  LOS  Material Name  Density Name
-------- ------ --- --------------- ------- -------------
   7.829     1  100 steel            7.8295 /box.r
Weight by region ID (in grams):

  ID   Weight  Region Names
----- -------- --------------------
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

# eliminate the time stamp lines which are obviously different and
# the file path which is not germane to the test
tr -d ' \t' < weight.ref | grep -v DensityTableUsed | grep -v TimeStamp > weight.ref_ns
tr -d ' \t' < weight.out | grep -v DensityTableUsed | grep -v TimeStamp > weight.out_ns

cmp weight.ref_ns weight.out_ns
STATUS=$?

if [ X$STATUS != X0 ] ; then
    echo "rtweight results differ $STATUS"
else
    echo "-> weight.sh succeeded (1 of 2)"
fi

# SECOND TEST =================================

# Need to do a slightly more elaborate test
# save first density file in case we fail here
mv .density .density0

rm -f weight2.log weight2.g weight2.ref weight2.out weight2.mged

cat > weight2.mged <<EOF
opendb weight2.g y
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

$MGED -c > weight2.log 2>&1 << EOF
`cat weight2.mged`
EOF

# test handling of a more complex
# density file
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

$RTWEIGHT -a 25 -e 35 -s128 -o weight2.out weight2.g boxes > weight2.log 2>&1

cat >> weight2.ref <<EOF
RT Weight Program Output:

Database Title: "Untitled BRL-CAD Database"
Time Stamp: Sat Jan 14 09:01:50 2012


Density Table Used:/path/to/.density

Material  Density(g/cm^3)  Name
    2         7.8200       Carbon Tool Steel
    3         2.7000       Aluminum, 6061-T6
    4         2.7400       Aluminum, 7079-T6
    6        19.3200       Gold, pure
    7         8.0300       Stainless, 18Cr-8Ni
    8         7.4700       Stainless 27Cr
    9         7.7150       Steel, tool
   10         7.8400       Carbon Steel
   12         3.0000       Gunner
   14        10.0000       Fuel
Weight by region name (in grams, density given in g/cm^3):

 Weight   Matl  LOS  Material Name  Density Name
-------- ------ --- --------------- ------- -------------
   7.822     2  100 Carbon Tool Ste  7.8200 /boxes/box1.r
   8.021     7  100 Stainless, 18Cr  8.0300 /boxes/box2.r
   3.001    12  100 Gunner           3.0000 /boxes/box3.r
Weight by region ID (in grams):

  ID   Weight  Region Names
----- -------- --------------------
 1000   15.843 /boxes/box1.r
	       /boxes/box2.r
 1010    3.001 /boxes/box3.r
RT Weight Program Output:

Database Title: "Untitled BRL-CAD Database"
Time Stamp: Sat Jan 14 09:01:50 2012


Total volume = 2.99933 cm^3

Centroid: X = 1.98812 cm
	  Y = 1.98833 cm
	  Z = 1.98831 cm

Total mass = 18.8433 grams

EOF

# eliminate the time stamp lines which are obviously different and
# the file path which is not germane to the test
tr -d ' \t' < weight2.ref | grep -v DensityTableUsed | grep -v TimeStamp > weight2.ref_ns
tr -d ' \t' < weight2.out | grep -v DensityTableUsed | grep -v TimeStamp > weight2.out_ns

cmp weight2.ref_ns weight2.out_ns
STATUS=$?

if [ X$STATUS != X0 ] ; then
    echo "rtweight results differ $STATUS"
fi


if [ X$STATUS = X0 ] ; then
    echo "-> weight.sh succeeded (2 of 2)"
else
    echo "-> weight.sh FAILED (2 of 2)"
fi

# remove all test products here, but only if all tests pass
if [ X$STATUS = X0 ] ; then
    # now remove all generated files
    rm -f weight.out_ns  weight.ref_ns
    rm -f weight2.ref_ns weight2.out_ns
    rm -f .density .density0
    rm -f weight.log  weight.g  weight.ref  weight.out  weight.mged
    rm -f weight2.log weight2.g weight2.ref weight2.out weight2.mged
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
