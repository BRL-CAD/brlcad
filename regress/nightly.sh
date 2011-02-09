#!/bin/sh
#                      N I G H T L Y . S H
# BRL-CAD
#
# Copyright (c) 2010-2011 United States Government as represented by
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

HOSTS="wopr liu amdws2 vast cocoa"
MASTERHOST="wopr"

export MYNAME=`hostname | awk -F '.' '{print $1}'`
export START_TIME=`date "+%y%m%d%H%M"`
export LOG_FILE=`pwd`/${MYNAME}_${START_TIME}.log

#
# if we are the master host, get things set up
#
if [ X$MYNAME == X$MASTERHOST ] ; then
    echo "fetching archive"

    # Delete any leftovers
    rm -rf $HOSTS
    rm -rf  brlcad

    # Fetch a clean copy of the repository
    CVS_RSH=ssh
    export CVS_RSH
    cvs  -z3 -d:ext:lbutler@cvs.sf.net:/cvsroot/brlcad co -P brlcad > $LOG_FILE 2>&1

    if [ ! -d brlcad ] ; then
	echo "unable to extract source from CVS repository"
	exit 1
    fi


    # Process the checkout we just got
    cd brlcad

    # extract New release numbers from README
    eval `awk '/Release/ {print $2}' < README | \
	awk -F. '{print "export MAJOR=" $1  "\nexport MINOR=" $2 "\nexport PATCH=" $3}'`

    # Extract old release numbers from configure.ac
    OLD_MAJOR=`awk -F\= '/^MAJOR_VERSION/ {print $2}' < configure.ac`
    OLD_MINOR=`awk -F\= '/^MINOR_VERSION/ {print $2}' < configure.ac`
    OLD_PATCH=`awk -F\= '/^PATCH_VERSION/ {print $2}' < configure.ac`


    # Update configure.ac with the release we found in README
    echo "update configure"
    if [ ! -f configure.ac.orig ] ; then
	mv configure.ac configure.ac.orig
    fi

    sed -e "s/$OLD_MAJOR\.$OLD_MINOR\.$OLD_PATCH/$MAJOR\.$MINOR\.$PATCH/" \
	-e "s/MAJOR_VERSION=$OLD_MAJOR/MAJOR_VERSION=$MAJOR/" \
	-e "s/MINOR_VERSION=$OLD_MINOR/MINOR_VERSION=$MINOR/" \
	-e "s/^PATCH_VERSION=$OLD_PATCH/PATCH_VERSION=$PATCH/" \
	< configure.ac.orig > configure.ac

    # get a build environment so we can "make dist"
    echo "autogen"
    sh ./autogen.sh >> $LOG_FILE 2>&1

    echo "configure"
    ./configure >> $LOG_FILE 2>&1

    # Prepare a source distribution
    echo "making dist"
    make dist >> $LOG_FILE 2>&1
    cd ..

    # create the source tree that all machine will build from
    tar xzf brlcad/brlcad-$MAJOR.$MINOR.$PATCH.tar.gz

    # Let the other regression hosts start doing their work
    echo "semaphores"
    for i in $HOSTS ; do
	echo "$MAJOR.$MINOR.$PATCH" > $i
    done
fi

# wait to be told we can proceed

while [ ! -f $MYNAME ] ; do

    NOW=`date "+%y%m%d%H%M"`
    DELTA=`expr $START_TIME - $NOW`

    if [ $DELTA -gt 300 ] ; then
	# we should log something here
	echo "$MYNAME giving up at $NOW" >> $LOG_FILE

	exit 1
    fi

    sleep 10
done

echo "Starting build"

VERSION=`cat $MYNAME`
rm $MYNAME
# start the build
case $MYNAME in
wopr)
    export CONF_FLAGS="" ;
    export MAKE_CMD="make" ;
    export MAKE_OPTS="-j11" ;;
liu)
    export CONF_FLAGS="" ;
    export MAKE_CMD="make" ;
    export MAKE_OPTS="-j2" ;;

amdws2)
    export CONF_FLAGS="" ;
    export MAKE_CMD="make" ;
    export MAKE_OPTS="-j2" ;;

vast)
    export CONF_FLAGS="CC=cc CFLAGS=-64 LDFLAGS=-64 --enable-64bit-build" ;
    export MAKE_CMD="/usr/gnu/bin/make" ;
    export MAKE_OPTS="-j5" ;;
cocoa)
    export CONF_FLAGS="" ;
    export MAKE_CMD="make" ;
    export MAKE_OPTS="-j2" ;;
*)
    echo "hostname \"$MYNAME\" not recognized"
    exit 1
esac

BUILD_DIR=`pwd`/${MYNAME}_${START_TIME}.dir
rm -f $BUILD_DIR
mkdir $BUILD_DIR
if [ ! -d $BUILD_DIR ] ; then
    echo "Creation of $BUILD_DIR failed"
    exit 1
fi

cd $BUILD_DIR

echo ../brlcad-$VERSION/configure \
    $CONF_FLAGS \
    --prefix=/usr/brlcad/rel-$VERSION \
    >> $LOG_FILE 2>&1

../brlcad-$VERSION/configure \
    $CONF_FLAGS \
    --prefix=/usr/brlcad/rel-$VERSION \
    >> $LOG_FILE 2>&1

echo "runnig: $MAKE_CMD $MAKE_OPTS" >> $LOG_FILE 2>&1

$MAKE_CMD $MAKE_OPTS > build.log 2>&1
STATUS=$?
if [ $STATUS != 0 ] ; then
    echo "Build failed with status $STATUS"
    exit 1
fi
if [ -s build.log ] ; then
    cd regress
    make test > test.log 2>&1
else
    echo "Build failed, zero length log file"
    exit 1
fi

# Local Variables:
# tab-width: 8
# mode: sh
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
