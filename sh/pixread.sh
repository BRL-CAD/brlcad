#!/bin/sh
# pixread.sh -- Obtain pictures from (multi-reel) magtapes.
# Operates via normal UNIX tape drive interface
# Reads indicated frames all concatenated together, no separating tape marks.
#
# Formats -
#	512x512	nrec=32 count=192
#	640x480 nrec=38 count=160

# base string for no rewind tape
NRW_BASE=/dev/nrmt

if test x$2 = x
then	echo "Usage:  $0 basename start_frame [nrec/image] [images/tape]"
	exit 1
fi

START_FRAME=$2

if test x$3 = x
then	NREC=32		# Expecting 512x512 on 6250 tapes
else	NREC=$3
fi

case $NREC in
	32)	NFRAME=192;;	# 512x512
	38)	NFRAME=160;;	# 640x480
	*)	NFRAME=`expr 6144 / $NREC`;;
esac

if test x$4 != x
then	NFRAME=$4
fi

echo "bs=24k  count=$NREC.  # images per tape=$NFRAME, first file=$1.$START_FRAME"

TAPE=/dev/null		# to defeat initial "mt off" cmd
i=0
while eval
do
	REEL=`expr '(' $i / $NFRAME ')' + 1`
	OFFSET=`expr $i % $NFRAME`
	INDEX=`expr $i + $START_FRAME`
	echo $1.$INDEX: reel $REEL offset $OFFSET
	if test x$OFFSET = x0
	then
		mt -f $TAPE off
		ANS=foo
		while test x$ANS != x0 -a x$ANS != x1
		do
			echo "Mount reel $REEL, enter drive number [0,1] and hit RETURN"
			read ANS
		done
		TAPE=${NRW_BASE}${ANS}
		echo Now using $TAPE
	fi
	dd if=$TAPE bs=24k count=$NREC of=$1.$INDEX
	i=`expr $i + 1`
done

mt -f $TAPE off

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
