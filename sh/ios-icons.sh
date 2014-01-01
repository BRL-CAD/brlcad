#!/bin/sh
#			I O S - I C O N S
# BRL-CAD
#
# Copyright (c) 2012-2014 United States Government as represented by
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
# Create an icon set for use with Apple IOS (ipod/iphone/ipad)
# applications.

RT=`command -pv rt`
if [ $? != 0 ]
then
    echo "Unable to find rt"
    exit 1
fi

OUTDIR="$1"
shift

echo "OUTDIR: $OUTDIR"
echo "*: $*"

if test "$OUTDIR" = "" ; then
    echo "Usage: ios-icons.sh <outdir> [rt opts] <.g file> <tops>"
    exit 1
fi

if [ ! -d $OUTDIR ]
then
    mkdir -p $OUTDIR
fi

# exit on failure
set -e

# app store icon
$RT -o $OUTDIR/Icon512.png -s 512 $*

# ipod/iphone icons (@2x for retina)
$RT -o $OUTDIR/Icon.png -s 57 $*
$RT -o $OUTDIR/Icon@2x.png -s 114 $*

# ipad icons (@2x for retina)
$RT -o $OUTDIR/Icon-72.png -s 72 $*
$RT -o $OUTDIR/Icon-72@2x.png -s 144 $*

# for stuff like settings menu, spotlight, ...
$RT -o $OUTDIR/Icon-Small.png -s 29 $*
$RT -o $OUTDIR/Icon-Small@2x.png -s 58 $*

file $OUTDIR/*

exit 0

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
