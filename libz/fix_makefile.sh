#!/bin/sh

sed -f ../libz/$1 Makefile

for FILE in  ../libz/*.c
do
	SRC=`echo $FILE | sed -e 's;.*/;;'`
	OBJ=`echo $SRC | sed -e 's/\.c\$/.o/'`
	echo "${OBJ}: ../libz/$SRC"
	echo "	$CC $CFLAGS -c ../libz/$SRC"
	echo
done
