#!/usr/bin/env perl
#
#author: Bryan Bishop <kanzure@gmail.com>
#url: http://heybryan.org/
#date: 2008-12-16
#license: public domain
#
# Generates a 3D model of a spiral via BRL-CAD.
# This script is in the public domain.
#
######################################################################

use warnings;
use constant PI => 4 * atan2 1, 1;

if (-e "spiral.g") {
  print "ERROR: spiral.g already exists.  Delete or move it.\n";
  exit 1;
}

$t_min = 0;
$t_max = 10 * PI;
$granularity = 1000; # Set the granularity of the model.

# in 1.s rpp 0 .5 0 1 0 1
# in 2.s rpp -2 2 0.1 0.9 0.1 0.9
# 1.s - 2.s

# First version: don't decrease or increase the size of the rectangles. That comes next.
$xmin = 0;
$xmax = .5;
$ymin = 0;
$ymax = 1;
$zmin = 0;
$zmax = 1;

$thickness = .1; # Thickness of the walls.
$xmin2 = -2; # $xmin+$thickness;
$xmax2 = 2; #$xmax-$thickness;
$ymin2 = $ymin + $thickness;
$ymax2 = $ymax - $thickness;
$zmin2 = $zmin + $thickness; # -1;
$zmax2 = $zmax - $thickness; #  1; # You want this going through so that you can cut the object.

# Future improvement possibility: make it spiral downward.

# generate the spiral
$i = 0; # $i is the counter.
for ($j = $t_min; $j <= $t_max; $j += $t_max / $granularity, $i++) {
  $x = $j * cos($j); $y = $j * sin($j);
  $ang = 90.0 - atan2($y, $x) * 180 / PI;
  $command = "front ; in mflb2_$i.s rpp $xmin $xmax $ymin $ymax $zmin $zmax ; in caboff1_$i.s rpp $xmin2 $xmax2 $ymin2 $ymax2 $zmin2 $zmax2 ; comb cross_section_region_$i.c u mflb2_$i.s - caboff1_$i.s ; draw cross_section_region_$i.c ; oed / /cross_section_region_$i.c/mflb2_$i.s ; rot 0 0 $ang ; translate $x $y 0 ; accept ;";
  print "#$i at\tx = $x\ty=$y\n\t$command\n\n";
  `mged -c spiral.g \'$command\'`;
}

# make the spiral into one grouped shape
`mged -c spiral.g 'g spiral.c *.c'`;

# make that shape into a solid region (so it now has mass and occupies space)
`mged -c spiral.g 'r spiral.r u spiral.c'`;

# ` g-stl -o spiral.g.stl spiral.g all.g ;`; 
# ` viewstl spiral.g.stl `;

print "All done.\n=n";
