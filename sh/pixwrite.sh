#!/bin/sh
#                     P I X W R I T E . S H
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
# Write some pictures onto one tape.
#
# Operates via normal UNIX tape drive interface.  Writes indicated
# frames on a single tape (typ. 192 frames at 512x512) concatenated
# all together, with no separating tape marks.

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
