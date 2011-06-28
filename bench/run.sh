#!/bin/sh
#                          R U N . S H
# BRL-CAD
#
# Copyright (c) 2004-2011 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following
# disclaimer in the documentation and/or other materials provided
# with the distribution.
#
# 3. The name of the author may not be used to endorse or promote
# products derived from this software without specific prior written
# permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
###
#
# This shell script runs the BRL-CAD Benchmark.  The benchmark suite
# will test the performance of a system by iteratively rendering
# several well-known datasets into 512x512 images where performance
# metrics are documented and fairly well understood.  The local
# machine's performance is compared to the base system (called VGR)
# and a numeric "VGR" multiplier of performance is computed.  This
# number is a simplified metric from which one may qualitatively
# compare cpu and cache performance, versions of BRL-CAD, and
# different compiler characteristics.
#
###

# Ensure /bin/sh
export PATH || (echo "This isn't sh."; sh $0 $*; kill $$)

# save the precious args
ARGS="$*"
NAME_OF_THIS=`basename $0`
PATH_TO_THIS=`dirname $0`
THIS="$PATH_TO_THIS/$NAME_OF_THIS"

# sanity check
if test ! -f "$THIS" ; then
    echo "INTERNAL ERROR: $THIS does not exist"
    if test ! "x$0" = "x$THIS" ; then
	echo "INTERNAL ERROR: dirname/basename inconsistency: $0 != $THIS"
    fi
    exit 1
fi

# force locale setting to C so things like date output as expected
LC_ALL=C

# commands that this script expects
for __cmd in echo pwd ; do
    echo "test" | $__cmd > /dev/null 2>&1
    if test ! x$? = x0 ; then
	echo "INTERNAL ERROR: $__cmd command is required"
	exit 1
    fi
done
echo "test" | grep "test" > /dev/null 2>&1
if test ! x$? = x0 ; then
    echo "INTERNAL ERROR: grep command is required"
    exit 1
fi
echo "test" | tr "test" "test" > /dev/null 2>&1
if test ! x$? = x0 ; then
    echo "INTERNAL ERROR: tr command is required"
    exit 1
fi
echo "test" | sed "s/test/test/" > /dev/null 2>&1
if test ! x$? = x0 ; then
    echo "INTERNAL ERROR: sed command is required"
    exit 1
fi


# determine the behavior of echo
case `echo "testing\c"; echo 1,2,3`,`echo -n testing; echo 1,2,3` in
    *c*,-n*) ECHO_N= ECHO_C='
' ECHO_T='	' ;;
    *c*,*  ) ECHO_N=-n ECHO_C= ECHO_T= ;;
    *)       ECHO_N= ECHO_C='\c' ECHO_T= ;;
esac


#######################
# log to tty and file #
#######################
log ( ) {

    # this routine writes the provided argument(s) to a log file as
    # well as echoing them to stdout if we're not in quiet mode.

    if test ! "x$LOGFILE" = "x" ; then
	echo "$*" >> "$LOGFILE"
    fi
    if test ! "x$QUIET" = "x1" ; then
	echo "$*"
    fi
}


##############################
# ensure a 0|1 boolean value #
##############################
booleanize ( ) {

    # this routine examines the value of the specified variable and
    # converts it to a simple zero or one value in order to simplify
    # checks later on

    for var in $* ; do

	VAR="$var"
	VALCMD="echo \$$VAR"
	VAL="`eval $VALCMD`"

	CMD=__UNSET__
	case "x$VAL" in
	    x)
		CMD="$VAR=0"
		;;
	    x0)
		CMD="$VAR=0"
		;;
	    x[nN]*)
		CMD="$VAR=0"
		;;
	    x*)
		CMD="$VAR=1"
		;;
	esac

	if test ! "x$CMD" = "x__UNSET__" ; then
	    eval $CMD
	    export $VAR
	fi

    done
}


# process the argument list for commands
NEWARGS=""
for arg in $ARGS ; do
    case "x$arg" in
	x*[cC][lL][eE][aA][nN])
	    CLEAN=1
	    shift
	    ;;
	x*[cC][lL][oO][bB][bB][eE][rR])
	    CLEAN=1
	    CLOBBER=1
	    shift
	    ;;
	x*[hH])
	    HELP=1
	    shift
	    ;;
	x*[hH][eE][lL][pP])
	    HELP=1
	    shift
	    ;;
	x*[iI][nN][sS][tT][rR][uU][cC][tT]*)
	    INSTRUCTIONS=1
	    shift
	    ;;
	x*[sS][tT][aA][rR][tT])
	    shift
	    ;;
	x*[qQ][uU][iI][eE][tT])
	    QUIET=1
	    shift
	    ;;
	x*[vV][eE][rR][bB][oO][sS][eE])
	    VERBOSE=1
	    shift
	    ;;
	x*[rR][uU][nN])
	    RUN=1
	    shift
	    ;;
	x*=*)
	    VAR=`echo $arg | sed 's/=.*//g'`
	    if test ! "x$VAR" = "x" ; then
		VAL=`echo $arg | sed 's/.*=//g'`
		CMD="$VAR=$VAL"
		eval $CMD
		export $VAR
	    fi
	    shift
	    ;;
	x*)
	    echo "WARNING: Passing unknown option [$1] to RT"
	    NEWARGS="$NEWARGS $arg"
	    ;;
    esac
done

# reload post-shifting
ARGS="$NEWARGS"

# validate and clean up options (all default to 0)
booleanize CLOBBER HELP INSTRUCTIONS QUIET VERBOSE RUN


###
# handle help before main processing
###
if test "x$HELP" = "x1" ; then
    echo "Usage: $0 [command(s)] [OPTION=value] [RT_OPTIONS]"
    echo ""
    echo "Available commands:"
    echo "  clean"
    echo "  clobber"
    echo "  help          (this is what you are reading right now)"
    echo "  instructions"
    echo "  quiet"
    echo "  verbose"
    echo "  run           (this is probably what you want)"
    echo ""
    echo "Available options:"
    echo "  RT=/path/to/rt_binary (e.g., rt)"
    echo "  DB=/path/to/reference/geometry (e.g. ../db)"
    echo "  PIX=/path/to/reference/images (e.g., ../pix)"
    echo "  LOG=/path/to/reference/logs (e.g., ../pix)"
    echo "  CMP=/path/to/pixcmp_tool (e.g., pixcmp)"
    echo "  ELP=/path/to/time_tool (e.g., elapsed.sh)"
    echo "  TIMEFRAME=#seconds (default 32)"
    echo "  MAXTIME=#seconds (default 300)"
    echo "  DEVIATION=%deviation (default 3)"
    echo "  AVERAGE=#frames (default 3)"
    echo ""
    echo "Available RT options:"
    echo "  -P# (e.g., -P1 to force single CPU)"
    echo "  See the 'rt' manpage for additional options"
    echo ""
    echo "The BRL-CAD Benchmark tests the overall performance of a system"
    echo "compared to a stable well-known baseline.  The Benchmark calculates a"
    echo "figure-of-merit for this system that may be directly compared to other"
    echo "Benchmark runs on other system configurations."
    echo ""
    echo "BRL-CAD is a powerful cross-platform open source solid modeling system."
    echo "For more information about BRL-CAD, see http://brlcad.org"
    echo ""
    echo "Run '$0 instructions' or see the manpage for additional information."
    exit 1
fi


###
# handle instructions before main processing
###
if test "x$INSTRUCTIONS" = "x1" ; then
    cat <<EOF

This program runs the BRL-CAD Benchmark.  The benchmark suite will
test the performance of a system by iteratively rendering several
well-known datasets into 512x512 images where performance metrics are
documented and fairly well understood.  The local machine's
performance is compared to the base system (called VGR) and a numeric
"VGR" mulitplier of performance is computed.  This number is a
simplified metric from which one may qualitatively compare cpu and
cache performance, versions of BRL-CAD, and different compiler
characteristics.

The suite is intended to be run from a source distribution of BRL-CAD
after the package has been compiled either directly or via a make
build system target.  There are, however, several environment
variables that will modify how the BRL-CAD benchmark behaves so that
it may be run in a stand-alone environment:

  RT - the rt binary (e.g. ../src/rt/rt or /usr/brlcad/bin/rt)
  DB - the directory containing the reference geometry (e.g. ../db)
  PIX - the directory containing the reference images (e.g. ../pix)
  LOG - the directory containing the reference logs (e.g. ../pix)
  CMP - the name of a pixcmp tool (e.g. ./pixcmp or cmp)
  ELP - the name of an elapsed time tool (e.g. ../sh/elapsed.sh)
  TIMEFRAME - the minimum number of seconds each trace needs to take
  MAXTIME - the maximum number of seconds to spend on any test
  DEVIATION - the minimum sufficient % deviation from the average
  AVERAGE - how many frames to average together
  VERBOSE - turn on extra debug output for testing/development
  QUIET - turn off all printing output (writes results to summary)
  INSTRUCTIONS - display detailed instructions
  RUN - start a benchmark analysis

The TIMEFRAME, MAXTIME, DEVIATION, and AVERAGE options control how the
benchmark will proceed including how long it should take.  Each
individual benchmark run will consume at least a minimum TIMEFRAME of
wallclock time so that the results can stabilize.  After consuming at
least the minimum TIMEFRAME, additional frames may be computed until
the standard deviation from the last AVERAGE count of frames is below
the specified DEVIATION.  When a test is run and it completes in less
than TIMEFRAME, the raytrace is restarted using double the number of
rays from the previous run.  If the machine is fast enough, the
benchmark may accelerate the number or rays being fired.  These
additional rays are hypersampled but without any jitter, so it's
effectively performing a multiplier amount of work over the initial
frame.

Plese send your BRL-CAD Benchmark results to the developers along with
detailed system information to <devs@brlcad.org>.  Include at least:

  0) Compiler name and version (e.g. gcc --version)
  1) All generated log files (i.e. *.log* after benchmark completes)
  2) Anything else you think might be relevant to performance

The manual page has even more information and specific usage examples.
Run 'brlman benchmark'.

EOF
    exit 0
fi


###
# handle clean/clobber before main processing
###
if test "x$CLEAN" = "x1" ; then
    ECHO=echo
    $ECHO
    if test "x$CLOBBER" = "x1" ; then
	$ECHO "About to wipe out all benchmark images and log files in `pwd`"
	$ECHO "Send SIGINT (type ^C) within 5 seconds to abort"
	sleep 5
    else
	$ECHO "Deleting most benchmark images and log files in `pwd`"
	$ECHO "Running '$0 clobber' will remove run logs."
    fi
    $ECHO

    for i in moss world star bldg391 m35 sphflake ; do
	$ECHO rm -f $i.log $i.pix $i-[0-9]*.log $i-[0-9]*.pix
	rm -f $i.log $i.pix $i-[0-9]*.log $i-[0-9]*.pix
    done
    if test "x$CLOBBER" = "x1" ; then
	# NEVER automatically delete the summary file, but go ahead with the rest
	for i in benchmark-[0-9]*-run.log ; do
	    $ECHO rm -f $i
	    rm -f $i
	done
    fi

    printed=no
    for i in summary benchmark-[0-9]*-run.log ; do
	if test -f "$i" ; then
	    if test "x$printed" = "xno" ; then
		$ECHO
		$ECHO "The following files must be removed manually:"
		printed=yes
	    fi
	    $ECHO $ECHO_N "$i " $ECHO_C
	fi
    done
    if test "x$printed" = "xyes" ; then
	$ECHO
    fi

    $ECHO
    if test "x$CLOBBER" = "x1" ; then
	$ECHO "Benchmark clobber complete."
    else
	$ECHO "Benchmark clean complete."
    fi
    exit 0
fi


###
# B E G I N
###

# make sure they ask for it
if test "x$RUN" = "x0" ; then
    echo "Type '$0 help' for usage."
    exit 1
fi

# where to write results
LOGFILE=run-$$-benchmark.log
touch "$LOGFILE"
if test ! -w "$LOGFILE" ; then
    if test ! "x$LOGFILE" = "x/dev/null" ; then
	echo "ERROR: Unable to log to $LOGFILE"
    fi
    LOGFILE=/dev/null
fi

VERBOSE_ECHO=:
ECHO=log
if test "x$QUIET" = "x1" ; then
    if test "x$VERBOSE" = "x1" ; then
	echo "Verbose output quelled by quiet option.  Further output disabled."
    fi
else
    if test "x$VERBOSE" = "x1" ; then
	VERBOSE_ECHO=echo
	echo "Verbose output enabled"
    fi
fi

$ECHO "B R L - C A D   B E N C H M A R K"
$ECHO "================================="
$ECHO "Running $THIS on `date`"
$ECHO "Logging output to $LOGFILE"
$ECHO "`uname -a 2>&1`"
$ECHO

########################
# search for resources #
########################
look_for ( ) {

    # utility function to search for a certain filesystem object in a
    # list of paths.

    look_for_type="$1" ; shift
    look_for_label="$1" ; shift
    look_for_var="$1" ; shift
    look_for_dirs="$*"

    if test "x$look_for_label" != "x" ; then
	$VERBOSE_ECHO  "Looking for $look_for_label"
    fi

    # get the value of the variable
    look_for_var_var="echo \"\$$look_for_var\""
    look_for_var_val="`eval ${look_for_var_var}`"

    if test "x${look_for_var_val}" = "x" ; then
	for look_for_dir in $look_for_dirs ; do

	    opts="-r"
	    case "x$look_for_type" in
		xfile)
		    opts="$opts -f"
		    ;;
		xdir*)
		    # should work without read bit
		    opts="-d -x"
		    ;;
		xexe*)
		    opts="$opts -f -r -x"
		    ;;
		xscr*)
		    opts="$opts -f -r -x"
		    ;;
	    esac
	    look_for_failed=no
	    for opt in $opts ; do
		if test ! $opt "${look_for_dir}" ; then
		    look_for_failed=yes
		    break
		fi
	    done
	    if test "x$look_for_failed" = "xno" ; then
		if test "x$look_for_label" != "x" ; then
		    $VERBOSE_ECHO "...found $look_for_type ${look_for_dir}"
		fi
		look_for_var_var="${look_for_var}=\"${look_for_dir}\""
		eval $look_for_var_var
		export $look_for_var
		break
	    fi
	done
    else
	if test "x$look_for_label" != "x" ; then
	    $VERBOSE_ECHO "...using $look_for_var_val from $look_for_var variable setting"
	fi
    fi
}

look_for executable "the BRL-CAD raytracer" RT \
    ${PATH_TO_THIS}/rt \
    ${PATH_TO_THIS}/../bin/rt \
    ${PATH_TO_THIS}/../src/rt/rt \
    ${PATH_TO_THIS}/src/rt/rt \
    ./rt \
    ../brlcadInstall/bin/rt.exe \
    ../src/rt/rt

look_for file "a benchmark geometry directory" DB \
    ${PATH_TO_THIS}/../share/brlcad/*.*.*/db/moss.g \
    ${PATH_TO_THIS}/share/brlcad/*.*.*/db/moss.g \
    ${PATH_TO_THIS}/../share/brlcad/db/moss.g \
    ${PATH_TO_THIS}/share/brlcad/db/moss.g \
    ${PATH_TO_THIS}/../share/db/moss.g \
    ${PATH_TO_THIS}/share/db/moss.g \
    ${PATH_TO_THIS}/../db/moss.g \
    ${PATH_TO_THIS}/db/moss.g \
    ./db/moss.g \
    ../brlcadInstall/share/brlcad/*.*.*/db/moss.g \
    ../db/moss.g
DB=`echo $DB | sed 's,/moss.g$,,'`

look_for directory "a benchmark reference image directory" PIX \
    ${PATH_TO_THIS}/../share/brlcad/*.*.*/pix \
    ${PATH_TO_THIS}/share/brlcad/*.*.*/pix \
    ${PATH_TO_THIS}/../share/brlcad/pix \
    ${PATH_TO_THIS}/share/brlcad/pix \
    ${PATH_TO_THIS}/../share/pix \
    ${PATH_TO_THIS}/share/pix \
    ${PATH_TO_THIS}/../pix \
    ${PATH_TO_THIS}/pix \
    ./pix

look_for directory "a benchmark reference log directory" LOG \
    $PIX \
    ${PATH_TO_THIS}/../share/brlcad/*.*.*/pix \
    ${PATH_TO_THIS}/share/brlcad/*.*.*/pix \
    ${PATH_TO_THIS}/../share/brlcad/pix \
    ${PATH_TO_THIS}/share/brlcad/pix \
    ${PATH_TO_THIS}/../share/pix \
    ${PATH_TO_THIS}/share/pix \
    ${PATH_TO_THIS}/../pix \
    ${PATH_TO_THIS}/pix \
    ./pix

look_for executable "a pixel comparison utility" CMP \
    ${PATH_TO_THIS}/pixcmp \
    ${PATH_TO_THIS}/../bin/pixcmp \
    ${PATH_TO_THIS}/../bench/pixcmp \
    ../brlcadInstall/bin/pixcmp.exe \
    ./pixcmp

look_for script "a time elapsed utility" ELP \
    ${PATH_TO_THIS}/elapsed.sh \
    ${PATH_TO_THIS}/../bin/elapsed.sh \
    ${PATH_TO_THIS}/sh/elapsed.sh \
    ${PATH_TO_THIS}/../sh/elapsed.sh \
    ./elapsed.sh


#####################
# output parameters #
#####################

# sanity check, output all the final settings together
if test "x${RT}" = "x" ; then
    $ECHO "ERROR:  Could not find the BRL-CAD raytracer"
    exit 1
else
    $ECHO "Using [$RT] for RT"
fi
if test "x${DB}" = "x" ; then
    $ECHO "ERROR:  Could not find the BRL-CAD database directory"
    exit 1
else
    $ECHO "Using [$DB] for DB"
fi
if test "x${PIX}" = "x" ; then
    $ECHO "ERROR:  Could not find the BRL-CAD reference images"
    exit 1
else
    $ECHO "Using [$PIX] for PIX"
fi
if test "x${LOG}" = "x" ; then
    $ECHO "ERROR:  Could not find the BRL-CAD reference logs"
    exit 1
else
    $ECHO "Using [$LOG] for LOG"
fi
if test "x${CMP}" = "x" ; then
    $ECHO "ERROR:  Could not find the BRL-CAD pixel comparison utility"
    exit 1
else
    $ECHO "Using [$CMP] for CMP"
fi
if test "x${ELP}" = "x" ; then
    $ECHO "ERROR:  Could not find the BRL-CAD time elapsed script"
    exit 1
else
    $ECHO "Using [$ELP] for ELP"
fi

# more sanity checks, make sure the binaries and scripts run
$RT -s1 -F/dev/debug ${DB}/moss.g LIGHT > /dev/null 2>&1
ret=$?
if test ! "x${ret}" = "x0" ; then
    $ECHO
    $ECHO "ERROR:  RT does not seem to work as expected"
    exit 2
fi

# create a temporary file named "null", fopen("/dev/null") does not work on
# windows (using cygwin), so punt.
> null
$CMP null null >/dev/null 2>&1
rm -f null

ret=$?
if test ! "x${ret}" = "x0" ; then
    $ECHO
    $ECHO "ERROR:  CMP does not seem to work as expected"
    exit 2
fi
$ELP 0 > /dev/null 2>&1
if test ! "x${ret}" = "x0" ; then
    $ECHO
    $ECHO "ERROR:  ELP does not seem to work as expected"
    exit 2
fi 


# utility function to set a variable if it's not already set to something
set_if_unset ( ) {
    set_if_unset_name="$1" ; shift
    set_if_unset_val="$1" ; shift

    set_if_unset_var="echo \"\$$set_if_unset_name\""
    set_if_unset_var_val="`eval ${set_if_unset_var}`"
    if test "x${set_if_unset_var_val}" = "x" ; then
	set_if_unset_var="${set_if_unset_name}=\"${set_if_unset_val}\""
	$VERBOSE_ECHO $set_if_unset_var
	eval $set_if_unset_var
	export $set_if_unset_name
    fi

    set_if_unset_var="echo \"\$$set_if_unset_name\""
    set_if_unset_val="`eval ${set_if_unset_var}`"
    $ECHO "Using [${set_if_unset_val}] for $set_if_unset_name"
}

# determine the minimum time requirement in seconds for a single test run
set_if_unset TIMEFRAME 32

# approximate maximum time in seconds that a given test is allowed to take
set_if_unset MAXTIME 300
if test $MAXTIME -lt $TIMEFRAME ; then
    $ECHO "ERROR: MAXTIME must be greater or equal to TIMEFRAME"
    exit 1
fi

# maximum deviation percentage
set_if_unset DEVIATION 3

# maximum number of iterations to average
set_if_unset AVERAGE 3

# end of settings, separate the output
$ECHO


##########################
# output run-time status #
##########################

# determine raytracer version
$ECHO "RT reports the following version information:"
versions="`$RT 2>&1 | grep BRL-CAD | grep Release`"
if test "x$versions" = "x" ; then
    $ECHO "Unknown"
else
    $ECHO "$versions"
fi
$ECHO


# if expr works, let the user know about how long this might take
if test "x`expr 1 - 1 2>/dev/null`" = "x0" ; then
    mintime="`expr 6 \* $TIMEFRAME`"
    if test $mintime -lt 1 ; then
	mintime=0 # zero is okay
    fi
    $ECHO "Minimum run time is `$ELP $mintime`"
    maxtime="`expr 6 \* $MAXTIME`"
    if test $maxtime -lt 1 ; then
	maxtime=1 # zero would be misleading
    fi
    $ECHO "Maximum run time is `$ELP $maxtime`"
    estimate="`expr 3 \* $mintime`"
    if test $estimate -lt 1 ; then
	estimate=1 # zero would be misleading
    fi
    if test $estimate -gt $maxtime ; then
	estimate="$maxtime"
    fi
    $ECHO "Estimated time is `$ELP $estimate`"
    $ECHO
else
    $ECHO "WARNING: expr is unavailable, unable to compute statistics"
    $ECHO
fi


#################################
# run and computation functions #
#################################

#
# run file_prefix geometry hypersample [..rt args..]
#   runs a single benchmark test assuming the following are preset:
#
#   RT := path/name of the raytracer to use
#   DB := path to the geometry file
#
# it is assumed that stdin will be the view/frame input
#
run ( ) {
    run_geomname="$1" ; shift
    run_geometry="$1" ; shift
    run_hypersample="$1" ; shift
    run_args="$*"
    run_view="`cat`"

    $VERBOSE_ECHO "DEBUG: Running $RT -B -M -s512 -H${run_hypersample} -J0 ${run_args} -o ${run_geomname}.pix ${DB}/${run_geomname}.g ${run_geometry}"

    $RT -B -M -s512 -H${run_hypersample} -J0 ${run_args} \
	-o ${run_geomname}.pix \
	${DB}/${run_geomname}.g ${run_geometry} 1>&2 <<EOF
$run_view
EOF
    retval=$?
    $VERBOSE_ECHO "DEBUG: Running $RT returned $retval"
    return $retval
}


#
# average [..numbers..]
#   computes the integer average for a set of given numbers
#
average ( ) {
    average_nums="$*"

    if test "x$average_nums" = "x" ; then
	$ECHO "ERROR: no numbers provided to average" 1>&2
	exit 1
    fi

    total=0
    count=0
    for num in $average_nums ; do
	total="`expr $total + $num`"
	count="`expr $count + 1`"
    done

    if test $count -eq 0 ; then
	$ECHO "ERROR: unexpected count in average" 1>&2
	exit 1
    fi

    echo "`expr $total / $count`"
    return 0
}


#
# getvals count [..numbers..]
#   extracts up to count integer values from a set of numbers
#
getvals ( ) {
    getvals_count="$1" ; shift
    getvals_nums="$*"

    if test "x$getvals_count" = "x" ; then
	getvals_count=10000
    elif test $getvals_count -eq 0 ; then
	echo ""
	return 0
    fi

    if test "x$getvals_nums" = "x" ; then
	echo ""
	return 0
    fi

    # get up to count values from the nums provided
    getvals_got=""
    getvals_counted=0
    for getvals_num in $getvals_nums ; do
	if test $getvals_counted -ge $getvals_count ; then
	    break
	fi
	# getvals_int="`echo $getvals_num | sed 's/\.[0-9]*//'`"
	getvals_int=`echo $getvals_num | awk '{print int($1+0.5)}'`
	getvals_got="$getvals_got $getvals_int"
	getvals_counted="`expr $getvals_counted + 1`"
    done

    echo "$getvals_got"
    return $getvals_counted
}


#
# variance count [..numbers..]
#   computes an integer variance for up to count numbers
#
variance ( ) {
    variance_count="$1" ; shift
    variance_nums="$*"

    if test "x$variance_count" = "x" ; then
	variance_count=10000
    elif test $variance_count -eq 0 ; then
	$ECHO "ERROR: cannot compute variance of zero numbers" 1>&2
	exit 1
    fi

    if test "x$variance_nums" = "x" ; then
	$ECHO "ERROR: cannot compute variance of nothing" 1>&2
	exit 1
    fi

    # get up to count values from the nums provided
    variance_got="`getvals $variance_count $variance_nums`"
    variance_counted="$?"

    if test $variance_counted -eq 0 ; then
	$ECHO "ERROR: unexpected zero count of numbers in variance" 1>&2
	exit 1
    elif test $variance_counted -lt 0 ; then
	$ECHO "ERROR: unexpected negative count of numbers in variance" 1>&2
	exit 1
    fi

    # compute the average of the nums we got
    variance_average="`average $variance_got`"

    # compute the variance numerator of the population
    variance_error=0
    for variance_num in $variance_got ; do
	variance_err_sq="`expr \( $variance_num - $variance_average \) \* \( $variance_num - $variance_average \)`"
	variance_error="`expr $variance_error + $variance_err_sq`"
    done

    # make sure the error is non-negative
    if test $variance_error -lt 0 ; then
	variance_error="`expr 0 - $variance_error`"
    fi

    # echo the variance result
    echo "`expr $variance_error / $variance_counted`"
}


#
# sqrt number
#   computes the square root of some number
#
sqrt ( ) {
    sqrt_number="$1"

    if test "x$sqrt_number" = "x" ; then
	$ECHO "ERROR: cannot compute the square root of nothing" 1>&2
	exit 1
    elif test $sqrt_number -lt 0 > /dev/null 2>&1 ; then
	$ECHO "ERROR: square root of negative numbers is only in your imagination" 1>&2
	exit 1
    fi

    sqrt_have_dc=yes
    echo "1 1 + p" | dc >/dev/null 2>&1
    if test ! x$? = x0 ; then
	sqrt_have_dc=no
    fi

    sqrt_root=""
    if test "x$sqrt_have_dc" = "xyes" ; then
	sqrt_root=`echo "$sqrt_number v p" | dc`
    else
	sqrt_have_bc=yes
	echo "1 + 1" | bc >/dev/null 2>&1
	if test ! "x$?" = "x0" ; then
	    sqrt_have_bc=no
	fi

	if test "x$sqrt_have_bc" = "xyes" ; then
	    sqrt_root=`echo "sqrt($sqrt_number)" | bc`
	else
	    sqrt_root=`echo $sqrt_number | awk '{print sqrt($1)}'`
	fi
    fi

    echo `echo $sqrt_root | awk '{print int($1+0.5)}'`

    return
}


#
# clean_obstacles base_filename
#   conditionally removes or saves backup of any .pix or .log files
#   output during bench.
#
clean_obstacles ( ) {
    base="$1" ; shift

    if test "x$base" = "x" ; then
	$ECHO "ERROR: argument mismatch, cleaner is missing base name"
	exit 1
    fi

    # look for an image file
    if test -f ${base}.pix; then
	if test -f ${base}-$$.pix ; then
	    # backup already exists, just delete obstacle
	    rm -f ${base}.pix
	else
	    # no backup exists yet, so keep it
	    mv -f ${base}.pix ${base}-$$.pix
	fi
    fi

    # look for a log file
    if test -f ${base}.log; then
	if test -f ${base}-$$.log ; then
	    # backup already exists, just delete obstacle
	    rm -f ${base}.log
	else
	    # no backup exists yet, so keep it
	    mv -f ${base}.log ${base}-$$.log
	fi
    fi
}


#
# bench test_name geometry [..rt args..]
#   runs a series of benchmark tests assuming the following are preset:
#
#   TIMEFRAME := maximum amount of wallclock time to spend per test
#
# is is assumed that stdin will be the view/frame input
#
bench ( ) {
    bench_testname="$1" ; shift
    bench_geometry="$1" ; shift
    bench_args="$*"

    if test "x$bench_testname" = "x" ; then
	$ECHO "ERROR: argument mismatch, bench is missing the test name"
	return 1
    fi
    if test "x$bench_geometry" = "x" ; then
	$ECHO "ERROR: argument mismatch, bench is missing the test geometry"
	return 1
    fi
    $VERBOSE_ECHO "DEBUG: Beginning bench testing on $bench_testname using $bench_geometry"

    bench_view="`cat`"

    $ECHO +++++ ${bench_testname}
    bench_hypersample=0
    bench_frame=0
    bench_rtfms=""
    bench_percent=100
    bench_start_time="`date '+%H %M %S'`"
    bench_overall_elapsed=-1

    # clear out before we begin, only saving backup on first encounter
    clean_obstacles "$bench_testname"

    # use -lt since -le makes causes a "floor(elapsed)" comparison and too many iterations
    while test $bench_overall_elapsed -lt $MAXTIME ; do

	bench_elapsed=-1
	# use -lt since -le makes causes a "floor(elapsed)" comparison and too many iterations
	while test $bench_elapsed -lt $TIMEFRAME ; do

	    # clear out any previous run, only saving backup on first encounter
	    clean_obstacles "$bench_testname"

	    bench_frame_start_time="`date '+%H %M %S'`"

	    run $bench_testname $bench_geometry $bench_hypersample $bench_args 2> ${bench_testname}.log << EOF
$bench_view
start $bench_frame;
end;
EOF
	    retval=$?

	    if test -f ${bench_testname}.pix.$bench_frame ; then mv -f ${bench_testname}.pix.$bench_frame ${bench_testname}.pix ; fi

	    # compute how long we took, rounding up to at least one
	    # second to prevent division by zero.
	    bench_elapsed="`$ELP --seconds $bench_frame_start_time`"
	    if test "x$bench_elapsed" = "x" ; then
		bench_elapsed=1
	    fi
	    if test $bench_elapsed -eq 0 ; then
		bench_elapsed=1
	    fi
	    if test "x$bench_hypersample" = "x0" ; then

		# just finished the first frame
		$VERBOSE_ECHO "DEBUG: ${bench_elapsed}s real elapsed,	1 ray/pixel,	`expr 262144 / $bench_elapsed` pixels/s (inexact wallclock)"
		bench_hypersample=1
		bench_frame="`expr $bench_frame + 1`"
	    else
		$VERBOSE_ECHO "DEBUG: ${bench_elapsed}s real elapsed,	`expr $bench_hypersample + 1` rays/pixel,	`expr \( 262144 \* \( $bench_hypersample + 1 \) / $bench_elapsed \)` pixels/s (inexact wallclock)"


		# increase the number of rays exponentially if we are
		# considerably faster than the TIMEFRAME required.
		if test `expr $bench_elapsed \* 128` -lt ${TIMEFRAME} ; then
		    # 128x increase, skip six frames
		    bench_hypersample="`expr $bench_hypersample \* 128 + 127`"
		    bench_frame="`expr $bench_frame + 7`"
		elif test `expr $bench_elapsed \* 64` -lt ${TIMEFRAME} ; then
		    # 64x increase, skip five frames
		    bench_hypersample="`expr $bench_hypersample \* 64 + 63`"
		    bench_frame="`expr $bench_frame + 6`"
		elif test `expr $bench_elapsed \* 32` -lt ${TIMEFRAME} ; then
		    # 32x increase, skip four frames
		    bench_hypersample="`expr $bench_hypersample \* 32 + 31`"
		    bench_frame="`expr $bench_frame + 5`"
		elif test `expr $bench_elapsed \* 16` -lt ${TIMEFRAME} ; then
		    # 16x increase, skip three frames
		    bench_hypersample="`expr $bench_hypersample \* 16 + 15`"
		    bench_frame="`expr $bench_frame + 4`"
		elif test `expr $bench_elapsed \* 8` -lt ${TIMEFRAME} ; then
		    # 8x increase, skip two frames
		    bench_hypersample="`expr $bench_hypersample \* 8 + 7`"
		    bench_frame="`expr $bench_frame + 3`"
		elif test `expr $bench_elapsed \* 4` -lt ${TIMEFRAME} ; then
		    # 4x increase, skip a frame
		    bench_hypersample="`expr $bench_hypersample \* 4 + 3`"
		    bench_frame="`expr $bench_frame + 2`"
		else
		    # 2x increase
		    bench_hypersample="`expr $bench_hypersample + $bench_hypersample + 1`"
		    bench_frame="`expr $bench_frame + 1`"
		fi
	    fi

	    # save the rtfm for variance computations then print it
	    bench_rtfm_line="`grep RTFM ${bench_testname}.log`"
	    bench_rtfm="`echo $bench_rtfm_line | awk '{print int($9+0.5)}'`"
	    if test "x$bench_rtfm" = "x" ; then
		bench_rtfm="0"
	    fi
	    bench_rtfms="$bench_rtfm $bench_rtfms"
	    if test ! "x$bench_rtfm_line" = "x" ; then
		$ECHO "$bench_rtfm_line"
	    fi

	    # did we fail?
	    if test $retval != 0 ; then
		$ECHO "RAYTRACE ERROR"
		break
	    fi

	    # see if we need to break out early
	    bench_overall_elapsed="`$ELP --seconds $bench_start_time`"
	    if test $bench_overall_elapsed -ge $MAXTIME ; then
		break;
	    fi
	done

	if test "x$bench_rtfm" = "x" ; then
	    bench_rtfm="0"
	fi
	if test "x$bench_rtfms" = "x" ; then
	    bench_rtfms="0"
	fi

	# outer loop for variance/deviation testing of last AVERAGE frames
	bench_variance="`variance $AVERAGE $bench_rtfms`"
	bench_deviation="`sqrt $bench_variance`"
	if test $bench_rtfm -eq 0 ; then
	    bench_percent=0
	else
	    bench_percent=`echo $bench_deviation $bench_rtfm | awk '{print int(($1 / $2 * 100)+0.5)}'`
	fi

	if test "x$VERBOSE" != "x" ; then
	    bench_vals="`getvals $AVERAGE $bench_rtfms`"
	    bench_avg="`average $bench_vals`"
	    if test $bench_avg -eq 0 ; then
		bench_avgpercent=0
	    else
		bench_avgpercent=`echo $bench_deviation $bench_avg | awk '{print $1 / $2 * 100}'`
	    fi
	    $VERBOSE_ECHO "DEBUG: average=$bench_avg ; variance=$bench_variance ; deviation=$bench_deviation ($bench_avgpercent%) ; last run was ${bench_percent}%"
	fi

	# early exit if we have a stable number
	if test $bench_percent -le $DEVIATION ; then
	    break
	fi

	bench_overall_elapsed="`$ELP --seconds $bench_start_time`"

	# undo the hypersample increase back one step
	bench_hypersample="`expr \( \( $bench_hypersample + 1 \) / 2 \) - 1`"
    done

    # the last run should be a relatively stable representative of the performance

    if test -f gmon.out; then mv -f gmon.out gmon.${bench_testname}.out; fi
    ${CMP} ${PIX}/${bench_testname}.pix ${bench_testname}.pix
    ret=$?
    if test $ret = 0 ; then
	# perfect match
	$ECHO ${bench_testname}.pix:  answers are RIGHT
    elif test $ret = 1 ; then
	# off by one, acceptable
	$ECHO ${bench_testname}.pix:  answers are RIGHT
    elif test $ret = 2 ; then
	# off by many, unacceptable
	$ECHO ${bench_testname}.pix:  WRONG WRONG WRONG WRONG WRONG WRONG
    else
	# some other failure
	$ECHO ${bench_testname}.pix:  BENCHMARK COMPARISON FAILURE
    fi

    $VERBOSE_ECHO "DEBUG: Done benchmark testing on $bench_testname"
    return $retval
}


#
# perf test_name geometry [..rt args..]
#
perf ( ) {
    perf_tests="$1" ; shift
    perf_args="$*"

    if test "x$perf_tests" = "x" ; then
	$ECHO "ERROR: no tests specified for calculating performance" 1>&2
	exit 1
    fi

    # figure out what machine this is
    perf_host=$HOSTNAME
    if test "x$perf_host" = "x" ; then
	perf_host="`hostname`"
    fi
    if test "x$perf_host" = "x" ; then
	perf_host="`uname -n`"
    fi
    if test "x$perf_host" = "x" ; then
	perf_host="unknown"
    fi

    # when did we do this thing
    perf_date="`date`"

    # make sure the log files exist
    perf_ref_files=""
    perf_cur_files=""
    for perf_test in $perf_tests ; do
	perf_ref_log=${LOG}/${perf_test}.log
	perf_cur_log=${perf_test}.log
	for perf_log in "$perf_cur_log" "$perf_ref_log" ; do
	    if test ! "x$perf_log" = "x" ; then
		if test ! -f "$perf_log" ; then
		    $ECHO "ERROR: file $perf_log does not exist" 1>&2
		fi
	    fi
	done
	perf_ref_files="$perf_ref_files $perf_ref_log"
	perf_cur_files="$perf_cur_files $perf_cur_log"
    done

    # extract the RTFM values from the log files, use TR to convert
    # newlines to tabs.  the trailing tab is signficant in case there
    # are not enough results.
    #
    # FIXME: should really iterate one file at a time so we don't
    # just zero-pad at the end
    perf_VGRREF=`grep RTFM $perf_ref_files | sed -n -e 's/^.*= *//' -e 's/ rays.*//p' | tr '\012' '\011' `
    perf_CURVALS=`grep RTFM $perf_cur_files | sed -n -e 's/^.*= *//' -e 's/ rays.*//p' | tr '\012' '\011' `

    # if there were no reference values, we cannot compute timings
    if test "x$perf_VGRREF" = "x" ; then
	$ECHO "ERROR: Cannot locate VGR reference values" 1>&2
    fi

    # report 0 if no RTFM values were found in the current run (likely
    # crashing), values are tab-delimited.
    if test "x$perf_CURVALS" = "x" ; then
	perf_CURVALS="0	0	0	0	0	0	"
    fi

    # Trick: Force args $1 through $6 to the numbers in $perf_CURVALS
    # This should be "set -- $perf_CURVALS", but 4.2BSD /bin/sh can't
    # handle it, and perf_CURVALS are all positive (ie, no leading
    # dashes), so this is safe.

    set $perf_CURVALS

    while test $# -lt 6 ; do
	$ECHO "WARNING: only $# RTFM times found, adding a zero result." 1>&2
	perf_CURVALS="${perf_CURVALS}0	"
	set $perf_CURVALS
    done

    # see if we have a calculator
    perf_have_dc=yes
    echo "1 1 + p" | dc >/dev/null 2>&1
    if test ! x$? = x0 ; then
	perf_have_dc=no
    fi

    perf_have_bc=yes
    echo "1 + 1" | bc >/dev/null 2>&1
    if test ! x$? = x0 ; then
	perf_have_bc=no
    fi

    for perf_ref in $perf_VGRREF ; do
	perf_cur=$1
	shift

	if test "x$perf_have_dc" = "xyes" ; then
	    perf_RATIO=`echo "2k $perf_cur $perf_ref / p" | dc`
	elif test "x$perf_have_bc" = "xyes" ; then
	    # presume bc as an alternate (tsk tsk)
	    perf_RATIO=`echo "scale=2; $perf_cur / $perf_ref" | bc`
	else
	    perf_RATIO=`echo $perf_cur $perf_ref | awk '{printf "%.2f", $1/$2}'`
	fi
	# Note: append new value and a trail TAB to existing list.
	perf_RATIO_LIST="${perf_RATIO_LIST}$perf_RATIO	"
    done

    # The number of plus signs must be one less than the number of elements.
    if test "x$perf_have_dc" = "xyes" ; then
	perf_MEAN_ABS=`echo 2k $perf_CURVALS +++++ 6/ p | dc`
	perf_MEAN_REL=`echo 2k $perf_RATIO_LIST +++++ 6/ p | dc`
    elif test "x$perf_have_bc" = "xyes" ; then
	perf_expr="scale=2; ( 0"
	for perf_val in $perf_CURVALS ; do
	    perf_expr="$perf_expr + $perf_val"
	done
	perf_expr="$perf_expr ) / 6"
	perf_MEAN_ABS=`echo $perf_expr | bc`

	perf_expr="scale=2; ( 0"
	for perf_val in $perf_RATIO_LIST ; do
	    perf_expr="$perf_expr + $perf_val"
	done
	perf_expr="$perf_expr ) / 6"
	perf_MEAN_REL=`echo $perf_expr | bc`
    else
	perf_MEAN_ABS=`echo $perf_CURVALS | awk '{printf "%.2f", ($1+$2+$3+$4+$5+$6)/6}'`
	perf_MEAN_REL=`echo $perf_RATIO_LIST | awk '{printf "%.2f", ($1+$2+$3+$4+$5+$6)/6}'`
    fi

    # Note: Both perf_RATIO_LIST and perf_CURVALS have an extra
    # trailing tab.  The question mark is for the mean field.

    echo "Abs  ${perf_host} ${perf_CURVALS}${perf_MEAN_ABS}	$perf_date"
    echo "*vgr ${perf_host} ${perf_RATIO_LIST}${perf_MEAN_REL}	$perf_args"
}


########################
# Run the actual tests #
########################

start="`date '+%H %M %S'`"
$ECHO "Running the BRL-CAD Benchmark tests... please wait ..."
$ECHO
ret=0

bench moss all.g $ARGS << EOF
viewsize 1.572026215e+02;
eye_pt 6.379990387e+01 3.271768951e+01 3.366661453e+01;
viewrot -5.735764503e-01 8.191520572e-01 0.000000000e+00 0.000000000e+00
	-3.461886346e-01 -2.424038798e-01 9.063078165e-01 0.000000000e+00
	7.424039245e-01 5.198368430e-01 4.226182699e-01 0.000000000e+00
	0.000000000e+00 0.000000000e+00 0.000000000e+00 1.000000000e+00 ;
EOF
ret=`expr $ret + $?`

bench world all.g $ARGS << EOF
viewsize 1.572026215e+02;
eye_pt 6.379990387e+01 3.271768951e+01 3.366661453e+01;
viewrot -5.735764503e-01 8.191520572e-01 0.000000000e+00 0.000000000e+00
	-3.461886346e-01 -2.424038798e-01 9.063078165e-01 0.000000000e+00
	7.424039245e-01 5.198368430e-01 4.226182699e-01 0.000000000e+00
	0.000000000e+00 0.000000000e+00 0.000000000e+00 1.000000000e+00 ;
EOF
ret=`expr $ret + $?`

bench star all $ARGS << EOF
viewsize 2.500000000e+05;
eye_pt 2.102677960e+05 8.455500000e+04 2.934714650e+04;
viewrot -6.733560560e-01 6.130643360e-01 4.132114880e-01 0.000000000e+00
	5.539599410e-01 4.823888300e-02 8.311441420e-01 0.000000000e+00
	4.896120540e-01 7.885590550e-01 -3.720948210e-01 0.000000000e+00
	0.000000000e+00 0.000000000e+00 0.000000000e+00 1.000000000e+00 ;
EOF
ret=`expr $ret + $?`

bench bldg391 all.g $ARGS << EOF
viewsize 1.800000000e+03;
eye_pt 6.345012207e+02 8.633251343e+02 8.310771484e+02;
viewrot -5.735764503e-01 8.191520572e-01 0.000000000e+00 0.000000000e+00
	-3.461886346e-01 -2.424038798e-01 9.063078165e-01 0.000000000e+00
	7.424039245e-01 5.198368430e-01 4.226182699e-01 0.000000000e+00
	0.000000000e+00 0.000000000e+00 0.000000000e+00 1.000000000e+00;
EOF
ret=`expr $ret + $?`

bench m35 all.g $ARGS <<EOF
viewsize 6.787387985e+03;
eye_pt 3.974533127e+03 1.503320754e+03 2.874633221e+03;
viewrot -5.527838919e-01 8.332423558e-01 1.171090926e-02 0.000000000e+00
	-4.815587087e-01 -3.308784486e-01 8.115544728e-01 0.000000000e+00
	6.800964482e-01 4.429747496e-01 5.841593895e-01 0.000000000e+00
	0.000000000e+00 0.000000000e+00 0.000000000e+00 1.000000000e+00 ;
EOF
ret=`expr $ret + $?`

bench sphflake scene.r $ARGS <<EOF
viewsize 2.556283261452611e+04;
orientation 4.406810841785839e-01 4.005093234738861e-01 5.226451688385938e-01 6.101102288499644e-01;
eye_pt 2.418500583758302e+04 -3.328563644344796e+03 8.489926952850350e+03;
EOF
ret=`expr $ret + $?`

$ECHO
$ECHO "... Done."
$ECHO
$ECHO "Total testing time elapsed: `$ELP $start`"

# see if we fail
if test ! "x$ret" = "x0" ; then
    $ECHO
    $ECHO "THE BENCHARK ANALYSIS DID NOT COMPLETE SUCCESSFULLY." 
    $ECHO
    $ECHO "A benchmark failure means this is not a viable install of BRL-CAD.  This may be"
    $ECHO "a new bug or (more likely) is a compilation configuration error.  Ensure your"
    $ECHO "compiler has strict aliasing disabled, compilation is unoptimized, and you have"
    $ECHO "installed BRL-CAD (some platforms require this).  If you still get a failure,"
    $ECHO "please report your configuration information to benchmark@brlcad.org"
    $ECHO
    $ECHO "Output was saved to $LOGFILE from `pwd`"
    $ECHO "Run '$0 clean' to remove generated pix files."
    $ECHO "Benchmark testing failed."
    exit 2
fi


##############################
# compute and output results #
##############################


performance="`perf 'moss world star bldg391 m35 sphflake' $ARGS`"
if test $? = 0 ; then
    cat >> summary <<EOF
$performance
EOF
fi

$ECHO
$ECHO "The following files have been generated and/or modified:"
$ECHO "  *.log ..... final log files for each individual raytrace test"
$ECHO "  *.pix ..... final pix image files for each individual raytrace test"
$ECHO "  *.log.* ... log files for previous frames and raytrace tests"
$ECHO "  *.pix.* ... pix image files for previous frames and raytrace tests"
$ECHO "  summary ... performance results summary, 2 lines per run"
$ECHO
$ECHO "Run '$0 clean' to remove generated pix files."
$ECHO

$ECHO "Summary:"
$ECHO "$performance"

vgr="`echo \"$performance\" | grep vgr | awk '{print int($9+0.5)}'`"

if test ! "x$vgr" = "x" ; then
    $ECHO
    $ECHO "#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#"
    $ECHO "Benchmark results indicate an approximate VGR performance metric of $vgr"
    ln=0
    lg=0
    if test $vgr -gt 0 ; then
	ln=`echo $vgr | awk '{printf "%.2f", log($1)}'`
	lg=`echo $vgr | awk '{printf "%.2f", log($1) / log(10)}'`
    fi
    $ECHO "Logarithmic VGR metric is $lg  (natural logarithm is $ln)"
    $ECHO "#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#"
    $ECHO
    $ECHO "These numbers seem to indicate that this machine is approximately $vgr times"
    $ECHO "faster than the reference machine being used for comparison, a VAX 11/780"
    $ECHO "running 4.3 BSD named VGR.  These results are in fact approximately $lg"
    $ECHO "orders of magnitude faster than the reference."
    $ECHO

    $ECHO "Here are some other approximated VGR results for perspective:"
    $ECHO "    120 on a 200MHz R5000 running IRIX 6.5"
    $ECHO "    250 on a 500 MHz Pentium III running RedHat 7.1"
    $ECHO "    550 on a dual 450 MHz UltraSPARC II running SunOS 5.8"
    $ECHO "   1000 on a dual 500 MHz G4 PowerPC running Mac OS X 10.2"
    $ECHO "   1500 on a dual 1.66 GHz Athlon MP 2000+ running RedHat 7.3"
    $ECHO "  52000 on a 4x4 CPU 2.93 GHz Xeon running RHEL Server 5.4"
    $ECHO "  65000 on a 512 CPU 400 MHz R12000 Running IRIX 6.5"
    $ECHO
fi


encourage_submission=yes
options=""
if test -f "${PATH_TO_THIS}/Makefile" ; then
    # See if this looks like an optimized build from a source distribution
    optimized=`grep O3 "${PATH_TO_THIS}/Makefile" | wc | awk '{print $1}'`
    if test $optimized -eq 0 ; then
	$ECHO "WARNING: This may not be an optimized compilation of BRL-CAD."
	$ECHO "Performance results may not be optimal."
	$ECHO
	options="$options --enable-optimized"
	encourage_submission=no
    fi
fi

if test -f moss.log ; then
    # See if this looks like a run-time disabled compilation
    runtime=`grep "debugging is disabled" moss.log | wc | awk '{print $1}'`
    if test $runtime -gt 0 ; then
	$ECHO "WARNING: This appears to be a compilation of BRL-CAD that has run-time"
	$ECHO "debugging disabled.  While this will generally give the best"
	$ECHO "performance results and is useful for long render tasks, it is"
	$ECHO "generally not utilized when comparing benchmark performance metrics."
	$ECHO
	options="$options --enable-runtime-debug"
	encourage_submission=no
    fi

    # See if this looks like a compile-time debug compilation
    runtime=`grep "debugging is enabled" moss.log | wc | awk '{print $1}'`
    if test $runtime -gt 0 ; then
	$ECHO "This appears to be a debug compilation of BRL-CAD."
	$ECHO
	options="$options --disable-debug"
    fi
fi

if test "x$encourage_submission" = "xno" ; then
    $ECHO "Official benchmark results are optimized builds with all run-time"
    $ECHO "features enabled and optionally without compile-time debug symbols."
    $ECHO
    if test -f "${PATH_TO_THIS}/Makefile" ; then
	$ECHO "For proper results, run 'make clean' and recompile with the"
	$ECHO "following configure options added:"
    else
	$ECHO "For proper results, you will need to install a version of the"
	$ECHO "benchmark that has been compiled with the following configure"
	$ECHO "options added:"
    fi
    $ECHO " $options"
    $ECHO
else
    # if this was a valid benchmark run, encourage submission of results.
    $ECHO "You are encouraged to submit your benchmark results and system"
    $ECHO "configuration information to benchmark@brlcad.org"
    $ECHO "                             ~~~~~~~~~~~~~~~~~~~~"

    # include information about the operating system and hardware in the log
    $ECHO "Including additional kernel and hardware information in the log."
    $ECHO
    blankit=no

    # BSD
    look_for executable "a sysctl command" SYSCTL_CMD `echo "$PATH" | tr ":" "\n" | sed 's/$/\/sysctl/g'`
    if test ! "x$SYSCTL_CMD" = "x" ; then
	$ECHO "Collecting system state information (via $SYSCTL_CMD)"
	preQUIET="$QUIET"
	QUIET=1
	$ECHO "==============================================================================="
	$ECHO "`$SYSCTL_CMD hw 2>&1`"
	$ECHO "`$SYSCTL_CMD kern 2>&1`"
	$ECHO "`$SYSCTL_CMD kernel 2>&1`"
	$ECHO
	QUIET="$preQUIET"
	blankit=yes
    fi

    # Solaris
    look_for executable "a prtdiag command" PRTDIAG_CMD `echo "$PATH" | tr ":" "\n" | sed 's/$/\/prtdiag/g'`
    if test ! "x$PRTDIAG_CMD" = "x" ; then
	$ECHO "Collecting system diagnostics information (via $PRTDIAG_CMD)"
	preQUIET="$QUIET"
	QUIET=1
	$ECHO "==============================================================================="
	$ECHO "`$PRTDIAG_CMD 2>&1`"
	$ECHO
	QUIET="$preQUIET"
	blankit=yes
    fi

    # AIX
    look_for executable "a prtconf command" PRTCONF_CMD `echo "$PATH" | tr ":" "\n" | sed 's/$/\/prtconf/g'`
    if test ! "x$PRTCONF_CMD" = "x" ; then
	$ECHO "Collecting system configuration information (via $PRTCONF_CMD)"
	preQUIET="$QUIET"
	QUIET=1
	$ECHO "==============================================================================="
	$ECHO "`$PRTCONF_CMD 2>&1`"
	$ECHO
	QUIET="$preQUIET"
	blankit=yes
    fi

    # SGI
    look_for executable "an hinv command" HINV_CMD `echo "$PATH" | tr ":" "\n" | sed 's/$/\/hinv/g'`
    if test ! "x$HINV_CMD" = "x" ; then
	$ECHO "Collecting system configuration information (via $HINV_CMD)"
	preQUIET="$QUIET"
	QUIET=1
	$ECHO "==============================================================================="
	$ECHO "`$HINV_CMD 2>&1`"
	$ECHO
	QUIET="$preQUIET"
	blankit=yes
    fi

    # Linux
    look_for file "a /proc/cpuinfo file" CPUINFO_FILE /proc/cpuinfo
    if test ! "x$CPUINFO_FILE" = "x" ; then
	$ECHO "Collecting system CPU information (via $CPUINFO_FILE)"
	preQUIET="$QUIET"
	QUIET=1
	$ECHO "==============================================================================="
	$ECHO "`cat $CPUINFO_FILE 2>&1`"
	$ECHO
	QUIET="$preQUIET"
	blankit=yes
    fi

    if test "x$blankit" = "xyes" ; then
	$ECHO
    fi
fi

# tell about the benchmark document
look_for file "" BENCHMARK_TR \
    ${PATH_TO_THIS}/../share/brlcad/*.*.*/doc/benchmark.tr \
    ${PATH_TO_THIS}/share/brlcad/*.*.*/doc/benchmark.tr \
    ${PATH_TO_THIS}/share/brlcad/doc/benchmark.tr \
    ${PATH_TO_THIS}/share/doc/benchmark.tr \
    ${PATH_TO_THIS}/doc/benchmark.tr \
    ${PATH_TO_THIS}/../doc/benchmark.tr \
    ./benchmark.tr

$ECHO "Read the benchmark.tr document for more details on the BRL-CAD Benchmark."
if test "x$BENCHMARK_TR" = "x" ; then
    $ECHO "The document should be available in the 'doc' directory of any source"
    $ECHO "or complete binary distribution of BRL-CAD."
else
    $ECHO "The document is available at $BENCHMARK_TR"
fi
$ECHO

$ECHO "Output was saved to $LOGFILE from `pwd`"
$ECHO "Benchmark testing complete."


# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
