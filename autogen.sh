#!/bin/sh
# script to prepare the sources
###

ARGS="$*"
PATH_TO_AUTOGEN="`dirname $0`"

# prepare build via autoreconf or manually
###
_prev_path="`pwd`"
cd "$PATH_TO_AUTOGEN"
which autoreconf > /dev/null 2>&1
if [ $? = 0 ] ; then
    echo -n "Automatically preparing build ... "
    autoreconf --force --install
    [ ! $? = 0 ] && echo "autoreconf failed" && exit 1
else
    echo -n "Preparing build ... "
    libtoolize --automake --copy --force || glibtoolize --automake --copy --force
    [ ! $? = 0 ] && echo "libtoolize failed" && exit 1
    aclocal && [ ! $? = 0 ] && echo "aclocal failed" && exit 2
    autoheader && [ ! $? = 0 ] && echo "autoheader failed" && exit 3
    automake --add-missing --copy && [ ! $? = 0 ] && echo "automake failed" && exit 4
    autoconf && [ ! $? = 0 ] && echo "autoconf failed" && exit 5
fi
cd "$_prev_path"
echo "done"
echo

# check for help arg, and bypass running make
###
which sed > /dev/null 2>&1
if [ $? = 0 ] ; then
    if [ "x`echo $ARGS | sed 's/.*help.*/help/'`" = "xhelp" ] ; then
	echo "$PATH_TO_AUTOGEN/configure $ARGS"
	$PATH_TO_AUTOGEN/configure $ARGS
	exit
    fi
elif [ "x$ARGS" = "x--help" ] ; then
    echo "$PATH_TO_AUTOGEN/configure $ARGS"
    $PATH_TO_AUTOGEN/configure $ARGS
    exit
fi

# summarize.  actually build if arg was provided.
###
if [ ! "x$ARGS" = "x" ] ; then
    echo "The BRL-CAD build system is now prepared.  Building here with:"
    echo "$PATH_TO_AUTOGEN/configure $ARGS"
    $PATH_TO_AUTOGEN/configure $ARGS
    make
else
    echo "The BRL-CAD build system is now prepared.  To build here, run:"
    echo "  $PATH_TO_AUTOGEN/configure"
    echo "  make"
fi
