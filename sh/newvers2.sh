#!/bin/sh
#			N E W V E R S . S H
#
# Update the "version" file, and create a new "vers.c" from it.
# May be run in any (first-level) subdirectory of the source tree.
#
#  Optional args:
#	variable name to put version string in (default="version")
#	title
#
# @(#)$Header$ (BRL)

if test $# -gt 0
then
	VARIABLE="$1"
	shift
else
	VARIABLE="version"
fi

if test $# -gt 0
then
	TITLE="$*"
else
	TITLE="Graphics Editor (MGED)"
fi

eval `machinetype.sh -b`	# sets MACHINE, UNIXTYPE, etc

# Obtain RELEASE, RCS_REVISION, and REL_DATE
if test -r ../gen.sh
then
	# XXX The following line causes DEC Alpha's /bin/sh to dump core.
	eval `grep '^RELEASE=' ../gen.sh`
	##RELEASE=4.4;	RCS_REVISION=11;	# Uncomment for Alpha workaround.
else
	RELEASE='??.??'
fi

DIR=`pwd`
if test x$DIR = x; then DIR="/unknown"; fi

if test ! -w version
then
	rm -f version; echo 0 > version; chmod 664 version
fi

awk '{version = $1 + 1; };END{printf "%d\n", version > "version"; }' < version

VERSION=`cat version`
DATE=`date`
PATH=$PATH:/usr/ucb:/usr/bsd
export PATH
if test x$UNIXTYPE = xBSD -o -f /usr/ucb/hostname -o -f /usr/bsd/hostname
then
	HOST=`hostname`
else
	HOST=`uname -n`
fi

if test "$USER" = ""
then
	USER="$LOGNAME"
fi

cat << EOF
char ${VARIABLE}[] = "\\
@(#) BRL-CAD Release ${RELEASE}   ${TITLE}\n\\
    ${DATE}, Compilation ${VERSION}\n\\
    ${USER}@${HOST}:${DIR}\n";
EOF
