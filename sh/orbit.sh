#!/bin/sh
#                          O R B I T . S H
# BRL-CAD
#
# Copyright (c) 2010-2011 United States Government as represented by
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
# Create an animation orbiting a given geometry.


FILE="$1"
WIDTH=512
HEIGHT=512
ELEVATION=35

if test "$2" = "" ; then
    echo "Usage: orbit.sh [-w <width>] [-n <height>] [-e <elevation>] filename.g obj1 obj2..."
    exit 1
fi

set -- `getopt es:S:w:n:W:N: $ARGS`
while : ; do
    case $1 in
	-e)
	    ELEVATION=$2; shift;;	
	-s|-S)
	    WIDTH=$2; HEIGHT=$2; shift;;
	-w|-W)
	    WIDTH=$2; shift;;
	-n|-N)
	    HEIGHT=$2; shift;;
	--)
	    break;
    esac
    shift
done

FILENAME=$1; shift;

echo "Using $FILENAME, We want a ${WIDTH}x${HEIGHT} orbit at ${ELEVATION} degrees of: $*"

exit 0

mkdir -p orbit.$$
for frame in `jot 360 0`
do
    echo -n "$frame: "
    time rt -o orbit.$$/orbit.$frame.pix -w$WIDTH -n$HEIGHT -e$ELEVATION $FILENAME $* 2>/dev/null && pix-png -w$WIDTH -n$HEIGHT orbit.$$/orbit.$frame.pix > orbit.$$/orbit.$frame.png
done && ffmpeg -o orbit.$$.mp4 orbit.$$/*png && rm -rf orbit.$$

exit 0

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
