#!/bin/sh
#                  C A K E I N C L U D E . S H
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
#
#  This shell script lists the names of all header files that a given
#  C source module includes.  No attempt is made to deal with
#  conditional inclusion of header files.
#
#  The convention for writing #include directives is:
#	#include "./file"	gives file in "current" directory (SRCDIR)
#	#include "/file"	gives file at absolute path
#	#include "file"		gives file in ${INCDIR}
#	#include <file>		leaves to CPP
#
#  Note that there must be whitespace between the "#" and the word "include"
#  to defeat the detection of the include.
#  This allows conditional includes (if needed) to be indicated
#  by a # SP include, so as to not cause trouble.
#
#  This routine does not handle nested #includes, which is bad style anyway.

if test $# -ne 1 -a $# -ne 3
then
	echo "Usage: $0 file.c [INCDIR SRCDIR]"
	exit 1
fi

if test x$3 != x
then
	INCDIR=$2
	SRCDIR=$3
else
	INCDIR=.
	SRCDIR=.
fi

grep '^#include' $1 | \
sed \
	-e '/debug.h/d' \
	-e '/"stdio.h"/d' \
	-e 's,[^"]*"\./\([^"]*\)".*,'${SRCDIR}'/\1,' \
	-e 's,[^"]*"/\([^"]*\)".*,/\1,' \
	-e 's,[^"]*"\([^"]*\)".*,'${INCDIR}'/\1,' \
	-e '/[^<]*<\(.*\)>/d'
exit 0

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
