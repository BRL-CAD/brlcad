#!/bin/sh
#                     F A C E T A L L . S H
# BRL-CAD
#
# Copyright (c) 2007-2024 United States Government as represented by
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
# This script uses facetize -r to attempt to facetize all regions in a geometry
# file, resuming the process if MGED crashes during conversion.
#
###

export IGNOREEOF=10
if [ $# -lt 2 -o $# -gt 3 ] ; then
    echo "Usage: $0 database.g top_object [tol]"
    exit -1
fi

if [ $# -eq 2 ] ; then
    export TOL=$2
else
    TOL="rel 0.01"
fi

# check for mged in path
if ! command -v mged
then
    echo "mged not found; make sure it is in the path"
    exit -1
fi

mged -c $1 <<EOF
tol $TOL
facetize -r $2 $2.bot
EOF
STATUS=$?

# first make an attempt on all regions
while [ X$STATUS != X0 ] ; do

    mged -c $1 <<EOF
    tol $TOL
    facetize -r --resume $2 $2.bot
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
