#!/bin/sh

BIN=$1/bin
export BIN

rm -f lights.pix

$BIN/asc2g lights.asc lights.g

$BIN/rt -M -B \
 -o lights.pix \
 lights.g\
 'all.g' \
 2> lights.log\
 <<EOF
viewsize 6.000000000000000e+01;
orientation 0.000000000000000e+00 0.000000000000000e+00 0.000000000000000e+00 1.000000000000000e+00;
eye_pt 0.000000000000000e+00 0.000000000000000e+00 2.950000000000000e+01;
start 0; clean;
end;

EOF

$BIN/asc2pix < lights_ref.asc  > lights_ref.pix
$BIN/pixdiff lights.pix lights_ref.pix > lights_diff.pix
