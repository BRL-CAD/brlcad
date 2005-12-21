#!/bin/sh
#                          R U N . S H
# BRL-CAD
#
# Copyright (C) 2004-2005 United States Government as represented by
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
# will test the performance of a given compiler by iteratively
# rendering several well-known datasets into 512x512 images where
# performance metrics are documented and fairly well understood.  The
# local machine's performance is compared to the base system (called
# VGR) and a numeric "VGR" mulitplier of performance is computed.
# This number is a simplified metric from which one may qualitatively
# compare cpu and cache performance, versions of BRL-CAD, and
# different compiler characteristics.
#
# The suite is intended to be run from a source distribution of
# BRL-CAD after the package has been compiled either directly or via a
# make build system target.  There are, however, several environment
# variables that will modify how the BRL-CAD benchmark behaves so that
# it may be run in a stand-alone environment:
#
#   RT - the rt binary (e.g. ../src/rt/rt or /usr/brlcad/bin/rt)
#   DB - the directory containing the reference geometry (e.g. ../db)
#   PIX - the directory containing the reference images (e.g. ../pix)
#   LOG - the directory containing the reference logs (e.g. ../pix)
#   CMP - the name of a pixcmp tool (e.g. ./pixcmp)
#   ELP - the name of an elapsed time tool (e.g. ../sh/elapsed.sh)
#   TIMEFRAME - the minimum number of seconds each trace needs to take
#   MAXTIME - the maximum number of seconds to spend on any test
#   DEVIATION - the minimum sufficient % deviation from the average
#   AVERAGE - how many frames to average together
#   DEBUG - turn on extra debug output for testing/development
#
# The TIMEFRAME, MAXTIME, DEVIATION, and AVERAGE options control how
# the benchmark will proceed including how long it should take.  Each
# individual benchmark run will consume at least a minimum TIMEFRAME
# of wallclock time so that the results can stabilize.  After
# consuming at least the minimum TIMEFRAME, additional frames may be
# computed until the standard deviation from the last AVERAGE count of
# frames is below the specified DEVIATION.  When a test is run and it
# completes in less than TIMEFRAME, the raytrace is restarted using
# double the number of rays from the previous run.  If the machine is
# fast enough, the benchmark may accelerate the number or rays being
# fired.  These additional rays are hypersampled but without any
# jitter, so it's effectively performing a multiplier amount of work
# over the initial frame.
#
# Plese send your BRL-CAD Benchmark results to the developers along
# with detailed system information to <devs@brlcad.org>.  Include at
# least:
#
#   0) Operating system type and version (e.g. uname -a)
#   1) Compiler name and version (e.g. gcc --version)
#   2) CPU configuration (e.g. cat /proc/cpuinfo or hinv or sysctl -a)
#   3) Cache (data and/or instruction) details for L1/L2/L3 and system
#      (e.g. cat /proc/cpuinfo or hinv or sysctl -a)
#   4) Output from this script (e.g. ./run.sh > run.sh.log 2>&1)
#   5) All generated log files (e.g. *.log* after running run.sh)
#   6) Anything else you think might be relevant to performance
#
# Authors -
#  Mike Muuss
#  Susan Muuss
#  Christopher Sean Morrison
#
#  @(#)$Header$ (BRL)


# Ensure /bin/sh
export PATH || (echo "This isn't sh."; sh $0 $*; kill $$)
name_of_this=`basename $0`
path_to_this=`dirname $0`


echo "B R L - C A D   B E N C H M A R K"
echo "================================="

# force locale setting to C so things like date output as expected
LC_ALL=C

# save the precious args
ARGS="$*"

# allow a debug hook, but don't announce it
if test "x${DEBUG}" = "x" ; then
#    DEBUG=1
    :
fi


########################
# search for resources #
########################

# utility function to search for a certain filesystem object in a list of paths
look_for ( ) {
    look_for_type="$1" ; shift
    look_for_label="$1" ; shift
    look_for_var="$1" ; shift
    look_for_dirs="$*"

    if test "x$look_for_label" != "x" ; then
	echo  "Looking for $look_for_label"
    fi
    
    # get the value of the variable
    look_for_var_var="echo \"\$$look_for_var\""
    look_for_var_val="`eval ${look_for_var_var}`"

    if test "x${look_for_var_val}" = "x" ; then
	for look_for_dir in $look_for_dirs ; do

	    if test "x$DEBUG" != "x" ; then
		echo "searching ${look_for_dir}"
		ls -lad ${look_for_dir}
	    fi
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
		    opts="$opts -x"
		    ;;
		xscr*)
		    opts="$opts -x"
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
		    echo "...found $look_for_type ${look_for_dir}"
		fi
		look_for_var_var="${look_for_var}=\"${look_for_dir}\""
		eval $look_for_var_var
		export $look_for_var
		break
	    fi
	done
    else
	if test "x$look_for_label" != "x" ; then
	    echo "...using $look_for_var_val from $look_for_var variable setting"
	fi
    fi
}

look_for executable "the BRL-CAD raytracer" RT \
    ${path_to_this}/rt \
    ${path_to_this}/../bin/rt \
    ${path_to_this}/../src/rt/rt \
    ${path_to_this}/src/rt/rt \
    ./rt

look_for directory "a benchmark geometry directory" DB \
    ${path_to_this}/../share/brlcad/*.*.*/db \
    ${path_to_this}/share/brlcad/*.*.*/db \
    ${path_to_this}/share/brlcad/db \
    ${path_to_this}/share/db \
    ${path_to_this}/../db \
    ${path_to_this}/db \
    ./db

look_for directory "a benchmark reference image directory" PIX \
    ${path_to_this}/../share/brlcad/*.*.*/pix \
    ${path_to_this}/share/brlcad/*.*.*/pix \
    ${path_to_this}/share/brlcad/pix \
    ${path_to_this}/share/pix \
    ${path_to_this}/../pix \
    ${path_to_this}/pix \
    ./pix

look_for directory "a benchmark reference log directory" LOG \
    $PIX \
    ${path_to_this}/../share/brlcad/*.*.*/pix \
    ${path_to_this}/share/brlcad/*.*.*/pix \
    ${path_to_this}/share/brlcad/pix \
    ${path_to_this}/share/pix \
    ${path_to_this}/../pix \
    ${path_to_this}/pix \
    ./pix

look_for executable "a pixel comparison utility" CMP \
    ${path_to_this}/pixcmp \
    ${path_to_this}/../bin/pixcmp \
    ${path_to_this}/../bench/pixcmp \
    ./pixcmp

look_for script "a time elapsed utility" ELP \
    ${path_to_this}/elapsed.sh \
    ${path_to_this}/../bin/elapsed.sh \
    ${path_to_this}/sh/elapsed.sh \
    ${path_to_this}/../sh/elapsed.sh \
    ./elapsed.sh

# end of searching, separate the output
echo


#####################
# output parameters #
#####################

# sanity check, output all the final settings together
if test "x${RT}" = "x" ; then
    echo "ERROR:  Could not find the BRL-CAD raytracer"
    exit 1
else
    echo "Using [$RT] for RT"
fi
if test "x${DB}" = "x" ; then
    echo "ERROR:  Could not find the BRL-CAD database directory"
    exit 1
else
    echo "Using [$DB] for DB"
fi
if test "x${PIX}" = "x" ; then
    echo "ERROR:  Could not find the BRL-CAD reference images"
    exit 1
else
    echo "Using [$PIX] for PIX"
fi
if test "x${LOG}" = "x" ; then
    echo "ERROR:  Could not find the BRL-CAD reference logs"
    exit 1
else
    echo "Using [$LOG] for LOG"
fi
if test "x${CMP}" = "x" ; then
    echo "ERROR:  Could not find the BRL-CAD pixel comparison utility"
    exit 1
else
    echo "Using [$CMP] for CMP"
fi
if test "x${ELP}" = "x" ; then
    echo "ERROR:  Could not find the BRL-CAD time elapsed script"
    exit 1
else
    echo "Using [$ELP] for ELP"
fi

# utility function to set a variable if it's not already set to something
set_if_unset ( ) {
    set_if_unset_name="$1" ; shift
    set_if_unset_val="$1" ; shift

    set_if_unset_var="echo \"\$$set_if_unset_name\""
    set_if_unset_var_val="`eval ${set_if_unset_var}`"
    if test "x${set_if_unset_var_val}" = "x" ; then
	set_if_unset_var="${set_if_unset_name}=\"${set_if_unset_val}\""
	if test "x$DEBUG" != "x" ; then
	    echo $set_if_unset_var
	fi
	eval $set_if_unset_var
	export $set_if_unset_name
    fi

    set_if_unset_var="echo \"\$$set_if_unset_name\""
    set_if_unset_val="`eval ${set_if_unset_var}`"
    echo "Using [${set_if_unset_val}] for $set_if_unset_name"
}

# determine the minimum time requirement in seconds for a single test run
set_if_unset TIMEFRAME 32

# approximate maximum time in seconds that a given test is allowed to take
set_if_unset MAXTIME 300
if test $MAXTIME -lt $TIMEFRAME ; then
    echo "ERROR: MAXTIME must be greater or equal to TIMEFRAME"
    exit 1
fi

# maximum deviation percentage
set_if_unset DEVIATION 3

# maximum number of iterations to average
set_if_unset AVERAGE 3

# end of settings, separate the output
echo


##########################
# output run-time status #
##########################

# determine raytracer version
echo "RT reports the following version information:"
versions="`$RT 2>&1 | grep BRL-CAD`"
if test "x$versions" = "x" ; then
    echo "Unknown"
else
    cat <<EOF
$versions
EOF
fi
echo

# if expr works, let the user know about how long this might take
if test "x`expr 1 - 1 2>/dev/null`" = "x0" ; then
    mintime="`expr $TIMEFRAME \* 6`"
    echo "Minimum run time is `$ELP $mintime`"
    maxtime="`expr $MAXTIME \* 6`"
    echo "Maximum run time is `$ELP $maxtime`"
    estimate="`expr $mintime \* 3`"
    if test $estimate -gt $maxtime ; then
	estimate="$maxtime"
    fi
    echo "Estimated   time is `$ELP $estimate`"
    echo
else
    echo "WARNING: expr is unavailable, unable to compute statistics"
    echo
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

    if test "x$DEBUG" != "x" ; then
	echo "DEBUG: Running $RT -B -M -s512 -H${run_hypersample} -J0 ${run_args} -o ${run_geomname}.pix ${DB}/${run_geomname}.g ${run_geometry}"
    fi

    $RT -B -M -s512 -H${run_hypersample} -J0 ${run_args} \
	-o ${run_geomname}.pix \
	${DB}/${run_geomname}.g ${run_geometry} 1>&2 <<EOF
$run_view
EOF
    retval=$?
    if test "x$DEBUG" != "x" ; then
	echo "DEBUG: Running $RT returned $retval"
    fi
    return $retval
}


#
# average [..numbers..]
#   computes the integer average for a set of given numbers
#
average ( ) {
    average_nums="$*"

    if test "x$average_nums" = "x" ; then
	echo "ERROR: no numbers provided to average" 1>&2
	exit 1
    fi

    total=0
    count=0
    for num in $average_nums ; do
	total="`expr $total + $num`"
	count="`expr $count + 1`"
    done

    if test $count -eq 0 ; then
	echo "ERROR: unexpected count in average" 1>&2
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
	echo "ERROR: cannot compute variance of zero numbers" 1>&2
	exit 1
    fi

    if test "x$variance_nums" = "x" ; then
	echo "ERROR: cannot compute variance of nothing" 1>&2
	exit 1
    fi

    # get up to count values from the nums provided
    variance_got="`getvals $variance_count $variance_nums`"
    variance_counted="$?"

    if test $variance_counted -eq 0 ; then
	echo "ERROR: unexpected zero count of numbers in variance" 1>&2
	exit 1
    elif test $variance_counted -lt 0 ; then
	echo "ERROR: unexpected negative count of numbers in variance" 1>&2
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
	echo "ERROR: cannot compute the square root of nothing" 1>&2
	exit 1
    elif test $sqrt_number -lt 0 > /dev/null 2>&1 ; then
	echo "ERROR: square root of negative numbers is only in your imagination" 1>&2
	exit 1
    fi

    sqrt_have_dc=yes
    echo "1 1 + p" | dc 2>&1 >/dev/null
    if test ! x$? = x0 ; then
	sqrt_have_dc=no
    fi

    sqrt_root=""
    if test "x$sqrt_have_dc" = "xyes" ; then
	sqrt_root=`echo "$sqrt_number v p" | dc`
    else
	sqrt_have_bc=yes
	echo "1 + 1" | bc 2>&1 >/dev/null
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
	echo "ERROR: argument mismatch, bench is missing the test name"
	return 1
    fi
    if test "x$bench_geometry" = "x" ; then
	echo "ERROR: argument mismatch, bench is missing the test geometry"
	return 1
    fi
    if test "x$DEBUG" != "x" ; then
	echo "DEBUG: Beginning bench testing on $bench_testname using $bench_geometry"
    fi

    bench_view="`cat`"

    echo +++++ ${bench_testname}
    bench_hypersample=0
    bench_frame=0
    bench_rtfms=""
    bench_percent=100
    bench_start_time="`date '+%H %M %S'`"
    bench_overall_elapsed=0

    while test $bench_overall_elapsed -lt $MAXTIME ; do

	bench_elapsed=0
	while test $bench_elapsed -lt $TIMEFRAME ; do

	    if test -f ${bench_testname}.pix; then mv -f ${bench_testname}.pix ${bench_testname}.pix.$$; fi
	    if test -f ${bench_testname}.log; then mv -f ${bench_testname}.log ${bench_testname}.log.$$; fi

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
		if test "x$DEBUG" != "x" ; then
		    echo "DEBUG: ${bench_elapsed}s real elapsed,	1 ray/pixel,	`expr 262144 / $bench_elapsed` pixels/s (inexact wallclock)"
		fi
		bench_hypersample=1
		bench_frame="`expr $bench_frame + 1`"
	    else
		if test "x$DEBUG" != "x" ; then
		    echo "DEBUG: ${bench_elapsed}s real elapsed,	`expr $bench_hypersample + 1` rays/pixel,	`expr \( 262144 \* \( $bench_hypersample + 1 \) / $bench_elapsed \)` pixels/s (inexact wallclock)"
		fi


	        # increase the number of rays exponentially if we are
	        # considerably faster than the TIMEFRAME required.
		if test `expr $bench_elapsed \* 32` -le ${TIMEFRAME} ; then
		    # 32x increase, skip four frames
		    bench_hypersample="`expr $bench_hypersample \* 32 + 31`"
		    bench_frame="`expr $bench_frame + 5`"
		elif test `expr $bench_elapsed \* 16` -le ${TIMEFRAME} ; then
		    # 16x increase, skip three frames
		    bench_hypersample="`expr $bench_hypersample \* 16 + 15`"
		    bench_frame="`expr $bench_frame + 4`"
		elif test `expr $bench_elapsed \* 8` -le ${TIMEFRAME} ; then
		    # 8x increase, skip two frames
		    bench_hypersample="`expr $bench_hypersample \* 8 + 7`"
		    bench_frame="`expr $bench_frame + 3`"
		elif test `expr $bench_elapsed \* 4` -le ${TIMEFRAME} ; then
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
		echo "$bench_rtfm_line"
	    fi

	    # did we fail?
	    if test $retval != 0 ; then
		echo "RAYTRACE ERROR"
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

	if test "x$DEBUG" != "x" ; then
	    bench_vals="`getvals $AVERAGE $bench_rtfms`"
	    bench_avg="`average $bench_vals`"
	    if test $bench_avg -eq 0 ; then
		bench_avgpercent=0
	    else
		bench_avgpercent=`echo $bench_deviation $bench_avg | awk '{print $1 / $2 * 100}'`
	    fi
	    echo "DEBUG: average=$bench_avg ; variance=$bench_variance ; deviation=$bench_deviation ($bench_avgpercent%) ; last run was ${bench_percent}%"
	fi

	# early exit if we have a stable number
	if test $bench_percent -le $DEVIATION ; then
	    break
	fi

	bench_overall_elapsed="`$ELP --seconds $bench_start_time`"

	# undo the hypersample increase back one step
	bench_hypersample="`expr \( \( $bench_hypersample + 1 \) / 2 \) - 1`"
    done

    # hopefully the last run is a stable representative of the performance

    if test -f gmon.out; then mv -f gmon.out gmon.${bench_testname}.out; fi
    ${CMP} ${PIX}/${bench_testname}.pix ${bench_testname}.pix
    if test $? = 0 ; then
	echo ${bench_testname}.pix:  answers are RIGHT
    else
	echo ${bench_testname}.pix:  WRONG WRONG WRONG WRONG WRONG WRONG
    fi

    if test "x$DEBUG" != "x" ; then
	echo "DEBUG: Done benchmark testing on $bench_testname"
    fi
    return $retval
}


#
# perf test_name geometry [..rt args..]
#
perf ( ) {
    perf_tests="$1" ; shift
    perf_args="$*"

    if test "x$perf_tests" = "x" ; then
	echo "ERROR: no tests specified for calculating performance" 1>&2
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
		    echo "ERROR: file $perf_log does not exist" 1>&2
		fi
	    fi
	done
	perf_ref_files="$perf_ref_files $perf_ref_log"
	perf_cur_files="$perf_cur_files $perf_cur_log"
    done

    # extract the RTFM values from the log files, use TR to convert
    # newlines to tabs.  the trailing tab is signficant in case there
    # are not enough results.
    perf_VGRREF=`grep RTFM $perf_ref_files | sed -n -e 's/^.*= *//' -e 's/ rays.*//p' | tr '\012' '\011' `
    perf_CURVALS=`grep RTFM $perf_cur_files | sed -n -e 's/^.*= *//' -e 's/ rays.*//p' | tr '\012' '\011' `

    # if there were no reference values, we cannot compute timings
    if test "x$perf_VGRREF" = "x" ; then
	echo "ERROR: Cannot locate VGR reference values" 1>&2
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
	echo "WARNING: only $# RTFM times found, adding a zero result." 1>&2
	perf_CURVALS="${perf_CURVALS}0	"
	set $perf_CURVALS
    done

    # see if we have a calculator
    perf_have_dc=yes
    echo "1 1 + p" | dc 2>&1 >/dev/null
    if test ! x$? = x0 ; then
	perf_have_dc=no
    fi

    for perf_ref in $perf_VGRREF ; do
	perf_cur=$1
	shift
	
	if test "x$perf_have_dc" = "xyes" ; then
	    perf_RATIO=`echo "2k $perf_cur $perf_ref / p" | dc`
	else
	    # presume bc as an alternate (tsk tsk)
	    perf_RATIO=`echo "scale=2; $perf_cur / $perf_ref" | bc`
	fi
        # Note: append new value and a trail TAB to existing list.
	perf_RATIO_LIST="${perf_RATIO_LIST}$perf_RATIO	"
    done

    # The number of plus signs must be one less than the number of elements.
    if test "x$perf_have_dc" = "xyes" ; then
	perf_MEAN_ABS=`echo 2k $perf_CURVALS +++++ 6/ p | dc`
	perf_MEAN_REL=`echo 2k $perf_RATIO_LIST +++++ 6/ p | dc`
    else
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
echo "Running the BRL-CAD Benchmark tests... please wait ..."
echo

bench moss all.g $ARGS << EOF
viewsize 1.572026215e+02;
eye_pt 6.379990387e+01 3.271768951e+01 3.366661453e+01;
viewrot -5.735764503e-01 8.191520572e-01 0.000000000e+00 0.000000000e+00
	-3.461886346e-01 -2.424038798e-01 9.063078165e-01 0.000000000e+00
	7.424039245e-01 5.198368430e-01 4.226182699e-01 0.000000000e+00
	0.000000000e+00 0.000000000e+00 0.000000000e+00 1.000000000e+00 ;
EOF

bench world all.g $ARGS << EOF
viewsize 1.572026215e+02;
eye_pt 6.379990387e+01 3.271768951e+01 3.366661453e+01;
viewrot -5.735764503e-01 8.191520572e-01 0.000000000e+00 0.000000000e+00
	-3.461886346e-01 -2.424038798e-01 9.063078165e-01 0.000000000e+00
	7.424039245e-01 5.198368430e-01 4.226182699e-01 0.000000000e+00
	0.000000000e+00 0.000000000e+00 0.000000000e+00 1.000000000e+00 ;
EOF

bench star all $ARGS << EOF
viewsize 2.500000000e+05;
eye_pt 2.102677960e+05 8.455500000e+04 2.934714650e+04;
viewrot -6.733560560e-01 6.130643360e-01 4.132114880e-01 0.000000000e+00
	5.539599410e-01 4.823888300e-02 8.311441420e-01 0.000000000e+00
	4.896120540e-01 7.885590550e-01 -3.720948210e-01 0.000000000e+00
	0.000000000e+00 0.000000000e+00 0.000000000e+00 1.000000000e+00 ;
EOF

bench bldg391 all.g $ARGS << EOF
viewsize 1.800000000e+03;
eye_pt 6.345012207e+02 8.633251343e+02 8.310771484e+02;
viewrot -5.735764503e-01 8.191520572e-01 0.000000000e+00 0.000000000e+00
	-3.461886346e-01 -2.424038798e-01 9.063078165e-01 0.000000000e+00
	7.424039245e-01 5.198368430e-01 4.226182699e-01 0.000000000e+00
	0.000000000e+00 0.000000000e+00 0.000000000e+00 1.000000000e+00;
EOF

bench m35 all.g $ARGS <<EOF
viewsize 6.787387985e+03;
eye_pt 3.974533127e+03 1.503320754e+03 2.874633221e+03;
viewrot -5.527838919e-01 8.332423558e-01 1.171090926e-02 0.000000000e+00
	-4.815587087e-01 -3.308784486e-01 8.115544728e-01 0.000000000e+00
	6.800964482e-01 4.429747496e-01 5.841593895e-01 0.000000000e+00
	0.000000000e+00 0.000000000e+00 0.000000000e+00 1.000000000e+00 ;
EOF

bench sphflake scene.r $ARGS <<EOF
viewsize 2.556283261452611e+04;
orientation 4.406810841785839e-01 4.005093234738861e-01 5.226451688385938e-01 6.101102288499644e-01;
eye_pt 2.418500583758302e+04 -3.328563644344796e+03 8.489926952850350e+03;
EOF

echo
echo "... Done."
echo
echo "Total testing time elapsed: `$ELP $start`"


##############################
# compute and output results #
##############################


performance="`perf 'moss world star bldg391 m35 sphflake' $ARGS`"
if test $? = 0 ; then
    cat >> summary <<EOF
$performance
EOF
fi

echo
echo "The following files have been generated and/or modified:"
echo "  *.log ..... final log files for each individual raytrace test"
echo "  *.pix ..... final pix image files for each individual raytrace test"
echo "  *.log.* ... log files for previous frames and raytrace tests"
echo "  *.pix.* ... pix image files for previous frames and raytrace tests"
echo "  summary ... performance results summary, 2 lines per run"
echo

echo "Summary:"
cat <<EOF
$performance
EOF

vgr="`cat <<EOF | grep vgr | awk '{print int($9+0.5)}'
$performance
EOF`"
if test ! "x$vgr" = "x" ; then
    echo
    echo "#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#"
    echo "Benchmark results indicate an approximate VGR performance metric of $vgr"
    ln=`echo $vgr | awk '{printf "%.2f", log($1)}'`
    log=`echo $vgr | awk '{printf "%.2f", log($1) / log(10)}'`
    echo "Logarithmic VGR metric is $log  (natural logarithm is $ln)"
    echo "#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#"
    echo
    echo "These numbers seem to indicate that this machine is approximately $vgr times"
    echo "faster than the reference machine being used for comparison, a VAX 11/780"
    echo "running 4.3 BSD named VGR.  These results are in fact approximately $log"
    echo "orders of magnitude faster than the reference."
    echo

    echo "Here are some other approximated VGR results for perspective:"
    echo "   120 on a 200MHz R5000 running IRIX 6.5"
    echo "   250 on a 500 MHz Pentium III running RedHat 7.1"
    echo "   550 on a dual 450 MHz UltraSPARC II running SunOS 5.8"
    echo "  1000 on a dual 500 MHz G4 PowerPC running Mac OS X 10.2"
    echo "  1500 on a dual 1.66 GHz Athlon MP 2000+ running RedHat 7.3"
    echo "  9000 on an 8 CPU 1.3 GHz Power4 running AIX 5.1"
    echo " 65000 on a 512 CPU 400 MHz R12000 Running IRIX 6.5"
    echo
fi


encourage_submission=yes
options=""
if test -f "${path_to_this}/Makefile" ; then
    # See if this looks like an optimized build from a source distribution
    optimized=`grep O3 "${path_to_this}/Makefile" | wc | awk '{print $1}'`
    if test $optimized -eq 0 ; then
	echo "WARNING: This may not be an optimized compilation of BRL-CAD."
	echo "Performance results may not be optimal."
	echo
	options="$options --enable-optimized"
	encourage_submission=no
    fi
fi

if test -f moss.log ; then
    # See if this looks like a run-time disabled compilation
    runtime=`grep "debugging is disabled" moss.log | wc | awk '{print $1}'`
    if test $runtime -gt 0 ; then
	echo "WARNING: This appears to be a compilation of BRL-CAD that has run-time"
	echo "debugging disabled.  While this will generally give the best"
	echo "performance results and is useful for long render tasks, it is"
	echo "generally not utilized when comparing benchmark performance metrics."
	echo
	options="$options --enable-runtime-debug"
	encourage_submission=no
    fi

    # See if this looks like a compile-time debug compilation
    runtime=`grep "debugging is enabled" moss.log | wc | awk '{print $1}'`
    if test $runtime -gt 0 ; then
	echo "This appears to be a debug compilation of BRL-CAD."
	echo
	options="$options --disable-debug"
    fi
fi

if test "x$encourage_submission" = "xno" ; then
    echo "Official benchmark results are optimized builds with all run-time"
    echo "features enabled and optionally without compile-time debug symbols."
    echo
    if test -f "${path_to_this}/Makefile" ; then
	echo "For proper results, run 'make clean' and recompile with the"
	echo "following configure options added:"
    else
	echo "For proper results, you will need to install a version of the"
	echo "benchmark that has been compiled with the following configure"
	echo "options added:"
    fi
    echo " $options"
    echo
fi

# tell about the benchmark document
look_for file "" BENCHMARK_TR \
    ${path_to_this}/../share/brlcad/*.*.*/doc/benchmark.tr \
    ${path_to_this}/share/brlcad/*.*.*/doc/benchmark.tr \
    ${path_to_this}/share/brlcad/doc/benchmark.tr \
    ${path_to_this}/share/doc/benchmark.tr \
    ${path_to_this}/doc/benchmark.tr \
    ${path_to_this}/../doc/benchmark.tr \
    ./benchmark.tr

echo "Read the benchmark.tr document for more details on the BRL-CAD Benchmark."
if test "x$BENCHMARK_TR" = "x" ; then
    echo "The document should be available in the 'doc' directory of any source"
    echo "or complete binary distribution of BRL-CAD."
else
    echo "The document is available at $BENCHMARK_TR"
fi
echo

# if this was a valid benchmark run, encourage submission of results.
if test "x$encourage_submission" = "xyes" ; then
    echo "You are encouraged to submit your benchmark results and system"
    echo "configuration information to benchmark@brlcad.org"
    echo
fi

echo "Benchmark testing complete."

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
