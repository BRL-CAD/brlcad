#!/bin/sh
#			mged/newvers.sh
#
# Update the "version" file, and create a new "vers.c" from it.
#
#	@(#)$Header$ (BRL)

if test ! -r version ; then echo 0 > version; fi
DIR=`pwd`
if test x$DIR = x; then DIR="mged.`machinetype.sh`"; fi
touch version
awk '	{	version = $1 + 1; }\
END	{	printf "char version[] = \"@(#) BRL-CAD Release 3.0 Graphics Editor (MGED) Compilation %d", version > "vers.c";\
		printf "%d\n", version > "version"; }' < version
if test x`machinetype.sh -s` = BSD
then
	/bin/echo '\n    '`date`'\n    '$USER'@'`hostname`':'$DIR'\n";' >> vers.c
else
	echo '\\n    '`date`'\\n    '$USER'@'`uname -n`':'$DIR'\\n";' >> vers.c
fi
