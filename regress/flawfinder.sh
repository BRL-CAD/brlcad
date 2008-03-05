#!/bin/sh

TOPSRC="$1"
if test "x$TOPSRC" = "x" ; then
    TOPSRC="."
fi
if test ! -f "$TOPSRC/misc/flawfinder" ; then
    echo "Unable to find flawfinder in misc directory, aborting"
    exit 1
fi

rm -f flawfinder.log

echo "running flawfinder..."

${TOPSRC}/misc/flawfinder --followdotdir --minlevel=5 --singleline --neverignore --falsepositive --quiet ${TOPSRC}/src/[^o]* > flawfinder.log 2>&1

NUMBER_WRONG=0
if test "x`grep \"No hits found.\" flawfinder.log`" = "x" ; then
    NUMBER_WRONG="`grep \"Hits = \" flawfinder.log | awk '{print $3}'`"
fi

if test "x$NUMBER_WRONG" = "x0" ; then
    echo "-> flawfinder.sh succeeded"
else
    if test -f flawfinder.log ; then
	cat flawfinder.log
    fi
    echo "-> flawfinder.sh FAILED"
fi

echo "running bio.h public header check..."

if test ! -f "$TOPSRC/include/bio.h" ; then
    echo "Unable to find include/bio.h, aborting"
    exit 1
fi

# make sure nobody includes bio.h in a public header
FOUND="`grep '[^f]bio.h' $TOPSRC/include/*.h | grep -v 'include/bio.h'`"
if test "x$FOUND" = "x" ; then
    echo "-> bio.h check succeeded"
else
    echo "-> bio.h check FAILED"
fi

echo "running common.h inclusion order check..."

if test ! -f "$TOPSRC/include/common.h" ; then
    echo "Unable to find include/common.h, aborting"
    exit 1
fi

# make sure common.h is always included first
COMMONFILES="`find $TOPSRC -type f \( -name \*.c -o -name \*.cpp -o -name \*.cxx -o -name \*.h -o -name \*.y -o -name \*.l \) -not -regex '.*src/other.*' -not -regex '.*~' -not -regex '.*\.log' -not -regex '.*Makefile.*' -not -regex '.*cache.*' -not -regex '.*\.svn.*' -exec grep -n -I -e '#[[:space:]]*include' {} /dev/null \; | grep '\"common.h\"' | sed 's/:.*//g'`"

FOUND=
for file in $COMMONFILES ; do 
    if test -f "`echo $file | sed 's/\.c$/\.l/g'`" ; then 
	continue
    fi
    MATCH="`grep '#[[:space:]]*include' $file /dev/null | head -n 1 | grep -v '\"common.h\"' | sed 's/:.*//g'`"
    if test ! "x$MATCH" = "x" ; then
	echo "Header (common.h) out of order: $MATCH"
	FOUND=1
    fi
done

if test "x$FOUND" = "x" ; then
    echo "-> common.h check succeeded"
else
    echo "-> common.h check FAILED"
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
