#!/bin/sh

TOPSRC="$1"
if test "x$TOPSRC" = "x" ; then
    TOPSRC="."
fi
if test ! -f "$TOPSRC/misc/flawfinder" ; then
    echo "Unable to find flawfinder in misc directory, aborting"
    exit 1
fi

HAVE_PYTHON=no
if test "`env python -V 2>&1 | awk '{print $1}'`" = "xPython" ; then
    HAVE_PYTHON=yes
fi

if test "x$HAVE_PYTHON" = "x" ; then

    echo "running flawfinder..."

    rm -f flawfinder.log
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

fi # HAVE_PYTHON


exit $NUMBER_WRONG

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
