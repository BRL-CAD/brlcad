#!/bin/sh
#
#  This script waits for the master script to check out a copy of the source
#  distribution, and then starts the regression test on the local machine.
#

# include standard utility functions
. ./library

echo "client_wait.sh"
echo "=============="


args=`getopt b:d:e:qr:u: $*`

if [ $? != 0 ] ; then
	echo "Usage: $0 [-r brlcad_root] [-b begin_time] [-e end_time] [-u user] -d regress_dir "
	exit 2
fi

set -- $args

BEGIN_HOUR=18
END_HOUR=7
MAILUSER=morrison@arl.army.mil
QUIET=0
SLEEP_DELTA=10

for i in $* ; do
	case "$i" in
		-b)
			echo "-b $2";
			BEGIN_HOUR=$2;
			shift 2;;
		-d)
			echo "-d $2";
			REGRESS_DIR=$2;
			export REGRESS_DIR
			shift 2;;
		-e)
			echo "-e $2";
			END_HOUR=$2;
			shift 2;;
		-q)
			echo "-q";
			QUIET=1;
			shift;;
		-r)
			echo "-r $2";
			BRLCAD_ROOT=$2;
			shift 2;;
		-s)
			echo "-S $2";
			SLEEP_DELTA=$;
			shift 2;;
		-u)
			echo "-d $2";
			MAIUSERUSER=$2;
			export MAILUSER
			shift 2;;
		--)
			shift; break;;
	esac
done

export BEGIN_HOUR
export END_HOUR
export QUIET
export BRLCAD_ROOT

if [ X$REGRESS_DIR = X ] ; then
	echo "must specify regression directory"
	exit 2
fi

START_HOUR=`date | awk '{print $4}' | awk -F: '{print $1}'`
if [ $START_HOUR -ge $END_HOUR ] ; then
    echo "$HOSTNAME started after end time ${END_HOUR}:00 (at ${START_HOUR}:`date | awk '{print $4}' | awk -F: '{print $2}'`)"
    exit 2
fi

#
#  First we must sleep until the "cvs export" is complete or we are past
#  when we are allowed to run
#
WAIT_MACH_TIME=`expr $SLEEP_DELTA \* 60`
HOUR=$START_HOUR
while [ ! -f $REGRESS_DIR/brlcad/sh/machinetype.sh ] ; do
    if [ $HOUR -ge $END_HOUR ] ; then
	echo "$HOSTNAME time expired waiting for creation of machinetype.sh"
	exit 2
    fi
    echo "Waiting for [$REGRESS_DIR/brlcad/sh/machinetype.sh] to get created...sleeping $WAIT_MACH_TIME seconds"
    sleep $WAIT_MACH_TIME
    HOUR=`date | awk '{print $4}' | awk -F: '{print $1}'`
done
echo "Verified that machinetype.sh exists"

ARCH=`${REGRESS_DIR}/brlcad/sh/machinetype.sh`
export ARCH

initializeVariable BRLCAD_ROOT "${REGRESS_DIR}/brlcad.$ARCH"

echo "Regression testing an [$ARCH] architecture in [$REGRESS_DIR]"

#
# try to acquire the semaphore every minute until we run out of time
#
COUNT=0
HOUR=`date | awk '{print $4}' | awk -F: '{print $1}'`
echo "HOUR=$HOUR, END_HOUR=$END_HOUR, BEGIN_HOUR=$BEGIN_HOUR"

# !!! booo, sun5 sh giving trouble with while ! true
if acquireLock ${REGRESS_DIR}/start_${ARCH}.semaphore 2 10 ; then LOCKED=1 ; else LOCKED=0 ; fi
while [ ! "x$LOCKED" = "x1" ]  ; do
    if [ $HOUR -ge $END_HOUR -o $HOUR -lt $BEGIN_HOUR ] ; then
	echo "ERROR: time expired on $HOSTNAME waiting for creation of start_${ARCH}.semaphore"
	exit 2
    fi
    HOUR=`date | awk '{print $4}' | awk -F: '{print $1}'`
    if acquireLock ${REGRESS_DIR}/start_${ARCH}.semaphore 2 10 ; then LOCKED=1 ; else LOCKED=0 ; fi
done

echo "$ARCH commencing build at `date`"
if [ ! -d ${REGRESS_DIR}/.regress.$ARCH ] ; then mkdir ${REGRESS_DIR}/.regress.$ARCH ; fi

#
# run the build script out of the source tree
#
echo "Building source"
echo "Running [./client_build.sh -d $REGRESS_DIR]"
./client_build.sh -d $REGRESS_DIR

releaseLock ${REGRESS_DIR}/start_${ARCH}.semaphore

echo "Done client_wait.sh"