#!/bin/sh
# A Shell script to run the BRL-CAD Benchmark Test
#  @(#)$Header$ (BRL)

# Ensure /bin/sh
export PATH || (echo "This isn't sh."; sh $0 $*; kill $$)

eval `machinetype.sh -b`	# sets MACHINE, UNIXTYPE, HAS_TCP
if test -f ../.rt.$MACHINE/rt
then
	RT=../.rt.$MACHINE/rt
	DB=../.db.$MACHINE
	LD_LIBRARY_PATH=../.libbu.$MACHINE:../.libbn.$MACHINE:../.librt.$MACHINE:../.libfb.$MACHINE:../.libpkg.$MACHINE:../.libsysv.$MACHINE:$LD_LIBRARY_PATH
else
	if test ! -f ../rt/rt
	then
		echo "Can't find RT"
		exit 1
	fi
	RT=../rt/rt
	DB=../db
	LD_LIBRARY_PATH=../libbu:../libbn:../librt:../libfb:../libpkg:../libsysv:$LD_LIBRARY_PATH
fi
export LD_LIBRARY_PATH

CMP=./pixcmp
if test ! -f $CMP
then
	cake pixcmp
fi

# Alliant NFS hack
if test x${MACHINE} = xfx
then
	cp ${RT} /tmp/rt
	cp ${CMP} /tmp/pixcmp
	RT=/tmp/rt
	CMP=/tmp/pixcmp
fi

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
${CMP} ../pix/moss.pix moss.pix
if test $? = 0
then
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
${CMP} ../pix/world.pix world.pix
if test $? = 0
then
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
${CMP} ../pix/star.pix star.pix
if test $? = 0
then
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
${CMP} ../pix/bldg391.pix bldg391.pix
if test $? = 0
then
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
${CMP} ../pix/m35.pix m35.pix
if test $? = 0
then
	echo m35.pix:  answers are RIGHT
else
	echo m35.pix:  WRONG WRONG WRONG WRONG WRONG WRONG
fi

echo
echo Testing complete, check times against reference files in ../pix/.

if test x$UNIXTYPE = xBSD
then	HOST=`hostname`
else	HOST=`uname -n`
fi

sh ../bench/perf.sh "$HOST" "`date`" "$*" >> summary
