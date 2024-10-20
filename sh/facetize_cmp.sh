#!/bin/sh
#                F A C E T I Z E _ C M P . S H
# BRL-CAD
#
# Copyright (c) 2010-2024 United States Government as represented by
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
	action="printf \"$format\" $* >> \"$LOGFILE\""
	eval "$action"
    fi
    if test ! "x$QUIET" = "x1" ; then
	action="printf \"$format\" $*"
	eval "$action"
    fi
}

# First two arguments should be MGED paths
CTRL_MGED=$1
shift;
TEST_MGED=$1
shift;

################
# elapsed time #
################
elapsed ( ) {

    # this routine reports the current time (in seconds)

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

if test $# -eq 0 ; then
    echo "No geometry files specified."
    echo ""
    HELP=1
fi

###
# handle help before main processing
###
if test "x$HELP" = "x1" ; then
    echo "Usage: $0 MGED1_PATH MGED2_PATH file1.g [file2.g ...]"
    exit 1
fi


#############
# B E G I N #
#############

# where to write results
LOGFILE=facetize-cmp-$$-run.log
touch "$LOGFILE"
if test ! -w "$LOGFILE" ; then
    if test ! "x$LOGFILE" = "x/dev/null" ; then
	echo "ERROR: Unable to log to $LOGFILE"
    fi
    LOGFILE=/dev/null
fi

VERBOSE_ECHO=:
ECHO=log

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
set_if_unset MAXTIME 300

# make sure we can find MGED
if test ! -f "$CTRL_MGED" ; then
    echo "ERROR: Unable to find $CTRL_MGED"
    echo ""
    echo "Aborting."
    exit 1
fi
if test ! -f "$TEST_MGED" ; then
    echo "ERROR: Unable to find $TEST_MGED"
    echo ""
    echo "Aborting."
    exit 1
fi

###########################
# define method function #
###########################

bot_facetize ( ) {
    obj="$1" ; shift

    # convert BoT with Control MGED

    # start the limit timer
    { sleep $MAXTIME && test "x`ps auxwww | grep "$work" | grep facetize | grep "ctrl_bot.bot" | awk '{print $2}'`" != "x" && rm -f "./ctrl_bot.bot.timeout" && touch "./ctrl_bot.bot.timeout" && kill -9 `ps auxwww | grep "$work" | grep facetize | grep "ctrl_bot.bot" | awk '{print $2}'` 2>&4 & } 4>&2 2>/dev/null
    spid=$!

    cmd="$CTRL_MGED -c \"$work\" killtree -f \"ctrl_bot.bot\""
    eval "$cmd" >/dev/null 2>&1
    ctrl_bot=$fail
    cmd="$CTRL_MGED -c \"$work\" facetize -r \"${obj}\" \"ctrl_bot.bot\" "
    $VERBOSE_ECHO "\$ $cmd"
    output=`eval time -p "$cmd" 2>&1 | grep -v Using`

    # stop the limit timer
    for pid in `ps xj | grep $spid | grep sleep | grep -v grep | awk '{print $2}'` ; do
        # must kill sleep children first or they can continue running orphaned
        kill $pid >/dev/null 2>&1
        wait $pid >/dev/null 2>&1
    done
    # must wait in order to suppress kill messages
    kill -9 $spid >/dev/null 2>&1
    wait $spid >/dev/null 2>&1

    $VERBOSE_ECHO "$output"
    ctrl_real_bot="`echo \"$output\" | tail -n 4 | grep real | awk '{print $2}'`"

    # verify BoT
    ctrl_found=`$CTRL_MGED -c "$work" search . -name \"ctrl_bot.bot\" 2>&1 | grep -v Using`
    if test "x$ctrl_found" != "xctrl_bot.bot" ; then
        ctrl_bot=$unable
    elif [ -e "./ctrl_bot.bot.timeout" ] ; then
        rm -f "./ctrl_bot.bot.timeout"
        ctrl_bot=$time
    fi

    # convert BoT with Test MGED

    # start the limit timer
    { sleep $MAXTIME && test "x`ps auxwww | grep "$work" | grep facetize | grep "test_bot.bot" | awk '{print $2}'`" != "x" && rm -f "./test_bot.bot.timeout" && touch "./test_bot.bot.timeout" && kill -9 `ps auxwww | grep "$work" | grep facetize | grep "test_bot.bot" | awk '{print $2}'` 2>&4 & } 4>&2 2>/dev/null
    spid=$!

    cmd="$TEST_MGED -c \"$work\" killtree -f \"test_bot.bot\""
    eval "$cmd" >/dev/null 2>&1
    test_bot=$fail
    cmd="$TEST_MGED -c \"$work\" facetize -r \"${obj}\" \"test_bot.bot\" "
    $VERBOSE_ECHO "\$ $cmd"
    output=`eval time -p "$cmd" 2>&1 | grep -v Using`

    # stop the limit timer
    for pid in `ps xj | grep $spid | grep sleep | grep -v grep | awk '{print $2}'` ; do
        # must kill sleep children first or they can continue running orphaned
        kill $pid >/dev/null 2>&1
        wait $pid >/dev/null 2>&1
    done
    # must wait in order to suppress kill messages
    kill -9 $spid >/dev/null 2>&1
    wait $spid >/dev/null 2>&1

    $VERBOSE_ECHO "$output"
    test_real_bot="`echo \"$output\" | tail -n 4 | grep real | awk '{print $2}'`"

    # verify BoT
    test_found=`$TEST_MGED -c "$work" search . -name \"test_bot.bot\" 2>&1 | grep -v Using`
    if test "x$test_found" != "xtest_bot.bot" ; then
        test_bot=$unable
    elif [ -e "./test_bot.bot.timeout" ] ; then
        rm -f "./test_bot.bot.timeout"
        test_bot=$time
    fi

    area_delta=10000000.0
    vol_delta=10000000.0
    if test "x$ctrl_found" = "xctrl_bot.bot" ; then
        if test "x$test_found" = "xtest_bot.bot" ; then
            # Calculate volume and area for both BoTs
	    rm -f calc_area_ctrl.rt
            $TEST_MGED -c "$work" "e ctrl_bot.bot ; ae 35 25 ; zoom 1.25 ; saveview -e \"rtarea -U 1\" -l /dev/stdout calc_area_ctrl.rt"
            ctrl_area="`sh calc_area_ctrl.rt 2>&1 | grep Cumulative | awk '{print $7}'`"
	    rm -f calc_area_test.rt
            $TEST_MGED -c "$work" "e test_bot.bot ; ae 35 25 ; zoom 1.25 ; saveview -e \"rtarea -U 1\" -l /dev/stdout calc_area_test.rt"
            test_area="`sh calc_area_test.rt 2>&1 | grep Cumulative | awk '{print $7}'`"
	    area_delta="`dc -e \"15k $test_area $ctrl_area - $ctrl_area / d * v p\"`"
            ctrl_vol="`gqa -g 10m-10mm -Av "$work" ctrl_bot.bot 2>&1 | grep Average | awk '{print $4}'`"
	    tmpvol=`printf "%f" $ctrl_vol`
	    ctrl_vol=`printf "%f" $tmpvol`
            test_vol="`gqa -g 10m-10mm -Av "$work" test_bot.bot 2>&1 | grep Average | awk '{print $4}'`"
	    tmpvol=`printf "%f" $test_vol`
	    test_vol=`printf "%f" $tmpvol`
	    vol_delta="`dc -e \"15k $test_vol $ctrl_vol - $ctrl_vol / d * v p\"`"
        fi
    fi

}

################
# start output #
################

$ECHO "Facetize Output Comparison Checker\n"
$ECHO "==================================\n"
$ECHO "Running $THIS on `date`\n"
$ECHO "Logging output to $LOGFILE\n"
$ECHO "`uname -a 2>&1`\n"
$ECHO "Using [${CTRL_MGED}] for Control MGED\n"
$ECHO "Using [${TEST_MGED}] for Test MGED\n"
$ECHO "Using [${MAXTIME}] for MAXTIME\n"
$ECHO "\n"

# aggregate stats
obj_count=0
file_count=0

# labels defined in one place
pass=ok
unable=UNABLE
fail=FAIL
time=time


##############################
# report data for object #
##############################
obj_report ( ) {

	# print result for this object

	$ECHO "%-6s  " \"$status\"
	$ECHO "area delta: %2.1f  " \"$area_delta\"
	$ECHO "vol delta: %2.1f  " \"$vol_delta\"
	$ECHO "areas (ctrl/test): %10.1f %10.1f  " \"$ctrl_area\" \"$test_area\"
	$ECHO "vols (ctrl/test): %10.1f %10.1f" \"$ctrl_vol\" \"$test_vol\"

	$ECHO "%*s%.0f %-7s %s:%s\n" \"`expr 7 - $obj_count : '.*'`\" \"\" $obj_count \"$object_type\" \"$file\" \"$object\"
}


$ECHO "%s" "-=-"
$ECHO "\n"
begin=`elapsed` # start elapsed runtime timer
while test $# -gt 0 ; do
    # iterates over every specified geometry file
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
    cmd="$CTRL_MGED -c \"$work\" search $OPATH $OBJECTS"
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
	found=`$CTRL_MGED -c "$work" search . -name \"${obj}\" 2>&1 | grep -v Using`
	if test "x$found" != "x$object" ; then
	    $ECHO "INTERNAL ERROR: Failed to find [$object] with [$obj] (got [$found])\n"
	    continue
	fi
	object_type=`$CTRL_MGED -c "$work" db get_type \"${obj}\" 2>&1 | grep -v Using`
	if test "x$object_type" = "xcomb" ; then
	    # identify regions specifically
	    region=`$CTRL_MGED -c "$work" get \"${obj}\" region 2>&1 | grep -v Using`
	    if test "x$region" = "xyes" ; then
		object_type="region"
		reg_count=`expr $reg_count + 1`
	    fi
	    if test "x$region" != "xyes" ; then
		# if we don't have any regions in the tree, skip
		have_regions=`$CTRL_MGED -c "$work" search \"${obj}\" -type region 2>&1 | grep -v Using`
		if test "x$have_regions" = "x" ; then
		    continue;
		fi
	    fi
	else
	    prim_count=`expr $prim_count + 1`
	fi

	bot_facetize ${obj}

	status=$pass
	if test "x$ctrl_bot" = "x$unable" ; then
	    status=$unable
	    unable_count=`expr $unable_count + 1`
	fi
	if test "x$ctrl_bot" = "x$time" ; then
	    status=$time
	    unable_count=`expr $unable_count + 1`
	fi

	# calculate stats for this object
	if test "x$status" = "x$pass" ; then
	    if ( ( awk "BEGIN {exit !($area_delta < 10.0)}" -a awk "BEGIN {exit !($vol_delta < 10.0)}" ) ); then
		status=$pass
		pass_count=`expr $pass_count + 1`
	    elif awk "BEGIN {exit !($area_delta > 1000000.0)}"; then
		status=$unable
		unable_count=`expr $unable_count + 1`
	    else
		status=$fail
		fail_count=`expr $fail_count + 1`
	    fi
	fi

	obj_count=`expr $obj_count + 1`

	# | awk '{print ($1+$2+$3)}'`

	# print result for this object
	obj_report

    done

    # restore stdin
    exec 0<&3

    file_count=`expr $file_count + 1`

    # remove the file if so directed
    if test "x$KEEP" = "x0" ; then
	rm -f "$work"
    fi

    shift
done
end=`elapsed` # stop elapsed runtime timer
$ECHO "%s" "-=-"
$ECHO "\n"

# calculate summary statistics
elp=`echo $begin $end | awk '{print $2-$1}'`
rate=`echo $pass_count $obj_count | awk '{print ($1/$2)*100.0}'`

# print summary
$ECHO "\n"
$ECHO "... Done.\n"
$ECHO "\n"
$ECHO "Summary\n"
$ECHO "=======\n"
$ECHO "Converted: %3.1f%%  ( %0.f of %.0f objects, %0.f files )\n" $rate $pass_count $obj_count $file_count
$ECHO "\n"
$ECHO "   Passed: %3.0f\n" $pass_count
$ECHO "   Failed: %3.0f\n" $fail_count
$ECHO "   Unable: %3.0f\n" $unable_count
$ECHO "  Timeout: %3.0f\n" $time_count
$ECHO "\n"
pass_percent=`echo $pass_count $obj_count | awk '{print ($1/$2)*100.0}'`
$ECHO " Success rate: %3.1f%%  ( %.0f of %.0f )\n" $pass_percent $pass_count $obj_count
$ECHO "\n"
$ECHO "  Elapsed: %3.1f seconds\n" $elp
$ECHO "\n"
$ECHO "Finished running $THIS on `date`\n"
$ECHO "Output was saved to $LOGFILE from `pwd`\n"
$ECHO "Testing complete.\n"

# Local Variables:
# tab-width: 8
# mode: sh
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
