#!/bin/sh
#
#  This is the master script for the BRL-CAD regression tests.  This script
#  is run first, to check out the source code and set up the semaphore files
#  for each of the client architectures.

USAGE="echo Usage: $0 -d regress_dir [-c cvs_binary] [-R cvs_repository]"


args=`getopt d:c:R: $*`

if [ $? != 0 ] ; then
	eval $USAGE
	exit 2
fi

set -- $args

CVS=cvs
REPOSITORY=/c/CVS

for i in $* ; do
	case "$i" in
		-R)	
			REPOSITORY=$2;
			shift 2;;
		-d)
			REGRESS_DIR=$2;
			export REGRESS_DIR
			shift 2;;
		-c)
			CVS=$2;
			shift 2;;
		--)
			shift; break;;
	esac
done

export CVS
export REPOSITORY

if [ X$REGRESS_DIR = X ] ; then
	echo "Must specify regression directory"
	eval $USAGE
	exit 2
fi

#
#  Make sure the regression director exists
#
if [ ! -d $REGRESS_DIR ] ; then
	echo directory $REGRESS_DIR not present
	exit
fi

#
# clean out the contents of the directory
#
rm -rf $REGRESS_DIR/*
if [ $? != 0 ] ; then
	echo "Error cleaning out regression directroy $REGRESS_DIR"
	exit
fi

cd $REGRESS_DIR


#
# Get a copy of the cad package
#
$CVS -Q -d $REPOSITORY get brlcad
$CVS -Q -d $REPOSITORY release brlcad

#
# Touch a file for each architecture that we support.  The clients wait for
# this file to be created before starting their build.
#
for ARCH in m4i64 m4i65 7d fbsd li; do
	touch start_$ARCH
done
