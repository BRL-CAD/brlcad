#!/bin/sh

# Main result sent to stdout, errors must go to stderr
#
# Build the symbolic link from the numbered version of a library
# to the no-number library.  e.g. link librt.so.10 to librt.so
#
# Also, for systems like SunOS 4.1.x and Linux that use major and minor
# version numbers, make a symlink to minor version 1 as well.
#
#  $Header$

if test $# != 1 -o ! -f "$1"
then
	echo "Usage: sharedliblink.sh libraryname.so.##" 1>&2
	echo "e.g.	../.librt.sun4/librt.so.10" 1>&2
	exit 1
fi

ONENUM="$1"
DIR=`dirname $ONENUM`
BASE=`basename $ONENUM`

cd $DIR

NONUM=`echo $BASE | sed -e 's/\\.so\\..*/.so/' `
TWONUM=$BASE.1			# Minor version 1

# If the shared library has a version number, link numbered to unnumbered.
# If no version number, don't do anything.
if test "$BASE" != "$NONUM"
then
	rm -f $NONUM $TWONUM
	ln -s $BASE $NONUM
	ln -s $BASE $TWONUM
fi
