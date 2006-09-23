#!/bin/sh

if [ $# -ne 1 ] ; then
    echo "Usage: $0 database.g"
    exit -1
fi

STATUS=1

# first make an attempt on all regions
while [ X$STATUS != X0 ] ; do

    mged -c $1 <<EOF
source facetall.mged
facetize_all_regions
q
EOF
    STATUS=$?

done
