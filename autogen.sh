#!/bin/sh
# script to prepare the sources
###

ARGS="$*"
PATH_TO_AUTOGEN="`dirname $0`"
MAJOR_VERSION=2
MINOR_VERSION=50

# environment check
###
_have_sed="`echo no | sed 's/no/yes/'`"
if [ $? = 0 ] ; then
  [ "x$_have_sed" = "xyes" ] && HAVE_SED=yes || HAVE_SED=no
fi

# version check
###
which autoconf > /dev/null 2>&1
[ $? = 0 ] && _autofound=yes || _autofound=no
_report_error=no
if [ ! "x$_autofound" = "xyes" ] ; then
    echo "ERROR:  Unable to locate the GNU Autotools."
    _report_error=yes
else
    _version_line="`autoconf -V | head -1`"
    if [ "x$HAVE_SED" = "xyes" ] ; then
	_maj_version="`echo $_version_line | sed 's/.*\([0-9]\)\..*/\1/'`"
	_min_version="`echo $_version_line | sed 's/.*\.\([0-9][0-9]\).*/\1/'`"
	if [ $? = 0 ] ; then
	    if [ $_maj_version -lt $MAJOR_VERSION ] ; then
		_report_error=yes
	    elif [ $_min_version -lt $MINOR_VERSION ] ; then
		_report_error=yes
	    fi
	fi
	echo "Found GNU Autotools version $_maj_version.$_min_version"
    else
	echo "Warning:  sed is not available to properly detect version of GNU Autotools"
    fi
    echo
fi
if [ "x$_report_error" = "xyes" ] ; then
    echo "ERROR:  To prepare the BRL-CAD build system from scratch,"
    echo "        At least version $MAJOR_VERSION.$MINOR_VERSION of the GNU Autotools must be installed."
    echo 
    echo "$PATH_TO_AUTOGEN/autogen.sh does not need to be run on the same machine that will"
    echo "run configure or make.  Either GNU Autotools will need to be installed"
    echo "or upgraded on this system, or $PATH_TO_AUTOGEN/autogen.sh must be run on the source"
    echo "code on another system and transferred to here. -- Cheers!"
    exit 1
fi

# prepare build via autoreconf or manually
###
_prev_path="`pwd`"
cd "$PATH_TO_AUTOGEN"
which autoreconf > /dev/null 2>&1
if [ $? = 0 ] ; then
    echo -n "Automatically preparing build ... "
    autoreconf --force --i
    [ ! $? = 0 ] && echo "ERROR: autoreconf failed" && exit 2
else
    echo -n "Preparing build ... "
    libtoolize --automake --copy --force || glibtoolize --automake --copy --force
    [ ! $? = 0 ] && echo "ERROR: libtoolize failed" && exit 2
    aclocal && [ ! $? = 0 ] && echo "ERROR: aclocal failed" && exit 2
    autoheader && [ ! $? = 0 ] && echo "ERROR: autoheader failed" && exit 2
    automake --add-missing --copy && [ ! $? = 0 ] && echo "ERROR: automake failed" && exit 2
    autoconf && [ ! $? = 0 ] && echo "ERROR: autoconf failed" && exit 2
fi
cd "$_prev_path"
echo "done"
echo

# check for help arg, and bypass running make
###
_help=no
if [ "x$HAVE_SED" = "xyes" ] ; then
    if [ "x`echo $ARGS | sed 's/.*help.*/help/'`" = "xhelp" ] ; then
	_help=yes
    fi
elif [ "x$ARGS" = "x--help" ] ; then
    _help=yes
fi
if [ "x$_help" = "xyes" ] ; then
    echo "Help was requested.  No configuration and compilation will be done."
    echo "Running: $PATH_TO_AUTOGEN/configure $ARGS"
    $PATH_TO_AUTOGEN/configure $ARGS
    exit 0
fi

# summarize.  actually build if arg was provided.
###
if [ "x$_help" = "xno" ] ; then
    if [ ! "x$ARGS" = "x" ] ; then
	echo "The BRL-CAD build system is now prepared.  Building here with:"
	echo "$PATH_TO_AUTOGEN/configure $ARGS"
	$PATH_TO_AUTOGEN/configure $ARGS
	[ ! $? = 0 ] && echo "ERROR: configure failed" && exit $?
	make
	[ ! $? = 0 ] && echo "ERROR: make failed" && exit $?
	exit 0
    fi
fi

echo "The BRL-CAD build system is now prepared.  To build here, run:"
echo "  $PATH_TO_AUTOGEN/configure"
echo "  make"
