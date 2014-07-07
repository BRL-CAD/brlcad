#!/bin/sh
#                   R E P O S I T O R Y . S H
# BRL-CAD
#
# Copyright (c) 2008-2014 United States Government as represented by
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

# go to the source directory so we don't have to get fancy parsing the
# file lists of directories with odd names
cd "${TOPSRC}"

if test ! -f "./include/common.h" ; then
    echo "Unable to find include/common.h, aborting"
    exit 1
fi

# get a source and header file list so we only walk the sources once

SRCFILES="`find src -type f \( -name \*.c -o -name \*.cpp -o -name \*.cxx -o -name \*.cc -o -name \*.h -o -name \*.y -o -name \*.l \) -not -regex '.*src/other.*' -not -regex '.*~' -not -regex '.*\.log' -not -regex '.*Makefile.*' -not -regex '.*cache.*' -not -regex '.*\.svn.*' -not -regex '.*src/libpkg.*' -not -regex '.*/shapelib/.*'`"

INCFILES="`find include -type f \( -name \*.c -o -name \*.cpp -o -name \*.cxx -o -name \*.cc -o -name \*.h -o -name \*.y -o -name \*.l \) -not -regex '.*src/other.*' -not -regex '.*~' -not -regex '.*\.log' -not -regex '.*Makefile.*' -not -regex '.*cache.*' -not -regex '.*\.svn.*' -not -regex '.*pkg.h'`"

BLDFILES="`find src -type f \( -name \*.cmake -o -name CMakeLists.txt \) -not -regex '.*src/other.*' -not -regex '.*~' -not -regex '.*cache.*' -not -regex '.*\.svn.*'`
`find misc -type f \( -name \*.cmake -o -name CMakeLists.txt \) -not -regex '.*src/other.*' -not -regex '.*~' -not -regex '.*cache.*' -not -regex '.*\.svn.*'`
CMakeLists.txt"


###
# TEST: make sure nobody includes private headers like bio.h in a
# public header
echo "running public header private header checks..."

for i in bio.h bin.h bselect.h ; do
    if test ! -f "include/$i" ; then
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
# TEST: make sure bio.h isn't redundant with system headers
echo "running bio.h redundancy check..."

# limit our search to files containing bio.h
FILES="`grep -I -e '#[[:space:]]*include' $SRCFILES $INCFILES | grep -E 'bio.h' | grep -v 'bio.h:' | sed 's/:.*//g' | sort | uniq`"
FOUND=
for file in $FILES ; do

    for header in "<stdio.h>" "<windows.h>" "<io.h>" "<unistd.h>" "<fcntl.h>" ; do
	MATCH="`grep -n -I -e '#[[:space:]]*include' $file /dev/null | grep $header`"
	if test ! "x$MATCH" = "x" ; then
	    echo "ERROR: #include $header is unnecessary with bio.h; remove $MATCH"
	    FOUND=1
	    continue
	fi
    done
done
if test "x$FOUND" = "x" ; then
    echo "-> bio.h check succeeded"
else
    echo "-> bio.h check FAILED"
# TODO: uncomment after fixing the existing cases
#    FAILED="`expr $FAILED + 1`"
fi


###
# TEST: make sure bin.h isn't redundant with system headers
echo "running bin.h redundancy check..."

# limit our search to files containing bin.h
FILES="`grep -I -e '#[[:space:]]*include' $SRCFILES $INCFILES | grep -E 'bin.h' | grep -v 'bin.h:' | sed 's/:.*//g' | sort | uniq`"
FOUND=
for file in $FILES ; do

    for header in "<winsock2.h>" "<netinet/in.h>" "<netinet/tcp.h>" "<arpa/inet.h>" ; do
	MATCH="`grep -n -I -e '#[[:space:]]*include' $file /dev/null | grep $header`"
	if test ! "x$MATCH" = "x" ; then
	    echo "ERROR: #include $header is unnecessary with bin.h; remove $MATCH"
	    FOUND=1
	    continue
	fi
    done
done
if test "x$FOUND" = "x" ; then
    echo "-> bin.h check succeeded"
else
    echo "-> bin.h check FAILED"
# TODO: uncomment after fixing the existing cases
#    FAILED="`expr $FAILED + 1`"
fi


###
# TEST: make sure common.h is always included first when included
echo "running common.h inclusion order check..."

# limit our search to files containing common.h or system headers for
# small performance savings.
FILES="`grep -I -e '#[[:space:]]*include' $SRCFILES $INCFILES | grep -E 'common.h|<' | sed 's/:.*//g' | sort | uniq`"

# This is how to do it if we needed to accommodate screwy filepaths
#FILES="`echo \"$SRCFILES
#$INCFILES\" | while read file ; do
#    if test \"x\`grep -I -e '#[[:space:]]*include' $file | grep -E 'common.h|<'\`\" != \"x\" ; then
#	echo $file
#    fi
#done`"

LEXERS="schema.h obj_grammar.c obj_grammar.cpp obj_scanner.h points_scan.c script.c"
EXEMPT="bin.h bio.h config_win.h pstdint.h uce-dirent.h ttcp.c $LEXERS"

FOUND=
for file in $FILES ; do
    # some files are exempt
    basefile="`basename $file`"
    if test ! "x`echo $EXEMPT | grep $basefile`" = "x" ; then
	continue
    fi

    # test files that include common.h
    if test ! "x`grep -I -e '#[[:space:]]*include' $file | grep '\"common.h\"'`" = "x" ; then
	# common.h is first?
	MATCH="`grep -n -I -e '#[[:space:]]*include' $file /dev/null | head -n 1 | grep -v '\"common.h\"'`"
	if test ! "x$MATCH" = "x" ; then
	    echo "ERROR: common.h include needs to be moved before: $MATCH"
	    FOUND=1
	    continue
	fi
    fi

    # test files that include system headers
    if test ! "x`grep -I -e '#[[:space:]]*include' $file | grep '<'`" = "x" ; then
	# non-system header is first?
	MATCH="`grep -n -I -e '#[[:space:]]*include' $file /dev/null | head -n 1 | grep -v '\"'`"
	if test ! "x$MATCH" = "x" ; then
	    echo "ERROR: common.h needs to be included before: $MATCH"
	    FOUND=1
	    continue
	fi
    fi

done
if test "x$FOUND" = "x" ; then
    echo "-> common.h check succeeded"
else
    echo "-> common.h check FAILED"
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
for func in fgets abort dirname getopt strcat strncat strlcat strcpy strncpy strlcpy strcmp strcasecmp stricmp strncmp strncasecmp unlink rmdir remove ; do
    echo "Searching for $func ..."
    MATCH="`grep -n -e [^a-zA-Z0-9_:]$func\( $INCFILES $SRCFILES /dev/null`"

    # handle implementation exceptions
    MATCH="`echo \"$MATCH\" \
| sed 's/.*\/bomb\.c:.*abort.*//g' \
| sed 's/.*\/bu\/str\.h.*//' \
| sed 's/.*\/bu\/log\.h.*//' \
| sed 's/.*\/cursor\.c.*//g' \
| sed 's/.*\/CONFIG_CONTROL_DESIGN.*//' \
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
| sed 's/.*\/bu_dirname\.c:.*dirname.*//' \
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


###
# TEST: make sure we don't get worse when it comes to testing for a
# platform feature vs. assuming a platform always had, has, and will
# have some characteristic feature.

echo "running platform symbol usage check"
PLATFORMS="WIN32 _WIN32 WIN64 _WIN64"
FOUND=0
for platform in $PLATFORMS ; do
    echo "Searching headers for $platform ..."
    MATCH=
    for file in $INCFILES /dev/null ; do
	regex="[^a-zA-Z0-9_]$platform[^a-zA-Z0-9_]|^$platform[^a-zA-Z0-9_]|[^a-zA-Z0-9_]$platform\$"
	this="`grep -n -e $regex $file /dev/null | grep -v pstdint.h`"
	if test "x$this" != "x" ; then
	    MATCH="$MATCH
$this"
	fi
    done
    if test "x$MATCH" != "x" ; then
	cnt="`echo \"$MATCH\" | tail -n +2 | wc -l | awk '{print $1}'`"
	echo "FIXME: Found $cnt header instances of $platform ..."
	echo "$MATCH
"
	FOUND=`expr $FOUND + 1`
    fi
done


for platform in $PLATFORMS ; do
    echo "Searching sources for $platform ..."
    MATCH=
    for file in $SRCFILES /dev/null ; do
	regex="[^a-zA-Z0-9_]$platform[^a-zA-Z0-9_]|^$platform[^a-zA-Z0-9_]|[^a-zA-Z0-9_]$platform\$"
	this="`grep -n -E $regex $file /dev/null | grep -v uce-dirent.h`"
	if test "x$this" != "x" ; then
	    MATCH="$MATCH
$this"
	fi
    done
    if test "x$MATCH" != "x" ; then
	cnt="`echo \"$MATCH\" | tail -n +2 | wc -l | awk '{print $1}'`"
	echo "FIXME: Found $cnt source instances of $platform ..."
	echo "$MATCH
"
	FOUND=`expr $FOUND + $cnt`
    fi
done

for platform in $PLATFORMS ; do
    echo "Searching build files for $platform ..."
    MATCH=
    for file in $BLDFILES /dev/null ; do
	regex="[^a-zA-Z0-9_]$platform[^a-zA-Z0-9_]|^$platform[^a-zA-Z0-9_]|[^a-zA-Z0-9_]$platform\$"
	this="`grep -n -E $regex $file /dev/null`"
	if test "x$this" != "x" ; then
	    MATCH="$MATCH
$this"
	fi
    done
    if test "x$MATCH" != "x" ; then
	cnt="`echo \"$MATCH\" | tail -n +2 | wc -l | awk '{print $1}'`"
	echo "FIXME: Found $cnt build system instances of $platform ..."
	echo "$MATCH
"
	FOUND=`expr $FOUND + $cnt`
    fi
done

# make sure no more WIN32 issues are introduced than existed
# previously.  for cases where it "seems" necessary, can find and fix
# a case that is not before adding another.  lets not increase this.
NEED_FIXING=200
if test $FOUND -lt `expr $NEED_FIXING + 1` ; then
    if test $FOUND -ne $NEED_FIXING ; then
	echo "********************************************************"
	echo "FIXME: UPDATE THE PLATFORM SYMBOL COUNT IN $0"
	echo "********************************************************"
    fi
    echo "-> platform symbol usage check succeeded"
else
    echo "-> platform symbol usage check FAILED"
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
