#!/bin/sh

set + 

BIN=$1/bin
export BIN

rm -f iges.log iges.g iges_file.iges iges_stdout_new.g iges_new.g iges_stdout.iges iges_file.iges

$BIN/mged -c 2> iges.log <<EOF
opendb iges.g y

units mm
size 1000
make box.s arb8
facetize -n box.nmg box.s
kill box.s
q
EOF

$BIN/g-iges -o iges_file.iges iges.g box.nmg 2>> iges.log> /dev/null
$BIN/g-iges iges.g box.nmg > iges_stdout.iges 2>> iges.log 

$BIN/iges-g -o iges_new.g -p iges_file.iges 2>> iges.log

if [ $? != 0 ] ; then
    /bin/echo g-iges/iges-g FAILED
else
    /bin/echo g-iges/iges-g completed successfully
fi


$BIN/iges-g -o iges_stdout_new.g -p iges_stdout.iges 2>> iges.log

if [ $? != 0 ] ; then
    /bin/echo g-iges/iges-g FAILED
else
    /bin/echo g-iges/iges-g completed successfully
fi
