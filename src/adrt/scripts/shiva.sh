#!/bin/sh

(igvt_master -p $PWD/$1 -g $PWD/$1/mesh.db -P 54321 -O 12345 || exit) &
sleep 1

#for a in 1 2 3 4
#do
#	/usr/bin/rsh -n n$a 'bin/igvt_slave -H shiva -P 54321 2>/dev/null >/dev/null &'
#done

crun -n 'bin/igvt_slave -H shiva -P 54321'

sleep 1
igvt_observer -H shiva -P 12345
sleep 1
killall igvt_master
crun killall igvt_slave
sleep 1
killall -9 igvt_master
crun killall -9 igvt_slave
