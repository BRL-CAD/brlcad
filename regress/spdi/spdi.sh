#!/bin/sh
#                         S P D I . S H
# BRL-CAD
#
# Copyright (c) 2010-2022 United States Government as represented by
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
    LOGFILE=`pwd`/spdi.log
    rm -f $LOGFILE
fi
log "=== TESTING spdi ==="

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

rm -f spdi.mged
cat > spdi.mged <<EOF

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

log "... creating geometry database (spdi.g)"
rm -f spdi.g
$MGED -c spdi.g << EOF > $LOGFILE 2>&1
`cat spdi.mged`
EOF

log "... rendering spdi"
rm -f spdi.pix
$RT -M -B -o spdi.pix spdi.g 'all.g' 2>> $LOGFILE <<EOF
viewsize 3.200000000000000e+03;
orientation 0.000000000000000e+00 0.000000000000000e+00 0.000000000000000e+00 1.000000000000000e+00;
eye_pt 0.000000000000000e+00 0.000000000000000e+00 2.413000000000000e+03;
start 0; clean;
end;

EOF

log "... running $PIXDIFF spdi.pix $PATH_TO_THIS/spdi.ref.pix > spdi.diff.pix"
rm -f spdi.diff.pix
$PIXDIFF spdi.pix "$PATH_TO_THIS/spdi.ref.pix" > spdi.diff.pix 2>> $LOGFILE
NUMBER_WRONG=`tail -n1 "$LOGFILE" | tr , '\012' | awk '/many/ {print $1}'`
log "spdi.pix $NUMBER_WRONG off by many"


if [ "X$NUMBER_WRONG" = "X0" ] ; then
    log "-> spdi.sh succeeded"
else
    log "-> spdi.sh FAILED, see $LOGFILE"
    cat "$LOGFILE"
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
