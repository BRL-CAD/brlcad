#!/bin/sh
#			  V E R S . S H
#
# Update the "version" file for creation of a new "vers.c" from it.
# May be run in any subdirectory of the source tree.  Output goes to
# stdout now so you'll likely need to run:
#
#	sh vers.sh variable_name "this is a title" > vers.c
#
#  Optional args:
#	variable name to put version string in (default="version")
#	title
#
# @(#)$Header$ (BRL)

path_to_vers_sh="`dirname $0`"

if test $# -gt 0 ; then
	VARIABLE="$1"
	shift
else
	VARIABLE="version"
fi

if test $# -gt 0 ; then
	TITLE="$*"
else
	TITLE="Graphics Editor (MGED)"
fi

# Obtain RELEASE number
if test -r $path_to_vers_sh/../configure.ac ; then
	version_script=`grep VERSION $path_to_vers_sh/../configure.ac | grep -v SUBST | head -4`
	eval $version_script
	if test ! "x$BRLCAD_VERSION" = "x" ; then
		RELEASE="$BRLCAD_VERSION"
	else
		RELEASE='??.??.??'
	fi
else
	RELEASE='??.??.??'
fi

DIR=`pwd`
if test x$DIR = x; then DIR="/unknown"; fi

if test ! -w version ; then
	rm -f version; echo 0 > version; chmod 664 version
fi

awk '{version = $1 + 1; };END{printf "%d\n", version > "version"; }' < version

VERSION=`cat version`
DATE=`date`
PATH=$PATH:/usr/ucb:/usr/bsd
export PATH

# figure out what machine this is
HOST=$HOSTNAME
if test "x$HOST" = "x" ; then
    HOST="`hostname`"
fi
if test "x$HOST" = "x" ; then
    HOST="`uname -n`"
fi
if test "x$HOST" = "x" ; then
    HOST="//unknown//"
fi

# figure out who this is
if test "x$USER" = "x" ; then
	USER="$LOGNAME"
fi
if test "x$USER" = "x" ; then
	USER="$LOGIN"
fi
if test "x$USER" = "x" ; then
	USER="`whoami`"
fi
if test "x$USER" = "x" ; then
	USER="//unknown//"
fi

cat << EOF
char ${VARIABLE}[] = "\\
@(#) BRL-CAD Release ${RELEASE}   ${TITLE}\n\\
    ${DATE}, Compilation ${VERSION}\n\\
    ${USER}@${HOST}:${DIR}\n";
EOF
