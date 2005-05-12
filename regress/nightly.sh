#!/bin/sh

HOSTS="wopr liu amdws2 vast"
MASTERHOST="wopr"

export MYNAME=`hostname | awk -F '.' '{print $1}'`
export START_TIME=`date "+%y%m%H%M"`
export LOG_FILE=`pwd`/${MYNAME}_${START_TIME}.log

if [ X$MYNAME == X$MASTERHOST ] ; then
	/bin/echo fetching archive
    rm -rf $HOSTS brlcad
    export CVS_RSH=ssh
    cvs  -z3 -d:pserver:anonymous@cvs.sf.net:/cvsroot/brlcad co -P brlcad > $LOG_FILE 2>&1

    cd brlcad
    /bin/sh ./autogen.sh >> $LOG_FILE 2>&1

    cd ..
    for i in $HOSTS ; do
	touch $i
    done
fi

# wait to be told we can proceed

while [ ! -f $MYNAME ] ; do

    NOW=`date "+%y%m%H%M"`
    DELTA=`expr $START_TIME - $NOW`

    if [ $DELTA -gt 300 ] ; then 
	# we should log something here
	/bin/echo $MYNAME giving up at $NOW >> $LOG_FILE

	exit
    fi

    sleep 10
done


/bin/sh brlcad/regress/main.sh
