#!/bin/sh
#                          O R B I T . S H
# BRL-CAD
#
# Copyright (c) 2010-2025 United States Government as represented by
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
#
# Create a simple animated visualization orbiting around geometry.
#
# The script passes all supplied arguments to rt, with each call also
# passing -a azimuth options from 0 to 359.  The output is either an
# mpg movie.
#


if test $# -lt 2 ; then
    echo "Usage: orbit.sh [-e <elevation>] [...other rt options...] filename.g obj1 obj2..."
    exit 1
fi

# find the .g in our argument list
for arg in "$@" ; do
    match="`echo $arg | grep -e '\.g$'`"
    if ! test "x$match" = "x" ; then
	file="$arg"
	break
    fi
done
if test "x$file" = "x" ; then
    echo "ERROR: .g file name not specified"
    echo "Usage: orbit.sh [-e <elevation>] [...other rt options...] filename.g obj1 obj2..."
    exit 1
fi

# render our frames, 360 every 3 degrees (120 images)
base="`basename \"$file\"`"
dir="orbit.$base.$$"
mkdir -p "$dir"
for frame in `jot -w %03d - 0 359 3` ; do
    echo "Frame $frame (`echo $frame*100/360 | bc`%)"
    # echo rt -o $dir/$base.$frame.png -a $frame "$*"
    if ! test -f $dir/$base.$frame.png ; then
	rt -o $dir/$base.$frame.png -a $frame "$@" 2>/dev/null
    fi
done

# ImageMagick for animated .gif
echo "Creating animated .gif"
rm -f $base.$$.gif
convert $dir/$base.*.png $base.$$.gif >/dev/null 2>&1

# FFMPEG for .mp4 movie
echo "Creating .mp4 movie"
rm -f $base.$$.mp4
ffmpeg -framerate 25 -pattern_type glob -i "$dir/$base.*.png" -c:v libx264 -pix_fmt yuv420p -r 25 $base.$$.mp4 >/dev/null 2>&1

# clean up
if test -f $base.$$.gif && test -f $base.$$.mp4 ; then
    echo "Run this to remove intermediate render frames:  rm -rf "$dir"
fi

echo "---"
echo "Orbit rendering complete."
exit 0

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
