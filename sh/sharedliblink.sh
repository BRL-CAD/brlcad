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

if test $# != 1
then
	echo "Usage: sharedliblink.sh libraryname.so.##" 1>&2
	echo "e.g.	../.librt.sun4/librt.so.10" 1>&2
	exit 1
fi

cd `dirname $1`

NONUM=`basename $1 | sed -e 's/\\.so\\..*/.so/`

# If the shared library has a version number, link numbered to unnumbered.
# If no version number, don't do anything.
if test `basename $1` != "$NONUM"
then
	rm -f $NONUM
	ln -s $1 $NONUM
	ln -s $1 $1.1		# Minor version 1
fi
