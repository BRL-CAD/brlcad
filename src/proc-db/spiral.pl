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
use constant PI => 4.0 * atan2(1, 1);

if (-e "spiral.g") {
  print "ERROR: spiral.g already exists.  Delete or move it.\n";
  exit 1;
}

$t_min = 0;
$t_max = 10 * PI;
$granularity = 1500; # Set the granularity of the model.

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

# default keypoint is bottom corner, so calculate the box base center
$xkey = ($xmax - $xmin) / 2.0;
$ykey = ($ymax - $ymin) / 2.0;

# Future improvement possibility: make it spiral downward.

# generate the spiral
$i = 0; # $i is the counter.
$command = "";
for ($j = $t_min; $j <= $t_max; $j += $t_max / $granularity, $i++) {
  $x = $j * cos($j); $y = $j * sin($j);
  $ang = 96.0 - atan2($y, $x) * 180.0 / PI; # 6 degrees fudge factor since we're not calculating the proper x/y
  $command .= "in box_$i.s rpp $xmin $xmax $ymin $ymax $zmin $zmax ; in hole_$i.s rpp $xmin2 $xmax2 $ymin2 $ymax2 $zmin2 $zmax2 ; comb seg_$i.c u box_$i.s - hole_$i.s ; draw seg_$i.c ; oed / /seg_$i.c/box_$i.s ; keypoint $xkey $ykey 0 ; rot 0 0 $ang ; translate $x $y 0 ; accept ;";
  # create 100 segments at a time to reduce process initialization overhead
  if ($i % 100 == 0) {
    print "#$i at\tx = $x\ty=$y\n\t$command\n\n";
    `mged -c spiral.g \'$command\'`;
    $command = "";
  }
}
# pick up any remaining segments if $granularity isn't a multiple of 100
if ($command ne "") {
  print "#$i at\tx = $x\ty=$y\n\t$command\n\n";
  `mged -c spiral.g \'$command\'`;
  $command = "";
}

print "Grouping the spiral segments together into one shape...(please wait)...";
`mged -c spiral.g 'g spiral.c *.c'`;
print "done.\n";

print "Making that shape into a solid region (so it now has mass and occupies space)...\n";
`mged -c spiral.g 'r spiral.r u spiral.c'`;

# `rt -a 35 -e 25 -s1024 spiral.g spiral.r`;
# `g-stl -o spiral.g.stl spiral.g all.g`; 
# `viewstl spiral.g.stl`;

print "All done.\n=n";
