#!/bin/sh

args=`getopt b:d:e:qr:u: $*`

if [ $? != 0 ] ; then
	echo "Usage: $0 [-r brlcad_root] [-b begin_time] [-e end_time] [-u user] -d regress_dir "
	exit 2
fi

set -- $args

BEGIN_HOUR=2
END_HOUR=7
REGRESS_DIR=/c/regress
MAILUSER=acst
QUIET=0
BRLCAD_ROOT=/usr/brlcad

for i in $* ; do
	case "$i" in
		-b)
			echo "-b $2";
			BEGIN_HOUR=$2;
			shift 2;;
		-d)
			echo "-d $2";
			REGRESS_DIR=$2;
			shift 2;;
		-e)
			echo "-e $2";
			END_HOUR=$2;
			shift 2;;
		-q)
			echo "-q";
			QUIET=1;
			shift;;
		-r)
			echo "-r $2";
			BRLCAD_ROOT=$2;
			shift 2;;
		-u)
			echo "-d $2";
			MAILUSER=$2;
			export MAILUSER
			shift 2;;
		--)
			shift; break;;
	esac
done


export BEGIN_HOUR
export END_HOUR
export REGRESS_DIR
export QUIET
export BRLCAD_ROOT

#
# Find out where the mail program lives
#
if [ X$MAILUSER != X ] ; then
	for MAIL in /bin/mail /usr/bin/mail ; do
		if [ -f $MAIL ] ; then
			break;
		fi
	done
fi

#
#  First we must sleep until the "cvs checkout" is complete or we are past
#  when we are allowed to run
#
COUNT=0
HOUR=`date | awk '{print $4}' | awk -F: '{print $1}'`
while [ $HOUR -lt $END_HOUR -a ! -f $REGRESS_DIR/brlcad/sh/machinetype.sh ]
do
	COUNT=`expr $COUNT + 1`
	sleep 600
	HOUR=`date | awk '{print $4}' | awk -F: '{print $1}'`
done



if [ $HOUR -ge $LASTHOUR ] ; then
	echo "`hostname` Regression starting too late, $HOUR after $LASTHOUR."
	echo "Slept $COUNT times"
	exit
fi

ARCH=`$REGRESS_DIR/brlcad/sh/machinetype.sh`
export ARCH

COUNT=0
HOUR=`date | awk '{print $4}' | awk -F: '{print $1}'`
while [ $HOUR -lt $END_HOUR -a ! -f $REGRESS_DIR/start_$ARCH ] ; do
	COUNT=`expr $COUNT + 1`
	sleep 60
	HOUR=`date | awk '{print $4}' | awk -F: '{print $1}'`
done
if [ ! $HOUR -lt $END_HOUR ] ; then
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
rm $REGRESS_DIR/start_$ARCH

PATH=$PATH:$BRLCAD_ROOT/bin
export PATH

mkdir $REGRESS_DIR/$ARCH
cd $REGRESS_DIR/brlcad

echo "$0: starting on `hostname`" >> $REGRESS_DIR/$ARCH/MAKE_LOG
echo ""				  >> $REGRESS_DIR/$ARCH/MAKE_LOG
echo "PATH = $PATH"		  >> $REGRESS_DIR/$ARCH/MAKE_LOG
echo "HOME = $HOME"		  >> $REGRESS_DIR/$ARCH/MAKE_LOG
echo ""		  		  >> $REGRESS_DIR/$ARCH/MAKE_LOG


echo ./setup.sh -s		  >> $REGRESS_DIR/$ARCH/MAKE_LOG
./setup.sh -s			  >> $REGRESS_DIR/$ARCH/MAKE_LOG 2>&1 << EOF
yes
EOF

echo ./gen.sh -s all		  >> $REGRESS_DIR/$ARCH/MAKE_LOG
./gen.sh -s all			  >> $REGRESS_DIR/$ARCH/MAKE_LOG 2>&1

echo "`hostname` compilation complete" >> $REGRESS_DIR/$ARCH/MAKE_LOG

echo ./gen.sh -s install	  >> $REGRESS_DIR/$ARCH/MAKE_LOG
./gen.sh -s install		  >> $REGRESS_DIR/$ARCH/MAKE_LOG 2>&1

echo "`hostname` installation complete" >> $REGRESS_DIR/$ARCH/MAKE_LOG

#
# Now we have an installed version of the software.  We can now excercise the
# programs to see if things are 
#

cd $REGRESS_DIR/$ARCH

if [ -f $REGRESS_DIR/brlcad/regress/solids.sh ] ; then
	echo solids.sh >> $REGRESS_DIR/$ARCH/MAKE_LOG
	$REGRESS_DIR/brlcad/regress/solids.sh \
		>> $REGRESS_DIR/$ARCH/MAKE_LOG 2>&1

fi
if [ -f $REGRESS_DIR/brlcad/regress/shaders.sh ] ; then
	echo shaders.sh >> $REGRESS_DIR/$ARCH/MAKE_LOG
	$REGRESS_DIR/brlcad/regress/shaders.sh \
		>> $REGRESS_DIR/$ARCH/MAKE_LOG 2>&1
fi

echo "`hostname` regression complete at `date`" >> $REGRESS_DIR/$ARCH/MAKE_LOG

#
# Now we check to see how we compare to our "reference" run
#
if [ -f $REGRESS_DIR/brlcad/regress/ref_${ARCH} ] ; then

	diff -c	$REGRESS_DIR/brlcad/regress/ref_${ARCH} \
		$REGRESS_DIR/$ARCH/MAKE_LOG \
		> $REGRESS_DIR/$ARCH/DIFFS
else
	cp $REGRESS_DIR/$ARCH/MAKE_LOG $REGRESS_DIR/brlcad/regress/ref_${ARCH}

	cat $REGRESS_DIR/$ARCH/MAKE_LOG  >> $REGRESS_DIR/$ARCH/DIFFS
	echo "No reference file available, installed this one" >> $REGRESS_DIR/$ARCH/DIFFS
fi

echo "--- Execution Test Status ---" >> $REGRESS_DIR/$ARCH/DIFFS
tail -10 $REGRESS_DIR/$ARCH/MAKE_LOG >> $REGRESS_DIR/$ARCH/DIFFS

if [ -s $REGRESS_DIR/$ARCH/DIFFS -a X$MAILUSER != X ] ; then
	$MAIL -s "Regression Errors $ARCH"  $REPORT_TO_USER \
		< $REGRESS_DIR/$ARCH/DIFFS
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
