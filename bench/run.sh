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
#   DB - the directory to the database geometry (e.g. ../db)
#   CMP - the path to a pixcmp tool (e.g. ./pixcmp)
#   TIMEFRAME - the minimum number of seconds each trace needs to take
#   DEBUG - turn on extra debug output for testing/development
#
# The TIMEFRAME option was added after several years to ensure that
# each individual benchmark run will consume at least a minimum amount
# of wallclock time to be considered useful/stable.  When a test is
# run and it completes in less than TIMEFRAME, the raytrace is
# restarted using double the number of rays from the previous run.
# These additional rays are hypersampled but without any jitter.
#
# Plese send your BRL-CAD Benchmark results to the developers along with
# detailed system information to <devs@brlcad.org>.  Include at least:
#
#   0) Operating system type and version (e.g. uname -a)
#   1) Compiler name and version (e.g. gcc --version)
#   2) CPU configuration(s) (e.g. cat /proc/cpuinfo or hinv or sysctl -a)
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
path_to_run_sh=`dirname $0`


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
    if test -x "$path_to_run_sh/../src/rt" ; then
	echo ...found $path_to_run_sh/../src/rt/rt
	RT="$path_to_run_sh/../src/rt/rt"
    fi
else
    echo ...using $RT from RT environment variable setting
fi

echo Looking for geometry database directory...
# find geometry database directory if we do not already know where it is
# DB environment variable overrides
if test "x${DB}" = "x" ; then
    if test -f "$path_to_run_sh/../db/sphflake.g" ; then
	echo ...found $path_to_run_sh/../db
	DB="$path_to_run_sh/../db"
    fi
else
    echo ...using $DB from DB environment variable setting
fi

echo Checking for pixel comparison utility...
# find pixel comparison utility
# CMP environment variable overrides
if test "x${CMP}" = "x" ; then
    if test -x $path_to_run_sh/pixcmp ; then
	echo ...found $path_to_run_sh/pixcmp
	CMP="$path_to_run_sh/pixcmp"
    else
	if test -f "$path_to_run_sh/pixcmp.c" ; then
	    echo ...need to build pixcmp

	    for compiler in $CC gcc cc ; do
		CC=$compiler

		$CC "$path_to_run_sh/pixcmp.c" >& /dev/null
		if test "x$?" = "x0" ; then
		    break
		fi
		if test -f "$path_to_run_sh/pixcmp" ; then
		    break;
		fi
	    done
	    
	    if test -f "$path_to_run_sh/pixcmp" ; then
		echo ...built pixcmp with $CC
		CMP="$path_to_run_sh/pixcmp"
	    fi
	fi
    fi
else
    echo ...using $CMP from CMP environment variable setting
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
if test "x${CMP}" = "x" ; then
    echo "ERROR:  Could not find the BRL-CAD pixel comparison utility"
    exit 1
else
    echo "Using [$CMP] for CMP"
fi

# determine the minimum time requirement in seconds for a single test run
if test "x${TIMEFRAME}" = "x" ; then
    TIMEFRAME=32
fi
echo "Using [$TIMEFRAME] for TIMEFRAME"

# maximum deviation percentage
if test "x${DEVIATION}" = "x" ; then
    DEVIATION=10
fi
echo "Using [$DEVIATION] for DEVIATION"

# approximate maximum time in seconds that a given test is allowed to take
if test "x${MAXTIME}" = "x" ; then
    MAXTIME=300
fi
echo "Using [$MAXTIME] for MAXTIME"

# let the user know about how long this might take
mintime="`expr $TIMEFRAME \* 6`"
echo "Minimum run time is `$path_to_run_sh/../sh/elapsed.sh $mintime`"
maxtime="`expr $MAXTIME \* 6`"
echo "Maximum run time is `$path_to_run_sh/../sh/elapsed.sh $maxtime`"
estimate="`expr $mintime \* 3`"
if test $estimate -gt $maxtime ; then
    estimate="$maxtime"
fi
echo "Estimated   time is `$path_to_run_sh/../sh/elapsed.sh $estimate`"

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
	${DB}/${run_geomname}.g ${run_geometry} \
	2> ${run_geomname}.log <<EOF
$run_view
EOF
    retval=$?
    if test "x$DEBUG" != "x" ; then
	echo "DEBUG: Running $RT returned $retval"
    fi
    return $retval
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
    benchmark_elapsed=0
    benchmark_hypersample=0
    benchmark_frame=0
    while test $benchmark_elapsed -lt $TIMEFRAME ; do

	if test -f ${benchmark_testname}.pix; then mv -f ${benchmark_testname}.pix ${benchmark_testname}.pix.$$; fi
	if test -f ${benchmark_testname}.log; then mv -f ${benchmark_testname}.log ${benchmark_testname}.log.$$; fi

	benchmark_start_time="`date '+%H %M %S'`"

	run $benchmark_testname $benchmark_geometry $benchmark_hypersample $benchmark_args << EOF
$benchmark_view
start $benchmark_frame;
end;
EOF
	retval=$?
	if test -f ${benchmark_testname}.pix.$benchmark_frame ; then mv -f ${benchmark_testname}.pix.$benchmark_frame ${benchmark_testname}.pix ; fi
	
	if test $retval != 0 ; then
	    echo "RAYTRACE ERROR"
	    break
	fi
	
	# compute how long we took, rounding up to at least one second
	# to prevent division by zero
	benchmark_elapsed="`$path_to_run_sh/../sh/elapsed.sh --seconds $benchmark_start_time`"
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
	    # considerably faster than the TIMEFRAME required
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
	grep RTFM ${benchmark_testname}.log
    done

    if test -f gmon.out; then mv -f gmon.out gmon.${benchmark_testname}.out; fi
    ${CMP} $path_to_run_sh/../pix/${benchmark_testname}.pix ${benchmark_testname}.pix
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
	-3.461886346e-01 -2.424038798e-01 9.063078165e-01 
0.000000000e+00 7.424039245e-01 5.198368430e-01 4.226182699e-01 
0.000000000e+00 0.000000000e+00 0.000000000e+00 0.000000000e+00 
1.000000000e+00 ;
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
echo "Total time spent testing: `$path_to_run_sh/../sh/elapsed.sh $start`"


# Compute and output the results

HOST="`hostname`"
if test $? != 0 ; then
    HOST="`uname -n`"
    if test $? != 0 ; then
	HOST="unknown"
    fi
fi

sh $path_to_run_sh/../bench/perf.sh "$HOST" "`date`" "$*" >> summary
ret=$?
if test $ret != 0 ; then
    tail -1 summary
    exit $ret
else
    echo
    echo "Summary:"
    tail -2 summary
fi
echo
echo Testing complete, read $path_to_run_sh/../doc/benchmark.tr for more details on the BRL-CAD Benchmark.

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
