#!/bin/sh
#                S H A R E D L I B L I N K . S H
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
# Main result sent to stdout, errors must go to stderr
#
# Build the symbolic link from the numbered version of a library to
# the no-number library.  e.g. link librt.so.10 to librt.so
#
# Also, for systems like SunOS 4.1.x and Linux that use major and
# minor version numbers, make a symlink to minor version 1 as well.
#
#  $Header$

if test $# != 1 -o ! -f "$1"
then
	echo "Usage: sharedliblink.sh libraryname.so.##" 1>&2
	echo "e.g.	../.librt.sun4/librt.so.10" 1>&2
	exit 1
fi

ONENUM="$1"
DIR=`dirname $ONENUM`
BASE=`basename $ONENUM`

cd $DIR

NONUM=`echo $BASE | sed -e 's/\\.so\\..*/.so/' `
TWONUM=$BASE.1			# Minor version 1

# If the shared library has a version number, link numbered to unnumbered.
# If no version number, don't do anything.
if test "$BASE" != "$NONUM"
then
	rm -f $NONUM $TWONUM
	ln -s $BASE $NONUM
	ln -s $BASE $TWONUM
fi

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
