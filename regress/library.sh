#!/bin/sh
#                      L I B R A R Y . S H
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

# save the precious args
ARGS="$*"
ARG0="$0"
ARG1="$1"
NAME_OF_THIS=`basename "$ARG0"`
PATH_TO_THIS=`dirname "$ARG0"`
THIS="$PATH_TO_THIS/$NAME_OF_THIS"
export ARGS ARG0 ARG1 NAME_OF_THIS PATH_TO_THIS THIS

is_absolute() {
    if test "x$1" = "x" ; then
	return
    fi
    case "$1" in
	/*)
	    true;
	    ;;
	*)
	    false;
	    ;;
    esac
}

# make sure if LOGFILE is specified externally that it refers to a
# full path.  some tests change directories.
if ! test "x$LOGFILE" = "x" && ! is_absolute "$LOGFILE" ; then
    LOGFILE="`pwd`/$LOGFILE"
fi


###
# log "string to print and log"
#
# prints a message to the current tty/output (unless QUIET variable is
# set) as well as appending to a log (if LOGFILE variable is set).
log ( ) {

    if test ! "x$LOGFILE" = "x" ; then
	echo "$*" >> "$LOGFILE"
    fi
    if test ! "x$QUIET" = "x1" ; then
	echo "$*"
    fi
}


###
# run command [arg1] [arg2] [...]
#
# helper function runs a given command, logs it to a file, and stashes
# the return status.  reads LOGFILE global, increments STATUS global.
run ( ) {
    log "... running $@"
    "$@" >> "$LOGFILE" 2>&1
    ret=$?
    case "x$STATUS" in
	'x'|*[!0-9]*)
	    break;;
	*)
	    if test $ret -ne 0 ; then
		STATUS="`expr $STATUS + 1`"
	    fi
	    ;;
    esac
    return $ret
}


###
# files_differ file1 file2 [diff args]
#
# comparison function returns success (zero) if the two specified
# files differ or either doesn't exist, returns error otherwise.
# Increments STATUS global.
files_differ ( ) {
    if test $# -lt 2 ; then
	log "INTERNAL ERROR: files_differ has wrong arg count ($# < 2)"
	exit 1
    fi
    if test "x$2" = "x" ; then
	log "INTERNAL ERROR: files_differ is missing filename #2"
	exit 1
    fi
    if test "x$1" = "x" ; then
	log "INTERNAL ERROR: files_differ is missing filename #1"
	exit 1
    fi

    ret=0
    if test ! -f "$2" ; then
	log "ERROR: $2 does not exist"
	ret=1
    elif test ! -f "$1" ; then
	log "ERROR: $1 does not exist"
	ret=1
    else
	file1="$1"
	file2="$2"
	shift 2
	log "... running diff $* $file1 $file2"
	if test "x`diff $* $file1 $file2`" = "x" ; then
	    log "ERROR: comparison failed  ($file1 and $file2 are identical, expected change)"
	    ret=1
	fi
    fi

    if test $ret -ne 0 ; then
	STATUS="`expr $STATUS + 1`"
	export STATUS
    fi
    return $ret
}


###
# files_match file1 file2 [diff args]
#
# comparison function returns success (zero) if two specified files
# both exist and have the same contents, returns failure otherwise.
# Increments STATUS global.
files_match ( ) {
    if test $# -lt 2 ; then
	log "INTERNAL ERROR: files_match has wrong arg count ($# < 2)"
	exit 1
    fi
    if test "x$2" = "x" ; then
	log "INTERNAL ERROR: files_match is missing filename #2"
	exit 1
    fi
    if test "x$1" = "x" ; then
	log "INTERNAL ERROR: files_match is missing filename #1"
	exit 1
    fi

    ret=0
    if test ! -f "$2" ; then
	log "ERROR: $2 does not exist"
	ret=1
    elif test ! -f "$1" ; then
	log "ERROR: $1 does not exist"
	ret=1
    else
	file1="$1"
	file2="$2"
	shift 2
	log "... running diff $* $file1 $file2"
	if test "x`diff $* $file1 $file2`" != "x" ; then
	    log "ERROR: comparison failed  ($file1 and $file2 are different, expected no change)"
	    # display diff in the log
	    log "BEGIN logging differences:"
	    run diff -u $* $file1 $file2
	    log "DONE logging differences."
	    ret=1
	fi
    fi

    if test $ret -ne 0 ; then
	STATUS="`expr $STATUS + 1`"
	export STATUS
    fi
    return $ret
}


###
# ensearch {command_to_find}
#
# prints the path to a given application binary or script, typically
# by looking in the bin or bench directory relative to this library
# script's location
ensearch ( ) {
    ensearch_file="$1"
    ensearch_dirs="\
	$ARG1/bin ../bin ../../bin ./bin \"$PATH_TO_THIS/../bin\" ../bench \"$PATH_TO_THIS/../bench\" \
	$ARG1/Release/bin ../Release/bin ../../Release/bin ./Release/bin \"$PATH_TO_THIS/../Release/bin\" \
	$ARG1/Debug/bin ../Debug/bin ../../Debug/bin ./Debug/bin \"$PATH_TO_THIS/../Debug/bin\" \
       	"

    if test "x$ensearch_file" = "x" ; then
	# nothing to do
	echo "ERROR: no file name specified" 1>&2
	echo ":"
	return
    fi

    for dir in $ensearch_dirs ; do
	ensearch_binary="$dir/$ensearch_file"
	#echo "Searching for $ensearch_binary" 1>&2
	if test -f "$ensearch_binary" ; then
	    echo "$ensearch_binary"

	    ensearch_path="`dirname $ensearch_binary`"
	    return
	fi
    done

    # found nothing
    echo "ERROR: could not find $ensearch_file" 1>&2
    echo ":"
    return
}


# Local Variables:
# tab-width: 8
# mode: sh
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
