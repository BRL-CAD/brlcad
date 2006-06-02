#!/bin/sh


rm -f spdi.g spdi.log spdi spdi.pix

../src/mged/mged -c spdi.g << EOF > spdi_mged.log 2>&1


set glob_compat_mode 0

in half.s half 0 0 1 -1
r half.r u half.s
mater half.r plastic 76 158 113 0
g all.g half.r

set radius 256
foreach p {1 2 3 4 5} {

  set sh [expr \$p \* 4]
  set y  [expr [expr \$p - 3] \* 640]

  foreach v {1 2 3 4 5} {
    set sp [expr \$v \* 0.2]
    set di [expr 1.0 - \$sp]
    set x  [expr [expr \$v - 3] \* 640]

    in sph_\${sp}_\${sh}.s sph \$x \$y \$radius \$radius
    r  sph_\${sp}_\${sh}.r u sph_\${sp}_\${sh}.s
    mater sph_\${sp}_\${sh}.r "plastic sp \$sp di \$di sh \$sh" 100 200 200 0

    g all.g sph_\${sp}_\${sh}.r
  }
}

set glob_compat_mode 1

in light1.s ell -464 339 2213 0 100 0 0 0 100 100 0 0
r light1.r u light1.s
mater light1.r "light i 1 s 1 v 0" 255 255 255 0
g all.g light1.r

q
EOF

echo "rendering..."

../src/rt/rt -M -B -o spdi.pix spdi.g 'all.g' 2>> spdi.log <<EOF
viewsize 3.200000000000000e+03;
orientation 0.000000000000000e+00 0.000000000000000e+00 0.000000000000000e+00 1.000000000000000e+00;
eye_pt 0.000000000000000e+00 0.000000000000000e+00 2.413000000000000e+03;
start 0; clean;
end;

EOF
../src/conv/asc2pix < $1/regress/spdipix.asc > spdi_ref.pix
../src/util/pixdiff spdi.pix spdi_ref.pix > spdi_diff.pix 2>> spdi.log
NUMBER_WRONG=`tr , '\012' < spdi.log | awk '/many/ {print $1}'`
/bin/echo spdi.pix $NUMBER_WRONG off by many

if [ "X$NUMBER_WRONG" = "X0" ] ; then
    /bin/echo '-> spdi.sh succeeded'
else
    /bin/echo '-> spdi.sh failed'
fi

exit $NUMBER_WRONG
# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
