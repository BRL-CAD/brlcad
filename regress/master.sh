#!/bin/sh
#
#  m a s t e r . s h
#
###
#
#  This is the master script for the BRL-CAD regression tests.  This script
#  is run first, from a machine that has access to a BRL-CAD source tree either
#  via cvs (default), source tree, tar, or tar.* (tar.gz, tar.bz, tar.Z, etc).
#  That source may then be optionally merged with another source tree, tar, or
#  tar.* of "update" patches to apply.
#
#  Then the resulting "package" may be optionally run on all (default) or some
#  of the systems available in the systems.d directory.  Each system defines
#  its architecture and whether the test should run locally (implying local
#  run), remote-automated, over RSH, or over SSH to invoke the regression
#  testing of the source (via the client.sh script).  Each system also defines
#  how to get the package to the system either via local filesystem (NFS
#  or manual move implied), remote-pulled, FTP, or SSH (scp).
#
#  Filesystem changes should all only occur within the REGRESS_DIR directory.
#  e.g. the REGRESS_DIR directory is the working directory.
#
###
#
#  GETTING STARTED
#
#  Run:
#  ./master.sh -h
#  to get help with command line options for modifying run-time behaviour.
#
#  The default behavior of the master.sh script is to regression test the
#  brlcad package that this master.sh script lives with and run on all of the
#  systems in the systems.d directory.  By default, the only defined system
#  should be "localhost" and most likely includes running all of the tests
#  found in the tests.d directory.  See the systems.d/localhost script and/or
#  the systems.d/README for an example on how to set up your own systems to
#  automatically regression test the brlcad package.
#
###############################################################################

# include standard utility functions
. `dirname $0`/library

###
# DEFAULT PUBLIC VARIABLE INITIALIZATION
########################################

# default name of the regression source
initializeVariable PACKAGE_NAME brlcad

# default working directory for checkouts and (un)packing
initializeVariable REGRESS_DIR /tmp/`whoami`

# number of minutes (greater than zero) to wait on the master lock
initializeVariable WAIT_FOR_LOCK_TIMEOUT 3

# fail on warnings (0==do not fail, 1==fail)
initializeVariable NO_WARNINGS 0

# clean out the regressio directory before starting
initializeVariable CLOBBER_FIRST 0

# name of log file for output
initializeVariable LOGFILE .master-${HOSTNAME}.log

# default system to regression test
# all is a special keyword to regression test all of the systems
# otherwise the tests must manually be run one at a time through the shell
initializeVariable REGRESS_SYSTEM all

# default test to run
# all is a special keyword to perform all tests
# otherwise the tests must manually be run one at a time through the shell (!)
initializeVariable REGRESS_TEST all

# CVS
###

# cvs binary to run
initializeVariable CVS cvs

# location of the cvs repository
initializeVariable REPOSITORY /c/CVS

# name of package to pull from cvs repository
initializeVariable CVS_TAG brlcad

###
# END PUBLIC VARIABLE INITIALIZATION
####################################

# private vars

# name of systems directory
initializeVariable SYSTEMS_D systems.d

# name of tests directory
initializeVariable TESTS_D tests.d

# current working directory
initializeVariable START_PWD $LPWD

# cvs log file (do not change unless modify status.sh as well)
initializeVariable CVS_LOGFILE .cvs-${HOSTNAME}.log

# build up the usage and help incrementally just so it is easier to modify later
USAGE="Usage: $0 [-?] [-w] [-C]"
USAGE="${USAGE} [-S regression_system]"
USAGE="${USAGE} [-T regression_test]"
USAGE="${USAGE} [-d regression_dir]"
USAGE="${USAGE} [-c cvs_binary]"
USAGE="${USAGE} [-r cvs_repository]"
USAGE="${USAGE} [-m cvs_module]"
USAGE="${USAGE} [-t timeout]"
USAGE="${USAGE} [-l logfile]"

HELP="\t-?\tHelp\n"
HELP="${HELP}\t-w\tFail on warnings\n"
HELP="${HELP}\t-C\tClean out the regression directory\n"
HELP="${HELP}\t-S\tName of system to regression test (default is [$REGRESS_SYSTEM])\n"
HELP="${HELP}\t-T\tName of which test to run (default is [$REGRESS_TEST])\n"
HELP="${HELP}\t-d\tLocation to run regression test (default is [$REGRESS_DIR])\n"
HELP="${HELP}\t-c\tcvs command to run optionally including arguments (default is [$CVS])\n"
HELP="${HELP}\t-r\tLocation of cvs repository root (default is [$REPOSITORY])\n"
HELP="${HELP}\t-m\tName of cvs module to check out (default is [$CVS_TAG])\n"
HELP="${HELP}\t-t\tNumber of minutes to wait for master lock (default is [$WAIT_FOR_LOCK_TIMEOUT])\n"
HELP="${HELP}\t-l\tName of log file for script output (default is [$LOGFILE])"


if [ ! "x$DEBUG" = "x" ] ; then echo "1=[$1] 2=[$2]" ; fi

if [ x$1 = "x-?" ] ; then
    log "$USAGE"
    log "$HELP"
    exit
fi

args=`getopt S:T:d:c:r:m:t:l:wChH? $*`

if [ $? != 0 ] ; then
    log "$USAGE"
    log "$HELP"
    exit
fi

set -- $args

for i in $* ; do
    case "$i" in
	-S)
	    REGRESS_SYSTEM=$2; shift 2;;
	-T)
	    REGRESS_TEST=$2; shift 2;;
	-d)
	    REGRESS_DIR=$2; shift 2;;
	-c)
	    CVS=$2; shift 2;;
	-r) 
	    REPOSITORY=$2; shift 2;;
	-m)
	    CVS_TAG=$2; shift 2;;
	-t)
	    WAIT_FOR_LOCK_TIMEOUT=$2; shift 2;;
	-w)
	    NO_WARNINGS=1; shift 1;;
	-C)
	    CLOBBER_FIRST=1; shift 1;;
	-l)
	    LOGFILE=$2; shift 2;;
	-h | -H)
	    log "$USAGE"; exit;;
	# dash question should never be reached
	-\?)
	    echo "Odd getopt..."; log "$USAGE"; log "$HELP"; exit;;
	--)
	    shift; break;;
    esac
done

# export for debugging purposes
export REGRESS_DIR CVS REPOSITORY CVS_TAG WAIT_FOR_LOCK_TIMEOUT NO_WARNINGS LOGFILE


# check our regression directory settings for useability

# sanity check -- make sure someone didn't request ""
# not a good idea to use "." either, but we do not check
if [ "x$REGRESS_DIR" = "x" ] ; then
	bomb "Must specify regression directory [REGRESS_DIR]"
fi
#
#  Make sure the regression directory exists and is writeable
#
if [ ! -d "$REGRESS_DIR" ] ; then
    warn "regression directory [$REGRESS_DIR] does not exist"
    log "Creating [$REGRESS_DIR]"
    mkdir -m 700 -p $REGRESS_DIR
    if [ ! -d "$REGRESS_DIR" ] ; then
	bomb "Unable to create [$REGRESS_DIR]"
    fi
fi
if [ ! -w "${REGRESS_DIR}/." ] ; then
    bomb "unable to write to [$REGRESS_DIR]"
fi

# done checking state


# until we actually hold a lock, we do not write to a log file that may be in use
# by another master script that we won't be able to capture.
# in the meantime, we write to a tempfile based on our pid and hostname
#
# we do not use the LOG variable just so the user still manually override
TLOG="${REGRESS_DIR}/.temp-master-$$-${HOSTNAME}.log"

if [ -f TLOG ] ; then
    bomb "name collision with existing temp master log [$TLOG]"
fi
touch "$TLOG"
export LOG=$TLOG

log "R E G R E S S I O N   M A S T E R" $TLOG
log "=================================" $TLOG
log "Running [$0] with PID [$$] from [$LPWD] on [$HOSTNAME]" $TLOG
log "Date is [`date`]" $TLOG
log "Public Settings:" $TLOG
log "\tREGRESS_DIR=$REGRESS_DIR" $TLOG
log "\tCVS=$CVS" $TLOG
log "\tREPOSITORY=$REPOSITORY" $TLOG
log "\tCVS_TAG=$CVS_TAG" $TLOG
log "\tWAIT_FOR_LOCK_TIMEOUT=$WAIT_FOR_LOCK_TIMEOUT" $TLOG
log "\tNO_WARNINGS=$NO_WARNINGS" $TLOG
log "\tCLOBBER_FIRST=$CLOBBER_FIRST" $TLOG
log "\tLOGFILE=$LOGFILE" $TLOG
log "Private Settings:" $TLOG
log "\tLPWD=$LPWD" $TLOG
log "\tHOSTNAME=$HOSTNAME" $TLOG
log "\tLOG=$LOG" $TLOG


# from here on we need a lock to avoid nfs and multiple run clobbering
if ! eval "acquireLock master $WAIT_FOR_LOCK_TIMEOUT 60 $REGRESS_DIR" ; then
    bomb "Unable to obtain master.lock" $TLOG
fi


# WE ARE NOW LOCKED AND LOADED
# now that we have the lock, from here on out we need to release the lock and exit
# politely.  cleanup/unlock should be called prior to any exit

# safely dump our temp log file to the single master log file
if [ -f ${REGRESS_DIR}/$LOGFILE ] ; then
    if [ ! "x$CLOBBER_FIRST" = x1 ] ; then
	mv ${REGRESS_DIR}/$LOGFILE ${REGRESS_DIR}/${LOGFILE}.$$
	warn "Moving previous master log file [${REGRESS_DIR}/$LOGFILE] to [${REGRESS_DIR}/${LOGFILE}.$$]" $TLOG
    else
	rm ${REGRESS_DIR}/$LOGFILE
	log "Clobbered previous master log file" $TLOG
    fi
fi
# now we need to move the existing log file to the "real" file and delete the working temp 

touch ${REGRESS_DIR}/${LOGFILE}
if [ -w ${REGRESS_DIR}/${LOGFILE} ] ; then
    cat $TLOG >> ${REGRESS_DIR}/${LOGFILE}
else
    bomb "unable to write log file [${REGRESS_DIR}/${LOGFILE}]"
fi
rm $TLOG
LOG=${REGRESS_DIR}/$LOGFILE ; export LOG

#
# clean out the contents of the directory
# delete everything except the current log file
#
if [ x$CLOBBER_FIRST = x1 ] ; then
    log "Cleaning out $REGRESS_DIR"
    for ENTRY in `ls -A $REGRESS_DIR` ; do
	if [ ! "x$ENTRY" = "x$LOGFILE" ] ; then
	    if [ ! "x$DEBUG" = "x" ] ; then log "Deleting [${REGRESS_DIR}/${ENTRY}]" ; fi
	    rm -rf "${REGRESS_DIR}/${ENTRY}"
	    if [ $? != 0 ] ; then
		bomb "Failure cleaning out regression directroy $REGRESS_DIR"
	    fi
	else
	    log "Skipping deletion of $LOGFILE in $REGRESS_DIR"
	fi
    done
fi

# safely dump our temp log file to the single master log file
if [ -f ${REGRESS_DIR}/$CVS_LOGFILE ] ; then
    warn "Moving previous cvs log file [${REGRESS_DIR}/$CVS_LOGFILE] to [${REGRESS_DIR}/${CVS_LOGFILE}.$$]"
    mv ${REGRESS_DIR}/$CVS_LOGFILE ${REGRESS_DIR}/${CVS_LOGFILE}.$$
    touch ${REGRESS_DIR}/$CVS_LOGFILE
fi

#
# Get a copy of the cad package
#
log "Starting cvs export..."
log "Running [$CVS -q -d $REPOSITORY export -D today -d $REGRESS_DIR -N $CVS_TAG >> $REGRESS_DIR/$CVS_LOGFILE]"
$CVS -q -d $REPOSITORY export -D today -d $REGRESS_DIR -N $CVS_TAG >> ${REGRESS_DIR}/$CVS_LOGFILE
# make sure cvs export exited nicely
if [ $? != 0 ] ; then 
    bomb "cvs export failed"
else
    log "...done with cvs export"
fi

#
# Touch a file for each architecture that we support.  The clients wait for
# this file to be created before starting their build.
#
for ARCH in m4i64 m4i65 7d fbsd li sun5; do
    releaseSemaphore start_${ARCH} $REGRESS_DIR
done

# done with the critical part, return the lock
releaseLock master $REGRESS_DIR

log "Done $0"
