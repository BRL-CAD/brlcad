#  Usage:  client.sh [hour [user]] 
#
#
# This is the client script for the BRL-CAD regression tests.  This script
# waits for the master script (master.sh) on the server to finish checking
# out a clean copy of the source.  When the master is done, it should 
# create a semaphore file in REGRESS_ROOT with the name start_$ARCH (where 
# ARCH is set from the output from machinetype.sh).
#
# If the master does not finish before the time set by $LASTHOUR, then this
# script will exit.  This is an attempt to limit the impact of this script
# on systems which have other functions to perform during working hours.
# The value of LASTHOUR can be set on the command line.  Values should be 
# in the range 0-24.  Setting LASTHOUR to 24 will cause the script to run
# regardless of the time.
#
# This script runs setup.sh, builds the package from sources, and does an 
# install.  The install goes to the location specified by REGRESS_ROOT.
# 
#

#
# Set the limit on how late we can start the build
#
if [ $# -ge 1 ] ; then
	LASTHOUR=$1
else
#	07:00  is the default limit on start time.
	LASTHOUR=7
fi
if [ $# -ge 2 ] ; then
	REPORT_TO_USER=$2
else
	REPORT_TO_USER="acst@arl.army.mil,cjohnson@arl.army.mil"
fi
#
# Set the installation directory
#
REGRESS_ROOT=/c/regress
if [ ! -d $REGRESS_ROOT ] ; then
	echo directory $REGRESS_DIR not present
	exit
fi

#
# Find out where the mail program lives
#
for MAIL in /bin/mail /usr/bin/mail ; do
	if [ -f $MAIL ] ; then
		break;
	fi
done

#
#  First we must sleep until the "cvs checkout" is complete or we are past
#  our start time.
#
COUNT=0
HOUR=`date | awk '{print $4}' | awk -F: '{print $1}'`
while [ ! -f $REGRESS_ROOT/brlcad/sh/machinetype.sh -a $HOUR -lt $LASTHOUR ]
do
	COUNT=`expr $COUNT + 1`
	sleep 600
	HOUR=`date | awk '{print $4}' | awk -F: '{print $1}'`
done
if [ ! $HOUR -lt $LASTHOUR ] ; then
	echo "`hostname` Regression starting too late, $HOUR after $LASTHOUR."
	echo "Slept $COUNT times"
	exit
fi

ARCH=`$REGRESS_ROOT/brlcad/sh/machinetype.sh`
export ARCH

BRLCAD_ROOT=/usr/brlcad
export BRLCAD_ROOT

# For machines like VIP which can't afford to clobber the production code.
if test -d /usr/brlcad-regress
then
	BRLCAD_ROOT=/usr/brlcad-regress
	export BRLCAD_ROOT
fi

COUNT=0
HOUR=`date | awk '{print $4}' | awk -F: '{print $1}'`
while [ ! -f $REGRESS_ROOT/start_$ARCH ] ; do
	COUNT=`expr $COUNT + 1`
	sleep 60
	HOUR=`date | awk '{print $4}' | awk -F: '{print $1}'`
done
if [ ! $HOUR -lt $LASTHOUR ] ; then
	echo "Too late to start regression, $HOUR after $LASTHOUR, Slept $COUNT times"
	exit
fi

#
#
#	Now we have got the go-ahead to build
#
#
# remove the semaphore file so that we don't accidentally re-start
#
rm $REGRESS_ROOT/start_$ARCH

PATH=$PATH:$BRLCAD_ROOT/bin
export PATH

mkdir $REGRESS_ROOT/$ARCH
cd $REGRESS_ROOT/brlcad

echo "$0: starting on `hostname`" >> $REGRESS_ROOT/$ARCH/MAKE_LOG
echo >> $REGRESS_ROOT/$ARCH/MAKE_LOG
echo "ARCH = $ARCH" 
echo "PATH = $PATH" >> $REGRESS_ROOT/$ARCH/MAKE_LOG
echo "HOME = $HOME" >> $REGRESS_ROOT/$ARCH/MAKE_LOG
echo >> $REGRESS_ROOT/$ARCH/MAKE_LOG


echo ./setup.sh -s >> $REGRESS_ROOT/$ARCH/MAKE_LOG
./setup.sh -s >> $REGRESS_ROOT/$ARCH/MAKE_LOG 2>&1 << EOF
yes
EOF

echo ./gen.sh -s all >> $REGRESS_ROOT/$ARCH/MAKE_LOG
./gen.sh -s all >> $REGRESS_ROOT/$ARCH/MAKE_LOG 2>&1

echo "`hostname` compilation complete" >> $REGRESS_ROOT/$ARCH/MAKE_LOG

echo ./gen.sh -s install >> $REGRESS_ROOT/$ARCH/MAKE_LOG
./gen.sh -s install >> $REGRESS_ROOT/$ARCH/MAKE_LOG 2>&1

echo "`hostname` installation complete" >> $REGRESS_ROOT/$ARCH/MAKE_LOG

#
# Now we have an installed version of the software.  We can now excercise the
# programs to see if things are 
#
#cd $BRLCAD_ROOT

cd $REGRESS_ROOT/$ARCH

if [ -f $HOME/brlcad_scripts/solids.sh ] ; then
	echo solids.sh >> $REGRESS_ROOT/$ARCH/MAKE_LOG
	$HOME/brlcad_scripts/solids.sh >> $REGRESS_ROOT/$ARCH/MAKE_LOG 2>&1
fi
if [ -f $HOME/brlcad_scripts/shaders.sh ] ; then
	echo shaders.sh >> $REGRESS_ROOT/$ARCH/MAKE_LOG
	$HOME/brlcad_scripts/shaders.sh >> $REGRESS_ROOT/$ARCH/MAKE_LOG 2>&1
fi

echo "`hostname` regression complete at `date`" >> $REGRESS_ROOT/$ARCH/MAKE_LOG

#
# Now we check to see how we compare to our "reference" run
#
if [ -f $HOME/brlcad_scripts/ref/$ARCH ] ; then

	diff -c	$HOME/brlcad_scripts/ref/$ARCH \
		$REGRESS_ROOT/$ARCH/MAKE_LOG \
		> $REGRESS_ROOT/$ARCH/DIFFS
else
	cp $REGRESS_ROOT/$ARCH/MAKE_LOG $HOME/brlcad_scripts/ref/$ARCH

	cat $REGRESS_ROOT/$ARCH/MAKE_LOG  >> $REGRESS_ROOT/$ARCH/DIFFS
	echo "No reference file available, installed this one" >> $REGRESS_ROOT/$ARCH/DIFFS
fi

echo "--- Execution Test Status ---" >> $REGRESS_ROOT/$ARCH/DIFFS
tail -10 $REGRESS_ROOT/$ARCH/MAKE_LOG >> $REGRESS_ROOT/$ARCH/DIFFS

if [ -s $REGRESS_ROOT/$ARCH/DIFFS ] ; then
	$MAIL -s "Regression Errors $ARCH"  $REPORT_TO_USER \
		< $REGRESS_ROOT/$ARCH/DIFFS
fi


#
# Build any appropriate binary distribution kit
#

case "${ARCH}" in
	fbsd)
	;;
	li)
	;;
	7d)
	;;
esac
