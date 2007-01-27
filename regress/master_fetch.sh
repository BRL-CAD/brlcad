#!/bin/sh

#
# If the user specified a working directory, we cd into it.
#
if [ $# = 1 ] ; then
    if [ -d $1 ] ; then
	cd $1
    else
	/bin/echo $1 is not a directory
    fi
fi

export MYNAME=`hostname | awk -F '.' '{print $1}'`
export START_TIME=`date "+%y%m%d%H%M"`
export LOG_FILE=`pwd`/${MYNAME}_fetch_${START_TIME}.log

# Delete any leftovers
rm -rf $HOSTS brlcad

# Fetch a clean copy of the repository
/bin/echo fetching archive > $LOG_FILE 2>&1
CVS_RSH=ssh
export CVS_RSH


# cvs -z3 -d:pserver:anonymous@cvs.sourceforge.net:/cvsroot/brlcad co -P brlcad >> $LOG_FILE 2>&1

cvs -z3 -d:ext:lbutler@cvs.sourceforge.net:/cvsroot/brlcad co -P brlcad


if [ ! -d brlcad ] ; then
    /bin/echo "unable to extract source from CVS repository"
    exit 1
fi

#
# prepare the build
#
/bin/sh brlcad/regress/master_prep.sh
