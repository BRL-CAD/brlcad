#!/bin/sh

# Main result sent to stdout, errors must go to stderr
#
# Build the symbolic link from the numbered version of a library
# to the no-number library.  e.g. link librt.so.10 to librt.so

if test $# != 1
then
	echo "Usage: sharedliblink.sh libraryname.so.##" 1>&2
	echo "e.g.	../.librt.sun4/librt.so.10" 1>&2
	exit 1
fi

cd `dirname $1`

NONUM=`basename $1 | sed -e 's/\\.so\\..*/.so/`

ln -s $1 $NONUM
