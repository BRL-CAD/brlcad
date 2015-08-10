#!/bin/sh
#                      A S C 2 D S P . S H
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

# source common library functionality, setting ARGS, NAME_OF_THIS,
# PATH_TO_THIS, and THIS.
. "$1/regress/library.sh"


FAILED=0

A2D="`ensearch asc2dsp`"
if test ! -f "$A2D" ; then
    echo "Unable to find asc2dsp, aborting"
    FAILED=1
fi

CV="`ensearch cv`"
if test ! -f "$CV" ; then
    echo "Unable to find cv, aborting"
    FAILED=1
fi

A2P="`ensearch asc2pix`"
if test ! -f "$A2P" ; then
    echo "Unable to find asc2pix, aborting"
    FAILED=1
fi

P2B="`ensearch pix-bw`"
if test ! -f "$P2B" ; then
    echo "Unable to find pix-bw, aborting"
    FAILED=1
fi

if [ $FAILED -ne 0 ] ; then
    echo "Unable to find asc2dsp requirements, aborting"
    echo "-> asc2dsp.sh ABORTED"
    exit 1
fi

FAILED=0

BASE1=asc2dsp-old
BASE2=asc2dsp-new
LOG=asc2dsp.log

TRASH="$LOG $BASE1.pix $BASE1.bw $BASE1.dsp $BASE2.dsp"

rm -f $TRASH

# we generate one dsp file the old way and one the new way--they should be identical
# old first
# convert dsp data file in asc hex format to pix format
ASC1="$1/regress/dsp/$BASE1.asc"
$A2P < $ASC1 > $BASE1.pix 2>>$LOG
# convert pix to bw format
# take the blue pixel only
$P2B -B1.0 $BASE1.pix > $BASE1.bw 2>>$LOG
# convert pix to dsp format
$CV huc nu16 $BASE1.bw $BASE1.dsp 1>>$LOG 2>>$LOG

# new
# convert dsp data file in asc decimal format to dsp format
ASC2="$1/regress/dsp/$BASE2.asc"
$A2D $BASE2.asc $BASE2.dsp 1>>$LOG 2>>$LOG

# the two dsp files should be identical
cmp $BASE1.dsp $BASE2.dsp
STATUS=$?

if [ $STATUS -gt 0 ] ; then
  FAILED=1
fi

if [ $FAILED = 0 ] ; then
    echo "-> asc2dsp.sh succeeded"
else
    echo "-> asc2dsp.sh FAILED"
fi

exit $FAILED

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
