#!/bin/sh
#                   F L A W F I N D E R . S H
# BRL-CAD
#
# Copyright (c) 2010-2011 United States Government as represented by
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

TOPSRC="$1"
if test "x$TOPSRC" = "x" ; then
    TOPSRC="."
fi
if test ! -f "$TOPSRC/misc/flawfinder" ; then
    echo "Unable to find flawfinder in misc directory, aborting"
    exit 1
fi

HAVE_PYTHON=no
if test "`env python -V 2>&1 | awk '{print $1}'`" = "xPython" ; then
    HAVE_PYTHON=yes
fi

if test "x$HAVE_PYTHON" = "x" ; then

    echo "running flawfinder..."

    rm -f flawfinder.log
    ${TOPSRC}/misc/flawfinder --followdotdir --minlevel=5 --singleline --neverignore --falsepositive --quiet ${TOPSRC}/src/[^o]* > flawfinder.log 2>&1

    NUMBER_WRONG=0
    if test "x`grep \"No hits found.\" flawfinder.log`" = "x" ; then
	NUMBER_WRONG="`grep \"Hits = \" flawfinder.log | awk '{print $3}'`"
    fi

    if test "x$NUMBER_WRONG" = "x0" ; then
	echo "-> flawfinder.sh succeeded"
    else
	if test -f flawfinder.log ; then
	    cat flawfinder.log
	fi
	echo "-> flawfinder.sh FAILED"
    fi

fi # HAVE_PYTHON


exit $NUMBER_WRONG

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
