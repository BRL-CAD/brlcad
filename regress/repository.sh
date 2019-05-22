#!/bin/sh
#                   R E P O S I T O R Y . S H
# BRL-CAD
#
# Copyright (c) 2008-2019 United States Government as represented by
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

# Ensure /bin/sh
export PATH || (echo "This isn't sh."; sh $0 $*; kill $$)

# source common library functionality, setting ARGS, NAME_OF_THIS,
# PATH_TO_THIS, and THIS.
. "$1/regress/library.sh"

TOPSRC="$1"
if test "x$TOPSRC" = "x" ; then
    TOPSRC="."
fi

if test "x$LOGFILE" = "x" ; then
    LOGFILE=`pwd`/repository.log
    rm -f $LOGFILE
fi
log "=== TESTING repository sources ==="

FAILED=0

# go to the source directory so we don't have to get fancy parsing the
# file lists of directories with odd names
cd "${TOPSRC}"

if test ! -f "./include/common.h" ; then
    log "Unable to find include/common.h, aborting"
    exit 1
fi

# get a source and header file list so we only walk the sources once
log "... scanning source tree"

SRCFILES="`find src -type f \( -name \*.c -o -name \*.cpp -o -name \*.cxx -o -name \*.cc -o -name \*.h -o -name \*.y -o -name \*.l \) -not -regex '.*src/other.*' -not -regex '.*misc/tools.*' -not -regex '.*misc/svn2git.*' -not -regex '.*~' -not -regex '.*\.log' -not -regex '.*Makefile.*' -not -regex '.*cache.*' -not -regex '.*\.svn.*' -not -regex '.*src/libpkg.*' -not -regex '.*/bullet/.*' -not -regex '.*/shapelib/.*'`"

INCFILES="`find include -type f \( -name \*.c -o -name \*.cpp -o -name \*.cxx -o -name \*.cc -o -name \*.h -o -name \*.y -o -name \*.l \) -not -regex '.*src/other.*' -not -regex '.*misc/tools.*' -not -regex '.*/bullet/.*' -not -regex '.*misc/svn2git.*' -not -regex '.*~' -not -regex '.*\.log' -not -regex '.*Makefile.*' -not -regex '.*cache.*' -not -regex '.*\.svn.*' -not -regex '.*pkg.h'`"

BLDFILES="`find src -type f \( -name \*.cmake -o -name CMakeLists.txt \) -not -regex '.*src/other.*' -not -regex '.*misc/tools.*' -not -regex '.*/bullet/.*' -not -regex '.*misc/svn2git.*' -not -regex '.*~' -not -regex '.*cache.*' -not -regex '.*\.svn.*'`
`find misc -type f \( -name \*.cmake -o -name CMakeLists.txt \) -not -regex '.*src/other.*' -not -regex '.*misc/tools.*' -not -regex '.*/bullet/.*' -not -regex '.*misc/svn2git.*' -not -regex '.*~' -not -regex '.*cache.*' -not -regex '.*\.svn.*' -not -regex '.*autoheader.*'`
CMakeLists.txt"


###
# TEST: make sure nobody includes private headers like bio.h in a
# public header
log "... running public header private header checks"

for i in bio.h bnetwork.h bsocket.h ; do
    if test ! -f "include/$i" ; then
	log "Unable to find include/$i, aborting"
	exit 1
    fi
    FOUND="`grep '[^f]${i}' $INCFILES /dev/null | grep -v 'include/${i}'`"

    if test "x$FOUND" = "x" ; then
	log "-> $i header check succeeded"
    else
	log "-> $i header check FAILED, see $LOGFILE"
	FAILED="`expr $FAILED + 1`"
    fi
done


###
# TEST: make sure bio.h isn't redundant with system headers
log "... running bio.h redundancy check"

# limit our search to files containing bio.h
FILES="`grep -I -e '#[[:space:]]*include' $SRCFILES $INCFILES | grep -E 'bio.h' | grep -v 'bio.h:' | grep -v 'obj_libgcv_rules.cpp' | grep -v 'obj_obj-g_rules.cpp' | sed 's/:.*//g' | sort | uniq`"
FOUND=
for file in $FILES ; do

    for header in "<stdio.h>" "<windows.h>" "<io.h>" "<unistd.h>" "<fcntl.h>" ; do
	MATCH="`grep -n -I -e '#[[:space:]]*include' $file /dev/null | grep $header`"
	if test ! "x$MATCH" = "x" ; then
	    log "ERROR: #include $header is unnecessary with bio.h; remove $MATCH"
	    FOUND=1
	    continue
	fi
    done
done
if test "x$FOUND" = "x" ; then
    log "-> bio.h check succeeded"
else
    log "-> bio.h check FAILED, see $LOGFILE"
    FAILED="`expr $FAILED + 1`"
fi


###
# TEST: make sure bnetwork.h isn't redundant with system headers
log "... running bnetwork.h redundancy check"

# limit our search to files containing bnetwork.h
FILES="`grep -I -e '#[[:space:]]*include' $SRCFILES $INCFILES | grep -E 'bnetwork.h' | grep -v 'bnetwork.h:' | sed 's/:.*//g' | sort | uniq`"
FOUND=
for file in $FILES ; do

    for header in "<winsock2.h>" "<netinet/in.h>" "<netinet/tcp.h>" "<arpa/inet.h>" ; do
	MATCH="`grep -n -I -e '#[[:space:]]*include' $file /dev/null | grep $header`"
	if test ! "x$MATCH" = "x" ; then
	    log "ERROR: #include $header is unnecessary with bnetwork.h; remove $MATCH"
	    FOUND=1
	    continue
	fi
    done
done
if test "x$FOUND" = "x" ; then
    log "-> bnetwork.h check succeeded"
else
    log "-> bnetwork.h check FAILED, see $LOGFILE"
# TODO: uncomment after fixing the existing cases
#    FAILED="`expr $FAILED + 1`"
fi


###
# TEST: make sure common.h is always included first when included
log "... running common.h inclusion order check"

# limit our search to files containing common.h or system headers for
# small performance savings.
FILES="`grep -I -e '#[[:space:]]*include' $SRCFILES $INCFILES | grep -E 'common.h|<' | sed 's/:.*//g' | sort | uniq`"

# This is how to do it if we needed to accommodate screwy filepaths
#FILES="`echo \"$SRCFILES
#$INCFILES\" | while read file ; do
#    if test \"x\`grep -I -e '#[[:space:]]*include' $file | grep -E 'common.h|<'\`\" != \"x\" ; then
#	log $file
#    fi
#done`"

LEXERS="schema.h csg_parser.c csg_scanner.h obj_libgcv_grammar.cpp obj_obj-g_grammar.cpp obj_grammar.c obj_scanner.h obj_parser.h obj_rules.l obj_util.h obj_grammar.cpp obj_rules.cpp points_scan.c script.c"
EXEMPT="bnetwork.h bio.h config_win.h pstdint.h pinttypes.h uce-dirent.h ttcp.c optionparser.h $LEXERS"

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
	    log "ERROR: common.h include needs to be moved before: $MATCH"
	    FOUND=1
	    continue
	fi
    fi

    # test files that include system headers
    if test ! "x`grep -I -e '#[[:space:]]*include' $file | grep '<'`" = "x" ; then
	# non-system header is first?
	MATCH="`grep -n -I -e '#[[:space:]]*include' $file /dev/null | head -n 1 | grep -v '\"'`"
	if test ! "x$MATCH" = "x" ; then
	    log "ERROR: common.h needs to be included before: $MATCH"
	    FOUND=1
	    continue
	fi
    fi

done
if test "x$FOUND" = "x" ; then
    log "-> common.h check succeeded"
else
    log "-> common.h check FAILED, see $LOGFILE"
    FAILED="`expr $FAILED + 1`"
fi


###
# TEST: make sure consistent API standards are being used, using libbu
# functions where they replace or wrap a standard C function
log "... running API usage check"

# 1800 - printf
# 360 - free
# 203 - malloc
# 89 - calloc
# 21 - realloc
FOUND=
for func in fgets abort dirname getopt strcat strncat strlcat strcpy strdup strncpy strlcpy strcmp strcasecmp stricmp strncmp strncasecmp unlink rmdir remove qsort ; do
    log "Searching for $func ..."
    MATCH="`grep -n -e [^a-zA-Z0-9_:]$func\( $INCFILES $SRCFILES /dev/null`"

    # handle implementation exceptions
    MATCH="`echo \"$MATCH\" \
| sed 's/.*\/bomb\.c:.*abort.*//g' \
| sed 's/.*\/bu\/path\.h.*//' \
| sed 's/.*\/bu\/str\.h.*//' \
| sed 's/.*\/bu\/log\.h.*//' \
| sed 's/.*\/cursor\.c.*//g' \
| sed 's/.*\/CONFIG_CONTROL_DESIGN.*//' \
| sed 's/.*\/rt\/db4\.h.*strncpy.*//' \
| sed 's/.*\/file\.c:.*remove.*//' \
| sed 's/.*\/optionparser\.h.*//g' \
| sed 's/.*\/str\.c:.*strcasecmp.*//' \
| sed 's/.*\/str\.c:.*strcmp.*//' \
| sed 's/.*\/str\.c:.*strdup.*//' \
| sed 's/.*\/str\.c:.*strlcat.*//' \
| sed 's/.*\/str\.c:.*strlcpy.*//' \
| sed 's/.*\/str\.c:.*strncasecmp.*//' \
| sed 's/.*\/str\.c:.*strncat.*//' \
| sed 's/.*\/str\.c:.*strncmp.*//' \
| sed 's/.*\/str\.c:.*strncpy.*//' \
| sed 's/.*\/tests\/dirname\.c:.*dirname.*//' \
| sed 's/.*\/ttcp.c:.*//' \
| sed 's/.*\/vls\.c:.*strncpy.*//' \
| sed 's/.*\/wfobj\/obj_util\.cpp:.*strncpy.*//' \
| sed '/^$/d' \
`"

    if test "x$MATCH" != "x" ; then
	log "ERROR: Found `echo \"$MATCH\" | wc -l | awk '{print $1}'` instances of $func"
	log "$MATCH"
	FOUND=1
    fi
done

if test "x$FOUND" = "x" ; then
    log "-> API usage check succeeded"
else
    log "-> API usage check FAILED, see $LOGFILE"
    FAILED="`expr $FAILED + 1`"
fi


###
# TEST: make sure we don't get worse when it comes to testing for a
# platform feature vs. assuming a platform always had, has, and will
# have some characteristic feature.

log "... running platform symbol usage check"
PLATFORMS="
aix
apple
cygwin
darwin
freebsd
haiku
hpux
linux
mingw
msdos
qnx
sgi
solaris
sun
sunos
svr4
sysv
ultrix
unix
vms
win16
win32
win64
wine
winnt
"
# build up a single regex that matches all platforms.
# looks for cpp-style lines like "#if defined(PLATFORM)" or cmake-style "IF(PLATFORM)"
regex=
for platform in $PLATFORMS ; do
    platformupper="`echo $platform | tr 'a-z' 'A-Z'`"
    if test "x$regex" = "x" ; then
	regex="^[[:space:]#]*\(if\|IF\).*[[:space:]\(]_*\($platform\|$platformupper\)_*\([[:space:]\)]\|\$\)"
    else
	regex="$regex\|^[[:space:]#]*\(if\|IF\).*[[:space:]\(]_*\($platform\|$platformupper\)_*\([[:space:]\)]\|\$\)"
    fi
done

FOUND=0
grepcmd="grep -n -E"

MATCHES=
log "Searching headers ..."
for file in $INCFILES /dev/null ; do
    this="`eval $grepcmd $regex $file /dev/null | grep -v pstdint.h |grep -v pinttypes.h`"
    if test "x$this" != "x" ; then
	MATCHES="$MATCHES
$this"
    fi
done
if test "x$MATCHES" != "x" ; then
    cnt="`echo \"$MATCHES\" | sort | uniq | tail -n +2 | wc -l | awk '{print $1}'`"
    log "FIXME: Found $cnt header instances"
    log "$MATCHES
" | sort | uniq
    FOUND=`expr $FOUND + $cnt`
fi


MATCHES=
log "Searching sources ..."
for file in $SRCFILES /dev/null ; do
    this="`eval $grepcmd $regex $file /dev/null | grep -v uce-dirent.h | grep -v mime.c `"
    if test "x$this" != "x" ; then
	MATCHES="$MATCHES
$this"
    fi
done
if test "x$MATCHES" != "x" ; then
    cnt="`echo \"$MATCHES\" | sort | uniq | tail -n +2 | wc -l | awk '{print $1}'`"
    log "FIXME: Found $cnt source instances"
    log "$MATCHES
" | sort | uniq
    FOUND=`expr $FOUND + $cnt`
fi


MATCHES=
log "Searching build files ..."
for file in $BLDFILES /dev/null ; do
    this="`eval $grepcmd $regex $file /dev/null`"
    if test "x$this" != "x" ; then
	MATCHES="$MATCHES
$this"
    fi
done
if test "x$MATCHES" != "x" ; then
    cnt="`echo \"$MATCHES\" | sort | uniq | tail -n +2 | wc -l | awk '{print $1}'`"
    log "FIXME: Found $cnt build system instances"
    log "$MATCHES
" | sort | uniq
    FOUND=`expr $FOUND + $cnt`
fi


# make sure no more platform-specific defines are introduced than
# existed previously.  for cases where it "seems" necessary, find and
# fix some other case before adding more.  lets not increase this.
NEED_FIXING=166
if test $FOUND -lt `expr $NEED_FIXING + 1` ; then
    if test $FOUND -ne $NEED_FIXING ; then
	log "********************************************************"
	log "FIXME: UPDATE THE PLATFORM SYMBOL COUNT IN $0 - expected $NEED_FIXING, found $FOUND"
	log "********************************************************"
    fi
    log "-> platform symbol usage check succeeded"
else
    log "-> platform symbol usage check FAILED, see $LOGFILE - expected $NEED_FIXING, found $FOUND"
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
