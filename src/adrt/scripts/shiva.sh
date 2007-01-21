#!/bin/sh
#                        S H I V A . S H
# BRL-CAD
#
# Copyright (c) 2007 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#
###
#
# Author -
#  Justin Shumaker
#
###

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

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
