#!/bin/sh
# A Shell script to test LGT
#  @(#)$Header$ (BRL)

# Ensure /bin/sh
export PATH || (echo "This isn't sh."; sh $0 $*; kill $$)

eval `machinetype.sh -b`	# sets MACHINE, UNIXTYPE, HAS_TCP
if test -f ../.lgt.$MACHINE/lgt
then
	LGT=../.lgt.$MACHINE/lgt
	DB=../.db.$MACHINE
	LD_LIBRARY_PATH=../.libcursor.$MACHINE:../.libtermio.$MACHINE:../.libbu.$MACHINE:../.libbn.$MACHINE:../.librt.$MACHINE:../.libfb.$MACHINE:../.libpkg.$MACHINE:../.libsysv.$MACHINE:$LD_LIBRARY_PATH
else
	if test -f ../lgt/lgt
	then
		echo "Can't find LGT"
		exit 1
	fi
	LGT=../lgt/lgt
	DB=../db
	LD_LIBRARY_PATH=../libcursor:../libtermio:../libbu:../libbn:../librt:../libfb:../libpkg:../libsysv:$LD_LIBRARY_PATH
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
	cp ${LGT} /tmp/lgt
	cp ${CMP} /tmp/pixcmp
	LGT=/tmp/lgt
	CMP=/tmp/pixcmp
fi

# This test of LGT assumes a "live" framebuffer here.
if test x$FB_FILE = x
then
	echo "Environment variable FB_FILE must be set to a real framebuffer"
	echo "for this test of LGT to operate correctly."
	exit 1
fi

# Run the first set of tests

${LGT} ${DB}/lgt-test.g all.g << EOF
# read light source data base
v ../db/lgt-test.ldb
# read material data base
w ../db/lgt-test.mdb
# force display window to be 512x512 pixels, should cause 8-to-1 zoom
T 512
# clear frame buffer
E
# set maximum bounces to 20
K 20
# turn hidden-line drawing off
k 0
# configure grid: 64 rays across
G 64 0 0 0.0
# turn perspective off
p 0
# change background to mauve
b 111 66 66
# read key frame 
j ../db/lgt-test.key
# display view #3
R
# save image in file
H lgt3a.pix
# change frame buffer to disk file
o lgt3b.pix
# save a script of previous commands, to produce the "b" version.
S script3
# disable reading of key frame file
j
# change frame buffer back to default
o
# configure grid: 2.45 mm cells, centered about model origin, view size 225 mm
G 2.45 1 1 225.0
# change background to firebrick
b 142 35 35
# force frame buffer size to be 455 pixels across, should cause 4-to-1 zoom
T 455
# set automatic perspective at 0.25
p 0.25
# set anti-aliasing over-sampling factor to 2
A 2
# set maximum bounces to 20
K 20
# display view #1
R
# save image in file
H lgt1a.pix
# change frame buffer to disk file
o lgt1b.pix
# save a script
S script1
# change frame buffer back to default
o
# turn off anti-aliasing
A 1
# revert to auto-sizing of frame buffer
T 0
# set automatic perspective at 0.25
p 0.25
# select hidden-line drawing, reverse video
k 2
# configure grid: 128 rays across, centered at model RPP, view size 225mm
G 128 0 0 225.0
# display view #2
R
# save image in file
H lgt2a.pix
# change frame buffer to disk file
o lgt2b.pix
# save a script
S script2
# change frame buffer back to default
o
# bye
q
EOF
if test $? = 0
then	:;
else	echo "Abnormal lgt exit"; exit 1
fi

# Run the second set of tests.  The scripts were created above.
 ./script1 < /dev/null &&
 ./script2 < /dev/null &&
 ./script3 < /dev/null || { echo "Script test failed."; exit 1; }

FAIL=0
for i in 1 2 3
do
	if ${CMP} ../pix/lgt${i}.pix lgt${i}a.pix
	then :;
	else echo "Test $i A failed."; FAIL=1
	fi
	if ${CMP} ../pix/lgt${i}.pix lgt${i}b.pix
	then :;
	else echo "Test $i B failed."; FAIL=1
	fi
done
if test "$FAIL" -eq 0
then
	echo "LGT Tested OK."
	exit 0
fi
echo "*** LGT does NOT work on this platform ***"
exit 2

