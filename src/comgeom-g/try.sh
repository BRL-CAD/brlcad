#!/bin/sh
#			T R Y . S H
#
#  Script for testing comgeom-g
#
#  $Header$

CASE=$1

if test x$2 = x
then
	OPT=""
	DB=foo.g
else
	shift
	OPT="$*"
	DB=/dev/null
fi

case $CASE in

1)
	comgeom-g $OPT -v4 m35.cg4 $DB
	break;;

2)
	comgeom-g $OPT m35a2.cg5 $DB
	break;;

3)
	comgeom-g $OPT apache.cg $DB
	break;;

4)
	comgeom-g $OPT avlb.cg5 $DB
	break;;

5)
	comgeom-g $OPT -v1 atr.cg1 $DB
	break;;

*)
	echo "Try /$1/ unknown"
	exit 1;
	break;;
esac

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
