#!/bin/sh

TOPSRC="$1"
if test "x$TOPSRC" = "x" ; then
    TOPSRC="."
fi
if test ! -f "$TOPSRC/misc/flawfinder" ; then
    /bin/echo "Unable to find flawfinder in misc directory, aborting"
    exit 1
fi

rm -f flawfinder.log

/bin/echo running flawfinder...

${TOPSRC}/misc/flawfinder --followdotdir --minlevel=5 --singleline --neverignore --quiet ${TOPSRC}/src/[^o]* > flawfinder.log 2>&1

NUMBER_WRONG=0
if test "x`grep \"No hits found.\" flawfinder.log`" = "x" ; then
    NUMBER_WRONG="`grep \"Hits = \" flawfinder.log | awk '{print $3}'`"
fi

if test "x$NUMBER_WRONG" = "x0" ; then
    /bin/echo '-> flawfinder.sh succeeded'
else
    if test -f flawfinder.log ; then
	/bin/cat flawfinder.log
    fi
    /bin/echo '-> flawfinder.sh FAILED'
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
