#!/bin/sh
#                   R E P O S I T O R Y . S H
# BRL-CAD
#
# Copyright (c) 2008-2012 United States Government as represented by
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
# Basic checks of the repository sources to make sure maintenance
# burden code clean-up problems don't creep in.
#
###

TOPSRC="$1"
if test "x$TOPSRC" = "x" ; then
    TOPSRC="."
fi

FAILED=0

if test ! -f "${TOPSRC}/include/common.h" ; then
    echo "Unable to find include/common.h, aborting"
    exit 1
fi

# get a source and header file list so we only walk the sources onces

SRCFILES="`find ${TOPSRC}/src -type f \( -name \*.c -o -name \*.cpp -o -name \*.cxx -o -name \*.h -o -name \*.y -o -name \*.l \) -not -regex '.*src/other.*' -not -regex '.*~' -not -regex '.*\.log' -not -regex '.*Makefile.*' -not -regex '.*cache.*' -not -regex '.*\.svn.*' -not -regex '.*src/libpkg.*' -not -regex '.*/shapelib/.*'`"

INCFILES="`find ${TOPSRC}/include -type f \( -name \*.c -o -name \*.cpp -o -name \*.cxx -o -name \*.h -o -name \*.y -o -name \*.l \) -not -regex '.*src/other.*' -not -regex '.*~' -not -regex '.*\.log' -not -regex '.*Makefile.*' -not -regex '.*cache.*' -not -regex '.*\.svn.*' -not -regex '.*pkg.h'`"


###
# TEST: make sure nobody includes private headers like bio.h in a public header
echo "running public header private header checks..."

for i in bio.h bin.h bselect.h ; do
    if test ! -f "${TOPSRC}/include/$i" ; then
	echo "Unable to find include/$i, aborting"
	exit 1
    fi
    FOUND="`grep '[^f]${i}' $INCFILES /dev/null | grep -v 'include/${i}'`"
    
    if test "x$FOUND" = "x" ; then
	echo "-> $i header check succeeded"
    else
	echo "-> $i header check FAILED"
	FAILED="`expr $FAILED + 1`"
    fi
done


###
# TEST: make sure common.h is always included first
echo "running common.h inclusion order check..."

COMMONSRCS="`grep -n -I -e '#[[:space:]]*include' $SRCFILES /dev/null | grep '\"common.h\"' | sed 's/:.*//g'`"
COMMONINCS="`grep -n -I -e '#[[:space:]]*include' $INCFILES /dev/null | grep '\"common.h\"' | sed 's/:.*//g'`"

FOUND=
for file in $COMMONSRCS $COMMONINCS ; do
    if test -f "`echo $file | sed 's/\.c$/\.l/g'`" ; then
	continue
    fi
    MATCH="`grep '#[[:space:]]*include' $file /dev/null | head -n 1 | grep -v '\"common.h\"' | sed 's/:.*//g'`"
    if test ! "x$MATCH" = "x" ; then
	if test "x`head $file | grep BRL-CAD`" = "x" ; then
	    # not a BRL-CAD file (or it's lexer-generated) , so skip it
	    continue
	fi
	echo "ERROR: Header (common.h) out of order: $MATCH"
	FOUND=1
    fi
done
if test "x$FOUND" = "x" ; then
    echo "-> common.h check succeeded"
else
    echo "-> common.h check FAILED"
    FAILED="`expr $FAILED + 1`"
fi


###
# TEST: make sure there isn't a per-target CPPFLAGS in a Makefile.am
# support for per-target CPPFLAGS wasn't added until automake 1.7
echo "running CPPFLAGS check..."

if test ! -f "${TOPSRC}/Makefile.am" ; then
    echo "Unable to find the top-level Makefile.am, aborting"
    exit 1
fi

AMFILES="`find ${TOPSRC} -type f -name Makefile.am -exec grep -n -I -e '_CPPFLAGS[[:space:]]*=' {} /dev/null \; | grep -v 'AM_CPPFLAGS' | grep -v 'BREP_CPPFLAGS' | awk '{print $1}'`"

FOUND=
for file in $AMFILES ; do
    if test "x`echo \"$file\" | sed 's/.*#.*//g'`" = "x" ; then
	# skip commented lines
	continue
    fi
    echo "ERROR: Target-specific CPPFLAGS found in $file"
    FOUND=1
done
if test "x$FOUND" = "x" ; then
    echo "-> cppflags check succeeded"
else
    echo "-> cppflags check FAILED"
    FAILED="`expr $FAILED + 1`"
fi


###
# TEST: make sure consistent API standards are being used, using libbu
# functions where they replace or wrap a standard C function
echo "running API usage check"

# 1800 - printf
# 360 - free
# 203 - malloc
# 89 - calloc
# 21 - realloc
FOUND=
for func in fgets abort dirname getopt strcat strncat strlcat strcpy strncpy strlcpy strcmp strcasecmp stricmp strncmp strncasecmp stricmp unlink rmdir remove ; do
    echo "Searching for $func ..."
    MATCH="`grep -n -e [^a-zA-Z0-9_]$func\( $INCFILES $SRCFILES /dev/null`"

    # handle implementation exceptions
    MATCH="`echo \"$MATCH\" \
| sed 's/.*\/bomb\.c:.*abort.*//g' \
| sed 's/.*\/bu\.h.*//' \
| sed 's/.*\/cursor\.c.*//g' \
| sed 's/.*\/db\.h.*strncpy.*//' \
| sed 's/.*\/file\.c:.*remove.*//' \
| sed 's/.*\/str\.c:.*strcasecmp.*//' \
| sed 's/.*\/str\.c:.*strcmp.*//' \
| sed 's/.*\/str\.c:.*strlcat.*//' \
| sed 's/.*\/str\.c:.*strlcpy.*//' \
| sed 's/.*\/str\.c:.*strncasecmp.*//' \
| sed 's/.*\/str\.c:.*strncat.*//' \
| sed 's/.*\/str\.c:.*strncmp.*//' \
| sed 's/.*\/str\.c:.*strncpy.*//' \
| sed 's/.*\/test_dirname\.c:.*dirname.*//' \
| sed 's/.*\/ttcp.c:.*//' \
| sed 's/.*\/vls\.c:.*strncpy.*//' \
| sed '/^$/d' \
`"

    if test "x$MATCH" != "x" ; then
	echo "ERROR: Found `echo \"$MATCH\" | wc -l | awk '{print $1}'` instances of $func ..."
	echo "$MATCH"
	FOUND=1
    fi
done

if test "x$FOUND" = "x" ; then
    echo "-> API usage check succeeded"
else
    echo "-> API usage check FAILED"
    FAILED="`expr $FAILED + 1`"
fi


exit $FAILED

# Local Variables:
# tab-width: 8
# mode: sh
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
