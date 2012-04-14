#!/bin/sh
#                     C O M G E O M . S H
# BRL-CAD
#
# Copyright (c) 2008-2012 United States Government as represented by
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
# Test conversion from .g to GIFT (v5) and back.
#
###

# Ensure /bin/sh
export PATH || (echo "This isn't sh."; sh $0 $*; kill $$)

# source common library functionality, setting ARGS, NAME_OF_THIS,
# PATH_TO_THIS, and THIS.
. "$1/regress/library.sh"
ASC2G="`ensearch asc2g`"
if test ! -f "$ASC2G" ; then
    echo "Unable to find asc2g, aborting"
    exit 1
fi
VDECK="`ensearch vdeck`"
if test ! -f "$VDECK" ; then
    echo "Unable to find vdeck, aborting"
    exit 1
fi
COMGEOM="`ensearch comgeom-g`"
if test ! -f "$COMGEOM" ; then
    echo "Unable to find comgeom-g, aborting"
    exit 1
fi
GZIP="`which gzip`"
if test ! -f "$GZIP" ; then
    echo "Unable to find gzip, aborting"
    exit 1
fi

FAILURES=0

TFILS='vdeck.log m35.g m35-baseline.cg m35.cg comgeom-g.log solids regions region_ids'

echo "...testing 'vdeck' command..."

rm -f $TFILS

# make our starting database
$GZIP -d -c $1/regress/tgms/m35.asc.gz > m35.asc
$ASC2G m35.asc m35.g

# using vdeck interactively to convert .g to GIFT
#(following example in red.sh and mged test)
$VDECK m35.g >> vdeck.log 2>&1 <<EOF
i all.g
d
q
EOF

# assemble pieces to compare with test version
cat solids     >  m35.cg
cat regions    >> m35.cg
cat region_ids >> m35.cg

# get test version
$GZIP -d -c $1/regress/tgms/m35.cg.gz > m35-baseline.cg

cmp m35.cg m35-baseline.cg
STATUS=$?

if [ X$STATUS != X0 ] ; then
    echo "vdeck results differ $STATUS"
    FAILURES="`expr $FAILURES + 1`"
    export FAILURES
else
    echo "vdeck test succeeded (1 of 2)"
fi

# the part 2 test is simple for now--just convert back and check
# for a successful exit
echo "...testing 'comgeom-g' command..."
$COMGEOM m35.cg t.g 1>comgeom-g.log 2> comgeom-g.log
STATUS=$?

if [ X$STATUS != X0 ] ; then
    echo "comgeom-g converted errors: $STATUS"
    FAILURES="`expr $FAILURES + 1`"
    export FAILURES
else
    echo "comgeom-g test succeeded (2 of 2)"
fi

if test $FAILURES -eq 0 ; then
    echo "-> vdeck/comgeom-g check succeeded"
    # clean up
    rm $TFILS
else
    echo "-> vdeck/comgeom-g check FAILED"
fi

exit $FAILED


# Local Variables:
# tab-width: 8
# mode: sh
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
