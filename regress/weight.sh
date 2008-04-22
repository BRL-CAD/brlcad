#!/bin/sh

LD_LIBRARY_PATH=../src/other/tcl/unix:../src/other/tk/unix:$$LD_LIBRARY_PATH
DYLD_LIBRARY_PATH=../src/other/tcl/unix:../src/other/tk/unix:$$DYLD_LIBRARY_PATH
export LD_LIBRARY_PATH DYLD_LIBRARY_PATH

TOP_SRCDIR=$1

rm -f weight.log .density weight.g weight.ref weight.out weight.mged

cat > weight.mged <<EOF
opendb weight.g y
units cm
in box rpp 0 1 0 1 0 1
r box.r u box
EOF

../src/mged/mged -c > weight.log 2>&1 << EOF
`cat weight.mged`
EOF

cat > .density <<EOF
1 7.8295        steel
EOF

../src/rt/rtweight -a 25 -e 35 -s128 -o weight.out weight.g box.r > weight.log 2>&1


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


Total volume = 0.999991 cm.^3

Centroid: X = 0.5 cm.
          Y = 0.5 cm.
          Z = 0.5 cm.

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
