#!/bin/sh
#                       R T E D G E . S H
# BRL-CAD
#
# Copyright (c) 2019-2021 United States Government as represented by
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
    LOGFILE="`pwd`/rtedge.log"
    rm -f "$LOGFILE"
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
$GZIP -d -c "$PATH_TO_THIS/havoc.g.gz" > rtedge.havoc.g

# get our references
rm -f rtedge.ref.pix
rm -f rtedge.ref2.pix
rm -f rtedge.ref3.pix
rm -f rtedge.ref4.pix
rm -f rtedge.ref5.pix

log "... running rtedge reference gzip decompressions"
$GZIP -d -c "$PATH_TO_THIS/rtedge.ref.pix.gz" > rtedge.ref.pix 2>> "$LOGFILE"
$GZIP -d -c "$PATH_TO_THIS/rtedge.ref2.pix.gz" > rtedge.ref2.pix 2>> "$LOGFILE"
$GZIP -d -c "$PATH_TO_THIS/rtedge.ref3.pix.gz" > rtedge.ref3.pix 2>> "$LOGFILE"
$GZIP -d -c "$PATH_TO_THIS/rtedge.ref4.pix.gz" > rtedge.ref4.pix 2>> "$LOGFILE"
$GZIP -d -c "$PATH_TO_THIS/rtedge.ref5.pix.gz" > rtedge.ref5.pix 2>> "$LOGFILE"


# === #1 ===
rm -f rtedge.pix
rm -f rtedge.diff.pix

cmd='echo $RTEDGE -s 1024 -o rtedge.pix rtedge.havoc.g havoc'
log "... rendering rtedge #1: `eval $cmd`"
`eval $cmd` 2>> "$LOGFILE"

cmd='echo $PIXDIFF rtedge.pix rtedge.ref.pix'
log "... comparing rtedge #1: `eval $cmd`"
`eval $cmd` > rtedge.diff.pix 2>> "$LOGFILE"
NUMBER_WRONG=`tail -n1 "$LOGFILE" | tr , '\012' | awk '/many/ {print $1}'`

if [ "X$NUMBER_WRONG" = "X0" ] ; then
    log "... -> rtedge.pix is correct"
else
    log "... -> rtedge.pix $NUMBER_WRONG off by many"
    FAILURES="`expr $FAILURES + 1`"
fi


# === #2 ===
rm -f rtedge.2.pix
rm -f rtedge.diff2.pix

cmd="$RTEDGE -s 1024 -o rtedge.2.pix -c \"set fg=0/255/0 bg=255/0/0\" rtedge.havoc.g havoc"
log "... rendering rtedge #2: $cmd"
eval "$cmd" 2>> "$LOGFILE"

cmd="$PIXDIFF rtedge.2.pix rtedge.ref2.pix"
log "... comparing rtedge #2: $cmd"
eval "$cmd" > rtedge.diff2.pix 2>> "$LOGFILE"
NUMBER_WRONG=`tail -n1 "$LOGFILE" | tr , '\012' | awk '/many/ {print $1}'`

if [ "X$NUMBER_WRONG" = "X0" ] ; then
    log "... -> rtedge.2.pix is correct"
else
    log "... -> rtedge.2.pix $NUMBER_WRONG off by many"
    FAILURES="`expr $FAILURES + 1`"
fi


# === #3 ===
rm -f rtedge.3.pix
rm -f rtedge.diff3.pix

cmd="$RTEDGE -s 1024 -o rtedge.3.pix -c\"set rc=1 dr=1\" rtedge.havoc.g havoc"
log "... rendering rtedge #3: $cmd"
eval "$cmd" 2>> "$LOGFILE"

cmd="$PIXDIFF rtedge.3.pix rtedge.ref3.pix"
log "... comparing rtedge #3: $cmd"
eval "$cmd" > rtedge.diff3.pix 2>> "$LOGFILE"
NUMBER_WRONG=`tail -n1 "$LOGFILE" | tr , '\012' | awk '/many/ {print $1}'`

if [ "X$NUMBER_WRONG" = "X0" ] ; then
    log "... -> rtedge.3.pix is correct"
else
    log "... -> rtedge.3.pix $NUMBER_WRONG off by many"
    FAILURES="`expr $FAILURES + 1`"
fi


# === #4 ===
rm -f rtedge.4.pix
rm -f rtedge.diff4.pix

view="
  viewsize 8.000e+03;
  orientation 2.4809e-01 4.7650e-01 7.4809e-01 3.8943e-01;
  eye_pt 2.2146e+04 7.1103e+03 7.1913e+03;
  start 0; clean;
  end;
"

cmd="$RT -B -M -F rtedge.4.pix -C255/255/255 rtedge.havoc.g havoc"
log "... rendering rt #4: $cmd"
eval "$cmd" 2>> "$LOGFILE" <<EOF
$view
EOF

cmd="$RTEDGE -B -M -F rtedge.4.pix -c\"set dr=1 dn=1 ov=1\" -c\"set fg=255,200,0\" rtedge.havoc.g havoc"
log "... rendering rtedge #4: $cmd"
eval "$cmd" 2>> "$LOGFILE" <<EOF
$view
EOF

cmd="$PIXDIFF rtedge.4.pix rtedge.ref4.pix"
log "... comparing rtedge #4: $cmd"
eval "$cmd" > rtedge.diff4.pix 2>> "$LOGFILE"
NUMBER_WRONG=`tail -n1 "$LOGFILE" | tr , '\012' | awk '/many/ {print $1}'`

if [ "X$NUMBER_WRONG" = "X0" ] ; then
    log "... -> rtedge.4.pix is correct"
else
    log "... -> rtedge.4.pix $NUMBER_WRONG off by many"
    FAILURES="`expr $FAILURES + 1`"
fi


# === #5 ===
rm -f rtedge.5.pix
rm -f rtedge.diff5.pix

view="
  viewsize 8.000e+03;
  orientation 2.4809e-01 4.7650e-01 7.4809e-01 3.8943e-01;
  eye_pt 2.2146e+04 7.1103e+03 7.1913e+03;
  start 0; clean;
  end;
"

cmd="$RT -B -M -F rtedge.5.pix -C255/255/255 rtedge.havoc.g weapons"
log "... rendering rt #5: $cmd"
eval "$cmd" 2>> "$LOGFILE" <<EOF
$view
EOF

cmd="$RTEDGE -B -M -F rtedge.5.pix -c\"set dr=1 dn=1 om=3\" -c\"set fg=0,0,0 bg=200,200,200\" -c\"set oo=\\\"weapons\\\" \" rtedge.havoc.g havoc_front havoc_middle havoc_tail landing_gear main_rotor"
log "... rendering rtedge #5: $cmd"
eval "$cmd" 2>> "$LOGFILE" <<EOF
$view
EOF

cmd="$PIXDIFF rtedge.5.pix rtedge.ref5.pix"
log "... comparing rtedge #5: $cmd"
eval "$cmd" > rtedge.diff5.pix 2>> "$LOGFILE"
NUMBER_WRONG=`tail -n1 "$LOGFILE" | tr , '\012' | awk '/many/ {print $1}'`

if [ "X$NUMBER_WRONG" = "X0" ] ; then
    log "... -> rtedge.5.pix is correct"
else
    log "... -> rtedge.5.pix $NUMBER_WRONG off by many"
    FAILURES="`expr $FAILURES + 1`"
fi


# === Summary ===

if test $FAILURES -eq 0 ; then
    log "-> rtcheck check succeeded"
else
    log "-> rtcheck check FAILED, see $LOGFILE"
    cat "$LOGFILE"
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
