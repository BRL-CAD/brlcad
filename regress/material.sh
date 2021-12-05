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
    LOGFILE=`pwd`/g-dot.log
    rm -f $LOGFILE
fi
log "=== TESTING 'art' ==="

MGED="`ensearch mged`"
if test ! -f "$MGED" ; then
    log "Unable to find mged, aborting"
    exit 1
fi

# Test 1: Make Sure Art Can Render an Exisitng Item
log "... making sure art renders existing"
<< EOF
art -o test1.pix share/db/moss.g all.g
EOF

# Test 2: Make Sure Art Can Renders a New OSL
log "... making sure art renders new OSL"
$MGED -c >> $LOGFILE 2>&1 << EOF
test.g make sph sph
test.g r sph.r u sph
test.g material create glass
test.g material set glass optical.shader path/to/disney_shader.osl
test.g material set glass optical.shader_properties ... transparency 0.8
test.g material set sph.r glass
art -o test2.pix test.g sph.r
q
EOF

# Test 3: Make Sure Art Can Renders a New MTLX
log "... making sure art renders new MTLX"
$MGED -c >> $LOGFILE 2>&1 <<EOF
test.g make tor tor
test.g r tor.r u tor
test.g material create plastic
test.g material set plastic optical.shader path/to/adobe.mtlx
test.g material set tor.r plastic
art -o test3.pix test.g tor.r
q
EOF

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
