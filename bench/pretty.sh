#!/bin/sh
export PATH || (echo "This isn't sh."; sh $0 $*; kill $$)
eval `machinetype.sh -b`	# sets MACHINE, UNIXTYPE, HAS_TCP
if test -f ../.rt.$MACHINE/rt ; then
  RT=../.rt.$MACHINE/rt
  DB=../.db.$MACHINE
  LD_LIBRARY_PATH=../.libbu.$MACHINE:../.libbn.$MACHINE:../.librt.$MACHINE:../.libfb.$MACHINE:../.libpkg.$MACHINE:../.libsysv.$MACHINE:$LD_LIBRARY_PATH
else
  if test ! -f ../rt/rt ; then
    echo "Can't find RT"
    exit 1
  fi
  RT=../rt/rt
  DB=../db
  LD_LIBRARY_PATH=../libbu:../libbn:../librt:../libfb:../libpkg:../libsysv:$LD_LIBRARY_PATH
fi
export LD_LIBRARY_PATH

# Alliant NFS hack
if test x${MACHINE} = xfx ; then
  cp ${RT} /tmp/rt
  RT=/tmp/rt
fi

# WARNING THIS IS A REAL CPU HOG
if test -f cube.pix; then mv -f cube.pix cube.pix.$$; fi
if test -f cube.log; then mv -f cube.log cube.log.$$; fi
$RT -p90 -f1024 -H3 -M $*\
  -o cube.pix\
  $DB/cube.g\
  'all.g' \
  2> cube.log\
  <<EOF
6.847580140e+03
3.699276190e+03 3.032924070e+03 3.658674860e+03
-5.735762590e-01 8.191521640e-01 0.000000000e+00 0.000000000e+00 
-3.461885920e-01 -2.424037690e-01 9.063078880e-01 0.000000000e+00 
7.424040310e-01 5.198366640e-01 4.226181980e-01 0.000000000e+00 
0.000000000e+00 0.000000000e+00 0.000000000e+00 1.000000000e+00 
EOF

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
