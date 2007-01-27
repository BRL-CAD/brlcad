#!/bin/sh

# This file is designed to be run from nightly.sh
echo $MAJOR $MINOR $PATCH

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
esac


# note thate MYNAME and START_TIME are set in the nightly.sh script for us
export BUILD_DIR=`pwd`/${MYNAME}_${START_TIME}.dir
rm -f $BUILD_DIR
mkdir $BUILD_DIR
cd $BUILD_DIR

VERSION=`../brlcad/configure -V | awk '/BRL/ {print $NF}' | sed 's/\./-/g'`

/bin/echo $MYNAME $ARCHITECTURE starting configure >> $LOG_FILE
../brlcad/configure $CONF_FLAGS --prefix=/usr/brlcad/rel-${VERSION} >> $LOG_FILE 2>&1
$MAKE_CMD $MAKE_OPTS > build.log 2>&1
make install >& install.log 2>&1
cd regress
make test > test.log 2>&1
