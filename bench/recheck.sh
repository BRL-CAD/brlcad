#!/bin/sh
# A Shell script to re-check the most recent results of the benchmark
#  @(#)$Header$ (BRL)

# Ensure /bin/sh
export PATH || (echo "This isn't sh.  Feeding myself to sh."; sh $0 $*; kill $$)

MTYPE=`machinetype.sh`
if test -f ../.util.$MTYPE/pixdiff
then
	UTIL=../.util.$MTYPE
else
	if test -f ../util/pixdiff
	then
		UTIL=../util
	else
		echo "Can't find pixdiff"
		exit 1
	fi
fi

echo moss.pix
$UTIL/pixdiff ../pix/moss.pix moss.pix | $UTIL/pix-fb
echo
echo

echo world.pix
$UTIL/pixdiff ../pix/world.pix world.pix  | $UTIL/pix-fb
echo
echo

echo star.pix
$UTIL/pixdiff ../pix/star.pix star.pix  | $UTIL/pix-fb
echo
echo

echo bldg391.pix
$UTIL/pixdiff ../pix/bldg391.pix bldg391.pix  | $UTIL/pix-fb
