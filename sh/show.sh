#!/bin/sh

# Regardless of file type, show it on the framebuffer.
# The flags are for the display program.

if test "$1" = ""
then
	echo "Usage: show.sh file.pix [flags]"
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

set -x
case $SUFFIX in
pix)
	pix-fb -w$WIDTH -n$HEIGHT $FLAGS $FILE;;
bw)
	bw-fb -w$WIDTH -n$HEIGHT $FLAGS $FILE;;
rle)
	rle-fb $FLAGS $FILE;;
jpg|jpeg)
	jpeg-fb $FLAGS $FILE;;
gif)
	gif-fb -co $FLAGS $FILE;;
*)
	echo "$SUFFIX is not supported by this script"
	exit 1
esac

