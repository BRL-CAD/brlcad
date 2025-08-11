#!/bin/bash
#              U N C P O W E R P L A N T 2 G . S H
# BRL-CAD
#
# Copyright (c) 2014-2025 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# version 2.1 as published by the Free Software Foundation.
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
# Script to convert the UNC powerplant model into a BRL-CAD .g file
# with hierarchy.  Powerplant is retrieved from:
#
#   http://gamma.cs.unc.edu/POWERPLANT/complete.html
#

if test -e powerplant.g ; then
    echo "ERROR: powerplant.g already exists. Delete to procede. Refusing to overwrite."
    exit 1
fi

ppdirs="`ls -1d ppsection* 2>/dev/null | wc | awk '{print $1}'`"
if [ $ppdirs -eq 0 ] ; then
    if ! test -f complete.tar.gz ; then
	echo "NOTICE: Downloading the powerplant model..."
	curl -O http://gamma.cs.unc.edu/POWERPLANT/unix/complete.tar.gz
    fi
    if ! test -f complete.tar.gz ; then
	echo "ERROR: Download seemingly failed, aborting."
	exit 2
    fi
    echo "NOTICE: Unpacking the powerplant model..."
    tar zxvf complete.tar.gz
fi

# sanity double-check
ppdirs="`ls -1d ppsection* 2>/dev/null | wc | awk '{print $1}'`"
if [ $ppdirs -eq 0 ] ; then
    echo "ERROR: Unpacking seemingly failed, aborting."
    exit 3
fi

# make sure we got mged, assuming ply-g if we have it
rm -f powerplant.g
output="`mged -c powerplant.g quit 2>&1`"
if test $? != 0 ; then
    echo "Output: $output"
    echo "ERROR: Unable to run mged, check PATH. Aborting."
    exit 4
fi

# let's go
for i in {1..30} ; do
    for j in {a..z} ; do
	if [ -e ppsection$i/part_$j ] ;	then
	    for k in {0..200} ; do
		if [ -e ppsection$i/part_$j/g$k.ply ] ; then
		    echo "== Converting ppsection$i/part_$j/g$k.ply =="
		    rm -f part_$i-$j-$k.g
		    ply-g ppsection$i/part_$j/g$k.ply part_$i-$j-$k.g
		    mged part_$i-$j-$k.g mvall g$k.bot.r part_$i-$j-$k.r
		    mged part_$i-$j-$k.g mvall g$k.bot part_$i-$j-$k.bot
		    mged powerplant.g dbconcat part_$i-$j-$k.g
		    mged powerplant.g g part_$i-$j part_$i-$j-$k.r
		    rm -f part_$i-$j-$k.g
		fi
	    done
	    mged powerplant.g g section_$i part_$i-$j
	fi
    done
    mged powerplant.g g powerplant section_$i
done

echo "Done, see powerplant.g"


# Local Variables:
# tab-width: 8
# mode: sh
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
