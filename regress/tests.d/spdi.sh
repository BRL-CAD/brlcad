#!/bin/sh

if [ "$#" = "1" ]
then
        REGRESS_ROOT=$1
else
        REGRESS_ROOT=/c/regress
fi
export REGRESS_ROOT

ARCH=`$REGRESS_ROOT/brlcad/sh/machinetype.sh`
export ARCH

BRLCAD_ROOT="$REGRESS_ROOT/brlcad.$ARCH"
export BRLCAD_ROOT

PATH=$PATH:$REGRESS_ROOT/$ARCH/bin
export PATH

BIN=$BRLCAD_ROOT/bin
export BIN

rm -f spdi.g spdi.log spdi spdi.pix

$BIN/mged -c spdi.g << EOF


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
    mater sph_\${sp}_\${sh}.r "plastic {sp \$sp di \$di sh \$sh}" 100 200 200 0

    g all.g sph_\${sp}_\${sh}.r
  }
}

set glob_compat_mode 1

in light1.s ell -464 339 2213 0 100 0 0 0 100 100 0 0
r light1.r u light1.s
mater light1.r "light {invisible 1 angle 180 infinite 1}" 255 255 255 0
g all.g light1.r

Z
e all.g
press top
size 3200
saveview spdi
q
EOF

mv spdi spdi.orig
sed "s,^rt,$BIN/rt," < spdi.orig > spdi
rm spdi.orig
chmod 775 spdi
echo "rendering..."
./spdi
