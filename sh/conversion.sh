#!/bin/sh
#                   C O N V E R S I O N . S H
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
#
# Check the status of our conversion capability on given geometry
#
###

# Ensure /bin/sh
export PATH || (echo "This isn't sh."; sh $0 $*; kill $$)

# save the precious args
ARGS="$*"
NAME_OF_THIS=`basename $0`
PATH_TO_THIS=`dirname $0`
THIS="$PATH_TO_THIS/$NAME_OF_THIS"

# sanity check
if test ! -f "$THIS" ; then
    echo "INTERNAL ERROR: $THIS does not exist"
    if test ! "x$0" = "x$THIS" ; then
	echo "INTERNAL ERROR: dirname/basename inconsistency: $0 != $THIS"
    fi
    exit 1
fi

# force locale setting to C so things like date output as expected
LC_ALL=C

# force posix behavior
POSIX_PEDANTIC=1
POSIXLY_CORRECT=1
export POSIX_PEDANTIC POSIXLY_CORRECT


#######################
# log to tty and file #
#######################
log ( ) {

    # this routine writes the provided argument(s) to a log file as
    # well as echoing them to stdout if we're not in quiet mode.

    if test $# -lt 1 ; then
	format=""
    else
	format="$1"
	shift
    fi

    if test ! "x$LOGFILE" = "x" ; then
	action="printf \"$format\n\" $* >> \"$LOGFILE\""
	eval "$action"
    fi
    if test ! "x$QUIET" = "x1" ; then
	action="printf \"$format\n\" $*"
	eval "$action"
    fi
}


##############################
# ensure a 0|1 boolean value #
##############################
booleanize ( ) {

    # this routine examines the value of the specified variable and
    # converts it to a simple zero or one value in order to simplify
    # checks later on

    for var in $* ; do

	VAR="$var"
	VALCMD="echo \$$VAR"
	VAL="`eval $VALCMD`"

	CMD=__UNSET__
	case "x$VAL" in
	    x)
		CMD="$VAR=0"
		;;
	    x0)
		CMD="$VAR=0"
		;;
	    x[nN]*)
		CMD="$VAR=0"
		;;
	    x*)
		CMD="$VAR=1"
		;;
	esac

	if test ! "x$CMD" = "x__UNSET__" ; then
	    eval $CMD
	    export $VAR
	fi

    done
}


################
# elapsed time #
################
elapsed ( ) {

    # this routine either reports the current time (in seconds)

    elp=`date '+%s'`
    echo $elp
    return
}


####################
# handle arguments #
####################

# process the argument list for commands
while test $# -gt 0 ; do
    arg="$1"
    case "x$arg" in
	x*[hH])
	    HELP=1
	    ;;
	x*[hH][eE][lL][pP])
	    HELP=1
	    ;;
	x*[iI][nN][sS][tT][rR][uU][cC][tT]*)
	    INSTRUCTIONS=1
	    ;;
	x*[qQ][uU][iI][eE][tT])
	    QUIET=1
	    ;;
	x*[vV][eE][rR][bB][oO][sS][eE])
	    VERBOSE=1
	    ;;
	x*[kK][eE][eE][pP])
	    KEEP=1
	    ;;
	x*=*)
	    VAR=`echo $arg | sed 's/=.*//g' | sed 's/^[-]*//g'`
	    if test ! "x$VAR" = "x" ; then
		VAL=`echo $arg | sed 's/.*=//g'`
		CMD="$VAR=\"$VAL\""
		eval $CMD
		export $VAR
	    fi
	    ;;
	x*)
	    # should be geometry database files, so stop
	    break
	    ;;
    esac
    shift
done

# validate and clean up options (all default to 0)
booleanize HELP INSTRUCTIONS VERBOSE KEEP


###
# handle instructions before main processing
###
if test "x$INSTRUCTIONS" = "x1" ; then
    cat <<EOF

This script is used to test BRL-CAD's explicit surface polygonal
conversion capabilities.  It takes a list of one or more geometry
files and for every geometry object in the file, it facetizes the
object into NMG and BoT format.  The script times, tabulates, and
reports conversion results.

The script is useful for a variety of purposes including for
developers to perform regression testing as BRL-CAD's conversion
capabilities are modified and improved.  It's also useful for users to
get a report of which objects in a given BRL-CAD geometry database
will convert, what percentage, and how long the conversion will take.

There are several environment variables that will modify how this
script behaves:

  GED          - file path to geometry editor to use for converting
  SEARCH       - file to geometry editor to use for searching
  OPATH        - geometry path to use for object search (default .)
  OBJECTS      - parameters for selecting objects to convert
  KEEP         - retain the converted geometry instead of deleting
  MAXTIME      - maximum number of seconds allowed for each conversion
  QUIET        - turn off all output (writes results to log file)
  VERBOSE      - turn on extra debug output for testing/development
  INSTRUCTIONS - display these more detailed instructions

The GED option allows you to specify a specific pathname for MGED.
The default is to search the system path for 'mged'.

The SEARCH option allows you to specify a different MGED that will be
used for finding objects.  As MGED's "search" command was added in
release 7.14.0, this option allows you to find objects using a 7.14.0+
version of MGED but then attempt to convert geometry with a
potentially older version of MGED.  The default is to use the same
mged (specified by the GED option) being used for conversions.

The OBJECTS option allows you to specify which objects you want to
convert.  Any of the parameters recognized by the GED 'search' command
can be used.  See the 'search' manual page for details on all
available parameters.  Examples:

OBJECTS="-type region"  # only convert regions
OBJECTS="-not -type comb"  # only convert primitives

The KEEP option retains the converted geometry file after conversion
processing has ended.  The default is to delete the working copy.

The MAXTIME option specifies how many seconds are allowed to elapse
before the conversion is aborted.  Some conversions can take days or
months (or infinite in the case of a bug), so this places an upper
bound on the per-object conversion time.
EOF
    exit 0
fi


if test $# -eq 0 ; then
    echo "No geometry files specified."
    echo ""
    HELP=1
fi

###
# handle help before main processing
###
if test "x$HELP" = "x1" ; then
    echo "Usage: $0 [command(s)] [OPTION=value] file1.g [file2.g ...]"
    echo ""
    echo "Available commands:"
    echo "  help          (this is what you are reading right now)"
    echo "  instructions"
    echo "  quiet"
    echo "  verbose"
    echo ""
    echo "Available options:"
    if test "x$GED" = "x" ; then
	echo "  GED=/path/to/mged (default mged)"
    else
	echo "  GED=/path/to/mged (using $GED)"
    fi
    if test "x$SEARCH" = "x" ; then
	echo "  SEARCH=/path/to/search-enabled/mged (default mged)"
    else
	echo "  SEARCH=/path/to/search-enabled/mged (using $SEARCH)"
    fi
    if test "x$OPATH" = "x" ; then
	echo "  OPATH=/path/to/objects (default .)"
    else
	echo "  OPATH=/path/to/objects (using $OPATH)"
    fi
    if test "x$OBJECTS" = "x" ; then
	echo "  OBJECTS=\"search params\" (default \"\" for all objects)"
    else
	echo "  OBJECTS=\"search params\" (using \"$OBJECTS\")"
    fi
    if test "x$KEEP" = "x" ; then
	echo "  KEEP=boolean (default \"no\")"
    else
	echo "  KEEP=boolean (using \"$KEEP\")"
    fi
    if test "x$MAXTIME" = "x" ; then
	echo "  MAXTIME=#seconds (default 300)"
    else
	echo "  MAXTIME=#seconds (using $MAXTIME)"
    fi
    echo ""
    echo "BRL-CAD is a powerful cross-platform open source solid modeling system."
    echo "For more information about BRL-CAD, see http://brlcad.org"
    echo ""
    echo "Run '$0 instructions' for additional information."
    exit 1
fi


#############
# B E G I N #
#############

# where to write results
LOGFILE=conversion-$$-run.log
touch "$LOGFILE"
if test ! -w "$LOGFILE" ; then
    if test ! "x$LOGFILE" = "x/dev/null" ; then
	echo "ERROR: Unable to log to $LOGFILE"
    fi
    LOGFILE=/dev/null
fi

VERBOSE_ECHO=:
ECHO=log
if test "x$QUIET" = "x1" ; then
    if test "x$VERBOSE" = "x1" ; then
	echo "Verbose output quelled by quiet option.  Further output disabled."
    fi
else
    if test "x$VERBOSE" = "x1" ; then
	VERBOSE_ECHO=echo
	echo "Verbose output enabled"
    fi
fi

if test "x$KEEP" = "x1" ; then
    $VERBOSE_ECHO "Converted geometry file will be retained"
fi

###
# ensure variable is set to something
###
set_if_unset ( ) {
    set_if_unset_name="$1" ; shift
    set_if_unset_val="$1" ; shift

    set_if_unset_var="echo \"\$$set_if_unset_name\""
    set_if_unset_var_val="`eval ${set_if_unset_var}`"
    if test "x${set_if_unset_var_val}" = "x" ; then
	set_if_unset_var="${set_if_unset_name}=\"${set_if_unset_val}\""
	eval $set_if_unset_var
	export $set_if_unset_name
    fi

    set_if_unset_var="echo \"\$$set_if_unset_name\""
    set_if_unset_val="`eval ${set_if_unset_var}`"
}

# approximate maximum time in seconds that a given conversion is allowed to take
set_if_unset GED mged
set_if_unset SEARCH $GED
set_if_unset MAXTIME 300

# commands that this script expects, make sure we can find MGED
MGED="`which $GED`"
if test ! -f "$MGED" ; then
    echo "ERROR: Unable to find $GED"
    echo ""
    echo "Configure the GED variable or check your PATH."
    echo "Run '$0 instructions' for additional information."
    echo ""
    echo "Aborting."
    exit 1
fi
SGED="`which $SEARCH`"
if test ! -f "$SGED" ; then
    echo "ERROR: Unable to find $SEARCH"
    echo ""
    echo "Configure the SEARCH variable or check your PATH."
    echo "Run '$0 instructions' for additional information."
    echo ""
    echo "Aborting."
    exit 1
fi


################
# start output #
################

$ECHO "B R L - C A D   C O N V E R S I O N"
$ECHO "==================================="
$ECHO "Running $THIS on `date`"
$ECHO "Logging output to $LOGFILE"
$ECHO "`uname -a 2>&1`"
$ECHO "Using [${MGED}] for GED"
$ECHO "Using [${MAXTIME}] for MAXTIME"
$ECHO

# iterate over every specified geometry file
files=0
count=0
nmg_count=0
bot_count=0
$ECHO "%s" "-=-"
begin=`elapsed` # start elapsed runtime timer
while test $# -gt 0 ; do
    file="$1"
    if ! test -f "$file" ; then
	echo "Unable to read file [$file]"
	shift
	continue
    fi

    work="${file}.conversion.g"
    cmd="cp \"$file\" \"$work\""
    $VERBOSE_ECHO "\$ $cmd"
    eval "$cmd"

    # execute in a coprocess
    if test "x$OBJECTS" = "x" ; then OBJECTS='-print' ; fi
    cmd="$SGED -c \"$work\" search $OPATH $OBJECTS"
    objects=`eval "$cmd" 2>&1 | grep -v Using`
    $VERBOSE_ECHO "\$ $cmd"
    $VERBOSE_ECHO "$objects"

    # stash stdin on fd3 and set stdin to our object list.  this is
    # necessary because if a kill timer is called, input being read on
    # the while loop will be terminated early.
    exec 3<&0
    exec 0<<EOF
$objects
EOF

    # iterate over every geometry object in the current file
    while read object ; do

	obj="`basename \"$object\"`"
	found=`$SGED -c "$work" search . -name \"${obj}\" 2>&1 | grep -v Using`
	if test "x$found" != "x$object" ; then
	    $ECHO "INTERNAL ERROR: Failed to find [$object] with [$obj] (got [$found])"
	    continue
	fi

	# start the limit timer.  this will kill the upcoming facetize
	# job if more than MAXTIME seconds have elapsed.  in order for
	# this to work while still ECHO'ing a message and without
	# leaving orphaned 'sleep' processes that accumualte, this
	# method had to be executed in the current shell environment.

	{ sleep $MAXTIME && test "x`ps auxwww | grep "$work" | grep facetize | grep "${obj}.nmg" | awk '{print $2}'`" != "x" && `touch "./${obj}.nmg.extl"` && kill -9 `ps auxwww | grep "$work" | grep facetize | grep "${obj}.nmg" | awk '{print $2}'` 2>&4 & } 4>&2 2>/dev/null
        spid=$!

	# convert NMG
	nmg=fail
	cmd="$GED -c "$work" facetize -n \"${obj}.nmg\" \"${obj}\""
	$VERBOSE_ECHO "\$ $cmd"
	output=`eval time -p "$cmd" 2>&1 | grep -v Using`

	# stop the limit timer.  when we get here, see if there is a
	# sleep process still running.  if any found, the sleep
	# proceses are killed and waited for so that we successfully
	# avoid the parent shell reporting a "Terminated" process kill
	# message.

	for pid in `ps xj | grep $spid | grep sleep | grep -v grep | awk '{print $2}'` ; do
	    # must kill sleep children first or they can continue running orphaned
	    kill $pid >/dev/null 2>&1
	    wait $pid >/dev/null 2>&1
	done
	# must wait in order to suppress kill messages
	kill -9 $spid >/dev/null 2>&1
	wait $spid >/dev/null 2>&1

	$VERBOSE_ECHO "$output"
	real_nmg="`echo \"$output\" | tail -n 4 | grep real | awk '{print $2}'`"

	# verify NMG
	found=`$SGED -c "$work" search . -name \"${obj}.nmg\" 2>&1 | grep -v Using`
	if test "x$found" = "x${object}.nmg" ; then
	    nmg=pass
	    nmg_count=`expr $nmg_count + 1`
	fi
	if [ -e "./${obj}.nmg.extl" ] ; then
            `rm "./${obj}.nmg.extl"`
	    nmg=extl
	fi

	# start the limit timer, same as above.
	{ sleep $MAXTIME && test "x`ps auxwww | grep "$work" | grep facetize | grep "${obj}.bot" | awk '{print $2}'`" != "x" && `touch "./${obj}.bot.extl"` && kill -9 `ps auxwww | grep "$work" | grep facetize | grep "${obj}.bot" | awk '{print $2}'` 2>&4 & } 4>&2 2>/dev/null
        spid=$!

	# convert BoT
	bot=fail
	cmd="$GED -c "$work" facetize \"${obj}.bot\" \"${obj}\""
	$VERBOSE_ECHO "\$ $cmd"
	output=`eval time -p "$cmd" 2>&1 | grep -v Using`

	# stop the limit timer, same as above.
	for pid in `ps xj | grep $spid | grep sleep | grep -v grep | awk '{print $2}'` ; do
	    # must kill sleep children first or they can continue running orphaned
	    kill $pid >/dev/null 2>&1
	    wait $pid >/dev/null 2>&1
	done
	# must wait in order to suppress kill messages
	kill -9 $spid >/dev/null 2>&1
	wait $spid >/dev/null 2>&1

	$VERBOSE_ECHO "$output"
	real_bot="`echo \"$output\" | tail -n 4 | grep real | awk '{print $2}'`"

	# verify BoT
	found=`$SGED -c "$work" search . -name \"${obj}.bot\" 2>&1 | grep -v Using`
	if test "x$found" = "x${object}.bot" ; then
	    bot=pass
	    bot_count=`expr $bot_count + 1`
	fi
	if [ -e "./${obj}.bot.extl" ] ; then
            `rm "./${obj}.bot.extl"`
	    bot=extl
	fi

	# print result for this object
	status=fail
	if test "x$nmg" = "xpass" && test "x$bot" = "xpass" ; then
	    status=ok
	fi

	count=`expr $count + 1`

	$ECHO "%-4s\tnmg: %s %ss\tbot: %s %ss %6.0fs %*s%.0f %s:%s" \
               \"$status\" \"$nmg\" \"$real_nmg\" \"$bot\" \"$real_bot\" \"$SECONDS\" \
               \"`expr 7 - $count : '.*'`\" \"#\" $count \"$file\" \"$object\"

    done

    # restore stdin
    exec 0<&3

    files=`expr $files + 1`

    # remove the file if so directed
    if test "x$KEEP" = "x0" ; then
	rm -f "$work"
    fi

    shift
done
end=`elapsed` # stop elapsed runtime timer
$ECHO "%s" "-=-"

# calculate summary statistics
elp=`echo $begin $end | awk '{print $2-$1}'`
nmg_fail=`echo $nmg_count $count | awk '{print $2-$1}'`
bot_fail=`echo $bot_count $count | awk '{print $2-$1}'`
if test $count -eq 0 ; then
    nmg_percent=0
    bot_percent=0
    rate=0
    avg=0
else
    nmg_percent=`echo $nmg_count $count | awk '{print ($1/$2)*100.0}'`
    bot_percent=`echo $bot_count $count | awk '{print ($1/$2)*100.0}'`
    rate=`echo $nmg_count $bot_count $count | awk '{print ($1+$2)/($3+$3)*100.0}'`
    avg=`echo $elp $count | awk '{print $1/$2}'`
fi

# print summary
$ECHO
$ECHO "... Done."
$ECHO
$ECHO "Summary:"
$ECHO
$ECHO "   Files:  %.0f" $files
$ECHO " Objects:  %.0f" $count
$ECHO "Failures:  %.0f NMG, %.0f BoT" $nmg_fail $bot_fail
$ECHO "NMG conversion:  %.1f%%  (%.0f of %.0f objects)" $nmg_percent $nmg_count $count
$ECHO "BoT conversion:  %.1f%%  (%.0f of %.0f objects)" $bot_percent $bot_count $count
$ECHO "  Success rate:  %.1f%%" $rate
$ECHO
$ECHO "Elapsed:  %.0f seconds" $elp
$ECHO "Average:  %.1f seconds per object" $avg
$ECHO
$ECHO "Finished running $THIS on `date`"
$ECHO "Output was saved to $LOGFILE from `pwd`"
$ECHO "Conversion testing complete."


# Local Variables:
# tab-width: 8
# mode: sh
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
