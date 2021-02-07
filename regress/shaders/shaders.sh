#!/bin/sh
#                      S H A D E R S . S H
# BRL-CAD
#
# Copyright (c) 2010-2021 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following
# disclaimer in the documentation and/or other materials provided
# with the distribution.
#
# 3. The name of the author may not be used to endorse or promote
# products derived from this software without specific prior written
# permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
###

# Ensure /bin/sh
export PATH || (echo "This isn't sh."; sh $0 $*; kill $$)

# source common library functionality, setting ARGS, NAME_OF_THIS,
# PATH_TO_THIS, and THIS.
. "$1/regress/library.sh"

if test "x$LOGFILE" = "x" ; then
    LOGFILE=`pwd`/shaders.log
    rm -f $LOGFILE
fi
log "=== TESTING rendering with shaders ==="

MGED="`ensearch mged`"
if test ! -f "$MGED" ; then
    log "Unable to find mged, aborting"
    exit 1
fi

RT="`ensearch rt`"
if test ! -f "$RT" ; then
    log "Unable to find rt, aborting"
    exit 1
fi

PIXDIFF="`ensearch pixdiff`"
if test ! -f "$PIXDIFF" ; then
    log "Unable to find pixdiff, aborting"
    exit 1
fi

GENCOLOR="`ensearch gencolor`"
if test ! -f "$GENCOLOR" ; then
    log "Unable to find gencolor, aborting"
    exit 1
fi


log "... running $GENCOLOR -r205 0 16 32 64 128 | dd of=shaders.ebm.bw bs=1024 count=1"
rm -f shaders.ebm.bw
$GENCOLOR -r205 0 16 32 64 128 | dd of=shaders.ebm.bw bs=1024 count=1 2>> $LOGFILE


EAGLECAD=shaders.eagleCAD-working.pix
cp "$PATH_TO_THIS/shaders.eagleCAD-512x438.pix" $EAGLECAD

log "... creating shader geometry file (shaders.g)"
rm -f shaders.mged shaders.g shaders.rt shaders.half.prj shaders.ell_2.prj
cat > shaders.mged <<EOF
opendb shaders.g y

puts ""
puts "glob_compat_mode \$glob_compat_mode"
set glob_compat_mode 0
puts "glob_compat_mode \$glob_compat_mode "

in half.s half 0 0 1 -1
r half.r u half.s
prj_add shaders.half.prj "$EAGLECAD" 512 438
mater half.r "stack prj shaders.half.prj;plastic di=.8 sp=.3" 76 158 113 0

g all.g half.r

in half2.s half 1 0 0 -2200
r half2.r u half2.s - half.s - half3.s
mater half2.r "plastic" 250 80 80 0
g all.g half2.r

in half3.s half 0 -1 0 -2200
r half3.r u half3.s - half.s - half2.s
mater half3.r "plastic" 80 80 250 0
g all.g half3.r

in env.s ell 0 0 1000 0 1 0 0 0 1 1 0 0
r env.r u env.s
mater env.r "envmap fakestar" 255 255 255 0
g all.g env.r

in light1.s ell -464 339 2213 0 100 0 0 0 100 100 0 0
r light1.r u light1.s
mater light1.r "light invisible=1 angle=180 infinite=0" 255 255 255 0
g all.g light1.r

set radius 256

foreach p {1 2 3 4 5} {

  set sh [expr \$p * 4]
  set y  [expr [expr \$p - 3] * 640]

  foreach v {1 2 3 4 5} {
    set sp [expr \$v * 0.2]
    set di [expr 1.0 - \$sp]
    set x  [expr [expr \$v - 3]  \* 640]

    set n [expr ( \$p - 1 ) * 5 + \$v]

    in ell_\${n}.s ell \$x \$y 96 0 0 -48 \$radius 0 0 0 \$radius 0
    r  ell_\${n}.r u ell_\${n}.s

    g all.g ell_\${n}.r
  }
}

set glob_compat_mode 1

mater ell_1.r "stack camo s=48;plastic di=.9 sp=.3" 255 255 255 0

mater ell_4.r "stack fbmbump s=256;plastic" 255 255 255 0
mater ell_5.r "stack turbump s=256;plastic" 255 255 255 0
mater ell_9.r "stack fbmcolor s=256;plastic" 255 255 255 0
mater ell_10.r "stack turcolor s=256;plastic" 255 255 255 0
mater ell_15.r "stack gravel s=256;plastic" 255 255 255 0
mater ell_14.r "stack grunge s=256;plastic" 255 255 255 0

mater ell_6.r "stack bwtexture file=shaders.ebm.bw w=32 n=32;plastic" 200 200 200 0
mater ell_7.r "cloud" 200 200 200 0
mater ell_6.r "stack texture file=$EAGLECAD w=512 n=438;plastic" 200 200 200 0

mater ell_11.r "stack wood s=40 p=35;plastic" 80 40 10 0
mater ell_12.r "stack marble s=256 c2=250,100,100;plastic" 250 240 180 0

mater ell_8.r "mirror" 255 255 255 0

mater ell_3.r "glass tr=.7" 255 255 255 0
mater ell_13.r "plastic re=0.2" 255 255 255 0

mater ell_16.r "stack fakestar;plastic" 255 255 255 0

mater ell_20.r "air dpm=12" 255 255 255 0
db adjust ell_20.r  id 0 air 1

mater ell_19.r "tsplat s=150" 255 255 255 0
db adjust ell_19.r id 0 air 1

mater ell_24.r "fire" 255 255 255 0

mater ell_25.r "rtrans" 0 0 0 0

mater ell_21.r "stack checker s=12;plastic" 255 255 255 0

mater ell_22.r "cook" 255 255 255 0
mater ell_23.r "cglass" 255 255 255 0

mater ell_17.r "brdf" 255 255 255 0

mater ell_18.r "stack bump file=$EAGLECAD w=512 n=438;mirror" 255 255 255 0

#
# Set up a projection shader
#

press top
center -640 -1280 100
size 300
press top
prj_add shaders.ell_2.prj "$EAGLECAD" 512 438
mater ell_2.r "stack prj shaders.ell_2.prj;plastic di=.9" 200 200 200 0

#
# Now set up the view to render
#

Z
e all.g

#press top
#center 0 0 249
#size 3200

center 0 0 0
ae 310 60
set perspective 30
size 16000

saveview shaders.rt
q
EOF

$MGED -c >> $LOGFILE 2>&1 << EOF
`cat shaders.mged`
EOF

if [ ! -f shaders.rt ] ; then
    log "ERROR: mged failed to create shaders.rt"
    log "-> shaders.sh FAILED, see $LOGFILE"
    rm -f $EAGLECAD
    cat "$LOGFILE"
    exit 1
fi

# use our RT instead of searching PATH
run mv shaders.rt shaders.rt.orig
sed "s,^rt,$RT," < shaders.rt.orig > shaders.rt 2>> $LOGFILE
run rm -f shaders.rt.orig
run chmod 775 shaders.rt

# NOTICE: -P1 and -B are required because the 25th shader (top-right
# corner ellipsoid) is a random transparency shader.  that shader
# pulls values from a random number generator for each pixel so the
# render must run single-threaded in order to obtain a repeatably
# matching sequence.

log 'rendering shaders...'
rm -f shaders.rt.pix shaders.rt.log
run ./shaders.rt -B -U 1 -P 1

# determine the behavior of tail
case "x`echo 'tail' | tail -n 1 2>&1`" in
    *xtail*) TAIL_N="n " ;;
    *) TAIL_N="" ;;
esac

NUMBER_WRONG=1
if [ ! -f shaders.rt.pix ] ; then
    log "ERROR: shaders raytrace failed to create shaders.rt.pix"
    log "-> shaders.sh FAILED, see $LOGFILE"
    rm -f $EAGLECAD
    cat "$LOGFILE"
    exit 1
fi

if [ ! -f "$PATH_TO_THIS/shaders.ref.pix" ] ; then
    log "No reference file for $PATH_TO_THIS/shaders.rt.pix"
else
    log "... running $PIXDIFF shaders.rt.pix $PATH_TO_THIS/shaders.ref.pix > shaders.rt.diff.pix"
    rm -f shaders.rt.diff.pix
    $PIXDIFF shaders.rt.pix "$PATH_TO_THIS/shaders.ref.pix" > shaders.rt.diff.pix 2>> $LOGFILE

    NUMBER_WRONG=`tail -n1 "$LOGFILE" | tr , '\012' | awk '/many/ {print $1}' | tail -${TAIL_N}1`
    log "shaders.rt.pix $NUMBER_WRONG off by many"
fi


if [ X$NUMBER_WRONG = X0 ] ; then
    log "-> shaders.sh succeeded"
else
    log "-> shaders.sh FAILED, see $LOGFILE"
    cat "$LOGFILE"
fi

rm -f $EAGLECAD
exit $NUMBER_WRONG

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
