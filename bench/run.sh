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
# Original Authors:
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

# determine the minimum time requirement for a single test run
if test "x${TIMEFRAME}" = "x" ; then
    TIMEFRAME=60
fi
echo "Using [$TIMEFRAME] for TIMEFRAME"

echo 

# Run the tests

echo +++++ moss
elapsed=0
hypersample=0
frame=0
while test $elapsed -lt $TIMEFRAME ; do

if test -f moss.pix; then mv -f moss.pix moss.pix.$$; fi
if test -f moss.log; then mv -f moss.log moss.log.$$; fi

START_TIME="`date '+%H %M %S'`"
$RT -B -M -s512 -H${hypersample} -J0 $* \
  -o moss.pix \
  $DB/moss.g all.g \
  2> moss.log \
  << EOF
viewsize 1.572026215e+02;
eye_pt 6.379990387e+01 3.271768951e+01 3.366661453e+01;
viewrot -5.735764503e-01 8.191520572e-01 0.000000000e+00 
0.000000000e+00 -3.461886346e-01 -2.424038798e-01 9.063078165e-01 
0.000000000e+00 7.424039245e-01 5.198368430e-01 4.226182699e-01 
0.000000000e+00 0.000000000e+00 0.000000000e+00 0.000000000e+00 
1.000000000e+00 ;
start $frame;
end;
EOF
ret=$?
if test -f moss.pix.$frame ; then mv -f moss.pix.$frame moss.pix ; fi

if test $ret != 0 ; then
    echo "RAYTRACE ERROR"
    break
fi

elapsed="`$path_to_run_sh/../sh/elapsed.sh --seconds $START_TIME`"
if test "x$hypersample" = "x0" ; then
#    echo "${elapsed}s real elapsed,	1 ray/pixel,	`expr 262144 / $elapsed` primary rays/s (inexact wallclock)"
    hypersample=1
else
#    echo "${elapsed}s real elapsed,	`expr $hypersample + 1` rays/pixel,	`expr \( 262144 \* \( $hypersample + 1 \) / $elapsed \)` primary rays/s (inexact wallclock)"
    hypersample="`expr $hypersample + $hypersample + 1`"
fi
frame="`expr $frame + 1`"
grep RTFM moss.log
done

if test -f gmon.out; then mv -f gmon.out gmon.moss.out; fi
${CMP} $path_to_run_sh/../pix/moss.pix moss.pix
if test $? = 0 ; then
    echo moss.pix:  answers are RIGHT
else
    echo moss.pix:  WRONG WRONG WRONG WRONG WRONG WRONG
fi


echo +++++ world
elapsed=0
hypersample=0
frame=0
while test $elapsed -lt $TIMEFRAME ; do

if test -f world.pix; then mv -f world.pix world.pix.$$; fi
if test -f world.log; then mv -f world.log world.log.$$; fi

START_TIME="`date '+%H %M %S'`"
$RT -B -M -s512 -H${hypersample} -J0 $* \
  -o world.pix \
  $DB/world.g all.g \
  2> world.log \
  << EOF
viewsize 1.572026215e+02;
eye_pt 6.379990387e+01 3.271768951e+01 3.366661453e+01;
viewrot -5.735764503e-01 8.191520572e-01 0.000000000e+00
0.000000000e+00 -3.461886346e-01 -2.424038798e-01 9.063078165e-01 
0.000000000e+00 7.424039245e-01 5.198368430e-01 4.226182699e-01 
0.000000000e+00 0.000000000e+00 0.000000000e+00 0.000000000e+00 
1.000000000e+00 ;
start $frame;
end;
EOF
ret=$?
if test -f world.pix.$frame ; then mv -f world.pix.$frame world.pix ; fi

if test $ret != 0 ; then
    echo "RAYTRACE ERROR"
    break
fi

elapsed="`$path_to_run_sh/../sh/elapsed.sh --seconds $START_TIME`"
if test "x$hypersample" = "x0" ; then
#    echo "${elapsed}s real elapsed,	1 ray/pixel,	`expr 262144 / $elapsed` primary rays/s (inexact wallclock)"
    hypersample=1
else
#    echo "${elapsed}s real elapsed,	`expr $hypersample + 1` rays/pixel,	`expr \( 262144 \* \( $hypersample + 1 \) / $elapsed \)` primary rays/s (inexact wallclock)"
    hypersample="`expr $hypersample + $hypersample + 1`"
fi
frame="`expr $frame + 1`"
grep RTFM world.log
done

if test -f gmon.out; then mv -f gmon.out gmon.world.out; fi
${CMP} $path_to_run_sh/../pix/world.pix world.pix
if test $? = 0 ; then
    echo world.pix:  answers are RIGHT
else
    echo world.pix:  WRONG WRONG WRONG WRONG WRONG WRONG
fi


echo +++++ star
elapsed=0
hypersample=0
frame=0
while test $elapsed -lt $TIMEFRAME ; do

if test -f star.pix; then mv -f star.pix star.pix.$$; fi
if test -f star.log; then mv -f star.log star.log.$$; fi

START_TIME="`date '+%H %M %S'`"
$RT -B -M -s512 -H${hypersample} -J0 $* \
  -o star.pix \
  $DB/star.g all \
  2> star.log \
  <<EOF
viewsize 2.500000000e+05;
eye_pt 2.102677960e+05 8.455500000e+04 2.934714650e+04;
viewrot -6.733560560e-01 6.130643360e-01 4.132114880e-01 0.000000000e+00 
5.539599410e-01 4.823888300e-02 8.311441420e-01 0.000000000e+00 
4.896120540e-01 7.885590550e-01 -3.720948210e-01 0.000000000e+00 
0.000000000e+00 0.000000000e+00 0.000000000e+00 1.000000000e+00 ;
start $frame;
end;
EOF
ret=$?
if test -f star.pix.$frame ; then mv -f star.pix.$frame star.pix ; fi

if test $ret != 0 ; then
    echo "RAYTRACE ERROR"
    break
fi

elapsed="`$path_to_run_sh/../sh/elapsed.sh --seconds $START_TIME`"
if test "x$hypersample" = "x0" ; then
#    echo "${elapsed}s real elapsed,	1 ray/pixel,	`expr 262144 / $elapsed` primary rays/s (inexact wallclock)"
    hypersample=1
else
#    echo "${elapsed}s real elapsed,	`expr $hypersample + 1` rays/pixel,	`expr \( 262144 \* \( $hypersample + 1 \) / $elapsed \)` primary rays/s (inexact wallclock)"
    hypersample="`expr $hypersample + $hypersample + 1`"
fi
frame="`expr $frame + 1`"
grep RTFM star.log
done

if test -f gmon.out; then mv -f gmon.out gmon.star.out; fi
${CMP} $path_to_run_sh/../pix/star.pix star.pix
if test $? = 0 ; then
    echo star.pix:  answers are RIGHT
else
    echo star.pix:  WRONG WRONG WRONG WRONG WRONG WRONG
fi


echo +++++ bldg391
elapsed=0
hypersample=0
frame=0
while test $elapsed -lt $TIMEFRAME ; do

if test -f bldg391.pix; then mv -f bldg391.pix bldg391.pix.$$; fi
if test -f bldg391.log; then mv -f bldg391.log bldg391.log.$$; fi

START_TIME="`date '+%H %M %S'`"
$RT -B -M -s512 -H${hypersample} -J0 $* \
  -o bldg391.pix \
  $DB/bldg391.g all.g \
  2> bldg391.log \
  <<EOF
viewsize 1.800000000e+03;
eye_pt 6.345012207e+02 8.633251343e+02 8.310771484e+02;
viewrot -5.735764503e-01 8.191520572e-01 0.000000000e+00 
0.000000000e+00 -3.461886346e-01 -2.424038798e-01 9.063078165e-01 
0.000000000e+00 7.424039245e-01 5.198368430e-01 4.226182699e-01 
0.000000000e+00 0.000000000e+00 0.000000000e+00 0.000000000e+00 
1.000000000e+00 ;
start $frame;
end;
EOF
ret=$?
if test -f bldg391.pix.$frame ; then mv -f bldg391.pix.$frame bldg391.pix ; fi

if test $ret != 0 ; then
    echo "RAYTRACE ERROR"
    break
fi

elapsed="`$path_to_run_sh/../sh/elapsed.sh --seconds $START_TIME`"
if test "x$hypersample" = "x0" ; then
#    echo "${elapsed}s real elapsed,	1 ray/pixel,	`expr 262144 / $elapsed` primary rays/s (inexact wallclock)"
    hypersample=1
else
#    echo "${elapsed}s real elapsed,	`expr $hypersample + 1` rays/pixel,	`expr \( 262144 \* \( $hypersample + 1 \) / $elapsed \)` primary rays/s (inexact wallclock)"
    hypersample="`expr $hypersample + $hypersample + 1`"
fi
frame="`expr $frame + 1`"
grep RTFM bldg391.log
done

if test -f gmon.out; then mv -f gmon.out gmon.bldg391.out; fi
${CMP} $path_to_run_sh/../pix/bldg391.pix bldg391.pix
if test $? = 0 ; then
    echo bldg391.pix:  answers are RIGHT
else
    echo bldg391.pix:  WRONG WRONG WRONG WRONG WRONG WRONG
fi


echo +++++ m35
elapsed=0
hypersample=0
frame=0
while test $elapsed -lt $TIMEFRAME ; do

if test -f m35.pix; then mv -f m35.pix m35.pix.$$; fi
if test -f m35.log; then mv -f m35.log m35.log.$$; fi

START_TIME="`date '+%H %M %S'`"
$RT -B -M -s512 -H${hypersample} -J0 $* \
  -o m35.pix \
  $DB/m35.g all.g \
  2> m35.log \
  << EOF
viewsize 6.787387985e+03;
eye_pt 3.974533127e+03 1.503320754e+03 2.874633221e+03;
viewrot -5.527838919e-01 8.332423558e-01 1.171090926e-02 0.000000000e+00 
-4.815587087e-01 -3.308784486e-01 8.115544728e-01 0.000000000e+00 
6.800964482e-01 4.429747496e-01 5.841593895e-01 0.000000000e+00 
0.000000000e+00 0.000000000e+00 0.000000000e+00 1.000000000e+00 ;
start $frame;
end;
EOF
ret=$?
if test -f m35.pix.$frame ; then mv -f m35.pix.$frame m35.pix ; fi

if test $ret != 0 ; then
    echo "RAYTRACE ERROR"
    break
fi

elapsed="`$path_to_run_sh/../sh/elapsed.sh --seconds $START_TIME`"
if test "x$hypersample" = "x0" ; then
#    echo "${elapsed}s real elapsed,	1 ray/pixel,	`expr 262144 / $elapsed` primary rays/s (inexact wallclock)"
    hypersample=1
else
#    echo "${elapsed}s real elapsed,	`expr $hypersample + 1` rays/pixel,	`expr \( 262144 \* \( $hypersample + 1 \) / $elapsed \)` primary rays/s (inexact wallclock)"
    hypersample="`expr $hypersample + $hypersample + 1`"
fi
frame="`expr $frame + 1`"
grep RTFM m35.log
done

if test -f gmon.out; then mv -f gmon.out gmon.m35.out; fi
${CMP} $path_to_run_sh/../pix/m35.pix m35.pix
if test $? = 0 ; then
    echo m35.pix:  answers are RIGHT
else
    echo m35.pix:  WRONG WRONG WRONG WRONG WRONG WRONG
fi


echo +++++ sphflake
elapsed=0
hypersample=0
frame=0
while test $elapsed -lt $TIMEFRAME ; do

if test -f sphflake.pix; then mv -f sphflake.pix sphflake.pix.$$; fi
if test -f sphflake.log; then mv -f sphflake.log sphflake.log.$$; fi

START_TIME="`date '+%H %M %S'`"
$RT -B -M -s512 -H${hypersample} -J0 $* \
  -o sphflake.pix \
  $DB/sphflake.g \
  "scene.r" \
  2> sphflake.log \
  << EOF
viewsize 2.556283261452611e+04;
orientation 4.406810841785839e-01 4.005093234738861e-01 5.226451688385938e-01 6.101102288499644e-01;
eye_pt 2.418500583758302e+04 -3.328563644344796e+03 8.489926952850350e+03;
start $frame;
end;
EOF
ret=$?
if test -f sphflake.pix.$frame ; then mv -f sphflake.pix.$frame sphflake.pix ; fi

if test $ret != 0 ; then
    echo "RAYTRACE ERROR"
    break
fi

elapsed="`$path_to_run_sh/../sh/elapsed.sh --seconds $START_TIME`"
if test "x$hypersample" = "x0" ; then
#    echo "${elapsed}s real elapsed,	1 ray/pixel,	`expr 262144 / $elapsed` primary rays/s (inexact wallclock)"
    hypersample=1
else
#    echo "${elapsed}s real elapsed,	`expr $hypersample + 1` rays/pixel,	`expr \( 262144 \* \( $hypersample + 1 \) / $elapsed \)` primary rays/s (inexact wallclock)"
    hypersample="`expr $hypersample + $hypersample + 1`"
fi
frame="`expr $frame + 1`"
grep RTFM sphflake.log
done


if test -f gmon.out; then mv -f gmon.out gmon.sphflake.out; fi
${CMP} $path_to_run_sh/../pix/sphflake.pix sphflake.pix
if test $? = 0 ; then
    echo sphflake.pix:  answers are RIGHT
else
    echo sphflake.pix:  WRONG WRONG WRONG WRONG WRONG WRONG
fi

if test x$UNIXTYPE = xBSD ; then
    HOST=`hostname`
else
    HOST=`uname -n`
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
