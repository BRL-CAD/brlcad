#!/bin/sh
#			newvers.sh
#
# This version for 4.2BSD

if test ! -r version ; then echo 0 > version; fi
DIR=`pwd`
if test x$DIR = x; then DIR="mged.`machinetype.sh`"; fi
touch version
awk '	{	version = $1 + 1; }\
END	{	printf "char version[] = \"@(#) BRL Graphics Editor (MGED) Version 2.%d", version > "vers.c";\
		printf "%d\n", version > "version"; }' < version
/bin/echo '\n    '`date`'\n    '$USER'@'`hostname`':'$DIR'\n";' >> vers.c
