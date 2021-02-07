#!/bin/sh
#                         I G E S . S H
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
    cat "$LOGFILE"
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
    cat "$LOGFILE"
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
    cat "$LOGFILE"
    exit 1
fi

# test that the first g-iges -o output matches the stdout output
files_match iges.export.iges iges.export.stdout.iges -I 'G'
if test $? -ne 0 ; then
    STATUS="`expr $STATUS + 1`"
    export STATUS
fi

# G TO IGES TO G

# test IGES -> G
output="iges.import.g"
rm -f "$output"
run $IGESG -o "$output" iges.export.iges
if [ ! -f "$output" ] ; then
    log "ERROR: iges-g failed to create $output"
    log "-> iges.sh FAILED, see $LOGFILE"
    cat "$LOGFILE"
    exit 1
fi

# test IGES -> G (with -p)
output="iges.import2.g"
rm -f "$output"
run $IGESG -o "$output" -p iges.export.iges
if [ ! -f "$output" ] ; then
    log "ERROR: iges-g failed to create $output"
    log "-> iges.sh FAILED, see $LOGFILE"
    cat "$LOGFILE"
    exit 1
fi

# test IGES -> G (with -p -N name)
output="iges.import3.g"
rm -f "$output"
run $IGESG -o "$output" -p -N box.nmg iges.export.iges
if [ ! -f "$output" ] ; then
    log "ERROR: iges-g failed to create $output"
    log "-> iges.sh FAILED, see $LOGFILE"
    cat "$LOGFILE"
    exit 1
fi

# test that these produced different output
files_differ iges.import.g iges.import2.g
if test $? -ne 0 ; then
    STATUS="`expr $STATUS + 1`"
    export STATUS
fi
# FIXME: these should match but -N is creating 'box.nmgA'
# files_match iges.import2.g iges.import3.g
# if test $? -ne 0 ; then
#     STATUS="`expr $STATUS + 1`"
#     export STATUS
# fi
# FIXME: these should match but -N is creating 'box.nmgA'
# files_match iges.import.g iges.import3.g
# if test $? -ne 0 ; then
#     STATUS="`expr $STATUS + 1`"
#     export STATUS
# fi

# G TO IGES TO G TO IGES (ROUND TRIP)

# make sure we don't permute vertices or introduce some other
# unintended change.

# test G -> IGES #2a via -o
output="iges.import.export.iges"
rm -f "$output"
run $GIGES -o "$output" iges.import.g box.nmg
if [ ! -f "$output" ] ; then
    log "ERROR: g-iges failed to create $output"
    log "-> iges.sh FAILED, see $LOGFILE"
    cat "$LOGFILE"
    exit 1
fi

# test G -> IGES #2b via -o
output="iges.import2.export.iges"
rm -f "$output"
run $GIGES -o "$output" iges.import2.g box.nmg
if [ ! -f "$output" ] ; then
    log "ERROR: g-iges failed to create $output"
    log "-> iges.sh FAILED, see $LOGFILE"
    cat "$LOGFILE"
    exit 1
fi

# test G -> IGES #2c via -o
output="iges.import3.export.iges"
rm -f "$output"
run $GIGES -o "$output" iges.import3.g box.nmg
if [ ! -f "$output" ] ; then
    log "ERROR: g-iges failed to create $output"
    log "-> iges.sh FAILED, see $LOGFILE"
    cat "$LOGFILE"
    exit 1
fi

# COMPARE RESULTS

# test that initial g-iges output does NOT match final BoT output
files_differ iges.export.iges iges.import.export.iges -I 'G'
if test $? -ne 0 ; then
    STATUS="`expr $STATUS + 1`"
    export STATUS
fi

# FIXME: these should match but vertices are permuted!
# test that initial g-iges output DOES match final NMG output
# files_match iges.export.iges iges.import2.export.iges -I 'G'
# if test $? -ne 0 ; then
#     STATUS="`expr $STATUS + 1`"
#     export STATUS
# fi

# test that initial g-iges output DOES match final named NMG output
# FIXME: these should match but iges-g -N is creating 'box.nmgA'
# files_match iges.export.iges iges.import3.export.iges -I 'G'
# if test $? -ne 0 ; then
#     STATUS="`expr $STATUS + 1`"
#     export STATUS
# fi

# COMPLEX TEST

# check another TGM known to have a conversion failure which should be graceful
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
$GZIP -d -c "$PATH_TO_THIS/m35.asc.gz" > iges.m35.asc
$ASC2G iges.m35.asc iges.m35.g

# and test it (note it should work with the '-f' option, but fail
# without any options)
run $GIGES -f -o iges.m35.r516.iges iges.m35.g r516
if test $? -ne 0 ; then
    STATUS="`expr $STATUS + 1`"
    export STATUS
fi

# TODO: add full m35 conversion test
# run $GIGES -f -o iges.m35.iges iges.m35.g component
# if test $? -ne 0 ; then
#     STATUS="`expr $STATUS + 1`"
#     export STATUS
# fi

if [ X$STATUS = X0 ] ; then
    log "-> iges.sh succeeded"
else
    log "-> iges.sh FAILED, see $LOGFILE"
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
