#!/bin/sh
#
#  This script waits for the master script to check out a copy of the source
#  distribution, and then starts the regression test on the local machine.
#

args=`getopt b:d:e:qr:u: $*`

if [ $? != 0 ] ; then
	echo "Usage: $0 [-r brlcad_root] [-b begin_time] [-e end_time] [-u user] -d regress_dir "
	exit 2
fi

set -- $args

BEGIN_HOUR=20
END_HOUR=7
MAILUSER=acst
QUIET=0
BRLCAD_ROOT=/usr/brlcad
SLEEP_DELTA=60


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
			MAILUSER=$2;
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
    echo "`hostname` started after end time $END_HOUR (at $START_HOUR)"
    exit 2
fi

#
#  First we must sleep until the "cvs checkout" is complete or we are past
#  when we are allowed to run
#
WAIT_MACH_TIME=`expr $SLEEP_DELTA \* 10`
HOUR=$START_HOUR
while [ ! -f $REGRESS_DIR/brlcad/sh/machinetype.sh ] ; do
    if [ $HOUR -ge $END_HOUR ] ; then
	echo "`hostname` time expired waiting for creation of machinetype.sh"
	exit 2
    fi
    sleep $WAIT_MACH_TIME
    HOUR=`date | awk '{print $4}' | awk -F: '{print $1}'`
done

ARCH=`$REGRESS_DIR/brlcad/sh/machinetype.sh`
export ARCH

COUNT=0
HOUR=`date | awk '{print $4}' | awk -F: '{print $1}'`
while [ ! -f $REGRESS_DIR/start_$ARCH ] ; do
    if [ $HOUR -ge $END_HOUR -o $HOUR -lt BEGIN_HOUR ] ; then
	echo "`hostname` time expired waiting for creation of start_$ARCH"
	exit 2
    fi
    sleep $SLEEP_DELTA
    HOUR=`date | awk '{print $4}' | awk -F: '{print $1}'`
done

#
#  Now we have got the go-ahead to build.
#  Remove the semaphore file so that we don't accidentally re-start
#
rm $REGRESS_DIR/start_$ARCH

echo "$ARCH commencing build"
