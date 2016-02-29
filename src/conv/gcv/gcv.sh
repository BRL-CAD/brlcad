#!/bin/sh

INPUT_FILES="
test1
test1.stl
stp::test1
stp::test1.stp
stp::test1.stl
"
OUTPUT_FILES="
$INPUT_FILES
test2
test2.stl
stp::test2
stp::test2.stp
stp::test2.stl
"
OPTIONS="
--in-format=stl
--out-format=stl
--in-format=stp
--out-format=stp
"

echo "./gcv"
./gcv
echo "./gcv -h"
./gcv -h
echo "./gcv --help"
./gcv --help
echo "./gcv -f -h"
./gcv -f -h

for arg1 in $INPUT_FILES
do
	for arg2 in $OUTPUT_FILES
	do
		echo "./gcv $arg1 $arg2"
		./gcv $arg1 $arg2
	done
done


for opt1 in $OPTIONS
do
	for arg1 in $INPUT_FILES
	do
		for arg2 in $OUTPUT_FILES
		do
			echo "./gcv $opt1 $arg1 $arg2"
			./gcv $opt1 $arg1 $arg2
		done
	done
	for opt2 in $OPTIONS
	do
		for arg1 in $INPUT_FILES
		do
			for arg2 in $OUTPUT_FILES
			do
				echo "./gcv $opt1 $opt2 $arg1 $arg2"
				./gcv $opt1 $opt2 $arg1 $arg2
			done
		done
	done
done








