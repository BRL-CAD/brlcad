#!/bin/sh
# A Shell script to run the BRL-CAD Benchmark Test
#  @(#)$Header$ (BRL)

# Ensure /bin/sh
export PATH || (echo "This isn't sh.  Feeding myself to sh."; sh $0 $*; kill $$)

eval `machinetype.sh -b`	# sets MACHINE, UNIXTYPE, HAS_TCP
if test -f ../.rt.$MACHINE/rt
then
	RT=../.rt.$MACHINE/rt
	DB=../.db.$MACHINE
else
	if test -f ../rt/rt
	then
		RT=../rt/rt
		DB=../db
	else
		echo "Can't find RT"
		exit 1
	fi
fi

# Run the tests
set -x

mv -f moss.pix moss.pix.$$
mv -f moss.log moss.log.$$
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
./pixcmp ../pix/moss.pix moss.pix
if test $? = 0
then
	echo moss.pix:  RIGHT answers
else
	echo moss.pix:  WRONG WRONG WRONG
fi


mv -f world.pix world.pix.$$
mv -f world.log world.log.$$
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
./pixcmp ../pix/world.pix world.pix
if test $? = 0
then
	echo world.pix:  RIGHT answers
else
	echo world.pix:  WRONG WRONG WRONG
fi


mv -f star.pix star.pix.$$
mv -f star.log star.log.$$
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
./pixcmp ../pix/star.pix star.pix
if test $? = 0
then
	echo star.pix:  RIGHT answers
else
	echo star.pix:  WRONG WRONG WRONG
fi


mv -f bldg391.pix bldg391.pix.$$
mv -f bldg391.log bldg391.log.$$
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
./pixcmp ../pix/bldg391.pix bldg391.pix
if test $? = 0
then
	echo bldg391.pix:  RIGHT answers
else
	echo bldg391.pix:  WRONG WRONG WRONG
fi

echo Testing complete, check times against reference files in ../pix/.

if test x$UNIXTYPE = xBSD
then	HOST=`hostname`
else	HOST=`uname -n`
fi

sh ../bench/perf.sh "$HOST" "`date`" "$*" >> summary
