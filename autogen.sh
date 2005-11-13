#!/bin/sh
#                        a u t o g e n . s h
#
# Copyright (C) 2005 United States Government as represented by
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
# Script for automatically preparing the sources for compilation by
# performing the myrid of necessary steps.  The script attempts to
# detect proper version support, and outputs warnings about particular
# systems that have autotool peculiarities.
#
# Author: Christopher Sean Morrison
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
LIBTOOL_PATCH_VERSION=2

LIBTOOL_M4="${PATH_TO_AUTOGEN}/misc/libtool.m4"


##################
# argument check #
##################
HELP=no
QUIET=no
VERBOSE=no
VERSION_ONLY=no
for arg in $ARGS ; do
    case "x$arg" in
	x--help) HELP=yes ;;
	x-[hH]) HELP=yes ;;
	x--quiet) QUIET=yes ;;
	x-[qQ]) QUIET=yes ;;
	x--verbose) VERBOSE=yes ;;
	x-[vV]) VERBOSE=yes ;;
	x--version) VERSION_ONLY=yes ;;
	*)
	    echo "Unknown option: $arg"
	    exit 1
	    ;;
    esac
done


#####################
# environment check #
#####################

# force locale setting to C so things like date output as expected
LC_ALL=C

# commands that this script expects
for __cmd in echo head tail ; do
    echo "test" | $__cmd > /dev/null 2>&1
    if [ $? != 0 ] ; then
	echo "ERROR: '${__cmd}' command is required"
	exit 2
    fi
done

# determine the behavior of echo
case `echo "testing\c"; echo 1,2,3`,`echo -n testing; echo 1,2,3` in
    *c*,-n*) ECHO_N= ECHO_C='
' ECHO_T='	' ;;
    *c*,*  ) ECHO_N=-n ECHO_C= ECHO_T= ;;
    *)       ECHO_N= ECHO_C='\c' ECHO_T= ;;
esac

# determine the behavior of head
case "x`echo 'head' | head -n 1 2>&1`" in
    *xhead*) HEAD_N="n " ;;
    *) HEAD_N="" ;;
esac

# determine the behavior of tail
case "x`echo 'tail' | tail -n 1 2>&1`" in
    *xtail*) TAIL_N="n " ;;
    *) TAIL_N="" ;;
esac

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

_have_sed="`echo no | sed 's/no/yes/' 2>/dev/null`"
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
    _version_line="`$AUTOCONF --version | head -${HEAD_N}1`"
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
    _version_line="`$AUTOMAKE --version | head -${HEAD_N}1`"
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
HAVE_ALT_LIBTOOLIZE=no
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
    for tool in glibtoolize libtoolize15 libtoolize14 libtoolize13 ; do
	$VERBOSE_ECHO "Checking libtoolize alternate: $tool --version"
	_glibtoolize="`$tool --version > /dev/null 2>&1`"
	if [ $? = 0 ] ; then
	    $VERBOSE_ECHO "Found $tool --version"
	    _glti="`which $tool`"
	    if [ "x$_glti" = "x" ] ; then
		$VERBOSE_ECHO "Cannot find $tool with which"
		continue;
	    fi
	    if test ! -f "$_glti" ; then
		$VERBOSE_ECHO "Cannot use $tool, $_glti is not a file"
		continue;
	    fi
	    _gltidir="`dirname $_glti`"
	    if [ "x$_gltidir" = "x" ] ; then
		$VERBOSE_ECHO "Cannot find $tool path with dirname of $_glti"
		continue;
	    fi
	    if test ! -d "$_gltidir" ; then
		$VERBOSE_ECHO "Cannot use $tool, $_gltidir is not a directory"
		continue;
	    fi
	    HAVE_ALT_LIBTOOLIZE=yes
	    LIBTOOLIZE="$tool"
	    $ECHO
	    $ECHO "Fortunately, $tool was found which means that your system may simply"
	    $ECHO "have a non-standard or incomplete GNU Autotools install.  If you have"
	    $ECHO "sufficient system access, it may be possible to quell this warning by"
	    $ECHO "running:"
	    $ECHO
	    sudo -V > /dev/null 2>&1
	    if [ $? = 0 ] ; then
		$ECHO "   sudo ln -s $_glti $_gltidir/libtoolize"
		$ECHO
	    else
		$ECHO "   ln -s $_glti $_gltidir/libtoolize"
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
    _version_line="`$LIBTOOLIZE --version | head -${HEAD_N}1`"
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


#########################
# check if version only #
#########################
$VERBOSE_ECHO "Checking whether to only output version information"
if [ "x$VERSION_ONLY" = "xyes" ] ; then
    exit 0
fi


##############
# stash path #
##############
_prev_path="`pwd`"
cd "$PATH_TO_AUTOGEN"


#####################
# detect an aux dir #
#####################
_configure_file=/dev/null
if test -f configure.ac ; then
    _configure_file=configure.ac
elif test -f configure.in ; then
    _configure_file=configure.in
fi

_aux_dir=.
if test "x$HAVE_SED" = "xyes" ; then
    _aux_dir="`cat $_configure_file | grep AC_CONFIG_AUX_DIR | tail -${TAIL_N}1 | sed 's/^[ ]*AC_CONFIG_AUX_DIR(\(.*\)).*/\1/'`"
    if test ! -d "$_aux_dir" ; then
	_aux_dir=.
    else
	$VERBOSE_ECHO "Detected auxillary directory: $_aux_dir"
    fi
fi


##########################################
# make sure certain required files exist #
##########################################

for file in AUTHORS COPYING ChangeLog INSTALL NEWS README ; do
    if test ! -f $file ; then
	$VERBOSE_ECHO "Touching ${file} since it does not exist"
	touch $file
    fi
done


########################################################
# protect COPYING & INSTALL from overwrite by automake #
########################################################
for file in COPYING INSTALL ; do
    if test ! -f $file ; then
	continue
    fi
    $VERBOSE_ECHO "cp -pf ${file} \"${_aux_dir}/${file}.backup\""
    cp -pf ${file} "${_aux_dir}/${file}.backup"
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


############################
# search alternate m4 dirs #
############################
SEARCH_DIRS=""
for dir in m4 ; do
    if [ -d $dir ] ; then
	$VERBOSE_ECHO "Found extra aclocal search directory: $dir"
	SEARCH_DIRS="$SEARCH_DIRS -I $dir"
    fi
done


############################################
# prepare build via autoreconf or manually #
############################################
reconfigure_manually=no
if [ "x$HAVE_AUTORECONF" = "xyes" ] && [ "x$HAVE_LIBTOOLIZE" = "xyes" ] ; then
    $ECHO
    $ECHO $ECHO_N "Automatically preparing build ... $ECHO_C"

    $VERBOSE_ECHO "$AUTORECONF $SEARCH_DIRS -i -f"
    autoreconf_output="`$AUTORECONF $SEARCH_DIRS -i -f 2>&1`"
    ret=$?
    $VERBOSE_ECHO "$autoreconf_output"

    if [ ! $ret = 0 ] ; then
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
#  aclocal [-I m4]
#  libtoolize --automake -c -f
#  aclocal [-I m4]
#  autoconf -f
#  autoheader
#  automake -a -c -f
####
if [ "x$reconfigure_manually" = "xyes" ] ; then
    $ECHO
    $ECHO $ECHO_N "Preparing build ... $ECHO_C"

    $VERBOSE_ECHO "$ACLOCAL $SEARCH_DIRS"
    aclocal_output="`$ACLOCAL $SEARCH_DIRS 2>&1`"
    ret=$?
    $VERBOSE_ECHO "$aclocal_output"

    if [ ! $ret = 0 ] ; then
	if [ ! "x`echo $aclocal_output | grep cache`" = "x" ] ; then
	    # retry without an autom4te.cache directory if it exists

	    if test -d autom4te.cache ; then
		if test -d autom4te.cache.backup ; then
		    $ECHO "ERROR: $ACLOCAL failed"
		    $VERBOSE_ECHO
		    $VERBOSE_ECHO "Unable to retry aclocal without autom4te.cache"
		    $VERBOSE_ECHO "There is an autom4te.cache.backup directory in the way"
		    $VERBOSE_ECHO "Suggest running: rm -rf *cache*"
		    exit 2
		else
		    $VERBOSE_ECHO "mv autom4te.cache autom4te.cache.backup"
		    mv autom4te.cache autom4te.cache.backup
		fi
		$VERBOSE_ECHO "Retrying aclocal without the autom4te.cache directory"
		$VERBOSE_ECHO "$ACLOCAL $SEARCH_DIRS"
		aclocal_output="`$ACLOCAL $SEARCH_DIRS 2>&1`"
		ret=$?
		$VERBOSE_ECHO "$aclocal_output"

		if [ ! $ret = 0 ] ; then
		    # still did not work after we removed the backup, so restore it
		    $VERBOSE_ECHO "mv autom4te.cache.backup autom4te.cache"
		    mv autom4te.cache.backup autom4te.cache
		else
		    # worked after we removed the backup, so remove the backup
		    $VERBOSE_ECHO "rm -rf autom4te.cache.backup"
		    rm -rf autom4te.cache.backup
		fi
	    fi
	fi
    fi

    if [ ! $ret = 0 ] ; then $ECHO "ERROR: $ACLOCAL failed" && exit 2 ; fi
    if [ "x$HAVE_LIBTOOLIZE" = "xyes" ] ; then
	$VERBOSE_ECHO "$LIBTOOLIZE --automake -c -f"
	libtoolize_output="`$LIBTOOLIZE --automake -c -f 2>&1`"
	ret=$?
	$VERBOSE_ECHO "$libtoolize_output"

	if [ ! $ret = 0 ] ; then $ECHO "ERROR: $LIBTOOLIZE failed" && exit 2 ; fi
    else
	if [ "x$HAVE_ALT_LIBTOOLIZE" = "xyes" ] ; then
	    $VERBOSE_ECHO "$LIBTOOLIZE --automake --copy --force"
	    libtoolize_output="`$LIBTOOLIZE --automake --copy --force 2>&1`"
	    ret=$?
	    $VERBOSE_ECHO "$libtoolize_output"

	    if [ ! $ret = 0 ] ; then $ECHO "ERROR: $LIBTOOLIZE failed" && exit 2 ; fi
	fi
    fi

    # re-run again as instructed by libtoolize
    $VERBOSE_ECHO "$ACLOCAL $SEARCH_DIRS"
    aclocal_output="`$ACLOCAL $SEARCH_DIRS 2>&1`"
    ret=$?
    $VERBOSE_ECHO "$aclocal_output"

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

    $VERBOSE_ECHO
    $VERBOSE_ECHO "$AUTOCONF -f"
    autoconf_output="`$AUTOCONF -f 2>&1`"
    ret=$?
    $VERBOSE_ECHO "$autoconf_output"

    if [ ! $ret = 0 ] ; then
 	# retry without an autom4te.cache directory if it exists
	if [ ! "x`echo $autoconf_output | grep autom4te`" = "x" ] ; then
	    if test -d autom4te.cache ; then
		if test -d autom4te.cache.backup ; then
		    $ECHO "ERROR: $AUTOCONF failed"
		    $VERBOSE_ECHO
		    $VERBOSE_ECHO "Unable to retry autoconf without autom4te.cache"
		    $VERBOSE_ECHO "There is an autom4te.cache.backup directory in the way"
		    $VERBOSE_ECHO "Suggest running: rm -rf *cache*"
		    exit 2
		else
		    $VERBOSE_ECHO "mv autom4te.cache autom4te.cache.backup"
		    mv autom4te.cache autom4te.cache.backup
		fi
		$VERBOSE_ECHO "Retrying autoconf without the autom4te.cache directory"
		$VERBOSE_ECHO "$AUTOCONF -f"
		autoconf_output="`$AUTOCONF -f 2>&1`"
		ret=$?
		$VERBOSE_ECHO "$autoconf_output"

		if [ ! $ret = 0 ] ; then
		    # still did not work after we removed the backup, so restore it
		    $VERBOSE_ECHO "mv autom4te.cache.backup autom4te.cache"
		    mv autom4te.cache.backup autom4te.cache
		else
		    # worked after we removed the backup, so remove the backup
		    $VERBOSE_ECHO "rm -rf autom4te.cache.backup"
		    rm -rf autom4te.cache.backup
		fi
	    fi
	fi
    fi

    if [ ! $ret = 0 ] ; then
	# retry without the -f and with backwards support for missing macros
	configure_ac_changed="no"
	if test "x$HAVE_SED" = "xyes" ; then
	    if test ! -f configure.ac.backup ; then
		$VERBOSE_ECHO cp $_configure_file configure.ac.backup
		cp $_configure_file configure.ac.backup
	    fi

	    ac2_59_macros="AC_C_RESTRICT AC_INCLUDES_DEFAULT AC_LANG_ASSERT AC_LANG_WERROR AS_SET_CATFILE"
	    ac2_55_macros="AC_COMPILER_IFELSE AC_FUNC_MBRTOWC AC_HEADER_STDBOOL AC_LANG_CONFTEST AC_LANG_SOURCE AC_LANG_PROGRAM AC_LANG_CALL AC_LANG_FUNC_TRY_LINK AC_MSG_FAILURE AC_PREPROC_IFELSE"
	    ac2_54_macros="AC_C_BACKSLASH_A AC_CONFIG_LIBOBJ_DIR AC_GNU_SOURCE AC_PROG_EGREP AC_PROG_FGREP AC_REPLACE_FNMATCH AC_FUNC_FNMATCH_GNU AC_FUNC_REALLOC AC_TYPE_MBSTATE_T"

	    macros_to_search=""
	    if [ $AUTOCONF_MAJOR_VERSION -lt 2 ] ; then
		macros_to_search="$ac2_59_macros $ac2_55_macros $ac2_54_macros"
	    else
		if [ $AUTOCONF_MINOR_VERSION -lt 54 ] ; then
		    macros_to_search="$ac2_59_macros $ac2_55_macros $ac2_54_macros"
		elif [ $AUTOCONF_MINOR_VERSION -lt 55 ] ; then
		    macros_to_search="$ac2_59_macros $ac2_55_macros"
		elif [ $AUTOCONF_MINOR_VERSION -lt 59 ] ; then
		    macros_to_search="$ac2_59_macros"
		fi
	    fi

	    if [ -w $_configure_file ] ; then
		for feature in $macros_to_search ; do
		    $VERBOSE_ECHO "Searching for $feature in $_configure_file with sed"
		    sed "s/^\($feature.*\)$/dnl \1/g" < $_configure_file > configure.ac.sed
		    if [ ! "x`cat $_configure_file`" = "x`cat configure.ac.sed`" ] ; then
			$VERBOSE_ECHO cp configure.ac.sed $_configure_file
			cp configure.ac.sed $_configure_file
			if [ "x$configure_ac_changed" = "xno" ] ; then
			    configure_ac_changed="$feature"
			else
			    configure_ac_changed="$feature $configure_ac_changed"
			fi
		    fi
		    rm -f configure.ac.sed
		done
	    else
		$VERBOSE_ECHO "$_configure_file is not writable so not attempting to edit"
	    fi
	fi
	$VERBOSE_ECHO
	$VERBOSE_ECHO "$AUTOCONF"
	autoconf_output="`$AUTOCONF 2>&1`"
	ret=$?
	$VERBOSE_ECHO "$autoconf_output"

	if [ ! $ret = 0 ] ; then

	    # failed so restore the backup
	    if test -f configure.ac.backup ; then
		$VERBOSE_ECHO cp configure.ac.bacukp $_configure_file
		cp configure.ac.backup $_configure_file
	    fi

	    # test if libtool is busted
	    if test -f "$LIBTOOL_M4" ; then
		found_libtool="`$ECHO $autoconf_output | grep AC_PROG_LIBTOOL`"
		if test ! "x$found_libtool" = "x" ; then
		    if test -f acinclude.m4 ; then
			if test ! -f acinclude.m4.backup ; then
			    $VERBOSE_ECHO cp acinclude.m4 acinclude.m4.backup
			    cp acinclude.m4 acinclude.m4.backup
			fi
		    fi
		    $VERBOSE_ECHO cat "$LIBTOOL_M4" >> acinclude.m4
		    cat "$LIBTOOL_M4" >> acinclude.m4

		    $ECHO
		    $ECHO "Restarting the configuration steps with a local libtool.m4"

		    $VERBOSE_ECHO sh $0 "$1" "$2" "$3" "$4" "$5" "$6" "$7" "$8" "$9"
		    sh "$0" "$1" "$2" "$3" "$4" "$5" "$6" "$7" "$8" "$9"
		    exit $?
		fi
	    fi
	    cat <<EOF
$autoconf_output
EOF
	    $ECHO "ERROR: $AUTOCONF failed"
	    exit 2

	else
	    # autoconf sans -f and possibly sans unsupported options succeeded so warn verbosely

	    if [ ! "x$configure_ac_changed" = "xno" ] ; then
		$ECHO
		$ECHO "Warning:  Unsupported macros were found and removed from $_configure_file"
		$ECHO
		$ECHO "The $_configure_file file was edited in an attempt to successfully run"
		$ECHO "autoconf by commenting out the unsupported macros.  Since you are"
		$ECHO "reading this, autoconf succeeded after the edits were made.  The"
		$ECHO "original $_configure_file is saved as configure.ac.backup but you should"
		$ECHO "consider either increasing the minimum version of autoconf required"
		$ECHO "by this script or removing the following macros from $_configure_file:"
		$ECHO
		$ECHO "$configure_ac_changed"
		$ECHO
		$ECHO $ECHO_N "Continuing build preparation ... $ECHO_C"
	    fi
	fi
    fi

    $VERBOSE_ECHO "$AUTOHEADER"
    autoheader_output="`$AUTOHEADER 2>&1`"
    ret=$?
    $VERBOSE_ECHO "$autoheader_output"

    if [ ! $ret = 0 ] ; then $ECHO "ERROR: $AUTOHEADER failed" && exit 2 ; fi

    $VERBOSE_ECHO "$AUTOMAKE -a -c -f"
    automake_output="`$AUTOMAKE -a -c -f 2>&1`"
    ret=$?
    $VERBOSE_ECHO "$automake_output"

    if [ ! $ret = 0 ] ; then
	# retry without the -f
	$VERBOSE_ECHO
	$VERBOSE_ECHO "$AUTOMAKE -a -c"
	automake_output="`$AUTOMAKE -a -c 2>&1`"
	ret=$?
	$VERBOSE_ECHO "$automake_output"

	if [ ! $ret = 0 ] ; then
	    if test -f "$LIBTOOL_M4" ; then
		found_libtool="`$ECHO $automake_output | grep AC_PROG_LIBTOOL`"
		if test ! "x$found_libtool" = "x" ; then
		    if test -f acinclude.m4 ; then
			if test ! -f acinclude.m4.backup ; then
			    $VERBOSE_ECHO cp acinclude.m4 acinclude.m4.backup
			    cp acinclude.m4 acinclude.m4.backup
			fi
		    fi
		    $VERBOSE_ECHO cat "$LIBTOOL_M4" >> acinclude.m4
		    cat "$LIBTOOL_M4" >> acinclude.m4

		    $ECHO
		    $ECHO "Restarting the configuration steps with a local libtool.m4"

		    $VERBOSE_ECHO sh $0 "$1" "$2" "$3" "$4" "$5" "$6" "$7" "$8" "$9"
		    sh "$0" "$1" "$2" "$3" "$4" "$5" "$6" "$7" "$8" "$9"
		    exit $?
		fi
	    fi
	    cat <<EOF
$automake_output
EOF
	fi
	$ECHO "ERROR: $AUTOMAKE failed"
	exit 2
    fi
fi


#########################################
# restore COPYING & INSTALL from backup #
#########################################
spacer=no
for file in COPYING INSTALL ; do
    curr="$file"
    if test ! -f "$curr" ; then
	continue
    fi
    back="${_aux_dir}/${file}.backup"
    if test ! -f "$back" ; then
	continue
    fi

    # full contents for comparison
    current="`cat $curr`"
    backup="`cat $back`"
    if test "x$current" != "x$backup" ; then
	if test "x$spacer" = "xno" ; then
	    $VERBOSE_ECHO
	    spacer=yes
	fi
	# restore the backup
	$VERBOSE_ECHO "Restoring $file from backup (automake -f likely clobbered it)"
	$VERBOSE_ECHO "cp -pf \"$back\" \"${curr}\""
	cp -pf "$back" "$curr"
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
if [ "x$HAVE_SED" = "xyes" ] ; then
    if [ "x`echo $ARGS | sed 's/.*[hH][eE][lL][pP].*/help/'`" = "xhelp" ] ; then
	_help=yes
    fi
fi
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
