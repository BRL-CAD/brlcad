#!/bin/sh

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
	   /bin/echo $1 does not exist giving up at $NOW >> $LOG_FILE
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
	/bin/echo $MYNAME giving up at $NOW >> $LOG_FILE
	exit 1
    fi

    sleep 10
done

/bin/echo "Starting build"
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
    echo hostname \"$MYNAME\" not recognized
    exit 1
esac

#
# create the build directory
#
BUILD_DIR=`pwd`/${MYNAME}_${START_TIME}.dir
rm -f $BUILD_DIR
mkdir $BUILD_DIR
if [ ! -d $BUILD_DIR ] ; then
    echo create $BUILD_DIR failed
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

echo runnig: $MAKE_CMD $MAKE_OPTS >> $LOG_FILE 2>&1

#
# build
#
$MAKE_CMD $MAKE_OPTS > build.log 2>&1
STATUS=$?
if [ $STATUS != 0 ] ; then
    echo build failed status $STATUS
    exit 1
fi

#
# run the regression tests
#
if [ ! -s build.log ] ; then
    echo build failed zero length log
    exit 1
fi

cd regress
make test > test.log 2>&1
if [ X`grep failed test.log` != X ] ; then
    /bin/echo regression test failed
else
    /bin/echo regression test succeeded
fi
make install
