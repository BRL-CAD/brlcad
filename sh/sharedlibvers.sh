#!/bin/sh

# Main result sent to stdout, errors must go to stderr
# Must include the dot, as well as the version number(s)
#
# New convention is to only use MAJOR numbers, not minor numbers.

if test $# != 0 -a $# != 1
then
	echo "Usage: sharedlibvers.sh [directory/libraryname]" 1>&2
	echo "e.g.	../.librt.sun4/librt.so" 1>&2
	exit 1
fi


# Obtain RELEASE, RCS_REVISION, and REL_DATE
if test -r ../gen.sh
then
	# XXX The following line causes DEC Alpha's /bin/sh to dump core.
	eval `grep '^RELEASE=' ../gen.sh`
	##RELEASE=4.4;	RCS_REVISION=11;	# Uncomment for Alpha workaround.
else
	echo "$0: Unable to get release number"  1>&2
	exit 1
fi

if test $# = 1
then
	DIR=`dirname $1`
else
	DIR=.
fi

cd $DIR

if test $# = 1
then
	echo $1.$RCS_REVISION
	if test ! -f $1.$RCS_REVISION
	then
		echo "$0: ERROR, $1.$RCS_REVISION does not actually exist" 1>&2
	fi
else
	echo .$RCS_REVISION
fi

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
