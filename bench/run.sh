#!/bin/sh
#                          R U N . S H
# BRL-CAD
#
# Copyright (c) 2004 United States Government as represented by the
# U.S. Army Research Laboratory.
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
# A Shell script to run the BRL-CAD Benchmark Test
#  @(#)$Header$ (BRL)

# set RT, DB, and/or CMP environment variables to override default locations

echo "B R L - C A D   B E N C H M A R K"
echo "================================="

# Ensure /bin/sh
export PATH || (echo "This isn't sh."; sh $0 $*; kill $$)
path_to_run_sh=`dirname $0`


echo Looking for RT...
# find the raytracer
# RT environment variable overrides
if test "x${RT}" = "x" ; then
  # see if we find the rt binary
  if test -x "$path_to_run_sh/../src/rt" ; then
    echo ...found $path_to_run_sh/../src/rt/rt
    RT="$path_to_run_sh/../src/rt/rt"
    LD_LIBRARY_PATH="$path_to_run_sh/../src/rt:$LD_LIBRARY_PATH"
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
export LD_LIBRARY_PATH

echo 

# Run the tests

echo +++++ moss
if test -f moss.pix; then mv -f moss.pix moss.pix.$$; fi
if test -f moss.log; then mv -f moss.log moss.log.$$; fi
time $RT -B -M -s512 $* \
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
start 0;
end;
EOF
if test -f gmon.out; then mv -f gmon.out gmon.moss.out; fi
${CMP} $path_to_run_sh/../pix/moss.pix moss.pix
if test $? = 0 ; then
  echo moss.pix:  answers are RIGHT
else
  echo moss.pix:  WRONG WRONG WRONG WRONG WRONG WRONG
fi

echo +++++ world
if test -f world.pix; then mv -f world.pix world.pix.$$; fi
if test -f world.log; then mv -f world.log world.log.$$; fi
time $RT -B -M -s512 $* \
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
start 0;
end;
EOF
if test -f gmon.out; then mv -f gmon.out gmon.world.out; fi
${CMP} $path_to_run_sh/../pix/world.pix world.pix
if test $? = 0 ; then
  echo world.pix:  answers are RIGHT
else
  echo world.pix:  WRONG WRONG WRONG WRONG WRONG WRONG
fi

echo +++++ star
if test -f star.pix; then mv -f star.pix star.pix.$$; fi
if test -f star.log; then mv -f star.log star.log.$$; fi
time $RT -B -M -s512 $* \
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
start 0;
end;
EOF
if test -f gmon.out; then mv -f gmon.out gmon.star.out; fi
${CMP} $path_to_run_sh/../pix/star.pix star.pix
if test $? = 0 ; then
  echo star.pix:  answers are RIGHT
else
  echo star.pix:  WRONG WRONG WRONG WRONG WRONG WRONG
fi

echo +++++ bldg391
if test -f bldg391.pix; then mv -f bldg391.pix bldg391.pix.$$; fi
if test -f bldg391.log; then mv -f bldg391.log bldg391.log.$$; fi
time $RT -B -M -s512 $* \
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
start 0;
end;
EOF
if test -f gmon.out; then mv -f gmon.out gmon.bldg391.out; fi
${CMP} $path_to_run_sh/../pix/bldg391.pix bldg391.pix
if test $? = 0 ; then
  echo bldg391.pix:  answers are RIGHT
else
  echo bldg391.pix:  WRONG WRONG WRONG WRONG WRONG WRONG
fi

echo +++++ m35
if test -f m35.pix; then mv -f m35.pix m35.pix.$$; fi
if test -f m35.log; then mv -f m35.log m35.log.$$; fi
time $RT -B -M -s512 $* \
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
start 0;
end;
EOF
if test -f gmon.out; then mv -f gmon.out gmon.m35.out; fi
${CMP} $path_to_run_sh/../pix/m35.pix m35.pix
if test $? = 0 ; then
  echo m35.pix:  answers are RIGHT
else
  echo m35.pix:  WRONG WRONG WRONG WRONG WRONG WRONG
fi

echo +++++ sphflake
if test -f sphflake.pix; then mv -f sphflake.pix sphflake.pix.$$; fi
if test -f sphflake.log; then mv -f sphflake.log sphflake.log.$$; fi
time $RT -B -M -s512 $* \
  -o sphflake.pix \
  $DB/sphflake.g \
  "scene.r" \
  2> sphflake.log \
  << EOF
viewsize 2.556283261452611e+04;
orientation 4.406810841785839e-01 4.005093234738861e-01 5.226451688385938e-01 6.101102288499644e-01;
eye_pt 2.418500583758302e+04 -3.328563644344796e+03 8.489926952850350e+03;
start 0;
end;
EOF
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
if ! test $ret = 0 ; then
  tail -1 summary
  exit $ret
else
  echo
  echo "Summary:"
  tail -2 summary
fi
echo
echo Testing complete, check times against reference files in $path_to_run_sh/../pix/.

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
