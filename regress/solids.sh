#!/bin/sh
#                       S O L I D S . S H
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

RT="`ensearch rt`"
if test ! -f "$RT" ; then
    echo "Unable to find rt, aborting"
    exit 1
fi
MGED="`ensearch mged`"
if test ! -f "$MGED" ; then
    echo "Unable to find mged, aborting"
    exit 1
fi
A2P="`ensearch asc2pix`"
if test ! -f "$A2P" ; then
    echo "Unable to find asc2pix, aborting"
    exit 1
fi
GENCOLOR="`ensearch gencolor`"
if test ! -f "$GENCOLOR" ; then
    echo "Unable to find gencolor, aborting"
    exit 1
fi
PIXDIFF="`ensearch pixdiff`"
if test ! -f "$PIXDIFF" ; then
    echo "Unable to find pixdiff, aborting"
    exit 1
fi

# for test 1:
rm -f solids-simple.rt solids-simple.g solids-simple.rt.pix solids-simple.pix.diff
rm -f solids-simple.log solids-simple-diff.log solids-simple.rt.log
# for test 2:
rm -f dsp.pix ebm.bw solids.rt solids.g solids.rt.pix solids.pix.diff
rm -f solids.log solids-diff.log solids.rt.log

# Trim 1025 byte sequence down to exactly 1024.
$GENCOLOR -r205 0 16 32 64 128 | dd of=ebm.bw bs=1024 count=1 > solids.log 2>&1

# TEST 1 ==========================================================
# first check a simple TGM
# generate TGM from script
TGM="$1/regress/tgms/solids-simple.mged"
$MGED -c >> solids-simple.log 2>&1 < "$TGM"

if [ ! -f solids-simple.rt ] ; then
    echo "mged failed to create solids-simple.rt script"
    echo "-> solids.sh FAILED (test 1 of 2)"
    exit 1
fi
mv solids-simple.rt solids-simple.orig.rt
export RT
sed "s,^rt,$RT -B -P 1 ," < solids-simple.orig.rt > solids-simple.rt
rm -f solids-simple.orig.rt
chmod 775 solids-simple.rt

echo 'rendering solids...'
./solids-simple.rt
if [ ! -f solids-simple.rt.pix ] ; then
	echo raytrace failed
	exit 1
fi
if [ ! -f "$PATH_TO_THIS/solids-simplepix.asc" ] ; then
	echo No reference file for solids-simple.rt.pix
	exit 1
fi
$A2P < "$1/regress/solids-simplepix.asc" > solids-simple_ref.pix
$PIXDIFF solids-simple.rt.pix solids-simple_ref.pix > solids-simple.pix.diff \
    2> solids-simple-diff.log

tr , '\012' < solids-simple-diff.log | awk '/many/ {print $0}'
NUMBER_WRONG=`tr , '\012' < solids-simple-diff.log | awk '/many/ {print $1}'`
echo "solids-simple.rt.pix $NUMBER_WRONG off by many"

if [ X$NUMBER_WRONG = X0 ] ; then
    echo "-> solids.sh succeeded (test 1 of 2)"
else
    echo "-> solids.sh FAILED (test 1 of 2)"
fi

# TEST 2 ==========================================================
# then check a complex TGM
# generate TGM from script
TGM="$1/regress/tgms/solids.mged"
# generate required pix file
$A2P < "$1/regress/tgms/dsp.dat" > dsp.pix
if [ ! -f dsp.pix ] ; then
    echo "Failed to generate file 'dsp.pix'."
    echo "-> solids.sh FAILED (test 2 of 2)"
    exit 1
fi
if [ ! -f ebm.bw ] ; then
    echo "Failed to find file 'ebm.bw'."
    echo "-> solids.sh FAILED (test 2 of 2)"
    exit 1
fi
$MGED -c >> solids.log 2>&1 < "$TGM"

if [ ! -f solids.rt ] ; then
    echo "mged failed to create solids.rt script"
    echo "-> solids.sh FAILED (test 2 of 2)"
    exit 1
fi
mv solids.rt solids.orig.rt
export RT
sed "s,^rt,$RT -B -P 1 ," < solids.orig.rt > solids.rt
rm -f solids.orig.rt
chmod 775 solids.rt

echo 'rendering solids...'
./solids.rt
if [ ! -f solids.rt.pix ] ; then
	echo raytrace failed
	exit 1
fi
if [ ! -f "$PATH_TO_THIS/solidspix.asc" ] ; then
	echo No reference file for solids.rt.pix
	exit 1
fi
$A2P < "$1/regress/solidspix.asc" > solids_ref.pix
$PIXDIFF solids.rt.pix solids_ref.pix > solids.pix.diff \
    2> solids-diff.log

tr , '\012' < solids-diff.log | awk '/many/ {print $0}'
NUMBER_WRONG=`tr , '\012' < solids-diff.log | awk '/many/ {print $1}'`
echo "solids.rt.pix $NUMBER_WRONG off by many"

if [ X$NUMBER_WRONG = X0 ] ; then
    echo "-> solids.sh succeeded (test 2 of 2)"
else
    echo "-> solids.sh FAILED (test 2 of 2)"
fi

exit $NUMBER_WRONG

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
