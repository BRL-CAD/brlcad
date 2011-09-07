#!/bin/sh
#                     M A K E _ D E B . S H
# BRL-CAD
#
# Copyright (c) 2005-2011 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following
# disclaimer in the documentation and/or other materials provided
# with the distribution.
#
# 3. The name of the author may not be used to endorse or promote
# products derived from this software without specific prior written
# permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
###

set -e

ferror(){
    echo "=========================================================="
    echo $1
    echo $2
    echo "=========================================================="
    exit 1
}

# show help
if test -z $1 ;then
    echo "Script to create Debian binary and source packages."
    echo
    echo "Usage:"
    echo "  sh/make_deb.sh -b | -s [-t]"
    echo
    echo "Options:"
    echo "  -b       build the Debian binary package (deb file)"
    echo "  -s *     build the Debian source package"
    echo "  -t       as second argument: test for all prerequisites"
    echo
    echo "           * (use with a clean brlcad tree)"
    exit 1
fi

# too many parameters
if test $# -gt 2 ;then
    ferror "Too many arguments" "Exiting..."
fi

# unknown parameter
if test "$1" != "-s" && test "$1" != "-b" ; then
    ferror "Unknown first argument '$!'." "Exiting..."
fi
# check for test
TEST=0
if test $# -eq 2 && test "$2" != "-t" ; then
    ferror "Unknown second argument '$2'." "Exiting..."
elif test $# -eq 2 && test "$2" = "-t" ; then
   TEST=1
fi

# test if in project root
if test ! -f misc/debian/control ; then
    ferror "\"make_deb.sh\" should be run from project root directory." "Exiting..."
fi

# test if in debian-like system
if test ! -e /etc/debian_version ; then
    ferror "Refusing to build on a non-debian system." "Exiting..."
fi

# check needed packages
E=0
fcheck() {
    T="install ok installed"
    if test `dpkg -s $1 2>/dev/null | grep "$T" | wc -l` -eq 0 ; then
	# failed to find package $1
	LLIST=$LLIST" "$1
	E=1
    else
        # success
        E=0
	echo "Found package '$1'..."
        return
    fi

    # need to check for local, non-package versions
    # check for binaries
    echo "Testing for non-package version of $1..."
    if [ ! -f /usr/bin/$1 ]; then
	LLIST=$LLIST" "$1
	E=1
    else
        # success
        E=0
	echo "Found /usr/bin/$1..."
        return
    fi
    # one last check
    if [ ! -f /usr/local/bin/$1 ]; then
	LLIST=$LLIST" "$1
	E=1
    else
        # success
        E=0
	echo "Found /usr/local/bin/$1..."
        return
    fi
}

fcheck debhelper
fcheck fakeroot

if test "$1" = "-b" ;then
    fcheck build-essential
    fcheck make
    fcheck cmake
    fcheck libtool
    fcheck bc
    fcheck sed
    fcheck bison
    fcheck flex
    fcheck libxi-dev
    fcheck xsltproc
    fcheck libglu1-mesa-dev
    fcheck libpango1.0-dev
    #fcheck fop # allows pdf creation
fi

if [ $E -eq 1 ]; then
    ferror "Mandatory to install these packages first:" "$LLIST"
fi

if [ $TEST -eq 1 ]; then
    ferror "Testing complete--ready to create a Debian package."
fi

# set variables
BVERSION=`cat include/conf/MAJOR | sed 's/[^0-9]//g'`"."`cat include/conf/MINOR | sed 's/[^0-9]//g'`"."`cat include/conf/PATCH | sed 's/[^0-9]//g'`
CDATE=`date -R`
CFILE="debian/changelog"
RELEASE="0"

NJOBS=`getconf _NPROCESSORS_ONLN | sed "s/.*/&*2-1/" | bc`
if test ! $NJOBS -gt 0 2>/dev/null ;then
    NJOBS=1
fi

# if building sources, create *orig.tar.gz
rm -Rf debian
if test "$1" = "-s" ;then
    echo "building brlcad_$BVERSION.orig.tar.gz..."
    tar -czf "../brlcad_$BVERSION.orig.tar.gz" *
fi

# #
cp -Rf misc/debian/ .

# update debian/changelog if needed
if test -s $CFILE && test `sed -n '1p' $CFILE | grep "brlcad ($BVERSION-$RELEASE" | wc -l` -eq 0 ; then
    L1="brlcad ($BVERSION-$RELEASE) unstable; urgency=low\n\n"
    L2="  **** VERSION ENTRY AUTOMATICALLY ADDED BY \"sh\/make_deb.sh\" SCRIPT ****\n\n"
    L3=" -- Jordi Sayol <g.sayol@yahoo.es>  $CDATE\n\n/"
    sed -i "1s/^/$L1$L2$L3" $CFILE
    echo "\"$CFILE\" has been modified!"
fi

# create deb or source packages
case "$1" in
-b) fakeroot debian/rules clean
    DEB_BUILD_OPTIONS=parallel=$NJOBS fakeroot debian/rules binary
    ;;
-s) fakeroot dpkg-buildpackage -S -us -uc
    ;;
esac

# #
rm -Rf debian

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
