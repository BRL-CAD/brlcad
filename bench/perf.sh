1#!/bin/sh
# A Shell script to extract the 4 benchmark statistics, and build an
# entry for the tables in doc/benchmark.doc.
#  Mike Muuss & Susan Muuss, 11-Sept-88.
#  @(#)$Header$ (BRL)

# Ensure /bin/sh
export PATH || (echo "This isn't sh.  Feeding myself to sh."; sh $0 $*; kill $$)

if test x$3 = x
then
	echo "Usage:  $0 hostname note1 note2"
	exit 1
fi

#  Save three args, because arg list will be reused below.
#  They may have embedded spaces in them.
HOST="$1"
NOTE1="$2"
NOTE2="$3"

INFILES="moss.log world.log star.log bldg391.log"
VAXREF="122.64 59.33 46.28 45.95"

# Use TR to convert newlines to tabs.
CURVALS=`grep RTFM $INFILES | \
	sed -n -e 's/^.*= *//' -e 's/ rays.*//p' | \
	tr '\012' '\011' `

RATIO_LIST=""

# Trick:  Force args $1 through $4 to 4 numbers in $CURVALS
set -- $CURVALS

for ref in $VAXREF
do
	cur=$1
	shift
	RATIO=`echo "2k $cur $ref / p" | dc`
	# Note: append new value and a trail TAB to existing list.
	RATIO_LIST="${RATIO_LIST}$RATIO	"
done

MEAN_ABS=`echo 2k $CURVALS +++ 4/ p | dc`
MEAN_REL=`echo 2k $RATIO_LIST +++ 4/ p | dc`

# Note:  Both RATIO_LIST and CURVALS have an extra trailing tab.
# The question mark is for the mean field
echo "Abs	${HOST}	${CURVALS}${MEAN_ABS}	$NOTE1"
echo "*vgr	${HOST}	${RATIO_LIST}${MEAN_REL}	$NOTE2"
