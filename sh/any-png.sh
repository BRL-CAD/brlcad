#!/bin/sh
#                      A N Y - P N G . S H
# BRL-CAD
#
# Copyright (c) 2004-2011 United States Government as represented by
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
# Regardless of file type, convert it to PNG format.
# -r means to reverse black and white in output image.  (.pix only)
#

echo "************************************************************"
echo "DEPRECATION WARNING: This script is deprecated and may be"
echo "                     removed in a future release of BRL-CAD."
echo "************************************************************"

if test "$1" = "" ; then
    echo "Usage: any-png.sh [-r] image_file [flags]"
    exit 1
fi

FILTER=
if test "$1" = "-r" ; then
	# Reverse black and white.
    FILTER="pixelswap 0 0 0 255 255 255 |"
    shift
fi

OLDFILE=$1
shift
FLAGS="$*"

if test ! -f $OLDFILE ; then
    echo "$0: $OLDFILE does not exist"
    exit 1
fi

eval `pixinfo.sh $OLDFILE`      # sets BASE, SUFFIX, WIDTH, HEIGHT

NEWFILE=${BASE}.png
if test -f $NEWFILE ; then
    ls -l $NEWFILE
    echo "$0: $NEWFILE already exists, aborting"
    exit 1
fi

# The "r" (pre-read) flag to /dev/mem is essential to make stdin reading work.
PNGIFY="$FILTER fb-png -F\"/dev/memr -\" -w$WIDTH -n$HEIGHT "

# set -x

case $SUFFIX in
    pix)
	eval "< $OLDFILE $PNGIFY $NEWFILE";;
    bw)
	bw-pix $OLDFILE | eval $PNGIFY -#1 $NEWFILE;;
    rle)
	rle-pix $OLDFILE | eval $PNGIFY $NEWFILE;;
    jpg|jpeg)
	jpeg-fb -F"/dev/mem -" $OLDFILE | eval $PNGIFY $NEWFILE;;
    png)
	# This can sometimes be useful as a way of getting more compression.
	png-pix $OLDFILE | eval $PNGIFY $NEWFILE;;
    gif)
	gif-fb -c -F"/dev/mem -" $OLDFILE | eval $PNGIFY $NEWFILE;;
    *)
	echo "Image type /$SUFFIX/ is not supported by $0, aborting."
	exit 1;;
esac

if test $? -ne 0 ; then
    echo "*** ERROR *** $OLDFILE not converted. ***"
    exit 1
fi

chmod 444 $NEWFILE

echo "Converted $OLDFILE to $NEWFILE"
exit 0

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
