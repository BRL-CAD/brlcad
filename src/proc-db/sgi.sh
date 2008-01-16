#!/bin/sh
#                          s g i . s h
#
# Create and render the SGI cube (tm) logo as BRL-CAD geometry.
# The SGI cube is a registered trademark of SGI.
#
# Author -
#   Christopher Sean Morrison
#
# This script is in the public domain.
#
#####################################################################

SGI="`basename $0`"
rm -f $SGI.*

# cube dimensions
i=1000 ; j=700 ; radius=125

# starting position
x=0 ; y=0 ; z=0

# functions to create geometry for each direction of the cube's path
right ( ) {
    old=$x
    x=$((old + $2))
    mged -c $SGI.g in rcc.$1 rcc $old $y $z $((x - old)) 0 0 $radius
    mged -c $SGI.g in sph.$1 sph $x $y $z $radius
}
left ( ) {
    old=$x
    x=$((old - $2))
    mged -c $SGI.g in rcc.$1 rcc $old $y $z $((x - old)) 0 0 $radius
    mged -c $SGI.g in sph.$1 sph $x $y $z $radius
}
forward ( ) {
    old=$y
    y=$((old + $2))
    mged -c $SGI.g in rcc.$1 rcc $x $old $z 0 $((y - old)) 0 $radius
    mged -c $SGI.g in sph.$1 sph $x $y $z $radius
}
back ( ) {
    old=$y
    y=$((old - $2))
    mged -c $SGI.g in rcc.$1 rcc $x $old $z 0 $((y - old)) 0 $radius
    mged -c $SGI.g in sph.$1 sph $x $y $z $radius
}
up ( ) {
    old=$z
    z=$((old + $2))
    mged -c $SGI.g in rcc.$1 rcc $x $y $old 0 0 $((z - old)) $radius
    mged -c $SGI.g in sph.$1 sph $x $y $z $radius
}
down ( ) {
    old=$z
    z=$((old - $2))
    mged -c $SGI.g in rcc.$1 rcc $x $y $old 0 0 $((z - old)) $radius
    mged -c $SGI.g in sph.$1 sph $x $y $z $radius
}

###########################
echo "Creating the cube..."

forward	100 $i
left	101 $j
down	102 $i
right	103 $i
up	104 $j
back	105 $i
down	106 $i
forward	107 $j
left	108 $i
back	109 $i
right	110 $j
up	111 $i
left	112 $i
down	113 $j
forward	114 $i
up	115 $i
back	116 $j
right	117 $i

mged -c $SGI.g g cube.c rcc.* sph.*
mged -c $SGI.g r cube.r u cube.c
mged -c $SGI.g mater cube.r \"cook re=.8 di=1 sp=1 ri=10\" 250 250 250 0

###########################
echo "Rendering the cube..."

cat <<EOF | mged -c $SGI.g
B cube.r
ae 135 -40 180
saveview $SGI.rt
EOF

./$SGI.rt -s1024
pix-fb -s1024 $SGI.rt.pix

rm -f $SGI.rt* && mv $SGI.g sgi.g
echo "The SGI cube is in the sgi.g BRL-CAD geometry database file."
