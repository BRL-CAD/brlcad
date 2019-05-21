#!/bin/sh
#                       R T E D G E . S H
# BRL-CAD
#
# Copyright (c) 2019 United States Government as represented by
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
. "`dirname $0`/library.sh"

if test "x$LOGFILE" = "x" ; then
    LOGFILE=`pwd`/rtedge.log
    rm -f $LOGFILE
fi
log "=== TESTING rtedge ==="

RT="`ensearch rt`"
if test ! -f "$RT" ; then
    log "Unable to find rt, aborting"
    exit 1
fi

RTEDGE="`ensearch rtedge`"
if test ! -f "$RTEDGE" ; then
    log "Unable to find rtedge, aborting"
    exit 1
fi

PIXDIFF="`ensearch pixdiff`"
if test ! -f "$PIXDIFF" ; then
    log "Unable to find pixdiff, aborting"
    exit 1
fi

GZIP="`which gzip`"
if test ! -f "$GZIP" ; then
    log "Unable to find gzip, aborting"
    exit 1
fi

FAILURES=0

# make our database
rm -f rtedge.havoc.g
log "... running rtedge.havoc.g gzip decompress"
$GZIP -d -c "$PATH_TO_THIS/tgms/havoc.g.gz" > rtedge.havoc.g

# get our references
rm -f rtedge.ref.pix rtedge.2.ref.pix
log "... running rtedge reference gzip decompressions"
$GZIP -d -c "$PATH_TO_THIS/rtedge.ref.pix.gz" > rtedge.ref.pix
$GZIP -d -c "$PATH_TO_THIS/rtedge.ref2.pix.gz" > rtedge.ref2.pix

cmd="$RTEDGE -s 1024 -o rtedge.pix rtedge.havoc.g havoc"
log "... rendering rtedge #1: $cmd"
rm -f rtedge.pix
$cmd 2>> $LOGFILE

cmd="$PIXDIFF rtedge.pix rtedge.ref.pix"
log "... comparing rtedge #1: $cmd"
rm -f rtedge.diff.pix
$cmd > rtedge.diff.pix 2>> $LOGFILE
NUMBER_WRONG=`tail -n1 $LOGFILE | tr , '\012' | awk '/many/ {print $1}'`

if [ "X$NUMBER_WRONG" = "X0" ] ; then
    log "... -> rtedge.pix is correct"
else
    log "... -> rtedge.pix $NUMBER_WRONG off by many"
    FAILURES="`expr $FAILURES + 1`"
fi


cmd="$RTEDGE -s 1024 -o rtedge.2.pix -c\"set fg=0/255/0 bg=255/0/0\" rtedge.havoc.g havoc"
log "... rendering rtedge #2: $cmd"
rm -f rtedge.2.pix
eval $cmd 2>> $LOGFILE

cmd="$PIXDIFF rtedge.2.pix rtedge.ref2.pix"
log "... comparing rtedge #2: $cmd"
rm -f rtedge.diff2.pix
$cmd > rtedge.diff2.pix 2>> $LOGFILE
NUMBER_WRONG=`tail -n1 $LOGFILE | tr , '\012' | awk '/many/ {print $1}'`

if [ "X$NUMBER_WRONG" = "X0" ] ; then
    log "... -> rtedge.2.pix is correct"
else
    log "... -> rtedge.2.pix $NUMBER_WRONG off by many"
    FAILURES="`expr $FAILURES + 1`"
fi


if test $FAILURES -eq 0 ; then
    log "-> rtcheck check succeeded"
else
    log "-> rtcheck check FAILED, see $LOGFILE"
fi

exit $FAILURES

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
