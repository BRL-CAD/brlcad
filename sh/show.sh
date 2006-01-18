#!/bin/sh
#                         S H O W . S H
# BRL-CAD
#
# Copyright (c) 2004-2006 United States Government as represented by
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
# Regardless of file type, show it on the framebuffer.  Mostly uses
# BRL-CAD LIBFB ($FB_FILE) display; some programs may use X Windows
# $DISPLAY instead.  The args (flags) are for the display program.
#
#  -Mike Muuss, ARL
#
#  $Header$


if test "$1" = ""
then
	echo "Usage: show filename [filetype-specific-flags]"
	echo "	Regardless of file type, try to show it on the framebuffer."
	exit 1
fi

FILE=$1
shift
FLAGS="$*"

if test ! -f $FILE
then
	echo "$0: $FILE does not exist"
	exit 1
fi

eval `pixinfo.sh $FILE`	# sets BASE, SUFFIX, WIDTH, HEIGHT

# Since we don't have an easy (fast) way to query the current framebuffer
# for it's current size without opening a window, if the user isn't
# using a full-screen fbserv, they need to set a pair of environment
# variables to tell us what the maximum target image size is.
if test "$FB_WIDTH" = ""
then
	FB_WIDTH=1280
	FB_HEIGHT=1024
fi

case $SUFFIX in
pix)
	if test $WIDTH -gt $FB_WIDTH -o $HEIGHT -gt $FB_HEIGHT
	then
		decimate 3 $WIDTH $HEIGHT $FB_WIDTH $FB_HEIGHT <$FILE | pix-fb -w$FB_WIDTH -n$FB_HEIGHT
	else
		pix-fb -w$WIDTH -n$HEIGHT $FLAGS $FILE
	fi;;
bw)
	if test $WIDTH -gt $FB_WIDTH -o $HEIGHT -gt $FB_HEIGHT
	then
		decimate 1 $WIDTH $HEIGHT $FB_WIDTH $FB_HEIGHT <$FILE | bw-fb -w$FB_WIDTH -n$FB_HEIGHT
	else
		bw-fb -w$WIDTH -n$HEIGHT $FLAGS $FILE
	fi;;
yuv)
	if test $WIDTH -gt $FB_WIDTH -o $HEIGHT -gt $FB_HEIGHT
	then
		decimate 2 $WIDTH $HEIGHT $FB_WIDTH $FB_HEIGHT <$FILE | yuv-pix -w$WIDTH -n$HEIGHT | pix-fb -w$FB_WIDTH -n$FB_HEIGHT
	else
		yuv-pix -w$WIDTH -n$HEIGHT $FILE | pix-fb -w$WIDTH -n$HEIGHT
	fi;;
rle)
	if test $WIDTH -gt $FB_WIDTH -o $HEIGHT -gt $FB_HEIGHT
	then
		rle-pix $FILE | \
		decimate 3 $WIDTH $HEIGHT $FB_WIDTH $FB_HEIGHT | pix-fb -w$FB_WIDTH -n$FB_HEIGHT
	else
		rle-fb $FLAGS $FILE
	fi;;
jpg|jpeg)
	if test $WIDTH -gt $FB_WIDTH -o $HEIGHT -gt $FB_HEIGHT
	then
		jpeg-fb -F"/dev/mem -" $FLAGS $FILE | \
		decimate 3 $WIDTH $HEIGHT $FB_WIDTH $FB_HEIGHT | pix-fb -w$FB_WIDTH -n$FB_HEIGHT
	else
		jpeg-fb $FLAGS $FILE
	fi;;
png)
	if test $WIDTH -gt $FB_WIDTH -o $HEIGHT -gt $FB_HEIGHT
	then
		png-fb -F"/dev/mem -" $FLAGS $FILE | \
		decimate 3 $WIDTH $HEIGHT $FB_WIDTH $FB_HEIGHT | pix-fb -w$FB_WIDTH -n$FB_HEIGHT
	else
		png-fb $FLAGS $FILE
	fi;;
gif)
	if test $WIDTH -gt $FB_WIDTH -o $HEIGHT -gt $FB_HEIGHT
	then
		gif-fb -c -F"/dev/mem -" $FILE | \
		decimate 3 $WIDTH $HEIGHT $FB_WIDTH $FB_HEIGHT | pix-fb -w$FB_WIDTH -n$FB_HEIGHT
	else
		gif-fb -co $FLAGS $FILE
	fi;;
pr|ras)
	sun-pix $FILE | pix-fb -i -w$WIDTH -n$HEIGHT;;
gl)
	xviewgl $FILE;;
dl)
	xdl $FILE;;
grasp)
	xgrasp $FILE;;
mpg)
	mpeg_play $FILE;;
xbm)
	xloadimage $FILE;;
yuv)
	KEY=`echo $FILE | sed -e 's/[0-9]/#&/' -e 's/^/@/' -e 's/\\.yuv//' `
	fb-fb /dev/abf$KEY ;;
pcd)
	pcd-pix -p $FILE;;
*)
	echo "$SUFFIX is not supported by this script"
	exit 1
esac

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
