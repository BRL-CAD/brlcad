#!/bin/sh

# get common paths
LDIR=$1 . "$1/regress/dsp/dsp-common.sh"

# create 1x1 datasets [2]
#
# ---     ---
# | | and |X|
# ---     ---
#
# 1-1     1-2

WID=1
LEN=$WID
BASE=dsp-$WID

CASES='1 2'

FAILED=0

for i in $CASES ; do
  BASE2=$BASE-$i
  LOG=$BASE2.log
  TGM=$BASE2.g

  TRASH="$TGM $LOG $BASE2.rt.pix $BASE2.asc $BASE2.pix $BASE2.bw $BASE2.dsp"
  rm -f $TRASH

  # convert dsp data file in asc format to pix format
  DSPASC=$1/regress/dsp/$BASE2.asc
  echo "$A2P < $DSPASC > $BASE2.pix"
  $A2P < $DSPASC > $BASE2.pix

  # convert pix to bw format
  # take the blue pixel only
  echo "$P2B -B1.0 $BASE2.pix > $BASE2.bw"
  $P2B -B1.0 $BASE2.pix > $BASE2.bw

  # convert pix to dsp format
  echo "$CV huc nu16 $BASE2.bw $BASE2.dsp"
  $CV huc nu16 $BASE2.bw $BASE2.dsp

  # build a TGM
  $MGED -c > $LOG 2>&1 <<EOF
opendb $TGM y
in $BASE2.s dsp f $BASE2.dsp $WID $LEN 0 ad 1 1
r $BASE2.r u $BASE2.s
quit
EOF

  # and raytrace it
  $RT -a45 -e45 -o $BASE2.rt.pix $TGM $BASE2.r 1>> $LOG 2>> $LOG
  STATUS=$?
  if [ $STATUS -gt 0 ] ; then
    FAILED="`expr $FAILED + 1`"
  fi

done

exit $FAILED
