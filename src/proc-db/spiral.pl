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
$t_max = 10 * PI; # 2*PI per revolution, so 5 revolutions
$granularity = 1500; # this is the total number of segments

# This is the basic hollowed-out box shape:
# in box.s rpp 0 .5 0 1 0 1
# in hole.s rpp -2 2 0.1 0.9 0.1 0.9
# an individual segment is box.s - hole.s

# dimensions of the box
$xmin = 0;
$xmax = .5;
$ymin = 0;
$ymax = 1;
$zmin = 0;
$zmax = 1;

# Thickness of the walls.
$thickness = .1;

# You want this going through so that you make a hole.
$xmin2 = -2;
$xmax2 = 2;
$ymin2 = $ymin + $thickness;
$ymax2 = $ymax - $thickness;
$zmin2 = $zmin + $thickness;
$zmax2 = $zmax - $thickness;

# default keypoint is bottom corner, so calculate the box base center
$xkey = ($xmax - $xmin) / 2.0;
$ykey = ($ymax - $ymin) / 2.0;

# Future improvement possibility: make it spiral downward.

# generate the spiral
$i = 0; # $i is the counter.
$command = "";
for ($j = $t_min; $j <= $t_max; $j += $t_max / $granularity, $i++) {
  # x/y position of the box (not exact)
  $x = $j * cos($j); $y = $j * sin($j);

  # here we calculate the angle to rotate the box.
  # we add 6 degrees "fudge factor" since we're not calculating an exact x/y
  $ang = 96.0 - atan2($y, $x) * 180.0 / PI;

  # command to create a hollowed box segment, rotate, and move it into place
  $command .= "in box_$i.s rpp $xmin $xmax $ymin $ymax $zmin $zmax ;
               in hole_$i.s rpp $xmin2 $xmax2 $ymin2 $ymax2 $zmin2 $zmax2 ;
               comb seg_$i.c u box_$i.s - hole_$i.s ;
               draw seg_$i.c ;
               oed / /seg_$i.c/box_$i.s ;
               keypoint $xkey $ykey 0 ;
               rot 0 0 $ang ;
               translate $x $y 0 ;
               accept ;";

  # create 100 segments at a time to reduce process initialization overhead
  if ($i % 100 == 0) {
    print "#$i at\tx = $x\ty=$y\n\t$command\n\n";
    `mged -c spiral.g "$command"`;
    $command = "";
  }
}
# pick up any remaining segments if $granularity isn't a multiple of 100
if ($command ne "") {
  print "#$i at\tx = $x\ty=$y\n\t$command\n\n";
  `mged -c spiral.g "$command"`;
  $command = "";
}

print "Grouping the spiral segments together into one shape...(please wait)...";
`mged -c spiral.g 'g spiral.c *.c'`;
print "done.\n";

print "Making the shape into a solid region (so it has mass and occupies space)...\n";
`mged -c spiral.g r spiral.r u spiral.c`;

# `rt -W -a 35 -e 25 -s1024 spiral.g spiral.r`;

print "All done.\n";
