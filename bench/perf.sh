#!/bin/sh
# A Shell script to extract the benchmark statistics, and build an
# entry suitable for the tables in doc/benchmark.doc.
# Note well that the command-line args may have embedded spaces.
#  Mike Muuss & Susan Muuss, 11-Sept-88.
#  @(#)$Header$ (BRL)

# Ensure /bin/sh
export PATH || (sh $0 $*; kill $$)
path_to_perf_sh=`dirname $0`

NAME=$0
if test "x$1" = x
then
	echo "Usage:  $NAME hostname [note1] [note2]"
	exit 1
fi

#  Save three args, because arg list will be reused below.
#  They may have embedded spaces in them.
HOST="$1"
NOTE1="$2"
NOTE2="$3"

NEW_FILES=" \
	moss.log \
	world.log \
	star.log \
	bldg391.log \
	m35.log \
	sphflake.log \
"
REF_FILES=" \
	$path_to_perf_sh/../pix/moss.log \
	$path_to_perf_sh/../pix/world.log \
	$path_to_perf_sh/../pix/star.log \
	$path_to_perf_sh/../pix/bldg391.log \
	$path_to_perf_sh/../pix/m35.log \
	$path_to_perf_sh/../pix/sphflake.log \
"

for i in $NEW_FILES $REF_FILES
do
	if test ! -f $i
	then
		echo "${NAME}: file $i does not exist, aborting"
		exit 1
	fi
done

# Use TR to convert newlines to tabs.
VGRREF=`grep RTFM $REF_FILES | \
	sed -n -e 's/^.*= *//' -e 's/ rays.*//p' | \
	tr '\012' '\011' `
CURVALS=`grep RTFM $NEW_FILES | \
	sed -n -e 's/^.*= *//' -e 's/ rays.*//p' | \
	tr '\012' '\011' `

RATIO_LIST=""

# Trick:  Force args $1 through $6 to the numbers in $CURVALS
# This should be "set -- $CURVALS", but 4.2BSD /bin/sh can't handle it,
# and CURVALS are all positive (ie, no leading dashes), so this is safe.
set $CURVALS

while test $# -lt 6
do	echo "${NAME}: Warning, only $# times found, adding a zero."
	CURVALS="${CURVALS}0	"
	set $CURVALS
done

for ref in $VGRREF
do
	cur=$1
	shift
	RATIO=`echo "2k $cur $ref / p" | dc`
	# Note: append new value and a trail TAB to existing list.
	RATIO_LIST="${RATIO_LIST}$RATIO	"
done

# The number of plus signs must be one less than the number of elements.
MEAN_ABS=`echo 2k $CURVALS +++++ 6/ p | dc`
MEAN_REL=`echo 2k $RATIO_LIST +++++ 6/ p | dc`

# Note:  Both RATIO_LIST and CURVALS have an extra trailing tab.
# The question mark is for the mean field
echo "Abs	${HOST}	${CURVALS}${MEAN_ABS}	$NOTE1"
echo "*vgr	${HOST}	${RATIO_LIST}${MEAN_REL}	$NOTE2"
