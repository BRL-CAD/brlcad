#!/bin/sh
#
#  This script is intended to be run from the client_build.sh which is run
#  from the client_wait.sh script.  That is how the environment variables
#  get set.

. ./library

initializeVariable TESTS_DIR "$PWD/tests.d"

#
# Now we have an installed version of the software.  We can now excercise the
# programs to see if things are working properly
#

if [ -f ${REGRESS_DIR}/.regress.${ARCH}/MAKE_LOG ] ; then mv ${REGRESS_DIR}/.regress.${ARCH}/MAKE_LOG ${REGRESS_DIR}/.regress.${ARCH}/MAKE_LOG.bak ; fi

if [ -d $TESTS_DIR ] ; then
    echo "Running tests found in $TESTS_DIR"

    SCRIPTS=`ls -B $TESTS_DIR`
    for SCRIPT in $SCRIPTS ; do
	if [ -f ${TESTS_DIR}/$SCRIPT ] && [ -x ${TESTS_DIR}/$SCRIPT ] ; then
		echo "Running [${TESTS_DIR}/$SCRIPT] test" 
		echo "RUNNING TEST:" >> $REGRESS_DIR/.regress.${ARCH}/MAKE_LOG
		echo ${TESTS_DIR}/$SCRIPT >> $REGRESS_DIR/.regress.${ARCH}/MAKE_LOG
		${TESTS_DIR}/$SCRIPT >> $REGRESS_DIR/.regress.${ARCH}/MAKE_LOG 2>&1
	fi
    done
else
    echo "WARNING: could not find any tests to run in $TESTS_DIR"
fi

###cd $REGRESS_DIR/.regress.${ARCH}


echo "$HOSTNAME regression complete at `date`" >> $REGRESS_DIR/.regress.${ARCH}/MAKE_LOG

#
# Now we check to see how we compare to our "reference" run
#
if [ -f ./ref_${ARCH} ] ; then

	diff -c	-b ref_${ARCH} \
		$REGRESS_DIR/.regress.${ARCH}/MAKE_LOG \
		> $REGRESS_DIR/.regress.${ARCH}/DIFFS
else
	cp $REGRESS_DIR/.regress.${ARCH}/MAKE_LOG ./ref_${ARCH}

	echo "No reference file available, installed this one" >> $REGRESS_DIR/.regress.${ARCH}/DIFFS
	cat $REGRESS_DIR/.regress.${ARCH}/MAKE_LOG  >> $REGRESS_DIR/.regress.${ARCH}/DIFFS
fi

echo "--- Execution Test Status ---" >> $REGRESS_DIR/.regress.${ARCH}/DIFFS
tail -10 $REGRESS_DIR/.regress.${ARCH}/MAKE_LOG >> $REGRESS_DIR/.regress.${ARCH}/DIFFS

export MAILUSER=morrison
if [ -s $REGRESS_DIR/.regress.${ARCH}/DIFFS -a X$MAILUSER != X ] ; then
    echo "MAILING OUT DIFF FILE"
    mail -s "Regression Errors .regress.${ARCH}" $MAILUSER < $REGRESS_DIR/.regress.${ARCH}/DIFFS
# 	$MAIL -s "Regression Errors .regress.${ARCH}"  $REPORT_TO_USER \
# 		< $REGRESS_DIR/.regress.${ARCH}/DIFFS
fi

echo "Done $0"
