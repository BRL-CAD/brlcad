#!/bin/sh
export IGNOREEOF=10
if [ $# -lt 1 -o $# -gt 2 ] ; then
    echo "Usage: $0 database.g [tol]"
    exit -1
fi

if [ $# -eq 2 ] ; then
    export TOL=$2
else
TOL="rel 0.01"
fi

STATUS=1

# first make an attempt on all regions
while [ X$STATUS != X0 ] ; do

    mged -c $1 <<EOF
tol $TOL
facetize_all_regions $1.fail
EOF
    STATUS=$?

done
echo status $STATUS