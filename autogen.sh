#!/bin/sh
#                        a u t o g e n . s h
#
# Script for automatically preparing the sources for compilation by
# performing the myrid of necessary steps.  The script attempts to
# detect proper version support, and outputs warnings about particular
# systems that have autotool peculiarities.
#
# Author: Christopher Sean Morrison
# This script is in the public domain
#
######################################################################

ARGS="$*"
PATH_TO_AUTOGEN="`dirname $0`"

AUTOCONF_MAJOR_VERSION=2
AUTOCONF_MINOR_VERSION=50
AUTOCONF_PATCH_VERSION=0

AUTOMAKE_MAJOR_VERSION=1
AUTOMAKE_MINOR_VERSION=5
AUTOMAKE_PATCH_VERSION=0


#####################
# environment check #
#####################
_have_sed="`echo no | sed 's/no/yes/'`"
HAVE_SED=no
if [ $? = 0 ] ; then
  [ "x$_have_sed" = "xyes" ] && HAVE_SED=yes
fi


#################
# version check #
#################
autoconf -V > /dev/null 2>&1
[ $? = 0 ] && _autofound=yes || _autofound=no
_report_error=no
if [ ! "x$_autofound" = "xyes" ] ; then
  echo "ERROR:  Unable to locate GNU Autoconf."
  _report_error=yes
else
  _version_line="`autoconf -V | head -1`"
  if [ "x$HAVE_SED" = "xyes" ] ; then
    _maj_version="`echo $_version_line | sed 's/.*\([0-9]\)\..*/\1/'`"
    _min_version="`echo $_version_line | sed 's/.*\.\([0-9][0-9]\).*/\1/'`"
    if [ $? = 0 ] ; then
      if [ $_maj_version -lt $AUTOCONF_MAJOR_VERSION ] ; then
	_report_error=yes
      elif [ $_min_version -lt $AUTOCONF_MINOR_VERSION ] ; then
	_report_error=yes
      fi
    fi
    echo "Found GNU Autoconf version $_maj_version.$_min_version"
  else
    echo "Warning:  sed is not available to properly detect version of GNU Autoconf"
  fi
  echo
fi
if [ "x$_report_error" = "xyes" ] ; then
  echo "ERROR:  To prepare the BRL-CAD build system from scratch,"
  echo "        At least version $AUTOCONF_MAJOR_VERSION.$AUTOCONF_MINOR_VERSION of GNU Autoconf must be installed."
  echo 
  echo "$PATH_TO_AUTOGEN/autogen.sh does not need to be run on the same machine that will"
  echo "run configure or make.  Either the GNU Autotools will need to be installed"
  echo "or upgraded on this system, or $PATH_TO_AUTOGEN/autogen.sh must be run on the source"
  echo "code on another system and transferred to here. -- Cheers!"
  exit 1
fi


########################
# check for autoreconf #
########################
HAVE_AUTORECONF=yes
autoreconf -V > /dev/null 2>&1
if [ ! $? = 0 ] ; then
  HAVE_AUTORECONF=no
fi


########################
# check for libtoolize #
########################
HAVE_LIBTOOLIZE=yes
HAVE_GLIBTOOLIZE=no
libtoolize --version > /dev/null 2>&1
if [ ! $? = 0 ] ; then
  HAVE_LIBTOOLIZE=no
  if [ "x$HAVE_AUTORECONF" = "xno" ] ; then
    echo "Warning:  libtoolize does not appear to be available."
  else
    echo "Warning:  libtoolize does not appear to be available.  This means that"
    echo "autoreconf cannot be used."
  fi

  _glibtoolize="`glibtoolize --version 2>/dev/null`"
  if [ $? = 0 ] ; then
    HAVE_GLIBTOOLIZE=yes
    echo 
    echo "Fortunately, glibtoolize was found which means that your system may simply"
    echo "have a non-standard or incomplete GNU Autotools install.  If you have"
    echo "sufficient system access, it may be possible to quell this warning by"
    echo "running:"
    echo
    sudo -V > /dev/null 2>&1
    if [ $? = 0 ] ; then
      _glti="`which glibtoolize`"
      _gltidir="`dirname $_glti`"
      echo "   sudo ln -s $_glti $_gltidir/libtoolize"
    else
      echo "   ln -s $glti $_gltidir/libtoolize"
      echo 
      echo "Run that as root or with proper permissions to the $_gltidir directory"
    fi
    echo
  fi
fi


##############
# stash path #
##############
_prev_path="`pwd`"
cd "$PATH_TO_AUTOGEN"


##########################################
# make sure certain required files exist #
##########################################

if ! test -f AUTHORS ; then
  touch AUTHORS
fi
if ! test -f ChangeLog ; then
  touch ChangeLog
fi
if ! test -f INSTALL ; then
  touch INSTALL
fi
if ! test -f NEWS ; then
  touch NEWS
fi


##################################################
# make sure certain generated files do not exist #
##################################################

if test -f config.guess ; then
  mv -f config.guess config.guess.backup
fi
if test -f config.sub ; then
  mv -f config.sub config.sub.backup
fi
if test -f ltmain.sh ; then
  mv -f ltmain.sh ltmain.sh.backup
fi
if test -f libtool ; then
  mv -f libtool libtaol.backup
fi


############################################
# prepare build via autoreconf or manually #
############################################
if [ "x$HAVE_AUTORECONF" = "xyes" ] && [ "x$HAVE_LIBTOOLIZE" = "xyes" ] ; then
  echo -n "Automatically preparing build ..."
  autoreconf -i
  [ ! $? = 0 ] && echo "ERROR: autoreconf failed" && exit 2
else
  echo -n "Preparing build ..."
  if [ "x$HAVE_LIBTOOLIZE" = "xyes" ] ; then 
    libtoolize --automake -c -f
    [ ! $? = 0 ] && echo "ERROR: libtoolize failed" && exit 2
  else
    [ "x$HAVE_GLIBTOOLIZE" = "xyes" ] && glibtoolize --automake --copy --force
    [ ! $? = 0 ] && echo "ERROR: glibtoolize failed" && exit 2
  fi
  aclocal
  [ ! $? = 0 ] && echo "ERROR: aclocal failed" && exit 2
  autoheader 
  [ ! $? = 0 ] && echo "ERROR: autoheader failed" && exit 2
  automake -a -c 
  [ ! $? = 0 ] && echo "ERROR: automake failed" && exit 2
  autoconf -f
  [ ! $? = 0 ] && echo "ERROR: autoconf failed" && exit 2
fi


################
# restore path #
################
cd "$_prev_path"
echo "done"
echo


###############################################
# check for help arg, and bypass running make #
###############################################
_help=no
[ "x$HAVE_SED" = "xyes" ] && [ "x`echo $ARGS | sed 's/.*help.*/help/'`" = "xhelp" ] && _help=yes
[ "x$ARGS" = "x--help" ] && _help=yes
if [ "x$_help" = "xyes" ] ; then
  echo "Help was requested.  No configuration and compilation will be done."
  echo "Running: $PATH_TO_AUTOGEN/configure $ARGS"
  $PATH_TO_AUTOGEN/configure $ARGS
  exit 0
fi


##################################################
# summarize.  actually build if arg was provided #
##################################################
if [ ! "x$ARGS" = "x" ] ; then
  echo "The BRL-CAD build system is now prepared.  Building here with:"
  echo "$PATH_TO_AUTOGEN/configure $ARGS"
  $PATH_TO_AUTOGEN/configure $ARGS
  [ ! $? = 0 ] && echo "ERROR: configure failed" && exit $?
  make
  [ ! $? = 0 ] && echo "ERROR: make failed" && exit $?
  exit 0
fi

echo "The BRL-CAD build system is now prepared.  To build here, run:"
echo "  $PATH_TO_AUTOGEN/configure"
echo "  make"

# Local Variables: ***
# mode: sh ***
# tab-width: 8 ***
# sh-basic-offset: 2 ***
# sh-indentation: 2 ***
# indent-tabs-mode: t ***
# End: ***
# ex: shiftwidth=2 tabstop=8
