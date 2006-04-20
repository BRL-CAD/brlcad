#!/bin/sh

set +

rm -f iges.log iges.g iges_file.iges iges_stdout_new.g iges_new.g iges_stdout.iges iges_file.iges

STATUS=0

../src/mged/mged -c 2> iges.log <<EOF
opendb iges.g y

units mm
size 1000
make box.s arb8
facetize -n box.nmg box.s
kill box.s
q
EOF

../src/conv/iges/g-iges -o iges_file.iges iges.g box.nmg 2>> iges.log> /dev/null
../src/conv/iges/g-iges iges.g box.nmg > iges_stdout.iges 2>> iges.log

../src/conv/iges/iges-g -o iges_new.g -p iges_file.iges 2>> iges.log

if [ $? != 0 ] ; then
    /bin/echo g-iges/iges-g FAILED
    STATUS=-1
else
    /bin/echo g-iges/iges-g completed successfully
fi


../src/conv/iges/iges-g -o iges_stdout_new.g -p iges_stdout.iges 2>> iges.log

if [ $? != 0 ] ; then
    /bin/echo g-iges/iges-g FAILED
    STATUS=-1
else
    /bin/echo g-iges/iges-g completed successfully
fi


if [ X$STATUS = X0 ] ; then
    /bin/echo '-> iges.sh succeeded'
else
    /bin/echo '-> iges.sh failed'
fi


exit $STATUS
