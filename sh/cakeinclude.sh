#!/bin/sh
#			cakeinclude.sh
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
