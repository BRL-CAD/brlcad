#!/bin/sh
#                   F L A W F I N D E R . S H
# BRL-CAD
#
# Copyright (c) 2010-2022 United States Government as represented by
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
    LOGFILE=`pwd`/flawfinder.log
    rm -f $LOGFILE
fi
log "=== TESTING flawfinder ==="

if test ! -f "$PATH_TO_THIS/../misc/flawfinder" ; then
    log "Unable to find flawfinder in misc directory, aborting"
    exit 1
fi

if test "x`env python -V 2>&1 | awk '{print $1}'`" != "xPython" ; then
    log "No python available, skipping"
    exit 0
fi

SRCFILES="`find $PATH_TO_THIS/../src -type f \( -name \*.c -o -name \*.cpp -o -name \*.cxx -o -name \*.cc -o -name \*.h -o -name \*.y -o -name \*.l \) -not -regex '.*src/other.*' -not -regex '.*~' -not -regex '.*\.log' -not -regex '.*Makefile.*' -not -regex '.*cache.*' -not -regex '.*\.svn.*' -not -regex '.*src/libbu/realpath_bsd.c.*' -not -regex '.*tinygltf.*'`"
run ${PATH_TO_THIS}/../misc/flawfinder --context --followdotdir --minlevel=5 --singleline --neverignore --falsepositive --quiet ${SRCFILES} | grep -v running

NUMBER_WRONG=0
if test "x`grep \"No hits found.\" $LOGFILE`" = "x" ; then
    NUMBER_WRONG="`grep \"Hits = \" $LOGFILE | awk '{print $3}'`"
fi

if test "x$NUMBER_WRONG" = "x0" ; then
    log "-> flawfinder.sh succeeded"
else
    log "-> flawfinder.sh FAILED, see $LOGFILE"
    cat "$LOGFILE"
fi

exit $NUMBER_WRONG

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
