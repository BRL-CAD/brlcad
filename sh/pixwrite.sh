#!/bin/sh
# pixwrite.sh -- Write some pictures onto one tape
# Operates via normal UNIX tape drive interface
# Writes indicated frames on a single tape (typ. 192 frames at 512x512)
# concatenated all together, with no separating tape marks.

DEV=/dev/rmt1

if test x$2 = x
then
	echo "Usage:  $0 basename start_frame [end_frame]"
	exit 1
fi

START_FRAME=$2
if test x$3 = x
then	END_FRAME=`expr ${START_FRAME} + 191`;
else	END_FRAME=$3;
fi

# Be certain that the end frames are really there
if test ! -f $1.${START_FRAME}; then
	echo $1.${START_FRAME} missing
	exit 1
fi
if test ! -f $1.${END_FRAME}; then
	echo $1.${END_FRAME} missing
	exit 1
fi

echo " "
echo "$1 Frames ${START_FRAME} to ${END_FRAME} on ${DEV}"
echo ""

for i in `loop ${START_FRAME} ${END_FRAME}`
do
	dd if=$1.$i of=${DEV} bs=24k
done

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
