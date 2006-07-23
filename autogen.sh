#!/bin/sh
#                        a u t o g e n . s h
#
# Copyright (c) 2005-2006 United States Government as represented by
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
# Basically, if everything is set up and installed correctly, the
# script will validate that minimum versions of the GNU Build System
# tools are installed, account for several common configuration
# issues, and then simply run autoreconf for you.
#
# If autoreconf fails, which can happen for many valid configurations,
# this script proceeds to run manual configuration steps effectively
# providing a POSIX shell script (mostly complete) reimplementation of
# autoreconf.
#
# The AUTORECONF, AUTOCONF, AUTOMAKE, LIBTOOLIZE, ACLOCAL, AUTOHEADER
# environment variables may be used to override the default detected
# applications.
#
# Author: Christopher Sean Morrison
#
######################################################################

# set to the name of this project
SUITE="BRL-CAD"

# set to minimum acceptible version of autoconf
AUTOCONF_MAJOR_VERSION=2
AUTOCONF_MINOR_VERSION=52
AUTOCONF_PATCH_VERSION=0

# set to minimum acceptible version of automake
AUTOMAKE_MAJOR_VERSION=1
AUTOMAKE_MINOR_VERSION=6
AUTOMAKE_PATCH_VERSION=0

# set to minimum acceptible version of libtool
LIBTOOL_MAJOR_VERSION=1
LIBTOOL_MINOR_VERSION=4
LIBTOOL_PATCH_VERSION=2


##################
# USAGE FUNCTION #
##################
usage ( ) {
    $ECHO "Usage: $AUTOGEN_SH [-h|--help] [-v|--verbose] [-q|--quiet] [--version]"
    $ECHO "    --help     Help on $NAME_OF_AUTOGEN usage"
    $ECHO "    --verbose  Verbose progress output"
    $ECHO "    --quiet    Quiet suppressed progress output"
    $ECHO "    --version  Only perform GNU Build System version checks"
    $ECHO
    $ECHO "Description: This script will validate that minimum versions of the"
    $ECHO "GNU Build System tools are installed and then run autoreconf for you."
    $ECHO "Should autoreconf fail, manual preparation steps will be run"
    $ECHO "potentially accounting for several common configuration issues.  The"
    $ECHO "AUTORECONF, AUTOCONF, AUTOMAKE, LIBTOOLIZE, ACLOCAL, AUTOHEADER"
    $ECHO "environment variables and corresponding _OPTIONS variables"
    $ECHO "(e.g. AUTORECONF_OPTIONS) may be used to override the default"
    $ECHO "automatic detection behavior."
    $ECHO
    $ECHO "autogen.sh build preparation script by Christopher Sean Morrison"
    $ECHO "POSIX shell script, BSD style license, copyright 2005-2006"
}


##################
# argument check #
##################
ARGS="$*"
PATH_TO_AUTOGEN="`dirname $0`"
NAME_OF_AUTOGEN="`basename $0`"
AUTOGEN_SH="$PATH_TO_AUTOGEN/$NAME_OF_AUTOGEN"

LIBTOOL_M4="${PATH_TO_AUTOGEN}/misc/libtool.m4"

if [ "x$HELP" = "x" ] ; then
    HELP=no
fi
if [ "x$QUIET" = "x" ] ; then
    QUIET=no
fi
if [ "x$VERBOSE" = "x" ] ; then
    VERBOSE=no
fi
if [ "x$VERSION_ONLY" = "x" ] ; then
    VERSION_ONLY=no
fi
if [ "x$AUTORECONF_OPTIONS" = "x" ] ; then
    AUTORECONF_OPTIONS="-i -f"
fi
if [ "x$AUTOCONF_OPTIONS" = "x" ] ; then
    AUTOCONF_OPTIONS="-f"
fi
if [ "x$AUTOMAKE_OPTIONS" = "x" ] ; then
    AUTOMAKE_OPTIONS="-a -c -f"
fi
ALT_AUTOMAKE_OPTIONS="-a -c"
if [ "x$LIBTOOLIZE_OPTIONS" = "x" ] ; then
    LIBTOOLIZE_OPTIONS="--automake -c -f"
fi
ALT_LIBTOOLIZE_OPTIONS="--automake --copy --force"
if [ "x$ACLOCAL_OPTIONS" = "x" ] ; then
    ACLOCAL_OPTIONS=""
fi
if [ "x$AUTOHEADER_OPTIONS" = "x" ] ; then
    AUTOHEADER_OPTIONS=""
fi
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
	    echo
	    usage
	    exit 1
	    ;;
    esac
done


#####################
# environment check #
#####################

# sanity check before recursions potentially begin
if [ ! "x$0" = "x$AUTOGEN_SH" ] ; then
    echo "INTERNAL ERROR: dirname/basename inconsistency: $0 != $AUTOGEN_SH"
    exit 1
fi

# force locale setting to C so things like date output as expected
LC_ALL=C

# commands that this script expects
for __cmd in echo head tail ; do
    echo "test" | $__cmd > /dev/null 2>&1
    if [ $? != 0 ] ; then
	echo "INTERNAL ERROR: '${__cmd}' command is required"
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

# allow a recursive run to disable further recursions
if [ "x$RUN_RECURSIVE" = "x" ] ; then
    RUN_RECURSIVE=yes
fi


################################################
# check for help arg and bypass version checks #
################################################
if [ "x$HAVE_SED" = "xyes" ] ; then
    if [ "x`echo $ARGS | sed 's/.*[hH][eE][lL][pP].*/help/'`" = "xhelp" ] ; then
	HELP=yes
    fi
fi
if [ "x$HELP" = "xyes" ] ; then
    usage
    $ECHO "---"
    $ECHO "Help was requested.  No preparation or configuration will be performed."
    exit 0
fi


##########################
# autoconf version check #
##########################
_acfound=no
if [ "x$AUTOCONF" = "x" ] ; then
    for AUTOCONF in autoconf ; do
	$VERBOSE_ECHO "Checking autoconf version: $AUTOCONF --version"
	$AUTOCONF --version > /dev/null 2>&1
	if [ $? = 0 ] ; then
	    _acfound=yes
	    break
	fi
    done
else
    _acfound=yes
    $ECHO "Using AUTOCONF environment variable override: $AUTOCONF"
fi

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
    $ECHO "$NAME_OF_AUTOGEN_SH does not need to be run on the same machine that will"
    $ECHO "run configure or make.  Either the GNU Autotools will need to be installed"
    $ECHO "or upgraded on this system, or $NAME_OF_AUTOGEN_SH must be run on the source"
    $ECHO "code on another system and then transferred to here. -- Cheers!"
    exit 1
fi


##########################
# automake version check #
##########################
_amfound=no
if [ "x$AUTOMAKE" = "x" ] ; then
    for AUTOMAKE in automake ; do
	$VERBOSE_ECHO "Checking automake version: $AUTOMAKE --version"
	$AUTOMAKE --version > /dev/null 2>&1
	if [ $? = 0 ] ; then
	    _amfound=yes
	    break
	fi
    done
else
    _amfound=yes
    $ECHO "Using AUTOMAKE environment variable override: $AUTOMAKE"
fi


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
    $ECHO "$NAME_OF_AUTOGEN_SH does not need to be run on the same machine that will"
    $ECHO "run configure or make.  Either the GNU Autotools will need to be installed"
    $ECHO "or upgraded on this system, or $NAME_OF_AUTOGEN_SH must be run on the source"
    $ECHO "code on another system and then transferred to here. -- Cheers!"
    exit 1
fi


########################
# check for autoreconf #
########################
HAVE_AUTORECONF=no
if [ "x$AUTORECONF" = "x" ] ; then
    for AUTORECONF in autoreconf ; do
	$VERBOSE_ECHO "Checking autoreconf version: $AUTORECONF --version"
	$AUTORECONF --version > /dev/null 2>&1
	if [ $? = 0 ] ; then
	    HAVE_AUTORECONF=yes
	    break
	fi
    done
else
    HAVE_AUTORECONF=yes
    $ECHO "Using AUTORECONF environment variable override: $AUTORECONF"
fi


########################
# check for libtoolize #
########################
HAVE_LIBTOOLIZE=yes
HAVE_ALT_LIBTOOLIZE=no
_ltfound=no
if [ "x$LIBTOOLIZE" = "x" ] ; then
    LIBTOOLIZE=libtoolize
    $VERBOSE_ECHO "Checking libtoolize version: $LIBTOOLIZE --version"
    $LIBTOOLIZE --version > /dev/null 2>&1
    if [ ! $? = 0 ] ; then
	HAVE_LIBTOOLIZE=no
	$ECHO
	if [ "x$HAVE_AUTORECONF" = "xno" ] ; then
	    $ECHO "Warning:  libtoolize does not appear to be available."
	else
	    $ECHO "Warning:  libtoolize does not appear to be available.  This means that"
	    $ECHO "the automatic build preparation via autoreconf will probably not work."
	    $ECHO "Preparing the build by running each step individually, however, should"
	    $ECHO "work and will be done automatically for you if autoreconf fails."
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
else
    _ltfound=yes
    $ECHO "Using LIBTOOLIZE environment variable override: $LIBTOOLIZE"
fi


############################
# libtoolize version check #
############################
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
    $ECHO "$NAME_OF_AUTOGEN_SH does not need to be run on the same machine that will"
    $ECHO "run configure or make.  Either the GNU Autotools will need to be installed"
    $ECHO "or upgraded on this system, or $NAME_OF_AUTOGEN_SH must be run on the source"
    $ECHO "code on another system and then transferred to here. -- Cheers!"
    exit 1
fi


#####################
# check for aclocal #
#####################
if [ "x$ACLOCAL" = "x" ] ; then
    for ACLOCAL in aclocal ; do
	$VERBOSE_ECHO "Checking aclocal version: $ACLOCAL --version"
	$ACLOCAL --version > /dev/null 2>&1
	if [ $? = 0 ] ; then
	    break
	fi
    done
else
    $ECHO "Using ACLOCAL environment variable override: $ACLOCAL"
fi


########################
# check for autoheader #
########################
if [ "x$AUTOHEADER" = "x" ] ; then
    for AUTOHEADER in autoheader ; do
	$VERBOSE_ECHO "Checking autoheader version: $AUTOHEADER --version"
	$AUTOHEADER --version > /dev/null 2>&1
	if [ $? = 0 ] ; then
	    break
	fi
    done
else
    $ECHO "Using AUTOHEADER environment variable override: $AUTOHEADER"
fi


#########################
# check if version only #
#########################
$VERBOSE_ECHO "Checking whether to only output version information"
if [ "x$VERSION_ONLY" = "xyes" ] ; then
    $ECHO "---"
    $ECHO "Version info requested.  No preparation or configuration will be performed."
    exit 0
fi


#######################
# INITIALIZE FUNCTION #
#######################
initialize ( ) {

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
	_aux_dir="`grep AC_CONFIG_AUX_DIR $_configure_file | grep -v '.*#.*AC_CONFIG_AUX_DIR' | tail -${TAIL_N}1 | sed 's/^[ 	]*AC_CONFIG_AUX_DIR(\(.*\)).*/\1/' | sed 's/.*\[\(.*\)\].*/\1/'`"
	if test ! -d "$_aux_dir" ; then
	    _aux_dir=.
	else
	    $VERBOSE_ECHO "Detected auxillary directory: $_aux_dir"
	fi
    fi


    ################################
    # detect a recursive configure #
    ################################
    _det_config_subdirs="`grep AC_CONFIG_SUBDIRS $_configure_file | grep -v '.*#.*AC_CONFIG_SUBDIRS' | sed 's/^[ 	]*AC_CONFIG_SUBDIRS(\(.*\)).*/\1/' | sed 's/.*\[\(.*\)\].*/\1/'`"
    CONFIG_SUBDIRS=""
    for dir in $_det_config_subdirs ; do
	if test -d "$dir" ; then
	    $VERBOSE_ECHO "Detected recursive configure directory: $dir"
	    CONFIG_SUBDIRS="$CONFIG_SUBDIRS $dir"
	fi
    done


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


    #######################################
    # remove the autom4te.cache directory #
    #######################################
    if test -d autom4te.cache ; then
	$VERBOSE_ECHO "Found an autom4te.cache directory, deleting it"
	$VERBOSE_ECHO "rm -rf autom4te.cache"
	rm -rf autom4te.cache
    fi
    
} # end of initialize()


##############
# initialize #
##############

# stash path
_prev_path="`pwd`"

# Before running autoreconf or manual steps, some prep detection work
# is necessary or useful.  Only needs to occur once per directory.
initialize


############################################
# prepare build via autoreconf or manually #
############################################
reconfigure_manually=no
if [ "x$HAVE_AUTORECONF" = "xyes" ] ; then
    $ECHO
    $ECHO $ECHO_N "Automatically preparing build ... $ECHO_C"

    $VERBOSE_ECHO "$AUTORECONF $SEARCH_DIRS $AUTORECONF_OPTIONS"
    autoreconf_output="`$AUTORECONF $SEARCH_DIRS $AUTORECONF_OPTIONS 2>&1`"
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

############################
# LIBTOOL_FAILURE FUNCTION #
############################
libtool_failure ( ) {
    _autoconf_output="$1"

    if [ "x$RUN_RECURSIVE" = "xno" ] ; then
	exit 5
    fi

    if test -f "$LIBTOOL_M4" ; then
	found_libtool="`$ECHO $_autoconf_output | grep AC_PROG_LIBTOOL`"
	if test ! "x$found_libtool" = "x" ; then
	    if test -f acinclude.m4 ; then
		if test ! -f acinclude.m4.backup ; then
		    $VERBOSE_ECHO cp acinclude.m4 acinclude.m4.backup
		    cp acinclude.m4 acinclude.m4.backup
		fi
	    fi
	    $VERBOSE_ECHO cat "$LIBTOOL_M4" >> acinclude.m4
	    cat "$LIBTOOL_M4" >> acinclude.m4

	    # don't keep doing this
	    RUN_RECURSIVE=no
	    export RUN_RECURSIVE

	    $ECHO
	    $ECHO "Restarting the configuration steps with a local libtool.m4"
	    $VERBOSE_ECHO sh $AUTOGEN_SH "$1" "$2" "$3" "$4" "$5" "$6" "$7" "$8" "$9"
	    sh "$AUTOGEN_SH" "$1" "$2" "$3" "$4" "$5" "$6" "$7" "$8" "$9"
	    exit $?
	fi
    fi
}


###########################
# MANUAL_AUTOGEN FUNCTION #
###########################
# Manual configuration steps taken are as follows:
#  aclocal [-I m4]
#  libtoolize --automake -c -f
#  aclocal [-I m4]
#  autoconf -f
#  autoheader
#  automake -a -c -f
####
manual_autogen ( ) {
    $ECHO
    $ECHO $ECHO_N "Preparing build ... $ECHO_C"

    $VERBOSE_ECHO "$ACLOCAL $SEARCH_DIRS $ACLOCAL_OPTIONS"
    aclocal_output="`$ACLOCAL $SEARCH_DIRS $ACLOCAL_OPTIONS 2>&1`"
    ret=$?
    $VERBOSE_ECHO "$aclocal_output"

    if [ ! $ret = 0 ] ; then $ECHO "ERROR: $ACLOCAL failed" && exit 2 ; fi
    if [ "x$HAVE_LIBTOOLIZE" = "xyes" ] ; then
	$VERBOSE_ECHO "$LIBTOOLIZE $LIBTOOLIZE_OPTIONS"
	libtoolize_output="`$LIBTOOLIZE $LIBTOOLIZE_OPTIONS 2>&1`"
	ret=$?
	$VERBOSE_ECHO "$libtoolize_output"

	if [ ! $ret = 0 ] ; then $ECHO "ERROR: $LIBTOOLIZE failed" && exit 2 ; fi
    else
	if [ "x$HAVE_ALT_LIBTOOLIZE" = "xyes" ] ; then
	    $VERBOSE_ECHO "$LIBTOOLIZE $ALT_LIBTOOLIZE_OPTIONS"
	    libtoolize_output="`$LIBTOOLIZE $ALT_LIBTOOLIZE_OPTIONS 2>&1`"
	    ret=$?
	    $VERBOSE_ECHO "$libtoolize_output"

	    if [ ! $ret = 0 ] ; then $ECHO "ERROR: $LIBTOOLIZE failed" && exit 2 ; fi
	fi
    fi

    # re-run again as instructed by libtoolize
    $VERBOSE_ECHO "$ACLOCAL $SEARCH_DIRS $ACLOCAL_OPTIONS"
    aclocal_output="`$ACLOCAL $SEARCH_DIRS $ACLOCAL_OPTIONS 2>&1`"
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
    $VERBOSE_ECHO "$AUTOCONF $AUTOCONF_OPTIONS"
    autoconf_output="`$AUTOCONF $AUTOCONF_OPTIONS 2>&1`"
    ret=$?
    $VERBOSE_ECHO "$autoconf_output"

    if [ ! $ret = 0 ] ; then
	# retry without the -f and with backwards support for missing macros
	configure_ac_changed="no"
	configure_ac_backupd="no"
	if test "x$HAVE_SED" = "xyes" ; then
	    if test ! -f configure.ac.backup ; then
		$VERBOSE_ECHO cp $_configure_file configure.ac.backup
		cp $_configure_file configure.ac.backup
		configure_ac_backupd="yes"
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
		# make sure we made the backup file
		if test "x$configure_ac_backupd" = "xyes" ; then
		    $VERBOSE_ECHO cp configure.ac.backup $_configure_file
		    cp configure.ac.backup $_configure_file
		fi
	    fi

	    # test if libtool is busted
	    libtool_failure "$autoconf_output"

	    # let the user know what went wrong
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

    $VERBOSE_ECHO "$AUTOHEADER $AUTOHEADER_OPTIONS"
    autoheader_output="`$AUTOHEADER $AUTOHEADER_OPTIONS 2>&1`"
    ret=$?
    $VERBOSE_ECHO "$autoheader_output"

    if [ ! $ret = 0 ] ; then $ECHO "ERROR: $AUTOHEADER failed" && exit 2 ; fi

    $VERBOSE_ECHO "$AUTOMAKE $AUTOMAKE_OPTIONS"
    automake_output="`$AUTOMAKE $AUTOMAKE_OPTIONS 2>&1`"
    ret=$?
    $VERBOSE_ECHO "$automake_output"

    if [ ! $ret = 0 ] ; then
	# retry without the -f
	$VERBOSE_ECHO
	$VERBOSE_ECHO "$AUTOMAKE $ALT_AUTOMAKE_OPTIONS"
	automake_output="`$AUTOMAKE $ALT_AUTOMAKE_OPTIONS 2>&1`"
	ret=$?
	$VERBOSE_ECHO "$automake_output"

	if [ ! $ret = 0 ] ; then

	    # test if libtool is busted
	    libtool_failure "$automake_output"

	    # let the user know what went wrong
	    cat <<EOF
$automake_output
EOF
	fi
	$ECHO "ERROR: $AUTOMAKE failed"
	exit 2
    fi
}


##################################
# run manual configuration steps #
##################################
if [ "x$reconfigure_manually" = "xyes" ] ; then
    
    # XXX if this is a recursive configure, manual steps don't work
    # yet .. assume it's the libtool/glibtool problem.
    if [ ! "x$CONFIG_SUBDIRS" = "x" ] ; then
	$ECHO "Running the configuration steps individually does not yet work"
	$ECHO "well with a recursive configure."
	if [ ! "x$HAVE_ALT_LIBTOOLIZE" = "xyes" ] ; then
	    exit 3
	fi
	$ECHO "Assuming this is a libtoolize problem..."
	export LIBTOOLIZE
	RUN_RECURSIVE=no
	export RUN_RECURSIVE
	$ECHO
	$ECHO "Restarting the configuration steps with LIBTOOLIZE set to $LIBTOOLIZE"
	$VERBOSE_ECHO sh $AUTOGEN_SH "$1" "$2" "$3" "$4" "$5" "$6" "$7" "$8" "$9"
	sh "$AUTOGEN_SH" "$1" "$2" "$3" "$4" "$5" "$6" "$7" "$8" "$9"
	exit $?
    fi

    # run the build configuration steps manually for this directory
    manual_autogen

    # for projects using recursive configure, run the build
    # configuration steps for the subdirectories.
    for dir in $CONFIG_SUBDIRS ; do
	$VERBOSE_ECHO "Processing recursive configure in $dir"
	cd "$_prev_path"
	cd "$dir"
	manual_autogen
    done
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


#########################
# restore and summarize #
#########################
cd "$_prev_path"
$ECHO "done"
$ECHO
$ECHO "The $SUITE build system is now prepared.  To build here, run:"
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
