#!/bin/sh

sed -f ../libz/sfile Makefile

for FILE in  ../libz/*.c
do
	SRC=${FILE##.*/}
	OBJ=${SRC%\.c}.o
	echo "${OBJ}: ../libz/$SRC"
	echo "	$CC $CFLAGS -c ../libz/$SRC"
	echo
done
