#!/bin/sh
#                S H A R E D L I B V E R S . S H
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
# Main result sent to stdout, errors must go to stderr.
# Must include the dot, as well as the version number(s).
#
# New convention is to only use MAJOR numbers, not minor numbers.

if test $# != 0 -a $# != 1
then
	echo "Usage: sharedlibvers.sh [directory/libraryname]" 1>&2
	echo "e.g.	../.librt.sun4/librt.so" 1>&2
	exit 1
fi


# Obtain RELEASE, RCS_REVISION, and REL_DATE
if test -r ../gen.sh
then
	# XXX The following line causes DEC Alpha's /bin/sh to dump core.
	eval `grep '^RELEASE=' ../gen.sh`
	##RELEASE=4.4;	RCS_REVISION=11;	# Uncomment for Alpha workaround.
else
	echo "$0: Unable to get release number"  1>&2
	exit 1
fi

if test $# = 1
then
	DIR=`dirname $1`
else
	DIR=.
fi

cd $DIR

if test $# = 1
then
	echo $1.$RCS_REVISION
	if test ! -f $1.$RCS_REVISION
	then
		echo "$0: ERROR, $1.$RCS_REVISION does not actually exist" 1>&2
	fi
else
	echo .$RCS_REVISION
fi

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
