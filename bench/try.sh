#!/bin/sh
# A Shell script to run the BRL-CAD Benchmark Test,
# with output going to the current framebuffer, rather than a file.
#  @(#)$Header$ (BRL)

# Ensure /bin/sh
export PATH || (echo "This isn't sh."; sh $0 $*; kill $$)
path_to_run_sh=`dirname $0`

# sets MACHINE, UNIXTYPE, HAS_TCP
eval `machinetype.sh -b 2> /dev/null`	

if test "x$MACHINE" = "x" ; then
  echo Looking for machinetype.sh...
  eval `$path_to_run_sh/../sh/machinetype.sh -b 2> /dev/null`
  if test "x$MACHINE" = "x" ; then
    echo WARNING: could not find machinetype.sh
    # try _something_, linux is pretty popular
    MACHINE="li"
  else
    echo ...found local machinetype.sh
  fi
fi

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
	if $? = 0 ; then
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

# Alliant NFS hack
if test "x${MACHINE}" = "xfx" ; then
  cp ${RT} /tmp/rt
  cp ${CMP} /tmp/pixcmp
  RT=/tmp/rt
  CMP=/tmp/pixcmp
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

time $RT -B -M -s512 $* \
	$DB/moss.g all.g \
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

echo +++++ world

time $RT -B -M -s512 $* \
	$DB/world.g all.g \
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

echo +++++ star

time $RT -B -M -s512 $* \
	$DB/star.g all \
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

echo +++++ bldg391

time $RT -B -M -s512 $* \
	$DB/bldg391.g all.g \
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

echo +++++ m35

time $RT -B -M -s512 $* \
	$DB/m35.g all.g \
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

echo +++++ sphflake

time $RT -B -M -s512 $* \
	$DB/sphflake.g scene.r \
	<< EOF
viewsize 2.556283261452611e+04;
orientation 4.406810841785839e-01 4.005093234738861e-01 5.226451688385938e-01 6.101102288499644e-01;
eye_pt 2.418500583758302e+04 -3.328563644344796e+03 8.489926952850350e+03;
start 0;
end;
EOF

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
