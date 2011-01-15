#!/bin/sh
#                      P I X I N F O . S H
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
# Given the name of an image file, extract the base name,
# the suffix, and the dimensions.
#
#  e.g.:
#       data-w123-n456.pix
#       stuff.rle
#
#  The output of this script is one line, suitable for use in a
#  Bourne-shell accent-grave form, like this:
#
#       eval `pixinfo.sh $FILE`         # Sets BASE, SUFFIX, WIDTH, HEIGHT
#

if test "$1" = "" ; then
    echo "Usage: pixinfo.sh filename"
    echo "  gives /bin/sh definitions for BASE SUFFIX WIDTH HEIGHT"
    exit 1
fi

FILE="$1"

# Change dangerous shell characters to more harmless versions.
# This primarily affects the returned BASE string.
# LHS is added in a second step, in case there is no dot at all.
eval `basename "$FILE" | tr '&;$!=' '+,---' | \
    sed -e 's/\\(.*\\)\\.\\(.*\\)/\\1;SUFFIX=\\2;/' -e 's/^/LHS=/' `

# First, see if size is encoded in the name.
BASE=
case "$SUFFIX" in
    pix|bw)
	eval `echo $LHS|sed \
	-e 's/\\(.*\\)-w\\([0-9]*\\)-n\\([0-9]*\\)/;BASE=\\1;WIDTH=\\2;HEIGHT=\\3;/' \
	-e 's/^/GOOP=/' `
	# if BASE is not set, then the file name is not of this form,
	# and GOOP will have the full string in it.
	;;
esac

if test "$BASE" = ""
    then
    BASE="$LHS"
fi

# Second, see if some program can tell us more about this file,
# usually giving -w -n style info.
ARGS=""
case "$SUFFIX" in
    pix)
	eval `pixautosize -b 3 -f $FILE`;;      # Sets WIDTH, HEIGHT
    bw)
	eval `pixautosize -b 1 -f $FILE`;;      # Sets WIDTH, HEIGHT
    dpix)
	eval `pixautosize -b 24 -f $FILE`;;     # Sets WIDTH, HEIGHT
    dbw)
	eval `pixautosize -b 8 -f $FILE`;;      # Sets WIDTH, HEIGHT
    yuv)
	eval `pixautosize -b 2 -f $FILE`;;      # Sets WIDTH, HEIGHT
    png)
	eval `png-fb -H $FILE`;;                # Sets WIDTH, HEIGHT
    jpg|jpeg)
	ARGS=`jpeg-fb -h $FILE` ;;
    rle)
	ARGS=`rle-pix -H $FILE` ;;
    gif)
	ARGS=`gif2fb -h $FILE 2>&1 ` ;;
    pr|ras)
	ARGS=`sun-pix -h $FILE` ;;
    "")
	# No suffix given, this is a problem.
	ARGS="-w0 -n0"
	SUFFIX=unknown ;;
    *)
	#  Suffix not recognized, just act innocent.
	#  Calling routine may know what to do anyway, and just wants
	#  the BASE and SUFFIX strings
	ARGS="-w0 -n0" ;;
esac

set -- `getopt hs:S:w:n:W:N: $ARGS`
while : ; do
    case $1 in
	-h)
	    WIDTH=1024; HEIGHT=1024;;
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

echo "BASE=$BASE; SUFFIX=$SUFFIX; WIDTH=$WIDTH; HEIGHT=$HEIGHT"
exit 0

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
