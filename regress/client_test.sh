#!/bin/sh
#
#  This script is intended to be run from the client_build.sh which is run
#  from the client_wait.sh script.  That is how the environment variables
#  get set.


#
# Now we have an installed version of the software.  We can now excercise the
# programs to see if things are working properly
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
