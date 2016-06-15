#!/bin/sh
#                    R T W I Z A R D . S H
# BRL-CAD
#
# Copyright (c) 2010-2014 United States Government as represented by
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

# Ensure /bin/sh
export PATH || (echo "This isn't sh."; sh $0 $*; kill $$)

TOP_SRCDIR="$1"

# source common library functionality, setting ARGS, NAME_OF_THIS,
# PATH_TO_THIS, and THIS.
. "$1/regress/library.sh"

ASC2G="`ensearch asc2g`"
if test ! -f "$ASC2G" ; then
    echo "Unable to find asc2g, aborting"
    exit 1
fi
RTWIZARD="`ensearch rtwizard`"
if test ! -f "$RTWIZARD" ; then
    echo "Unable to find rtwizard, aborting"
    exit 1
fi
ASC2PIX="`ensearch asc2pix`"
if test ! -f "$ASC2PIX" ; then
    echo "Unable to find asc2pix, aborting"
    exit 1
fi
PIXDIFF="`ensearch pixdiff`"
if test ! -f "$PIXDIFF" ; then
    echo "Unable to find pixdiff, aborting"
    exit 1
fi
GZIP="`which gzip`"
if test ! -f "$GZIP" ; then
    echo "Unable to find gzip, aborting"
    exit 1
fi

# determine the behavior of tail
case "x`echo 'tail' | tail -n 1 2>&1`" in
    *xtail*) TAIL_N="n " ;;
    *) TAIL_N="" ;;
esac

rtwizard_prep () {
  rm -f rtwizard_test$1.pix rtwizard_test$1_ref.asc rtwizard_test$1_ref.pix rtwizard_test$1.pixdiff.log rtwizard_test$1.diff.pix
}

rtwizard_check () {
NUMBER_WRONG=1
if [ ! -f rtwizard_test1.pix ] ; then
	echo "rtwizard failed to create an image - Test #$1"
	echo '-> rtwizard.sh FAILED'
	exit 1
else
	if [ ! -f "$TOP_SRCDIR/regress/rtwizard/rtwizard_test$1_ref.asc.gz" ] ; then
		echo No reference file for $TOP_SRCDIR/regress/rtwizard_test$1.pix
	else
		$GZIP -d -c "$TOP_SRCDIR/regress/rtwizard/rtwizard_test$1_ref.asc.gz" > rtwizard_test$1_ref.asc
		$ASC2PIX < rtwizard_test$1_ref.asc > rtwizard_test$1_ref.pix
		$PIXDIFF rtwizard_test$1.pix rtwizard_test$1_ref.pix > rtwizard_test$1.diff.pix 2> rtwizard_test$1.pixdiff.log

		NUMBER_WRONG=`tr , '\012' < rtwizard_test$1.pixdiff.log | awk '/many/ {print $1}' | tail -${TAIL_N}1`
		export NUMBER_WRONG
		if [ X$NUMBER_WRONG = X0 ] ; then
		  echo "-> Type $1 image generation succeeded"
		else
		  echo "rtwizard_test$1.pix $NUMBER_WRONG off by many"
		  echo '-> rtwizard.sh FAILED'
		  exit 1
		fi
	fi
fi
}

# Create the .g file to be raytraced
rm -f rtwizard.m35.g
$ASC2G $1/db/m35.asc rtwizard.m35.g

rtwizard_prep 1
$RTWIZARD --no-gui -i rtwizard.m35.g -o rtwizard_test1.pix -c component  2>&1
rtwizard_check 1

# If we got this far, tests were successful
exit 0

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
