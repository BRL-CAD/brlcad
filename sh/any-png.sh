#!/bin/sh

# Regardless of file type, convert it to PNG format.
# -r means to reverse black and white in output image.  (.pix only)
#
#  $Header$

PATH=$PATH:/vld/mike/.bin.4d:
export PATH

if test "$1" = ""
then
	echo "Usage: any-png.sh [-r] image_file [flags]"
	exit 1
fi

FILTER=
if test "$1" = "-r"
then
	# Reverse black and white.
	FILTER="pixelswap 0 0 0 255 255 255 |"
	shift
fi

OLDFILE=$1
shift
FLAGS="$*"

if test ! -f $OLDFILE
then
	echo "$0: $OLDFILE does not exist"
	exit 1
fi

eval `pixinfo.sh $OLDFILE`	# sets BASE, SUFFIX, WIDTH, HEIGHT

NEWFILE=${BASE}.png
if test -f $NEWFILE
then
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

if test $? -ne 0
then
	echo "*** ERROR ***  $OLDFILE not converted. ***"
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
