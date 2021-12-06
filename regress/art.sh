#!/bin/sh
#                         G - D O T . S H
# BRL-CAD
#
# Copyright (c) 2010-2021 United States Government as represented by
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

# Ensure /bin/sh
export PATH || (echo "This isn't sh."; sh $0 $*; kill $$)

# source common library functionality, setting ARGS, NAME_OF_THIS,
# PATH_TO_THIS, and THIS.
. "$1/regress/library.sh"

if test "x$LOGFILE" = "x" ; then
    LOGFILE=`pwd`/art_output.log
    rm -f $LOGFILE
fi
log "=== TESTING 'art' ==="

MGED="`ensearch mged`"
if test ! -f "$MGED" ; then
    log "Unable to find mged, aborting"
    exit 1
fi

ART="`ensearch art`"
if test ! -f "$ART" ; then
    log "Unable to find art, aborting"
    exit 1
fi

rm -f `pwd`/test.g

# initialize test number
TEST_NO=1

# manually set status before running commands
STATUS=0

# Test 1: Make Sure Art Can Render an Exisitng Item
log "... making sure art renders existing"
$ART -o test.png -c "set samples=0" share/db/moss.g all.g &> $LOGFILE.$TEST_NO
if grep -q 'FAILED\|error' $LOGFILE.$TEST_NO; then STATUS=1; fi
cat $LOGFILE.$TEST_NO >> $LOGFILE
rm $LOGFILE.$TEST_NO
((++TEST_NO))

# Test 2: Intentional fail as 'scene.g' doesnt exist
log "... rainy day - art not called correctly"
$ART -o test.png -c "set samples=0" share/db/moss.g scene.g &> $LOGFILE.$TEST_NO
if ! grep -q 'FAILED\|error' $LOGFILE.$TEST_NO; then STATUS=1; fi
cat $LOGFILE.$TEST_NO >> $LOGFILE
rm $LOGFILE.$TEST_NO
((++TEST_NO))

# Test 3: Make Sure Art Can Renders a New OSL
log "... making sure art renders OSL from material"
$MGED -c test.g >> $LOGFILE 2>&1 << EOF
make sph sph
r sph.r u sph
material create glass glass
material set glass optical OSL as_glass.oso
material assign sph.r glass
q
EOF
$ART -o test.png -c "set samples=0" test.g sph.r &> $LOGFILE.$TEST_NO
if grep -q 'FAILED\|error' $LOGFILE.$TEST_NO; then STATUS=1; fi
cat $LOGFILE.$TEST_NO >> $LOGFILE
rm $LOGFILE.$TEST_NO
((++TEST_NO))

# Test 4: Make Sure Art Can Renders a New OSL
log "... rainy day - bad OSL name from material"
$MGED -c test.g >> $LOGFILE 2>&1 << EOF
material set glass optical OSL doest_exist.oso
q
EOF
$ART -o test.png -c "set samples=0" test.g sph.r &> $LOGFILE.$TEST_NO
if ! grep -q error $LOGFILE.$TEST_NO; then STATUS=1; fi
cat $LOGFILE.$TEST_NO >> $LOGFILE
rm $LOGFILE.$TEST_NO


# cleanup
rm `pwd`/output/test.png

# Test 3: Make Sure Art Can Renders a New MTLX
# log "... making sure art renders new MTLX"
# $MGED -c >> $LOGFILE 2>&1 <<EOF
#   make tor tor
#   r tor.r u tor
#   material create plastic
#   material set plastic optical.shader path/to/adobe.mtlx
#   material set tor.r plastic
# q
# $ART -o test3.pix test.g tor.r
# EOF

if [ X$STATUS = X0 ] ; then
    log "-> material.sh succeeded"
else
    log "-> material.sh FAILED, see $LOGFILE"
    cat "$LOGFILE"
fi

exit $STATUS

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
