#!/bin/sh

HOSTS="wopr liu amdws2 vast"
MASTERHOST="wopr"

export MYNAME=`hostname | awk -F '.' '{print $1}'`
export START_TIME=`date "+%y%m%d%H%M"`
export LOG_FILE=`pwd`/${MYNAME}_${START_TIME}.log

if [ X$MYNAME == X$MASTERHOST ] ; then
	/bin/echo fetching archive
    rm -rf $HOSTS brlcad
    export CVS_RSH=ssh
    cvs  -z3 -d:pserver:anonymous@cvs.sf.net:/cvsroot/brlcad co -P brlcad > $LOG_FILE 2>&1

    if [ ! -d brlcad ] ; then
	/bin/echo "unable to extract source from CVS repository"
	exit -1
    fi
    cd brlcad

    # extract existing release numbers, old an new
    eval `awk '/Release/ {print $2}' < README | \
	awk -F. '{print "export MAJOR=" $1  "\nexport MINOR=" $2 "\nexport PATCH=" $3}'`

    OLD_MAJOR=`awk -F\= '/^MAJOR_VERSION/ {print $2}' < configure.ac`
    OLD_MINOR=`awk -F\= '/^MINOR_VERSION/ {print $2}' < configure.ac`
    OLD_PATCH=`awk -F\= '/^PATCH_VERSION/ {print $2}' < configure.ac`


    # Update configure.ac
    /bin/echo update configure
    if [ ! -f configure.ac.orig ] ; then
	mv configure.ac configure.ac.orig
    fi

    sed -e "s/$OLD_MAJOR\.$OLD_MINOR\.$OLD_PATCH/$MAJOR\.$MINOR\.$PATCH/" \
	-e "s/MAJOR_VERSION=$OLD_MAJOR/MAJOR_VERSION=$MAJOR/" \
	-e "s/MINOR_VERSION=$OLD_MINOR/MINOR_VERSION=$MINOR/" \
	-e "s/^PATCH_VERSION=$OLD_PATCH/PATCH_VERSION=$PATCH/" \
	< configure.ac.orig > configure.ac

    /bin/echo autogen
    /bin/sh ./autogen.sh >> $LOG_FILE 2>&1

    /bin/echo semaphores
    cd ..
    for i in $HOSTS ; do
	touch $i
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
/bin/sh brlcad/regress/main.sh
