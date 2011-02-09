#!/bin/sh
#                  S L A V E _ B U I L D . S H
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

#
# If the user specified a working directory, we cd into it.
#
if [ $# = 1 ] ; then
    if [ -d $1 ] ; then
	cd $1
    else
     #
     # wait for the directory in case it hasn't been built
     #
     while [ ! -d $1 ] ; do

       NOW=`date "+%y%m%d%H%M"`
       DELTA=`expr $START_TIME - $NOW`

       if [ $DELTA -gt 300 ] ; then
	   # we log that we're quitting
	   echo "$1 does not exist giving up at $NOW" >> $LOG_FILE
	   exit 1
       fi
    fi
fi

export MYNAME=`hostname | awk -F '.' '{print $1}'`
export START_TIME=`date "+%y%m%d%H%M"`
export LOG_FILE=`pwd`/${MYNAME}_${START_TIME}.log

#
# wait to be told we can proceed
#
while [ ! -f $MYNAME ] ; do

    NOW=`date "+%y%m%d%H%M"`
    DELTA=`expr $START_TIME - $NOW`

    if [ $DELTA -gt 300 ] ; then
	# we log that we're quitting
	echo "$MYNAME giving up at $NOW" >> $LOG_FILE
	exit 1
    fi

    sleep 10
done

echo "Starting build"
VERSION=`cat $MYNAME`
rm $MYNAME

#
# get any host specific flags set up
#
case $MYNAME in
wopr)
    export CONF_FLAGS="--enable-everything" ;
    export MAKE_CMD="make" ;
    export MAKE_OPTS="-j11" ;;

liu)
    export CONF_FLAGS="--enable-everything" ;
    export MAKE_CMD="make" ;
    export MAKE_OPTS="-j2" ;;

amdws2)
    export CONF_FLAGS="--enable-everything" ;
    export MAKE_CMD="make" ;
    export MAKE_OPTS="-j2" ;;

vast)
    export CONF_FLAGS="CC=cc CFLAGS=-64 LDFLAGS=-64 --enable-everything --enable-64bit-build" ;
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

#
# create the build directory
#
BUILD_DIR=`pwd`/${MYNAME}_${START_TIME}.dir
rm -f $BUILD_DIR
mkdir $BUILD_DIR
if [ ! -d $BUILD_DIR ] ; then
    echo "Creation of $BUILD_DIR failed"
    exit 1
fi

#
# move into build directory
#
cd $BUILD_DIR

#
# configure
#
echo ../brlcad-$VERSION/configure \
    $CONF_FLAGS \
    --enable-optimized \
    --prefix=/usr/brlcad/rel-$VERSION \
    >> $LOG_FILE 2>&1

../brlcad-$VERSION/configure \
    $CONF_FLAGS \
    --enable-optimized \
    --prefix=/usr/brlcad/rel-$VERSION \
    >> $LOG_FILE 2>&1

echo "running: $MAKE_CMD $MAKE_OPTS" >> $LOG_FILE 2>&1

#
# build
#
$MAKE_CMD $MAKE_OPTS > build.log 2>&1
STATUS=$?
if [ $STATUS != 0 ] ; then
    echo "Build failed with status $STATUS"
    exit 1
fi

#
# run the regression tests
#
if [ ! -s build.log ] ; then
    echo "Build failed, zero length log file"
    exit 1
fi

cd regress
make test > test.log 2>&1
if [ X`grep failed test.log` != X ] ; then
    echo "regression testing FAILED"
else
    echo "regression testing succeeded"
fi
make install

# Local Variables:
# tab-width: 8
# mode: sh
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
