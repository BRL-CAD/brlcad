#!/bin/sh
#                         G - N F F . S H
# BRL-CAD
#
# Copyright (c) 2010-2014 United States Government as represented by
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

MGED="`ensearch mged`"
if test ! -f "$MGED" ; then
    echo "Unable to find mged, aborting"
    exit 1
fi
ASC2G="`ensearch asc2g`"
if test ! -f "$ASC2G" ; then
    echo "Unable to find asc2g, aborting"
    exit 1
fi
GZIP="`which gzip`"
if test ! -f "$GZIP" ; then
    echo "Unable to find gzip, aborting"
    exit 1
fi
GNFF="`ensearch g-nff`"
if test ! -f "$GNFF" ; then
    echo "Unable to find 'g-nff', aborting"
    exit 1
fi

FILS='g-nff.err g-nff.log g-nff.m35.nff g-nff.m35.asc g-nff.m35.g'

rm -f $FILS

# get known test failure.g file
$GZIP -d -c "$1/regress/tgms/m35.asc.gz" > g-nff.m35.asc
$ASC2G g-nff.m35.asc g-nff.m35.g

# .g to nff:
$GNFF -e g-nff.err -o g-nff.m35.nff g-nff.m35.g r516 2> g-nff.log 1>/dev/null
STATUS=$?

if [ X$STATUS != X0 ] ; then
    echo g-nff FAILED
else
    echo g-nff completed successfully
fi


if [ X$STATUS = X0 ] ; then
    echo "-> g-nff.sh succeeded"
else
    echo "-> g-nff.sh FAILED"
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
