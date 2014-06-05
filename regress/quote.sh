#!/bin/sh
#                          Q U O T E . S H
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
# for debugging uncomment the following:
#set -x
# Ensure /bin/sh
export PATH || (echo "This isn't sh."; sh $0 $*; kill $$)

# source common library functionality, setting ARGS, NAME_OF_THIS,
# PATH_TO_THIS, and THIS.
. "$1/regress/library.sh"

TEST=quote
TESTSCRIPT=$TEST.sh
TESTEXE=test_quote
TESTLOG=${TEST}.log
TESTCMD="`ensearch $TESTEXE`"
if test ! -f "$TESTCMD" ; then
    echo "Unable to find $TESTEXE, aborting"
    exit 1
fi

# another file for test output
ERRLOG=quote_test.stderr

rm -f $TESTLOG $ERRLOG

# the known number of expected failures is:
KNOWNEXP=0
# run the test and capture exit status
`$TESTCMD 1> $TESTLOG 2>$ERRLOG`
STATUS=$?
# STATUS contains number of UNEXPECTED failures
# get the failure numbers
if [ -f $ERRLOG ] ; then
  EXP=`cat $ERRLOG`
else
  EXP=0
fi
if [ $STATUS -ge 128 ] ; then
    # a signal received
    SIGNUM=`expr $STATUS - 128`
    echo "-> $TESTSCRIPT FAILED with signal $SIGNUM."
    if [ $SIGNUM -eq 11 ] ; then
	echo "       (SIGSEGV, invalid memory reference)"
    fi
    echo "     See file './regress/$TESTLOG' for results (may be empty with a code dump)."
    # allow the rest of the test to continue
    STATUS=`expr $STATUS - $SIGNUM - 128`
fi

# FIXME: this if/else block can be more efficient
if [ $STATUS -eq 0 ] ; then
    if [ $KNOWNEXP -ne 0 ] ; then
	if [ $EXP -eq $KNOWNEXP ] ; then
	    echo "-> $TESTSCRIPT succeeded with $EXP expected failed test(s)."
	    echo "     See file './regress/$TESTLOG' for results."
	    echo "   Do NOT use the failures in production code."
	fi
    elif [ $EXP -ne 0 ] ; then
	echo "-> $TESTSCRIPT succeeded with $EXP expected failed test(s)."
	echo "     But SURPRISE!  We expected $KNOWNEXP failed tests so something has changed!"
	echo "     See file './regress/$TESTLOG' for results and compare"
	echo "       with file './src/libbu/test_quote.c'."
	echo "   Do NOT use the failures in production code."
    else
	echo "-> $TESTSCRIPT succeeded with no failures of any kind."
	# but one more check
	if [ $KNOWNEXP -ne 0 ] ; then
	    echo "     But SURPRISE!  We expected $KNOWNEXP failed tests so something has changed!"
	    echo "     See file './regress/$TESTLOG' for results and compare"
	    echo "       with file './src/libbu/test_quote.c'."
	else
	    # remove test products since all appears well
	    # rm -f $TESTLOG $ERRLOG
	    touch -f $TESTLOG $ERRLOG
	fi
    fi
else
    echo "-> $TESTSCRIPT unexpectedly FAILED $STATUS test(s)."
    echo "   See file './regress/$TESTLOG' for results."
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
