#!/bin/sh
#                         P E R F . S H
# BRL-CAD
#
# Copyright (C) 2004-2005 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following
# disclaimer in the documentation and/or other materials provided
# with the distribution.
#
# 3. The name of the author may not be used to endorse or promote
# products derived from this software without specific prior written
# permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
###
# A Shell script to extract the benchmark statistics, and build an
# entry suitable for the tables in doc/benchmark.tr.
# Note well that the command-line args may have embedded spaces.
#  Mike Muuss & Susan Muuss, 11-Sept-88.
#  @(#)$Header$ (BRL)

# Ensure /bin/sh
export PATH || (sh $0 $*; kill $$)
path_to_perf_sh=`dirname $0`

NAME=$0
if test "x$1" = x ; then
  echo "Usage:  $NAME hostname [note1] [note2]"
  exit 1
fi

# force locale setting to C so things like date output as expected
LC_ALL=C

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

REF_DIR="../pix"
if test -f "$path_to_perf_sh/../pix/sphflake.log" ; then
    REF_DIR="$path_to_perf_sh/../pix"
elif test -f "$path_to_perf_sh/sphflake.log" ; then
    REF_DIR="$path_to_perf_sh"
elif test -f "$path_to_perf_sh/../share/brlcad/pix/sphflake.log" ; then
    REF_DIR="$path_to_perf_sh/../share/brlcad/pix"
fi

REF_FILES=" \
	$REF_DIR/moss.log \
	$REF_DIR/world.log \
	$REF_DIR/star.log \
	$REF_DIR/bldg391.log \
	$REF_DIR/m35.log \
	$REF_DIR/sphflake.log \
"

for i in $NEW_FILES $REF_FILES ;  do
 if test ! -f $i ; then
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

if test "x$VGRREF" = "x" ; then
  echo "Cannot locate VGR reference values, aborting"
  exit 1
fi

if test "x$CURVALS" = "x" ; then
  CURVALS="0	0	0	0	0	0	"
fi

# Trick:  Force args $1 through $6 to the numbers in $CURVALS
# This should be "set -- $CURVALS", but 4.2BSD /bin/sh can't handle it,
# and CURVALS are all positive (ie, no leading dashes), so this is safe.

set $CURVALS

while test $# -lt 6 ; do
  echo "${NAME}: Warning, only $# times found, adding a zero."
  CURVALS="${CURVALS}0	"
  set $CURVALS
done

_have_dc=yes
echo "1 1 + p" | dc 2>&1 >/dev/null
if test ! x$? = x0 ; then
  _have_dc=no
fi

for ref in $VGRREF ; do
  cur=$1
  shift

  if test "x$_have_dc" = "xyes" ; then
    RATIO=`echo "2k $cur $ref / p" | dc`
  else
    RATIO=`echo "scale=2; $cur / $ref" | bc`
  fi
        # Note: append new value and a trail TAB to existing list.
  RATIO_LIST="${RATIO_LIST}$RATIO	"
done

# The number of plus signs must be one less than the number of elements.
if test "x$_have_dc" = "xyes" ; then
  MEAN_ABS=`echo 2k $CURVALS +++++ 6/ p | dc`
  MEAN_REL=`echo 2k $RATIO_LIST +++++ 6/ p | dc`
else
  _expr="scale=2; ( 0"
  for val in $CURVALS ; do
    _expr="$_expr + $val"
  done
  _expr="$_expr ) / 6"
  MEAN_ABS=`echo $_expr | bc`

  _expr="scale=2; ( 0"
  for val in $RATIO_LIST ; do
    _expr="$_expr + $val"
  done
  _expr="$_expr ) / 6"
  MEAN_REL=`echo $_expr | bc`
fi

# Note:  Both RATIO_LIST and CURVALS have an extra trailing tab.
# The question mark is for the mean field
echo "Abs  ${HOST} ${CURVALS}${MEAN_ABS}	$NOTE1"
echo "*vgr ${HOST} ${RATIO_LIST}${MEAN_REL}	$NOTE2"

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
