#!/bin/sh
#                     F A C E T A L L . S H
# BRL-CAD
#
# Copyright (c) 2007-2011 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following
# disclaimer in the documentation and/or other materials provided
# with the distribution.
#
# 3. The name of the author may not be used to endorse or promote
# products derived from this software without specific prior written
# permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
###
#
# This script iterates over all regions in a geometry file and
# attempts to facetize them all.  The following scripts can then
#
# Replaces facetall.sh conversions assuming file didn't already
# contain bot objects:
#
# file=file.g ; for i in `mged -c $file search . -type bot 2>&1` ; do base=`echo $i|sed 's/.bot//g'` ; mged -c $file killtree $base \; mv $i $base ; done
#
# Great for calculating number of polygons in file:
#
# file=file.g ; total=0 ; for i in `mged -c $file search . -type bot 2>&1` ; do cnt=`mged -c $file l $i 2>&1 | tail -1 | awk '{print $2}' | tr -d :` ; total="`expr $cnt + $total`" ; echo "$total (+ $cnt)" ; done
#
# This script could use some TLC.
#
###

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

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
