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

SUITE="BRL-CAD"

AUTOCONF_MAJOR_VERSION=2
AUTOCONF_MINOR_VERSION=52
AUTOCONF_PATCH_VERSION=0

AUTOMAKE_MAJOR_VERSION=1
AUTOMAKE_MINOR_VERSION=6
AUTOMAKE_PATCH_VERSION=0

LIBTOOL_MAJOR_VERSION=1
LIBTOOL_MINOR_VERSION=4
LIBTOOL_PATCH_VERSION=3


##################
# argument check #
##################
HELP=no
[ "x$ARGS" = "x--help" ] && HELP=yes
QUIET=no
[ "x$ARGS" = "x--quiet" ] && QUIET=yes
[ "x$ARGS" = "x-Q" ] && QUIET=yes
VERBOSE=no
[ "x$ARGS" = "x--verbose" ] && VERBOSE=yes
[ "x$ARGS" = "x-V" ] && VERBOSE=yes


#####################
# environment check #
#####################
case `echo "testing\c"; echo 1,2,3`,`echo -n testing; echo 1,2,3` in
  *c*,-n*) ECHO_N= ECHO_C='
' ECHO_T='	' ;;
  *c*,*  ) ECHO_N=-n ECHO_C= ECHO_T= ;;
  *)       ECHO_N= ECHO_C='\c' ECHO_T= ;;
esac
TAIL_N=""
echo "test" | tail -n 1 2>/dev/null 1>&2
if [ $? = 0 ] ; then
    TAIL_N="n "
fi

VERBOSE_ECHO=:
ECHO=:
if [ "x$QUIET" = "xyes" ] ; then
  if [ "x$VERBOSE" = "xyes" ] ; then
    echo "Verbose output quelled by quiet option.  Further output disabled."
  fi
else
  ECHO=echo
  if [ "x$VERBOSE" = "xyes" ] ; then
    echo "Verbose output enabled"
    VERBOSE_ECHO=echo
  fi
fi

_have_sed="`$ECHO no | sed 's/no/yes/'`"
HAVE_SED=no
if [ $? = 0 ] ; then
  [ "x$_have_sed" = "xyes" ] && HAVE_SED=yes
fi
if [ "x$HAVE_SED" = "xno" ] ; then
  $ECHO "Warning:  sed does not appear to be available."
  $ECHO "GNU Autotools version checks are disabled."
fi


##########################
# autoconf version check #
##########################
_acfound=no
for AUTOCONF in autoconf ; do
  $VERBOSE_ECHO "Checking autoconf version: $AUTOCONF --version"
  $AUTOCONF --version > /dev/null 2>&1
  if [ $? = 0 ] ; then
    _acfound=yes
    break
  fi
done

_report_error=no
if [ ! "x$_acfound" = "xyes" ] ; then
  $ECHO "ERROR:  Unable to locate GNU Autoconf."
  _report_error=yes
else
  _version_line="`$AUTOCONF --version | head -${TAIL_N}1`"
  if [ "x$HAVE_SED" = "xyes" ] ; then
    _maj_version="`echo $_version_line | sed 's/.* \([0-9]\)\.[0-9][0-9].*/\1/'`"
    _maj_version="`echo $_maj_version | sed 's/.*[A-Z].*//'`"
    [ "x$_maj_version" = "x" ] && _maj_version=0
    _min_version="`echo $_version_line | sed 's/.* [0-9]\.\([0-9][0-9]\).*/\1/'`"
    _min_version="`echo $_min_version | sed 's/.*[A-Z].*//'`"
    [ "x$_min_version" = "x" ] && _min_version=0

    if [ $_maj_version -lt $AUTOCONF_MAJOR_VERSION ] ; then
      _report_error=yes
    elif [ $_maj_version -eq $AUTOCONF_MAJOR_VERSION ] ; then
      if [ $_min_version -lt $AUTOCONF_MINOR_VERSION ] ; then
	_report_error=yes
      fi
    fi

    $ECHO "Found GNU Autoconf version $_maj_version.$_min_version"
  fi
fi
if [ "x$_report_error" = "xyes" ] ; then
  $ECHO
  $ECHO "ERROR:  To prepare the ${SUITE} build system from scratch,"
  $ECHO "        at least version $AUTOCONF_MAJOR_VERSION.$AUTOCONF_MINOR_VERSION.$AUTOCONF_PATCH_VERSION of GNU Autoconf must be installed."
  $ECHO 
  $ECHO "$PATH_TO_AUTOGEN/autogen.sh does not need to be run on the same machine that will"
  $ECHO "run configure or make.  Either the GNU Autotools will need to be installed"
  $ECHO "or upgraded on this system, or $PATH_TO_AUTOGEN/autogen.sh must be run on the source"
  $ECHO "code on another system and then transferred to here. -- Cheers!"
  exit 1
fi


##########################
# automake version check #
##########################
_amfound=no
for AUTOMAKE in automake ; do
  $VERBOSE_ECHO "Checking automake version: $AUTOMAKE --version"
  $AUTOMAKE --version > /dev/null 2>&1
  if [ $? = 0 ] ; then
    _amfound=yes
    break
  fi
done

_report_error=no
if [ ! "x$_amfound" = "xyes" ] ; then
  $ECHO
  $ECHO "ERROR: Unable to locate GNU Automake."
  _report_error=yes
else
  _version_line="`$AUTOMAKE --version | head -${TAIL_N}1`"
  if [ "x$HAVE_SED" = "xyes" ] ; then
    _maj_version="`echo $_version_line | sed 's/.* \([0-9]\)\.[0-9].*/\1/'`"
    _maj_version="`echo $_maj_version | sed 's/.*[A-Z].*//'`"
    [ "x$_maj_version" = "x" ] && _maj_version=0
    _min_version="`echo $_version_line | sed 's/.* [0-9]\.\([0-9]\).*/\1/'`"
    _min_version="`echo $_min_version | sed 's/.*[A-Z].*//'`"
    [ "x$_min_version" = "x" ] && _min_version=0
    _pat_version="`echo $_version_line | sed 's/.* [0-9]\.[0-9][\.-]p*\([0-9]*\).*/\1/'`"
    _pat_version="`echo $_pat_version | sed 's/.*[A-Z].*//'`"
    [ "x$_pat_version" = "x" ] && _pat_version=0

    if [ $_maj_version -lt $AUTOMAKE_MAJOR_VERSION ] ; then
      _report_error=yes
    elif [ $_maj_version -eq $AUTOMAKE_MAJOR_VERSION ] ; then
      if [ $_min_version -lt $AUTOMAKE_MINOR_VERSION ] ; then
	_report_error=yes
      elif [ $_min_version -eq $AUTOMAKE_MINOR_VERSION ] ; then
	if [ $_pat_version -lt $AUTOMAKE_PATCH_VERSION ] ; then
	  _report_error=yes
	fi
      fi
    fi

    $ECHO "Found GNU Automake version $_maj_version.$_min_version.$_pat_version"
  fi
fi
if [ "x$_report_error" = "xyes" ] ; then
  $ECHO
  $ECHO "ERROR:  To prepare the ${SUITE} build system from scratch,"
  $ECHO "        at least version $AUTOMAKE_MAJOR_VERSION.$AUTOMAKE_MINOR_VERSION.$AUTOMAKE_PATCH_VERSION of GNU Automake must be installed."
  $ECHO 
  $ECHO "$PATH_TO_AUTOGEN/autogen.sh does not need to be run on the same machine that will"
  $ECHO "run configure or make.  Either the GNU Autotools will need to be installed"
  $ECHO "or upgraded on this system, or $PATH_TO_AUTOGEN/autogen.sh must be run on the source"
  $ECHO "code on another system and then transferred to here. -- Cheers!"
  exit 1
fi


########################
# check for autoreconf #
########################
HAVE_AUTORECONF=no
for AUTORECONF in autoreconf ; do
  $VERBOSE_ECHO "Checking autoreconf version: $AUTORECONF --version"
  $AUTORECONF --version > /dev/null 2>&1
  if [ $? = 0 ] ; then
    HAVE_AUTORECONF=yes
    break
  fi
done


########################
# check for libtoolize #
########################
HAVE_LIBTOOLIZE=yes
HAVE_ALTLIBTOOLIZE=no
LIBTOOLIZE=libtoolize
_ltfound=no
$VERBOSE_ECHO "Checking libtoolize version: $LIBTOOLIZE --version"
$LIBTOOLIZE --version > /dev/null 2>&1
if [ ! $? = 0 ] ; then
  HAVE_LIBTOOLIZE=no
  if [ "x$HAVE_AUTORECONF" = "xno" ] ; then
    $ECHO "Warning:  libtoolize does not appear to be available."
  else
    $ECHO "Warning:  libtoolize does not appear to be available.  This means that"
    $ECHO "autoreconf cannot be used."
  fi

  # look for some alternates
  for tool in glibtoolize libtoolize15 libtoolize13 ; do
    $VERBOSE_ECHO "Checking libtoolize alternate: $tool --version"
    _glibtoolize="`$tool --version > /dev/null 2>&1`"
    if [ $? = 0 ] ; then
      HAVE_ALTLIBTOOLIZE=yes
      LIBTOOLIZE="$tool"
      $ECHO 
      $ECHO "Fortunately, $tool was found which means that your system may simply"
      $ECHO "have a non-standard or incomplete GNU Autotools install.  If you have"
      $ECHO "sufficient system access, it may be possible to quell this warning by"
      $ECHO "running:"
      $ECHO
      sudo -V > /dev/null 2>&1
      if [ $? = 0 ] ; then
	_glti="`which $tool`"
	_gltidir="`dirname $_glti`"
	$ECHO "   sudo ln -s $_glti $_gltidir/libtoolize"
	$ECHO
      else
	$ECHO "   ln -s $glti $_gltidir/libtoolize"
	$ECHO 
	$ECHO "Run that as root or with proper permissions to the $_gltidir directory"
	$ECHO
      fi
      _ltfound=yes
      break
    fi
  done
else
  _ltfound=yes
fi

_report_error=no
if [ ! "x$_ltfound" = "xyes" ] ; then
  $ECHO
  $ECHO "ERROR: Unable to locate GNU Libtool."
  _report_error=yes
else
  _version_line="`$LIBTOOLIZE --version | head -${TAIL_N}1`"
  if [ "x$HAVE_SED" = "xyes" ] ; then
    _maj_version="`echo $_version_line | sed 's/.* \([0-9]\)\.[0-9].*/\1/'`"
    _maj_version="`echo $_maj_version | sed 's/.*[A-Z].*//'`"
    [ "x$_maj_version" = "x" ] && _maj_version=0
    _min_version="`echo $_version_line | sed 's/.* [0-9]\.\([0-9]\).*/\1/'`"
    _min_version="`echo $_min_version | sed 's/.*[A-Z].*//'`"
    [ "x$_min_version" = "x" ] && _min_version=0
    _pat_version="`echo $_version_line | sed 's/.* [0-9]\.[0-9][\.-]p*\([0-9]*\).*/\1/'`"
    _pat_version="`echo $_pat_version | sed 's/.*[A-Z].*//'`"
    [ "x$_pat_version" = "x" ] && _pat_version=0

    if [ $_maj_version -lt $LIBTOOL_MAJOR_VERSION ] ; then
      _report_error=yes
    elif [ $_maj_version -eq $LIBTOOL_MAJOR_VERSION ] ; then
      if [ $_min_version -lt $LIBTOOL_MINOR_VERSION ] ; then
	_report_error=yes
      elif [ $_min_version -eq $LIBTOOL_MINOR_VERSION ] ; then
	if [ $_pat_version -lt $LIBTOOL_PATCH_VERSION ] ; then
	  _report_error=yes
	fi
      fi
    fi

    $ECHO "Found GNU Libtool version $_maj_version.$_min_version.$_pat_version"
  fi
fi
if [ "x$_report_error" = "xyes" ] ; then
  $ECHO
  $ECHO "ERROR:  To prepare the ${SUITE} build system from scratch,"
  $ECHO "        at least version $LIBTOOL_MAJOR_VERSION.$LIBTOOL_MINOR_VERSION.$LIBTOOL_PATCH_VERSION of GNU Libtool must be installed."
  $ECHO 
  $ECHO "$PATH_TO_AUTOGEN/autogen.sh does not need to be run on the same machine that will"
  $ECHO "run configure or make.  Either the GNU Autotools will need to be installed"
  $ECHO "or upgraded on this system, or $PATH_TO_AUTOGEN/autogen.sh must be run on the source"
  $ECHO "code on another system and then transferred to here. -- Cheers!"
  exit 1
fi


#####################
# check for aclocal #
#####################
for ACLOCAL in aclocal ; do
  $VERBOSE_ECHO "Checking aclocal version: $ACLOCAL --version"
  $ACLOCAL --version > /dev/null 2>&1
  if [ $? = 0 ] ; then
    break
  fi
done


########################
# check for autoheader #
########################
for AUTOHEADER in autoheader ; do
  $VERBOSE_ECHO "Checking autoheader version: $AUTOHEADER --version"
  $AUTOHEADER --version > /dev/null 2>&1
  if [ $? = 0 ] ; then
    break
  fi
done


##############
# stash path #
##############
_prev_path="`pwd`"
cd "$PATH_TO_AUTOGEN"


#####################
# detect an aux dir #
#####################
_aux_dir=.
_configure_file=/dev/null
if test -f configure.ac ; then
  _configure_file=configure.ac
elif test -f configure.in ; then
  _configure_file=configure.in
fi
_aux_dir="`cat $_configure_file | grep AC_CONFIG_AUX_DIR | tail -${TAIL_N}1 | sed 's/^[ ]*AC_CONFIG_AUX_DIR(\(.*\)).*/\1/'`"
if test ! -d "$_aux_dir" ; then
  _aux_dir=.
else
  $VERBOSE_ECHO "Detected auxillary directory: $_aux_dir"
fi


##########################################
# make sure certain required files exist #
##########################################

for file in AUTHORS COPYING ChangeLog INSTALL NEWS README ; do
  if test ! -f $file ; then
    touch $file
  fi
done


############################################
# protect COPYING & INSTALL from overwrite #
############################################
for file in COPYING INSTALL ; do
  if test -f $file ; then
    if test -d "${_aux_dir}" ; then
      if test ! -f "${_aux_dir}/${file}.backup" ; then
	$VERBOSE_ECHO "cp -pf ${file} \"${_aux_dir}/${file}.backup\""
	cp -pf ${file} "${_aux_dir}/${file}.backup"
      fi
    fi
  fi
done


##################################################
# make sure certain generated files do not exist #
##################################################
for file in config.guess config.sub ltmain.sh ; do
  if test -f "${_aux_dir}/${file}" ; then
    $VERBOSE_ECHO "mv -f \"${_aux_dir}/${file}\" \"${_aux_dir}/${file}.backup\""
    mv -f "${_aux_dir}/${file}" "${_aux_dir}/${file}.backup"
  fi
done


#########################################
# list common misconfigured search dirs #
#########################################
SEARCH_DIRS=""
for dir in m4 ; do
# /usr/local/share/aclocal /sw/share/aclocal m4 ; do
  if [ -d $dir ] ; then
    $VERBOSE_ECHO "Found extra aclocal search directory: $dir"
    SEARCH_DIRS="$SEARCH_DIRS -B $dir"
  fi
done


############################################
# prepare build via autoreconf or manually #
############################################
reconfigure_manually=no
if [ "x$HAVE_AUTORECONF" = "xyes" ] && [ "x$HAVE_LIBTOOLIZE" = "xyes" ] ; then
  $ECHO
  $ECHO $ECHO_N "Automatically preparing build ... $ECHO_C"

  if [ "x$VERBOSE" = "xyes" ] ; then
    $VERBOSE_ECHO "$AUTORECONF $SEARCH_DIRS -i -f"
    $AUTORECONF $SEARCH_DIRS -i -f
  else
    $AUTORECONF $SEARCH_DIRS -i -f > /dev/null 2>&1
  fi
  if [ ! $? = 0 ] ; then
    $ECHO "Warning: $AUTORECONF failed"

    if test -f ltmain.sh ; then
      $ECHO "libtoolize being run by autoreconf is not creating ltmain.sh in the auxillary directory like it should"
    fi

    $ECHO "Attempting to run the configuration steps individually"
    reconfigure_manually=yes
  fi
else
  reconfigure_manually=yes
fi

###
# Steps taken are as follows:
#  aclocal
#  libtoolize --automake -c -f
#  aclocal          
#  autoconf -f
#  autoheader
#  automake -a -c -f
####
if [ "x$reconfigure_manually" = "xyes" ] ; then
  $ECHO
  $ECHO $ECHO_N "Preparing build ... $ECHO_C"

  $VERBOSE_ECHO "$ACLOCAL $SEARCH_DIRS"
  $ACLOCAL $SEARCH_DIRS

  [ ! $? = 0 ] && $ECHO "ERROR: $ACLOCAL failed" && exit 2
  if [ "x$HAVE_LIBTOOLIZE" = "xyes" ] ; then 
    $VERBOSE_ECHO "$LIBTOOLIZE --automake -c -f"
    $LIBTOOLIZE --automake -c -f
    [ ! $? = 0 ] && $ECHO "ERROR: $LIBTOOLIZE failed" && exit 2
  else
    if [ "x$HAVE_ALTLIBTOOLIZE" = "xyes" ] ; then
      $VERBOSE_ECHO "$LIBTOOLIZE --automake --copy --force"
      $LIBTOOLIZE --automake --copy --force
      [ ! $? = 0 ] && $ECHO "ERROR: $LIBTOOLIZE failed" && exit 2
    fi
  fi

  # re-run again as instructed by libtoolize
  $VERBOSE_ECHO "$ACLOCAL"
  $ACLOCAL

  # libtoolize might put ltmain.sh in the wrong place
  if test -f ltmain.sh ; then
    if test ! -f "${_aux_dir}/ltmain.sh" ; then
      $ECHO
      $ECHO "Warning:  $LIBTOOLIZE is creating ltmain.sh in the wrong directory"
      $ECHO
      $ECHO "Fortunately, the problem can be worked around by simply copying the"
      $ECHO "file to the appropriate location (${_aux_dir}/).  This has been done for you."
      $ECHO
      $VERBOSE_ECHO "cp ltmain.sh \"${_aux_dir}/ltmain.sh\""
      cp ltmain.sh "${_aux_dir}/ltmain.sh"
      $ECHO $ECHO_N "Continuing build preparation ... $ECHO_C"
    fi
  fi

  $VERBOSE_ECHO "$AUTOCONF -f"
  $AUTOCONF -f
  [ ! $? = 0 ] && $ECHO "ERROR: $AUTOCONF failed" && exit 2

  $VERBOSE_ECHO "$AUTOHEADER"
  $AUTOHEADER
  [ ! $? = 0 ] && $ECHO "ERROR: $AUTOHEADER failed" && exit 2

  $VERBOSE_ECHO "$AUTOMAKE -a -c -f"
  $AUTOMAKE -a -c -f
  [ ! $? = 0 ] && $ECHO "ERROR: $AUTOMAKE failed" && exit 2
fi


#########################################
# restore COPYING & INSTALL from backup #
#########################################
for file in COPYING INSTALL ; do
  if test -f $file ; then
    if test -f "${_aux_dir}/${file}.backup" ; then
      $VERBOSE_ECHO "cp -pf \"${_aux_dir}/${file}.backup\" ${file}"
      cp -pf "${_aux_dir}/${file}.backup" ${file}
    fi
  fi
done


################
# restore path #
################
cd "$_prev_path"
$ECHO "done"
$ECHO


###############################################
# check for help arg, and bypass running make #
###############################################
[ "x$HAVE_SED" = "xyes" ] && [ "x`echo $ARGS | sed 's/.*[hH][eE][lL][pP].*/help/'`" = "xhelp" ] && _help=yes
if [ "x$HELP" = "xyes" ] ; then
  echo "Help was requested.  No configuration and compilation will be done."
  echo "Running: $PATH_TO_AUTOGEN/configure $ARGS"
  $VERBOSE_ECHO "$PATH_TO_AUTOGEN/configure $ARGS"
  $PATH_TO_AUTOGEN/configure $ARGS
  exit 0
fi


#############
# summarize #
#############
$ECHO "The ${SUITE} build system is now prepared.  To build here, run:"
$ECHO "  $PATH_TO_AUTOGEN/configure"
$ECHO "  make"


# Local Variables:
# mode: sh
# tab-width: 8
# sh-basic-offset: 4
# sh-indentation: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
