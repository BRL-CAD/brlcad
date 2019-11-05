#!/bin/sh
#                         I G E S . S H
# BRL-CAD
#
# Copyright (c) 2010-2019 United States Government as represented by
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
    LOGFILE=`pwd`/iges.log
    rm -f "$LOGFILE"
fi
log "=== TESTING iges conversion ==="

MGED="`ensearch mged`"
if test ! -f "$MGED" ; then
    log "Unable to find mged, aborting"
    exit 1
fi

GIGES="`ensearch g-iges`"
if test ! -f "$MGED" ; then
    log "Unable to find g-iges, aborting"
    exit 1
fi

IGESG="`ensearch iges-g`"
if test ! -f "$MGED" ; then
    log "Unable to find iges-g, aborting"
    exit 1
fi


STATUS=0

# CREATE G

output=iges.g
rm -f "$output"
log "... running mged to create facetized geometry ($output)"
$MGED -c >> "$LOGFILE" 2>&1 <<EOF
opendb $output y

units mm
size 1000
make box.s arb8
facetize -n box.nmg box.s
kill box.s
q
EOF
if [ ! -f "$output" ] ; then
    log "ERROR: mged failed to create $output"
    log "-> iges.sh FAILED, see $LOGFILE"
    exit 1
fi

# G TO IGES

# test G -> IGES via -o
output="iges.export.iges"
rm -f "$output"
run $GIGES -o "$output" iges.g box.nmg
if [ ! -f "$output" ] ; then
    log "ERROR: g-iges failed to create $output"
    log "-> iges.sh FAILED, see $LOGFILE"
    exit 1
fi

# test G -> IGES via stdout (can't use 'run')
output="iges.export.stdout.iges"
rm -f "$output"
log "... running $GIGES iges.g box.nmg > $output"
$GIGES iges.g box.nmg > $output 2>> "$LOGFILE"
if [ ! -f "$output" ] ; then
    log "ERROR: g-iges failed to create $output"
    log "-> iges.sh FAILED, see $LOGFILE"
    exit 1
fi

# G TO IGES TO G

# test IGES -> G
output="iges.import.g"
rm -f "$output"
run $IGESG -o "$output" -p -N box.nmg iges.export.iges
if [ ! -f "$output" ] ; then
    log "ERROR: iges-g failed to create $output"
    log "-> iges.sh FAILED, see $LOGFILE"
    exit 1
fi

# G TO IGES TO G TO IGES (ROUND TRIP)

# make sure we don't permute vertices or introduce some other
# unintended change.

# test G -> IGES #2 via -o
output="iges.export2.iges"
rm -f "$output"
run $GIGES -o "$output" iges.import.g box.nmg
if [ ! -f "$output" ] ; then
    log "ERROR: g-iges failed to create $output"
    log "-> iges.sh FAILED, see $LOGFILE"
    exit 1
fi

# test G -> IGES #2 via stdout (can't use 'run')
output="iges.export2.stdout.iges"
$GIGES iges.import.g box.nmg > "$output" 2>> "$LOGFILE"
if [ ! -f "$output" ] ; then
    log "ERROR: g-iges failed to create $output"
    log "-> iges.sh FAILED, see $LOGFILE"
    exit 1
fi

# FIXME: test that these two files are identical
#    iges.export.iges
#    iges.export2.iges

# FIXME: test that these two files are identical
#    iges.export.stdout.iges
#    iges.export2.stdout.iges

# test IGES -> G #3 with -p (output nmg, not bot)
run $IGESG -o iges.export3.stdout.g -p iges.export.stdout.iges
if [ $? != 0 ] ; then
    log "...iges-g (2) FAILED, see $LOGFILE"
    STATUS=1
else
    log "...iges-g (2) succeeded"
fi

# check one other TGM known to have a conversion failure which should be graceful
ASC2G="`ensearch asc2g`"
if test ! -f "$ASC2G" ; then
    log "Unable to find asc2g, aborting"
    exit 1
fi
GZIP="`which gzip`"
if test ! -f "$GZIP" ; then
    log "Unable to find gzip, aborting"
    exit 1
fi

# make our starting database
$GZIP -d -c "$PATH_TO_THIS/tgms/m35.asc.gz" > iges.m35.asc
$ASC2G iges.m35.asc iges.m35.g

# and test it (note it should work with the '-f' option, but fail
# without any options)
$GIGES -f -o iges_file3.iges iges.m35.g r516 2>> iges.log > /dev/null
STATUS=$?


if [ X$STATUS = X0 ] ; then
    log "-> iges.sh succeeded"
else
    log "-> iges.sh FAILED, see $LOGFILE"
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
