#!/bin/sh
#                          s g i . s h
#
# This is an example script for creating and rendering the SGI Cube
# logo as BRL-CAD geometry.
#
# The SGI cube is a registered trademark of SGI.
#
# This script is in the public domain.
#
#####################################################################

SGI="`basename $0`"
rm -f $SGI.*

# cube dimensions
i=1000 ; j=800 ; radius=100

# starting position
x=0 ; y=0 ; z=0

# functions to create geometry for each direction of the cube
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

# combine all the primitives together via unions
mged -c $SGI.g g cube.c rcc.* sph.*

# create a region from that combination (make it occupy physical space)
mged -c $SGI.g r cube.r u cube.c

# assign some material properties using a cook-torrence shader
mged -c $SGI.g mater cube.r \"cook re=.8 di=1 sp=1 ri=10\" 250 250 250 0

###########################
echo "Rendering the cube..."

# pipe in mged commands for setting up and saving the view to a render script
cat <<EOF | mged -c $SGI.g
B cube.r
ae 135 -35 180
set perspective 20
zoom .25
saveview $SGI.rt
EOF

# render the view we saved as a 1024x1024 image
./$SGI.rt -s1024

# convert from raw pix image format to png format
pix-png -s1024 < $SGI.rt.pix > $SGI.png

# display the png image in a framebuffer window
png-fb $SGI.png

# keep the geometry database as sgi.g and the rendering as sgi.png
mv $SGI.g sgi.g
mv $SGI.png sgi.png

# clean up after ourselves
rm -f $SGI.*

echo "The SGI cube is in the sgi.g BRL-CAD geometry database file."

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
