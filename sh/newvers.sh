#!/bin/sh
#			mged/newvers.sh
#
# Update the "version" file, and create a new "vers.c" from it.
#
#	@(#)$Header$ (BRL)

eval `machinetype.sh -b`	# sets MACHINE, UNIXTYPE, etc

# Obtain RELEASE, RCS_REVISION, and REL_DATE
if test -r ../gen.sh
then
	eval `grep '^RELEASE=' ../gen.sh`
else
	RELEASE='??.??'
fi

DIR=`pwd`
if test x$DIR = x; then DIR=".mged.$MACHINE"; fi

if test ! -r version ; then echo 0 > version; fi
touch version
awk '	{	version = $1 + 1; }\
END	{	printf "char version[] = \"@(#) BRL-CAD Release '$RELEASE' Graphics Editor (MGED) Compilation %d", version > "vers.c";\
		printf "%d\n", version > "version"; }' < version

# The ECHO command operates differently between BSD and SYSV
if test x$UNIXTYPE = xBSD
then
	/bin/echo '\n    '`date`'\n    '$USER'@'`hostname`':'$DIR'\n";' >> vers.c
else
	echo '\\n    '`date`'\\n    '$USER'@'`uname -n`':'$DIR'\\n";' >> vers.c
fi
