#!/bin/sh
# A Shell script to run the BRL-CAD Benchmark Test,
# with output going to the current framebuffer, rather than a file.
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
