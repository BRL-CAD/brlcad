#!/bin/sh

# Regardless of file type, show it on the framebuffer.
# Mostly uses BRL-CAD LIBFB ($FB_FILE) display;
# some programs may use X Windows $DISPLAY instead.
# The args (flags) are for the display program.

if test "$1" = ""
then
	echo "Usage: show file.pix [flags]"
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
*)
	echo "$SUFFIX is not supported by this script"
	exit 1
esac
