#!/bin/sh
#
# This status script will output and optionally mail the current status of a
# regression run.
#

# include standard utility functions
. `dirname $0`/library


# DEFAULT PUBLIC VARIABLE INITIALIZATION

# default working directory for checkouts and (un)packing
initializeVariable REGRESS_DIR /tmp/`whoami`

# name of reference architecture log files
initializeVariable REFERENCE_LOGS

# mail or display message by default
initializeVariable MAIL_RESULTS 0

# name of default user to mail to
initializeVariable MAIL_TO `whoami`

# default system to regression test
# all is a special keyword to regression test all of the systems
# otherwise the tests must manually be run one at a time through the shell
initializeVariable REGRESS_SYSTEM all

# END PUBLIC VARIABLE INITIALIZATION


# private vars

# name of systems directory
initializeVariable SYSTEMS_D systems.d

# fail on warnings (0==do not fail, 1==fail)
initializeVariable NO_WARNINGS 0

# build up the usage and help incrementally just so it is easier to modify later
USAGE="Usage: $0 [-?] [-w] [-m]"
USAGE="${USAGE} [-d regression_dir]"
USAGE="${USAGE} [-a address]"

HELP="\t-?\tHelp\n"
HELP="${HELP}\t-w\tFail on warnings\n"
HELP="${HELP}\t-m\tMail out results\n"
HELP="${HELP}\t-d\tLocation to run regression test (default is [$REGRESS_DIR])\n"
HELP="${HELP}\t-a\tAddress to send the status message to (default is [$MAIL_TO])"



# handle dash question special since some getopts don't play nice
if [ x$1 = "x-?" ] ; then
    log "$USAGE" /dev/tty
    log "$HELP" /dev/tty
    exit
fi

args=`getopt d:a:wmhH? $*`

if [ $? != 0 ] ; then
    log "$USAGE" /dev/tty
    log "$HELP" /dev/tty
    exit
fi

set -- $args

for i in $* ; do
    case "$i" in
	-d)
	    REGRESS_DIR="$2"; shift 2;;
	-m)
	    MAIL_RESULTS="1"; shift 1;;
	-w)
	    NO_WARNINGS="1"; shift 1;;
	-a)
	    MAIL_TO="$2"; MAIL_RESULTS="1"; shift 2;;
	-h | -H)
	    echo $USAGE; exit;;
	# dash question should never be reached
	-\?)
	    echo "Odd getopt..."; echo $USAGE; echo -e $HELP; exit;;
	--)
	    shift; break;;
    esac
done

###
# begin testing the state of the regression run purportedly happening in $REGRESS_DIR
#
# bad status gets incrementally saved in REGRESS_LOG
###
initializeVariable REGRESS_LOG ""


# check our regression directory settings for useability

# sanity check -- make sure someone didn't request ""
# not a good idea to use "." either, but we do not check
if [ "x$REGRESS_DIR" = "x" ] ; then
	echo "ERROR: Must specify regression directory [REGRESS_DIR=$REGRESS_DIR]"
	exit
fi

#
#  Make sure the regression directory exists
#
if [ ! -d "$REGRESS_DIR" ] ; then
    REGRESS_LOG="[$REGRESS_DIR] does not exist"
    log "$REGRESS_LOG"
#    mail "$MAIL_TO" "Regression Error: $REGRESS_LOG" "$REGRESS_LOG"
    exit
fi


# make sure the master script ran and completed successfully
MASTER_LOG_COUNT=`ls ${REGRESS_DIR}/.master-*.log | wc | awk '{print $1}'`
if [ $MASTER_LOG_COUNT -gt 0 ] ; then
    # assume this was the master log machine
    if [ -f ${REGRESS_DIR}/.master-${HOSTNAME}.log ] ; then
	MASTER_FILE="${REGRESS_DIR}/.master-${HOSTNAME}.log"
    # else if there is only one log, use it
    elif [ $MASTER_LOG_COUNT -eq 1 ] ; then
	MASTER_FILE=`ls ${REGRESS_DIR}/.master-*.log`
    # too many logs and none are mine, cannot choose
    else
	MASTER_FILE="___NONE___"
	REGRESS_LOG="${REGRESS_LOG}Could not determine if master.sh run completed\n\t(multiple log files and none are from $HOSTNAME)\n"
    fi

    if [ ! "x$DEBUG" = "x" ] ; then echo "Using master log file [$MASTER_FILE]" ; fi
    if [ ! "x$MASTER_FILE" = "x___NONE___" ] ; then
	MASTER_RETURN=`tail -1 $MASTER_FILE | awk '{print $1}'`
	if [ ! "x$DEBUG" = "x" ] ; then echo "MASTER_RETURN=[$MASTER_RETURN]" ; fi
	if [ ! "x$MASTER_RETURN" = "xDone" ] ; then
	    REGRESS_LOG="${REGRESS_LOG}the master script did not complete successfully\n\t(see [$MASTER_FILE] for details)\n"
	fi
    fi
    
else
    REGRESS_LOG="${REGRESS_LOG}master.sh script never ran\n\t(no log file available in [$REGRESS_DIR])\n"
fi


# make sure cvs completed successfully
CVS_LOG_COUNT=`ls ${REGRESS_DIR}/.cvs-*.log | wc | awk '{print $1}'`
if [ $CVS_LOG_COUNT -gt 0 ] ; then

    # assume this was the master log machine
    if [ -f "${REGRESS_DIR}/.cvs-${HOSTNAME}.log" ] ; then
	CVS_FILE="${REGRESS_DIR}/.cvs-${HOSTNAME}.log"
    # if there is only one log, use it
    elif [ $CVS_LOG_COUNT -eq 1 ] ; then
	CVS_FILE=`ls ${REGRESS_DIR}/.cvs-*.log`
    # too many logs and none are mine, cannot choose
    else
	CVS_FILE="___NONE___"
	REGRESS_LOG="${REGRESS_LOG}Could not determine if cvs export succeeded\n\t(multiple log files and none are from $HOSTNAME)\n"
    fi

    if [ ! "x$CVS_FILE" = "x___NONE___" ] ; then
	CVS_RETURN=`tail -1 $CVS_FILE | awk '{print $1}'`
	if [ ! "x$DEBUG" = "x" ] ; then echo "CVS_RETURN=[$CVS_RETURN]" ; fi
	if [ ! "x$CVS_RETURN" = "xOK:" ] ; then
	    if [ "x$CVS_RETURN" = "xERROR:" ] ; then
		REGRESS_LOG="${REGRESS_LOG}cvs export failed\n\t(see [$CVS_FILE] for details)\n"
	    else
		REGRESS_LOG="${REGRESS_LOG}cvs export appears to be in progress or was aborted\n\y(see [$CVS_FILE] for details)\n"
	    fi
	fi
    fi
else
    REGRESS_LOG="${REGRESS_LOG}cvs export seemingly failed\n\t(no cvs log file found in [$REGRESS_DIR])\n"
fi

# get all of the .regress.(ARCH) builds
ARCHES=`ls -1ad ${REGRESS_DIR}/.regress.* | awk -F. '{print $NF}'`


if [ "x$ARCHES" = x ] ; then
    REGRESS_LOG="${REGRESS_LOG}No clients appear to have ran\n\t(there are no .regress.ARCH directories in ${REGRESS_DIR})\n"
else
    # now iterate over the architectures we understand
    for ARCH in $ARCHES ; do
	if [ -d "$REGRESS_DIR/.regress.$ARCH" ] && [ -r "$REGRESS_DIR/.regress.$ARCH" ] ; then
	    echo "Found an [$ARCH] build"
	    
	    # is there no regress log (bad)
	    if [ ! -f "$REGRESS_DIR/.regress.$ARCH/MAKE_LOG" ] ; then
		REGRESS_LOG="${REGRESS_LOG}Architecture [$ARCH] never started build\n\t(no MAKE_LOG in $REGRESS_DIR/.regress.$ARCH)\n"

	    # there is a MAKE_LOG file -- the build at least started
	    else

		# Now we check to see how we compare to our "reference" run
		if [ -f ./ref_${ARCH} ] ; then
		    # diff options:
		    # -w no whitespace difference
		    # -B no blankline differences (not sun5 supported)
		    # -d optimal set of changes (not sun5 supported)
		    # -q report only if different (quick) (not sun5 supported)
		    DIFFERENT=`diff -w ./ref_${ARCH} $REGRESS_DIR/.regress.${ARCH}/MAKE_LOG | wc | awk '{print $1}'`
		    if [ ! "x$DIFFERENT" = x0 ] ; then
			REGRESS_LOG="${REGRESS_LOG}Architecture [${ARCH}] failed the regression test\n"
		    fi
		else
		    cp $REGRESS_DIR/.regress.${ARCH}/MAKE_LOG ./ref_${ARCH}		
		    REGRESS_LOG="${REGRESS_LOG}No reference file available for ${ARCH}, installed one (!)\n"
		fi
	    fi
	fi
    done
fi

log "$REGRESS_LOG"

# if mail output is requested, mail the results
if [ "x$MAIL_RESULTS" = "x1" ] ; then
    WC=`wc <<EOF
    $REGRESS_LOG
    EOF`
    if [ `echo $WC | awk '{print $2}'` -ne 0 ] ; then
        REGRESS_LOG="\n${REGRESS_LOG}\nSee $REGRESS_DIR/.regress.[ARCH]/MAKE_LOG for details\n"
	mail $MAIL_TO "Regression Errors" "$REGRESS_LOG"
    fi
    log "Results have been mailed to [$MAIL_TO]."
fi

log "Done status.sh"
exit