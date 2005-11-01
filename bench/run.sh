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
#  Christopher Sean Morrison
#
#  @(#)$Header$ (BRL)


# Ensure /bin/sh
export PATH || (echo "This isn't sh."; sh $0 $*; kill $$)
path_to_this=`dirname $0`


echo "B R L - C A D   B E N C H M A R K"
echo "================================="

# force locale setting to C so things like date output as expected
LC_ALL=C

# save the precious args
ARGS="$*"

echo Looking for RT...
# find the raytracer
# RT environment variable overrides
if test "x${RT}" = "x" ; then
    # see if we find the rt binary
    if test -x "$path_to_this/../src/rt/rt" ; then
	echo ...found $path_to_this/../src/rt/rt
	RT="$path_to_this/../src/rt/rt"
    elif test -x "$path_to_this/rt" ; then
	echo ...found $path_to_this/rt
	RT="$path_to_this/rt"
    elif test -x "$path_to_this/../bin/rt" ; then
	echo ...found $path_to_this/../bin/rt
	RT="$path_to_this/../bin/rt"
    fi
else
    echo ...using $RT from RT environment variable setting
fi

echo Looking for benchmark geometry ...
# find geometry database directory if we do not already know where it
# is. DB environment variable overrides
if test "x${DB}" = "x" ; then
    if test -f "$path_to_this/../db/sphflake.g" ; then
	echo ...found .g geometry files in $path_to_this/../db
	DB="$path_to_this/../db"
    elif test -f "$path_to_this/sphflake.g" ; then
	echo ...found .g geometry files in $path_to_this
	DB="$path_to_this"
    elif test -f "$path_to_this/../share/brlcad/db/sphflake.g" ; then
	echo ...found .g geometry files in $path_to_this/../share/brlcad/db
	DB="$path_to_this/../share/brlcad/db"
    elif test -f "sphflake.g" ; then
	echo ...found .g geometry files in .
	DB="."
    elif test -f "$path_to_this/../db/sphflake.asc" ; then
	echo ...found ascii geometry files in $path_to_this/../db

	echo Looking for asc2g converter ...
	if test "x${ASC2G}" = "x" ; then
	    ASC2G="asc2g"
	    if test -x "$path_to_this/../src/conv/asc2g" ; then
		echo ...found $path_to_this/../src/conv/asc2g
		ASC2G="$path_to_this/../src/conv/asc2g"
	    elif test -f "$path_to_this/../src/conv/asc2g.c" ; then
		echo ...need to compile asc2g

		for compiler in $CC gcc cc ; do
		    COMPILE="$compiler"

		    if test "x$COMPILE" = "x" ; then
			continue
		    fi

		    $COMPILE -o asc2g "$path_to_this/../src/conv/asc2g" -I"$path_to_this/../include" -DHAVE_CONFIG_H -L/usr/brlcad/lib -L"$path_to_this/../src/libwdb/.libs" -L"$path_to_this/../src/librt/.libs" -L"$path_to_this/../src/libbu/.libs" -L"$path_to_this/../src/libbn/.libs" -L"$path_to_this/../src/other/libtcl/.libs" -lwdb -lrt -lbu -lbn -ltcl
		    if test "x$?" = "x0" ; then
			break
		    fi
		    if test -f "asc2g" ; then
			break
		    fi
		done

		if test -f "asc2g" ; then
		    echo ...compiled asc2g with $COMPILE -o asc2g src/conv/asc2g.c
		    ASC2G="./asc2g"
		fi
	    fi
	else
	    echo ...using $ASC2G from ASC2G environment variable setting
	fi

	failed=no
	for geometry in moss world star bldg391 m35 sphflake ; do
	    echo ... creating ${geometry}.g
	    $ASC2G "$path_to_this/../db/${geometry}.asc" ${geometry}.g
	    if test "x$?" != "x0" ; then
		if test ! -f ${geometry}.g ; then
		    failed=yes
		    break;
		fi
	    fi
	done
	if test "x$failed" = "xno" ; then
	    DB="."
	    echo ...using $DB for geometry database directory
	fi
    fi
else
    echo ...using $DB from DB environment variable setting
fi

echo Looking for benchmark images ...
# find pix reference image directory if we do not already know where
# it is.  PIX environment variable overrides
if test "x${PIX}" = "x" ; then
    if test -f "$path_to_this/../pix/sphflake.pix" ; then
	echo ...found .pix image files in $path_to_this/../pix
	PIX="$path_to_this/../pix"
    elif test -f "$path_to_this/sphflake.pix" ; then
	echo ...found .pix image files in $path_to_this
	PIX="$path_to_this"
    elif test -f "$path_to_this/../share/brlcad/pix/sphflake.pix" ; then
	echo ...found .pix image files in $path_to_this/../share/brlcad/pix
	PIX="$path_to_this/../share/brlcad/pix"
    fi
else
    echo ...using $PIX from PIX environment variable setting
fi

echo Checking for pixel comparison utility...
# find pixel comparison utility
# CMP environment variable overrides
if test "x${CMP}" = "x" ; then
    if test -x $path_to_this/pixcmp ; then
	echo ...found $path_to_this/pixcmp
	CMP="$path_to_this/pixcmp"
    else
	if test -f "$path_to_this/pixcmp.c" ; then
	    echo ...need to build pixcmp

	    for compiler in $CC gcc cc ; do
		COMPILE="$compiler"

		if test "x$COMPILE" = "x" ; then
		    continue
		fi

		$COMPILE -o pixcmp "$path_to_this/pixcmp.c"
		if test "x$?" = "x0" ; then
		    break
		fi
		if test -f "pixcmp" ; then
		    break;
		fi
	    done

	    if test -f "pixcmp" ; then
		echo ...built pixcmp with $COMPILE -o pixcmp pixcmp.c
		CMP="./pixcmp"
	    fi
	fi
    fi
else
    echo ...using $CMP from CMP environment variable setting
fi

echo Checking for time elapsed utility...
# find time elapsed script
# ELP environment variable overrides
if test "x${ELP}" = "x" ; then
    if test -x $path_to_this/../sh/elapsed.sh ; then
	echo ...found $path_to_this/../sh/elapsed.sh
	ELP="$path_to_this/../sh/elapsed.sh"
    elif test -x $path_to_this/elapsed.sh ; then
	echo ...found $path_to_this/elapsed.sh
	ELP="$path_to_this/elapsed.sh"
    elif test -x $path_to_this/../bin/elapsed.sh ; then
	echo ...found $path_to_this/../bin/elapsed.sh
	ELP="$path_to_this/../bin/elapsed.sh"
    fi
else
    echo ...using $ELP from ELP environment variable setting
fi

# print results or choke
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

# determine the minimum time requirement in seconds for a single test run
if test "x${TIMEFRAME}" = "x" ; then
    TIMEFRAME=32
fi
echo "Using [$TIMEFRAME] for TIMEFRAME"

# approximate maximum time in seconds that a given test is allowed to take
if test "x${MAXTIME}" = "x" ; then
    MAXTIME=300
fi
if test $MAXTIME -lt $TIMEFRAME ; then
    echo "ERROR: MAXTIME must be greater or equal to TIMEFRAME"
    exit 1
fi
echo "Using [$MAXTIME] for MAXTIME"

# maximum deviation percentage
if test "x${DEVIATION}" = "x" ; then
    DEVIATION=3
fi
echo "Using [$DEVIATION] for DEVIATION"

# maximum number of iterations to average
if test "x${AVERAGE}" = "x" ; then
    AVERAGE=3
fi
echo "Using [$AVERAGE] for AVERAGE"
echo

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

# let the user know about how long this might take
mintime="`expr $TIMEFRAME \* 6`"
echo "Minimum run time is `$ELP $mintime`"
maxtime="`expr $MAXTIME \* 6`"
echo "Maximum run time is `$ELP $maxtime`"
estimate="`expr $mintime \* 3`"
if test $estimate -gt $maxtime ; then
    estimate="$maxtime"
fi
echo "Estimated   time is `$ELP $estimate`"

# allow a debug hook, but don't announce it
if test "x${DEBUG}" = "x" ; then
#    DEBUG=1
    :
fi
echo


#
# run file_prefix geometry hypersample [..rt args..]
#   runs a single benchmark test assuming the following are preset:
#
#   RT := path/name of the raytracer to use
#   DB :+ path to the geometry file
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
	echo "ERROR: unexpected zero count in variance" 1>&2
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
# benchmark test_name geometry [..rt args..]
#   runs a series of benchmark tests assuming the following are preset:
#
#   TIMEFRAME := maximum amount of wallclock time to spend per test
#
# is is assumed that stdin will be the view/frame input
#
benchmark ( ) {
    benchmark_testname="$1" ; shift
    benchmark_geometry="$1" ; shift
    benchmark_args="$*"

    if test "x$benchmark_testname" = "x" ; then
	echo "ERROR: argument mismatch, benchmark is missing the test name"
	return 1
    fi
    if test "x$benchmark_geometry" = "x" ; then
	echo "ERROR: argument mismatch, benchmark is missing the test geometry"
	return 1
    fi
    if test "x$DEBUG" != "x" ; then
	echo "DEBUG: Beginning benchmark testing on $benchmark_testname using $benchmark_geometry"
    fi

    benchmark_view="`cat`"

    echo +++++ ${benchmark_testname}
    benchmark_hypersample=0
    benchmark_frame=0
    benchmark_rtfms=""
    benchmark_percent=100
    benchmark_start_time="`date '+%H %M %S'`"
    benchmark_overall_elapsed=0

    while test $benchmark_overall_elapsed -lt $MAXTIME ; do

	benchmark_elapsed=0
	while test $benchmark_elapsed -lt $TIMEFRAME ; do

	    if test -f ${benchmark_testname}.pix; then mv -f ${benchmark_testname}.pix ${benchmark_testname}.pix.$$; fi
	    if test -f ${benchmark_testname}.log; then mv -f ${benchmark_testname}.log ${benchmark_testname}.log.$$; fi

	    benchmark_frame_start_time="`date '+%H %M %S'`"

	    run $benchmark_testname $benchmark_geometry $benchmark_hypersample $benchmark_args 2> ${benchmark_testname}.log << EOF
$benchmark_view
start $benchmark_frame;
end;
EOF
	    retval=$?

	    if test -f ${benchmark_testname}.pix.$benchmark_frame ; then mv -f ${benchmark_testname}.pix.$benchmark_frame ${benchmark_testname}.pix ; fi

	    # compute how long we took, rounding up to at least one
	    # second to prevent division by zero.
	    benchmark_elapsed="`$ELP --seconds $benchmark_frame_start_time`"
	    if test "x$benchmark_elapsed" = "x" ; then
		benchmark_elapsed=1
	    fi
	    if test $benchmark_elapsed -eq 0 ; then
		benchmark_elapsed=1
	    fi
	    if test "x$benchmark_hypersample" = "x0" ; then

	        # just finished the first frame
		if test "x$DEBUG" != "x" ; then
		    echo "DEBUG: ${benchmark_elapsed}s real elapsed,	1 ray/pixel,	`expr 262144 / $benchmark_elapsed` pixels/s (inexact wallclock)"
		fi
		benchmark_hypersample=1
		benchmark_frame="`expr $benchmark_frame + 1`"
	    else
		if test "x$DEBUG" != "x" ; then
		    echo "DEBUG: ${benchmark_elapsed}s real elapsed,	`expr $benchmark_hypersample + 1` rays/pixel,	`expr \( 262144 \* \( $benchmark_hypersample + 1 \) / $benchmark_elapsed \)` pixels/s (inexact wallclock)"
		fi


	        # increase the number of rays exponentially if we are
	        # considerably faster than the TIMEFRAME required.
		if test `expr $benchmark_elapsed \* 32` -le ${TIMEFRAME} ; then
		    # 32x increase, skip four frames
		    benchmark_hypersample="`expr $benchmark_hypersample \* 32 + 31`"
		    benchmark_frame="`expr $benchmark_frame + 5`"
		elif test `expr $benchmark_elapsed \* 16` -le ${TIMEFRAME} ; then
		    # 16x increase, skip three frames
		    benchmark_hypersample="`expr $benchmark_hypersample \* 16 + 15`"
		    benchmark_frame="`expr $benchmark_frame + 4`"
		elif test `expr $benchmark_elapsed \* 8` -le ${TIMEFRAME} ; then
		    # 8x increase, skip two frames
		    benchmark_hypersample="`expr $benchmark_hypersample \* 8 + 7`"
		    benchmark_frame="`expr $benchmark_frame + 3`"
		elif test `expr $benchmark_elapsed \* 4` -le ${TIMEFRAME} ; then
		    # 4x increase, skip a frame
		    benchmark_hypersample="`expr $benchmark_hypersample \* 4 + 3`"
		    benchmark_frame="`expr $benchmark_frame + 2`"
		else
		    # 2x increase
		    benchmark_hypersample="`expr $benchmark_hypersample + $benchmark_hypersample + 1`"
		    benchmark_frame="`expr $benchmark_frame + 1`"
		fi
	    fi

	    # save the rtfm for variance computations then print it
	    benchmark_rtfm_line="`grep RTFM ${benchmark_testname}.log`"
	    benchmark_rtfm="`echo $benchmark_rtfm_line | awk '{print int($9+0.5)}'`"
	    if test "x$benchmark_rtfm" = "x" ; then
		benchmark_rtfm="0"
	    fi
	    benchmark_rtfms="$benchmark_rtfm $benchmark_rtfms"
	    if test ! "x$benchmark_rtfm_line" = "x" ; then
		echo "$benchmark_rtfm_line"
	    fi

	    # did we fail?
	    if test $retval != 0 ; then
		echo "RAYTRACE ERROR"
		break
	    fi

	    # see if we need to break out early
	    benchmark_overall_elapsed="`$ELP --seconds $benchmark_start_time`"
	    if test $benchmark_overall_elapsed -ge $MAXTIME ; then
		break;
	    fi
	done

	if test "x$benchmark_rtfm" = "x" ; then
	    benchmark_rtfm="0"
	fi
	if test "x$benchmark_rtfms" = "x" ; then
	    benchmark_rtfms="0"
	fi

	# outer loop for variance/deviation testing of last AVERAGE frames
	benchmark_variance="`variance $AVERAGE $benchmark_rtfms`"
	benchmark_deviation="`sqrt $benchmark_variance`"
	if test $benchmark_rtfm -eq 0 ; then
	    benchmark_percent=0
	else
	    benchmark_percent=`echo $benchmark_deviation $benchmark_rtfm | awk '{print int(($1 / $2 * 100)+0.5)}'`
	fi

	if test "x$DEBUG" != "x" ; then
	    benchmark_vals="`getvals $AVERAGE $benchmark_rtfms`"
	    benchmark_avg="`average $benchmark_vals`"
	    if test $benchmark_avg -eq 0 ; then
		benchmark_avgpercent=0
	    else
		benchmark_avgpercent=`echo $benchmark_deviation $benchmark_avg | awk '{print $1 / $2 * 100}"`
	    fi
	    echo "DEBUG: average=$benchmark_avg ; variance=$benchmark_variance ; deviation=$benchmark_deviation ($benchmark_avgpercent%) ; last run was ${benchmark_percent}%"
	fi

	# early exit if we have a stable number
	if test $benchmark_percent -le $DEVIATION ; then
	    break
	fi

	benchmark_overall_elapsed="`$ELP --seconds $benchmark_start_time`"

	# undo the hypersample increase back one step
	benchmark_hypersample="`expr \( \( $benchmark_hypersample + 1 \) / 2 \) - 1`"
    done

    # hopefully the last run is a stable representative of the performance

    if test -f gmon.out; then mv -f gmon.out gmon.${benchmark_testname}.out; fi
    ${CMP} ${PIX}/${benchmark_testname}.pix ${benchmark_testname}.pix
    if test $? = 0 ; then
	echo ${benchmark_testname}.pix:  answers are RIGHT
    else
	echo ${benchmark_testname}.pix:  WRONG WRONG WRONG WRONG WRONG WRONG
    fi

    if test "x$DEBUG" != "x" ; then
	echo "DEBUG: Done benchmark testing on $benchmark_testname"
    fi
    return $retval
}


# Run the tests

start="`date '+%H %M %S'`"
echo "Running the BRL-CAD Benchmark tests... please wait ..."
echo

benchmark moss all.g $ARGS << EOF
viewsize 1.572026215e+02;
eye_pt 6.379990387e+01 3.271768951e+01 3.366661453e+01;
viewrot -5.735764503e-01 8.191520572e-01 0.000000000e+00 0.000000000e+00
	-3.461886346e-01 -2.424038798e-01 9.063078165e-01 0.000000000e+00
	7.424039245e-01 5.198368430e-01 4.226182699e-01 0.000000000e+00
	0.000000000e+00 0.000000000e+00 0.000000000e+00 1.000000000e+00 ;
EOF

benchmark world all.g $ARGS << EOF
viewsize 1.572026215e+02;
eye_pt 6.379990387e+01 3.271768951e+01 3.366661453e+01;
viewrot -5.735764503e-01 8.191520572e-01 0.000000000e+00 0.000000000e+00
	-3.461886346e-01 -2.424038798e-01 9.063078165e-01 0.000000000e+00
	7.424039245e-01 5.198368430e-01 4.226182699e-01 0.000000000e+00
	0.000000000e+00 0.000000000e+00 0.000000000e+00 1.000000000e+00 ;
EOF

benchmark star all $ARGS << EOF
viewsize 2.500000000e+05;
eye_pt 2.102677960e+05 8.455500000e+04 2.934714650e+04;
viewrot -6.733560560e-01 6.130643360e-01 4.132114880e-01 0.000000000e+00
	5.539599410e-01 4.823888300e-02 8.311441420e-01 0.000000000e+00
	4.896120540e-01 7.885590550e-01 -3.720948210e-01 0.000000000e+00
	0.000000000e+00 0.000000000e+00 0.000000000e+00 1.000000000e+00 ;
EOF

benchmark bldg391 all.g $ARGS << EOF
viewsize 1.800000000e+03;
eye_pt 6.345012207e+02 8.633251343e+02 8.310771484e+02;
viewrot -5.735764503e-01 8.191520572e-01 0.000000000e+00 0.000000000e+00
	-3.461886346e-01 -2.424038798e-01 9.063078165e-01 0.000000000e+00
	7.424039245e-01 5.198368430e-01 4.226182699e-01 0.000000000e+00
	0.000000000e+00 0.000000000e+00 0.000000000e+00 1.000000000e+00;
EOF

benchmark m35 all.g $ARGS <<EOF
viewsize 6.787387985e+03;
eye_pt 3.974533127e+03 1.503320754e+03 2.874633221e+03;
viewrot -5.527838919e-01 8.332423558e-01 1.171090926e-02 0.000000000e+00
	-4.815587087e-01 -3.308784486e-01 8.115544728e-01 0.000000000e+00
	6.800964482e-01 4.429747496e-01 5.841593895e-01 0.000000000e+00
	0.000000000e+00 0.000000000e+00 0.000000000e+00 1.000000000e+00 ;
EOF

benchmark sphflake scene.r $ARGS <<EOF
viewsize 2.556283261452611e+04;
orientation 4.406810841785839e-01 4.005093234738861e-01 5.226451688385938e-01 6.101102288499644e-01;
eye_pt 2.418500583758302e+04 -3.328563644344796e+03 8.489926952850350e+03;
EOF

echo
echo "... Done."
echo
echo "Total testing time elapsed: `$ELP $start`"


# Compute and output the results

HOST="`hostname`"
if test $? != 0 ; then
    HOST="`uname -n`"
    if test $? != 0 ; then
	HOST="unknown"
    fi
fi

TAIL="tail -"
if test "x`echo 'tail' | tail -n 1 2>&1`" = "xtail" ; then
    TAIL="tail -n "
fi

if test -f "$path_to_this/perf.sh" ; then
    PERF="$path_to_this/perf.sh"
elif test -f "$path_to_this/../bench/perf.sh" ; then
    PERF="$path_to_this/../bench/perf.sh"
else
    # see if it is in our path
    PERF="perf.sh"
    $PERF > /dev/null 2>&1
    if test "x$?" != "x1" ; then
	PERF="false"
    fi
fi
sh "$PERF" "$HOST" "`date`" "$*" >> summary
perf_ret=$?

echo
echo "The following files have been generated and/or modified:"
echo "  *.log ..... final log files for each individual raytrace test"
echo "  *.pix ..... final pix image files for each individual raytrace test"
echo "  *.log.* ... log files for previous frames and raytrace tests"
echo "  *.pix.* ... pix image files for previous frames and raytrace tests"
echo "  summary ... performance results summary, 2 lines per run"

if test $perf_ret != 0 ; then
    ${TAIL}1 summary
    exit $perf_ret
else
    echo
    ${TAIL}2 summary
fi

vgr="`${TAIL}1 summary | awk '{print int($9+0.5)}'`"
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

# See if this looks like a run-time disabled build
if test -f moss.g ; then
    runtime=`grep "debugging is disabled" moss.g | wc | awk '{print $1}'`
    if test $runtime -gt 0 ; then
	echo "WARNING: This appears to be a compilation of BRL-CAD that has run-time"
	echo "debugging disabled.  While this will generally give the best"
	echo "performance results and is useful for long render tasks, but it is"
	echo "generally not considered when comparing benchmark performance metrics."
	echo "Official benchmark results are optimized builds with all run-time"
	echo "features enabled."
	echo
	echo "For proper results, run 'make clean' and recompile using the"
	echo "following configure options:"
	echo "    --enable-runtime-debug  --enable-optimized"
	echo
    fi
fi

# See if this looks like an optimized build
if test -f "$path_to_this/Makefile" ; then
    optimized=`grep O3 "$path_to_this/Makefile" | wc | awk '{print $1}'`
    if test $optimized -eq 0 ; then
	echo "WARNING: This may not be an optimized compilation of BRL-CAD."
	echo "Performance results may not be optimal."
	echo
	echo "For proper results, run 'make clean' and recompile using the"
	echo "following configure options:"
	echo "    --enable-optimized"
	echo
    fi
fi

echo "Testing complete."
echo "Read the benchmark.tr document for more details on the BRL-CAD Benchmark."

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
