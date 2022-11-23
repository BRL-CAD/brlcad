#!/bin/sh
#                       U S A G E . S H
# BRL-CAD
#
# Copyright (c) 2018-2022 United States Government as represented by
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
#
# Basic Usage statement sanity check, no long lines
#
###

# Ensure /bin/sh
export PATH || (echo "This isn't sh."; sh $0 $*; kill $$)

# source common library functionality, setting ARGS, NAME_OF_THIS,
# PATH_TO_THIS, and THIS.
. "$1/regress/library.sh"

if test "x$LOGFILE" = "x" ; then
    LOGFILE=`pwd`/usage.log
    rm -f $LOGFILE
fi
log "=== TESTING utility usage statements ==="

# don't pop up a window on the commands that invoke tk
DISPLAY=/dev/null
export DISPLAY

# number of usage tests to run in parallel
NPSW=100
# number of seconds to wait for usage
WAIT=8

RT="`ensearch rt`"
if test ! -f "$RT" ; then
    log "Unable to find rt, aborting"
    exit 1
fi

works="`$RT -h -? 2>&1`"
if test $? -ne 0 ; then
    log "Unable to run rt, aborting"
    exit 1
elif test "x$works" = "x" ; then
    log "Unable to get usage from rt, aborting"
    exit 1
fi

exists="`timeout --version 2>&1 | head -1 | awk '{print $1}'`"
if test "x$exists" != "xtimeout" ; then
    timeout() { perl -e 'alarm shift; exec @ARGV' "$@"; }
fi


log "Testing usage statements ..."


# run a single command, wrapped in a function so we can background it
# and keep track of it.  writes counters to the log as a poor means of
# interprocess communication, as a way to send several pieces of data
# back to the parent despite being backgrounded.
test_usage ( ) {
    cmd=$1

    echo $cmd
    log "=== $cmd === (pid: $$)"

    usage="`timeout ${WAIT} $cmd -h -? 2>&1`"
    log "$usage"

    length=`echo "$usage" | grep -v -i DEPREC | grep -v -i WARN | grep -v -i ERROR | grep -v -i FAIL | awk '{print length, $0}' | sort -nr | head -1 | awk '{print $1}'`
    if test "x$length" = "x" ; then
	length=0
    fi
    lines=`echo "$usage" | wc -l 2>&1`
    if test "x$lines" = "x" ; then
	lines=0
    fi

    # This is only feasible if we're in single-process-at-a-time mode, but if
    # we are see if the execution of the command put out a -? file.  If it
    # did, that means it improperly handled the supplied options.
    if test $NPSW -eq 1 ; then
	QFILE=`pwd`/-?
	if test -f "$QFILE"; then
	    log "ERROR: command $cmd created a -? file"
	    rm -f $QFILE
	fi
    fi

    printf "  %-30s lines:%3d  maxline:%3d\n" "$cmd ..." "$lines" "$length"

    if test "x`echo $usage | grep -i usage`" != "x" ; then
	log "USAGE=\"\`expr \$USAGE + 1\`\" # $cmd"
    else
	log "NOUSE=\"\`expr \$NOUSE + 1\`\" # $cmd"
    fi
    if test $length -gt 80 ; then
	log "LONG=\"\`expr \$LONG + 1\`\" # $cmd"
    elif test $length -lt 8 ; then
	log "SHORT=\"\`expr \$SHORT + 1\`\" # $cmd"
    fi
    if test "x`echo $usage | grep -i DEPREC`" != "x" ; then
	log "DEPREC=\"\`expr \$DEPREC + 1\`\" # $cmd"
    elif test "x`echo $usage | grep -i WARN`" != "x" ; then
	log "WARNED=\"\`expr \$WARNED + 1\`\" # $cmd"
    fi
    if test "x`echo $usage | grep -i ERROR`" != "x" ; then
	log "FAILED=\"\`expr \$FAILED + 1\`\" # $cmd"
    elif test "x`echo $usage | grep -i FAIL`" != "x" ; then
	log "FAILED=\"\`expr \$FAILED + 1\`\" # $cmd"
    fi
    log "CNT=\"\`expr \$CNT + 1\`\" # $cmd"

    # log $length
}


# dark.  wait for all our children to die or kill them ourselves after
# they've lived long enough.
wait_on ( ) {
    pids="$1"
    waited=0
    for pid in $pids ; do
	while test "x`jobs -p | grep $pid`" != "x" ; do
	    if test $waited -gt `expr $WAIT + 2` ; then
		kill -9 $pid >/dev/null 2>&1
		# log "FAILED=\"\`expr \$FAILED + 1\`\" # $pid"
		# log "CNT=\"\`expr \$CNT + 1\`\" # $cmd"
		break
	    fi
	    sleep 1
	    waited="`expr $waited + 1`"
	done
	wait $pid >/dev/null 2>&1
    done
}

# test all commands
CNT=0
LONG=0
SHORT=0
USAGE=0
NOUSE=0
WARNED=0
FAILED=0
DEPREC=0
workers=0
pids=""
dir="`dirname $RT`"
for cmd in $dir/* ; do
    test_usage "$cmd" &

    pids="$pids $!"
    workers="`expr $workers + 1`"
    if ! test $workers -lt $NPSW ; then
	wait_on "$pids"
	pids=""
	workers=0
    fi
done
wait_on "$pids"

# tabulate results from tallies we stored in the log file
vars="`grep -E '(CNT=|LONG=|SHORT=|USAGE=|NOUSE=|WARNED=|FAILED=|DEPREC=)' $LOGFILE`"
while read var ; do
    eval $var # increments our VARIABLES above
done <<EOF
$vars
EOF

# print a summary
log ""
log "COMMAND SUMMARY:"
log ""
log "---------------------------------------------------------------------"
log "| TOTAL | Usage | NoUsage | Long | Short | Deprecated | Warn | Fail |"
log "`printf \"| %5d | %5d | %7d | %4d | %5d | %10d | %4d | %4d |\n\" $CNT $USAGE $NOUSE $LONG $SHORT $DEPREC $WARNED $FAILED`"
log "---------------------------------------------------------------------"


NEED_FIXING=54
if test $LONG -lt `expr $NEED_FIXING + 1` ; then
    if test $LONG -ne $NEED_FIXING ; then
	log "********************************************************"
	log "FIXME: UPDATE THE LONG USAGE COUNT IN $0 - expected $NEED_FIXING, found $LONG"
	log "********************************************************"
    else
	rm -f usage.log
    fi
    log "-> usage check succeeded"
else
    log "-> usage check FAILED, see $LOGFILE"
    cat "$LOGFILE"
fi

exit 0

# Local Variables:
# tab-width: 8
# mode: sh
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
