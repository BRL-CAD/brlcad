#!/bin/sh

HOSTS="wopr liu amdws2 vast cocoa"
MASTERHOST="wopr"

export MYNAME=`hostname | awk -F '.' '{print $1}'`
export START_TIME=`date "+%y%m%d%H%M"`
export LOG_FILE=`pwd`/${MYNAME}_${START_TIME}.log

#
# if we are the master host, get things set up
#
if [ X$MYNAME == X$MASTERHOST ] ; then
    /bin/echo fetching archive

    # Delete any leftovers
    rm -rf $HOSTS
    rm -rf  brlcad

    # Fetch a clean copy of the repository
    CVS_RSH=ssh
    export CVS_RSH
    cvs  -z3 -d:ext:lbutler@cvs.sf.net:/cvsroot/brlcad co -P brlcad > $LOG_FILE 2>&1

    if [ ! -d brlcad ] ; then
	/bin/echo "unable to extract source from CVS repository"
	exit -1
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
    /bin/echo update configure
    if [ ! -f configure.ac.orig ] ; then
	mv configure.ac configure.ac.orig
    fi

    sed -e "s/$OLD_MAJOR\.$OLD_MINOR\.$OLD_PATCH/$MAJOR\.$MINOR\.$PATCH/" \
	-e "s/MAJOR_VERSION=$OLD_MAJOR/MAJOR_VERSION=$MAJOR/" \
	-e "s/MINOR_VERSION=$OLD_MINOR/MINOR_VERSION=$MINOR/" \
	-e "s/^PATCH_VERSION=$OLD_PATCH/PATCH_VERSION=$PATCH/" \
	< configure.ac.orig > configure.ac

    # get a build environment so we can "make dist"
    /bin/echo autogen
    /bin/sh ./autogen.sh >> $LOG_FILE 2>&1

    /bin/echo configure
    ./configure >> $LOG_FILE 2>&1

    # Prepare a source distribution
    /bin/echo making dist
    make dist >> $LOG_FILE 2>&1
    cd ..

    # create the source tree that all machine will build from
    tar xzf brlcad/brlcad-$MAJOR.$MINOR.$PATCH.tar.gz

    # Let the other regression hosts start doing their work
    /bin/echo semaphores
    for i in $HOSTS ; do
	echo $MAJOR.$MINOR.$PATCH > $i
    done
fi

# wait to be told we can proceed

while [ ! -f $MYNAME ] ; do

    NOW=`date "+%y%m%d%H%M"`
    DELTA=`expr $START_TIME - $NOW`

    if [ $DELTA -gt 300 ] ; then
	# we should log something here
	/bin/echo $MYNAME giving up at $NOW >> $LOG_FILE

	exit
    fi

    sleep 10
done

/bin/echo "Starting build"

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
    echo hostname \"$MYNAME\" not recognized
    exit
esac

BUILD_DIR=`pwd`/${MYNAME}_${START_TIME}.dir
rm -f $BUILD_DIR
mkdir $BUILD_DIR
if [ ! -d $BUILD_DIR ] ; then
    echo create $BUILD_DIR failed
    exit -1
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

echo runnig: $MAKE_CMD $MAKE_OPTS >> $LOG_FILE 2>&1

$MAKE_CMD $MAKE_OPTS > build.log 2>&1
STATUS=$?
if [ $STATUS != 0 ] ; then
    echo build failed status $STATUS
    exit -1
fi
if [ -s build.log ] ; then
    cd regress
    make test > test.log 2>&1
else
    echo build failed zero length log
    exit -1
fi
