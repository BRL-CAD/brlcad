#!/bin/sh

PREFIX=$1
TOP_SRCDIR=$2

rm -f weight.log .density wgt.out weight.g weight.ref weight.out

$PREFIX/bin/mged -c > weight.log 2>&1 << EOF
opendb weight.g y
units cm
in box rpp 0 1 0 1 0 1
r box.r u box
EOF

cat > .density <<EOF
1 7.8295	steel	
EOF

$PREFIX/bin/rtweight -a 25 -e 35 -s128 -o wgt.out weight.g box.r > weight.log 2>&1


cat >> weight.ref <<EOF
RT Weight Program Output:

Database Title: "Untitled BRL-CAD Database"



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


Total volume = 0.999991 cm.^3

Centroid: X = 0.5 cm.
          Y = 0.5 cm.
          Z = 0.5 cm.

Total mass = 7.82943 grams

EOF
ref_values="`grep 0 weight.ref | grep -v 'Time Stamp' | sed 's/ //g'`"
if [ "x$ref_values" = "x" ] ; then
    echo "rtweight benchmark failed"
fi

out_values="`grep 0 wgt.out | grep -v 'Time Stamp' | sed 's/[ \t]//g'`"
if [ "x$out_values" = "x" ] ; then
    echo "rtweight benchmark failed (script error)"
fi

if  [ "x$ref_values" != "x$out_values" ] ; then
	echo "rtweight benchmark failed"
else
	echo "match"
fi
