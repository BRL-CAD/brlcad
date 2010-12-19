#!/bin/sh
# $Id$
# vi:ts=4 sw=4:

# run lint on each of the ".c" files in the test directory

if test $# = 1
then
	PROG_DIR=`pwd`
	TEST_DIR=$1
else
	PROG_DIR=..
	TEST_DIR=.
fi

echo '** '`date`
for i in ${TEST_DIR}/*.c
do
	make -f $PROG_DIR/makefile lint C_FILES=$i srcdir=$PROG_DIR
done
