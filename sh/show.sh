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

case $SUFFIX in
pix)
	if test $WIDTH -gt 1280 -o $HEIGHT -gt 1024
	then
		decimate 3 $WIDTH $HEIGHT 1280 1024 <$FILE | pix-fb -w1280 -n1024
	else
		pix-fb -w$WIDTH -n$HEIGHT $FLAGS $FILE
	fi;;
bw)
	if test $WIDTH -gt 1280 -o $HEIGHT -gt 1024
	then
		decimate 1 $WIDTH $HEIGHT 1280 1024 <$FILE | bw-fb -w1280 -n1024
	else
		bw-fb -w$WIDTH -n$HEIGHT $FLAGS $FILE
	fi;;
rle)
	if test $WIDTH -gt 1280 -o $HEIGHT -gt 1024
	then
		rle-pix $FILE | \
		decimate 3 $WIDTH $HEIGHT 1280 1024 | pix-fb -w1280 -n1024
	else
		rle-fb $FLAGS $FILE
	fi;;
jpg|jpeg)
	if test $WIDTH -gt 1280 -o $HEIGHT -gt 1024
	then
		jpeg-fb -F"/dev/mem -" $FLAGS $FILE | \
		decimate 3 $WIDTH $HEIGHT 1280 1024 | pix-fb -w1280 -n1024
	else
		jpeg-fb $FLAGS $FILE
	fi;;
gif)
	if test $WIDTH -gt 1280 -o $HEIGHT -gt 1024
	then
		gif-fb -c -F"/dev/mem -" $FILE | \
		decimate 3 $WIDTH $HEIGHT 1280 1024 | pix-fb -w1280 -n1024
	else
		gif-fb -co $FLAGS $FILE
	fi;;
pr|ras)
	sun-pix $FILE | pix-fb -i -w$WIDTH -n$HEIGHT;;
*)
	echo "$SUFFIX is not supported by this script"
	exit 1
esac

