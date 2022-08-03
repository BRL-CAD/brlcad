#!/bin/sh
#                       M G E D . S H
# BRL-CAD
#
# Copyright (c) 2008-2022 United States Government as represented by
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
# Basic series of MGED sanity checks
#
###

# Ensure /bin/sh
export PATH || (echo "This isn't sh."; sh $0 $*; kill $$)

# source common library functionality, setting ARGS, NAME_OF_THIS,
# PATH_TO_THIS, and THIS.
. "$1/regress/library.sh"

# log output to this file
if test "x$LOGFILE" = "x" ; then
    LOGFILE=`pwd`/mged.log
    rm -f $LOGFILE
fi
log "=== TESTING mged commands ==="

# don't pop up a window on the commands that invoke tk
DISPLAY=/dev/null
export DISPLAY

MGED="`ensearch mged`"
if test ! -f "$MGED" ; then
    log "Unable to find mged, aborting"
    exit 1
fi

# run this many commands in parallel
NPSW=64


# test a single command, return 0 if successful
check_command ( ) {
    cmd="$1"

    # make sure command exists and will run without error
    output="`$MGED -c mged.g $cmd 2>&1`"
    if test $? != 0 ; then
	log "ERROR: $cmd returned non-zero exit status $?"
	log "Output: $output"
	return 1
    fi
    if test "x`echo \"$output\" | grep -i invalid`" != "x" ; then
	log "ERROR: $cmd does not exist!"
	log "Output: $output"
	return 1
    fi
    if test "x`echo \"$output\" | grep -i error | grep -i -v _error | grep -i -v error_`" != "x" ; then
	log "ERROR: $cmd reported an error on default use"
	log "Output: $output"
	return 1
    fi

    # make sure command has help listed
    output="`$MGED -c mged.g help $cmd 2>&1`"
    if test "x`echo \"$output\" | grep -i 'no help found'`" != "x" ; then
	log "ERROR: $cmd does not have help"
	return 1
    fi

    return 0
}

log "setting up an almost empty database (mged.g) to make sure mged runs"
rm -f mged.g
$MGED -c >> $LOGFILE 2>&1 <<EOF
opendb mged.g y
in t.s sph 0 0 0 1
r t.r u t.s
g all t.r
quit
EOF
if test ! -f mged.g ; then
    cat "$LOGFILE"
    log "ERROR: Test file 'mged.g' is missing. Unable to run mged, aborting"
    exit 1
fi

log "testing mged commands..."

# collect all current commands
cmds="`$MGED -c mged.g '?' 2>&1 | grep -v Using`"
help="`$MGED -c mged.g help 2>&1 | grep -v Using`"
# cmds="$cmds `$MGED -c mged.g ?lib 2>&1`"
# cmds="$cmds `$MGED -c mged.g ?devel 2>&1`"

# turn off globbing as some of mged's commands have glob characters
set -f

# test all commands
FAILED=0
workers=0
pids=""
for cmd in $cmds ; do
    log "...$cmd"

    # BEGIN SPECIALIZATIONS
    # FIXME: there should be NO specializations
    if test "x$cmd" = "x%" ; then
	# % is special because it invokes a shell
	$MGED -c mged.g $cmd > /dev/null 2>&1 <<EOF
exit
EOF
	if test $? != 0 ; then
	    log "ERROR: $cmd returned non-zero exit status $?"
	    FAILED="`expr $FAILED + 1`"
	fi
	continue
    elif test "x$cmd" = "xedcolor" ; then
	# edcolor it kicks off an editor, ugh
	log "FIXME: Unable to test edcolor"
	log "It probably shouldn't kick off an editor without an argument"
	continue
    elif test "x$cmd" = "xgraph" ; then
	continue
    elif test "x$cmd" = "xigraph" ; then
	continue
    fi
    # END OF SPECIALIZATIONS

    check_command "$cmd" &
    pids="$pids $!"

    workers="`expr $workers + 1`"
    if ! test $workers -lt $NPSW ; then
	for pid in $pids ; do
	    wait $pid
	    if test $? != 0 ; then
		FAILED="`expr $FAILED + 1`"
	    fi
	done
	pids=""
	workers=0
    fi
done


if test $FAILED -eq 0 ; then
    log "-> mged check succeeded"
else
    log "-> mged check FAILED, see $LOGFILE"
    cat "$LOGFILE"
fi

exit $FAILED


# Local Variables:
# tab-width: 8
# mode: sh
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
