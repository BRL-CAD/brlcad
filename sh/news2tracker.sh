#!/bin/sh
#                    N E W S 2 T R A C K E R . S H
# BRL-CAD
#
# Copyright (c) 2006-2011 United States Government as represented by
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
# This script reads the NEWS file and associated commit comments and
# generates a comma separate value file suitable for import into a
# spreadsheet application.
#
# Example use that provides progress status and saves the items added
# within the last 60 days to a file named trackers.csv:
#
#   ./news2tracker.sh -v -o trackers.csv -c 60
#
# Author -
#   Christopher Sean Morrison
#
###

SELF="$0"
PROJECT="brlcad"
OUTPUT="output.csv"
CLOSED="45"

###
# utility functions
###


# identify
#
# prints out identifying information for this utility.
identify ( ) {
    echo ""
    echo "**************************************************"
    echo "*      n  e  w  s  2  t  r  a  c  k  e  r        *"
    echo "*  A tool for extracting NEWS as tracker data    *"
    echo "*      Author: Christopher Sean Morrison         *"
    echo "*     Version 2008.1, BRL-CAD BSD License        *"
    echo "*               devs@brlcad.org                  *"
    echo "**************************************************"
    echo ""
}


# usage
#
# prints out basic application usage information including identify
# version information and an example use.
usage ( ) {
    identify
    printf "Usage: $SELF [-? | -h | --help] [-V | --version] [-v | --verbose] [-o | --output {file}] [-c | --closed {days}] [project]\n\n"
    printf "  -h      \tthis usage help\n"
    printf "  -V      \tdisplay version information"
    printf "  -v      \tbasic progress output to console\n"
    printf "  -v -v   \tadds output of commands to console\n"
    printf "  -v -v -v\tadds output of svn commands to console\n"
    printf "  -o file \tfile name to output comma-separate values (default: $OUTPUT)\n"
    printf "  -c days \toutput items added within specified number of days (default: $CLOSED)\n"
    printf "  project  \tName of the project to extract data for (default: $PROJECT)\n\n"
    printf "Example: $SELF -v -o $OUTPUT -c 365 $PROJECT\n\n"
    exit 1
}


# isint {number}
#
# determines whether number is really a (non-floating point) integer
# number.
isint ( ) {
    case "x$1" in
	x*[!0-9]*)
	    return 1
	    ;;
	x)
	    return 1
	    ;;
    esac;
    return 0
}


# days [year month day]
#
# computes the number of days since epoch for either the provided
# year, month, and day in numeric form or from today otherwise.
days ( ) {
    if [ $# -eq 3 ] ; then
	_yer="$1"
	_mon="$2"
	_day="$3"
    elif [ $# -eq 0 ] ; then
	_yer="`date +%Y`"
	_mon="`date +%m`"
	_day="`date +%d`"
    else
	echo "ERROR: cannot compute the date" 1>&2
	return 5
    fi

    # echo "_yer=$_yer;_mon=$_mon;_day=$_day"

    if ! isint $_yer ; then
	echo "ERROR: $_yer is not a number" 1>&2
	return 1
    fi
    if ! isint $_mon ; then
	echo "ERROR: $_mon is not a number" 1>&2
	return 2
    fi
    if ! isint $_day ; then
	echo "ERROR: $_day is not a number" 1>&2
	return 3
    fi
    [ $_yer -lt 1970 ] && echo "ERROR: $_yer is before the 1970 epoch" 1>&2 && return 4

    _yer="`expr $_yer - 1970`"
    _mon="`expr $_mon \+ 0`"
    _day="`expr $_day \+ 0`"

    _dys=0
    case $_mon in
	 1) _dys=0 ;;
	 2) _dys=31 ;;
	 3) _dys=59 ;;
	 4) _dys=90 ;;
	 5) _dys=120 ;;
	 6) _dys=151 ;;
	 7) _dys=181 ;;
	 8) _dys=212 ;;
	 9) _dys=243 ;;
	10) _dys=273 ;;
	11) _dys=304 ;;
	12) _dys=334 ;;
    esac

    echo "`expr \( $_yer \* 365 \) \+ $_dys \+ $_day`"
    return 0;
}


#
# process command line arguments
#
VERBOSE=0
while [ $# -gt 0 ] ; do
    case "x$1" in
	x-v|x--verb*)
	    if [ $VERBOSE -eq 0 ] ; then
		echo "VERBOSE mode enabled"
		VERBOSE=1
	    else
		echo "VERBOSITY increased"
		VERBOSE=$(($VERBOSE + 1))
	    fi
	    ;;
	x-o|x--o*)
	    shift
	    OUTPUT="$1"
	    [ $VERBOSE -gt 0 ] && echo "Exporting NEWS data to $OUTPUT"
	    ;;
	x-c|x--c*)
	    shift
	    CLOSED="$1"
	    [ $VERBOSE -gt 0 ] && echo "Items added within $CLOSED days will be exported"
	    ;;
	x-\?|x-h|x--h*)
	    usage
	    exit 1
	    ;;
	x-V|x--vers*)
	    identify
	    exit 0
	    ;;
	x-*)
	    echo "UNKNOWN OPTION: $1"
	    usage
	    exit 1
	    ;;
	*)
	    PROJECT=$1
	    [ $VERBOSE -gt 0 ] && echo "Extracting NEWS items for project: $PROJECT"
    esac
    shift
done


# validate settings, semi-private variables
if ! isint $VERBOSE ; then
    echo "VERBOSE is somehow not a number!  Setting to 1."
    VERBOSE=1
fi

_SID=$$
_TTMP=/tmp/news2tracker.sh

# force locale setting to C so things like date output as expected
LC_ALL=C

# identify self
if [ $VERBOSE -gt 0 ] ; then
    identify
fi

###
# validate tools, environment
###
[ $VERBOSE -gt 0 ] && echo "VALIDATING TOOLS AND ENVIRONMENT"

# make sure svn works
[ $VERBOSE -gt 1 ] && echo "svn --version > /dev/null 2>&1"
svn --version > /dev/null 2>&1
if [ $? -ne 0 ] ; then
    usage
fi

# make sure we have a working directory
if [ ! -d "$_TTMP" ] ; then
    [ $VERBOSE -gt 1 ] && echo "mkdir -p $_TTMP"
    mkdir -p "$_TTMP"
    if [ ! -d "$_TTMP" ] ; then
	echo "Unable to create $_TTMP"
	exit 2
    fi
elif [ ! -w "$_TTMP" ] ; then
    echo "Unable to write to $_TTMP"
    exit 2
elif [ -f "$_TTMP" ] ; then
    echo "File $_TTMP is in the way (want to create a directory there)"
    exit 2
fi

# validate the session id
if [ "x$_SID" = "x" ] ; then
    echo "Unable to get a valid session identifier"
    exit 2
fi
[ $VERBOSE -gt 0 ] && echo "Session identifier is $_SID"

# make sure this session directory exists
_stmp="$_TTMP/$_SID"
if [ ! -d "$_stmp" ] ; then
    [ $VERBOSE -gt 1 ] && echo "mkdir -p $_stmp"
    mkdir -p "$_stmp"
    if [ ! -d "$_stmp" ] ; then
	echo "Unable to create $_stmp"
	exit 2
    fi
elif [ ! -w "$_stmp" ] ; then
    echo "Unable to write to $_stmp"
    exit 2
elif [ -f "$_stmp" ] ; then
    echo "File $_stmp is in the way (want to create a directory there)"
    exit 2
fi

# make sure we can write to the output file unhindered
if [ -f "$OUTPUT" ] ; then
    echo "File $OUTPUT already exists."
    echo "Remove existing file or specify a different output file."
    exit 2
fi
[ $VERBOSE -gt 1 ] && echo "touch \"$OUTPUT\""
touch "$OUTPUT"
if [ ! -f "$OUTPUT" ] ; then
    echo "Unable to create output file \"$OUTPUT\""
    exit 2
fi
rm -f "$OUTPUT"
if [ -f "$OUTPUT" ] ; then
    echo "Unable to remove the output file \"$OUTPUT\""
    exit 2
fi


###
# get the NEWS annotation
###
[ $VERBOSE -gt 0 ] && echo "GETTING THE NEWS ANNOTATION"

# get the NEWS annotation
[ $VERBOSE -gt 1 ] && echo "svn annotate --non-interactive NEWS 2>/dev/null"
_annotate="`svn annotate NEWS 2>/dev/null`"

# log the NEWS data
if [ $VERBOSE -gt 2 ] ; then
    echo "Wrote out NEWS annotation data to:"
    printf "\t$_stmp/${PROJECT}.annotate\n"
    echo "$_annotate" > "$_stmp/${PROJECT}.annotate"
fi
if [ $VERBOSE -gt 3 ] ; then
    echo "begin svn annotate NEWS output:"
    echo "$_annotate"
    echo "end of svn annotate NEWS output"
fi


###
# get the NEWS log
###
[ $VERBOSE -gt 0 ] && echo "GETTING THE NEWS LOG"

# get the NEWS log
[ $VERBOSE -gt 1 ] && echo "svn log --xml --non-interactive NEWS 2>/dev/null"
_log="`svn log --xml --non-interactive NEWS 2>/dev/null`"

# log the NEWS log
if [ $VERBOSE -gt 2 ] ; then
    echo "Wrote out NEWS log data to:"
    printf "\t$_stmp/${PROJECT}.log\n"
fi
echo "$_log" > "$_stmp/${PROJECT}.log"
if [ $VERBOSE -gt 3 ] ; then 
    echo "begin svn log NEWS output:"
    echo "$_log"
    echo "end of svn log NEWS output"
fi

###
# determine what news items there are
###
[ $VERBOSE -gt 0 ] && echo "DETERMINING WHAT NEWS ITEMS EXIST"

_itemLine=""
_itemLines=""
_revisions="`echo \"$_annotate\" | awk '{print $3, $1}' | grep '^*' | sort -r -n | uniq | awk '{print $2}'`"
for _revision in $_revisions ; do
    [ $VERBOSE -gt 1 ] && echo "Looking up $_revision"

    _date="`echo \"$_log\" | grep -C2 revision=\\"$_revision\\" | tail -1 | sed 's/.*\([0-9][0-9][0-9][0-9]\)-\([0-9][0-9]\)-\([0-9][0-9]\).*/\1 \2 \3/'`"

    [ $VERBOSE -gt 1 ] && echo "Date for $_revision is $_date"

    _today="`days`"
    _added="`days $_date`"
    if [ $(($_today - $_added)) -gt $CLOSED ] ; then
	break;
    fi
    
    [ $VERBOSE -gt 2 ] && echo "$_revision was added $(($_today - $_added)) days ago"

    # use the revision as the itemID
    _itemLine="$_revision"

    # category is NEWS item
    _itemLine="$_itemLine,NEWS"

    # get the description
    _itemDesc="`echo \"$_annotate\" | grep -E \" *$_revision\" | sed 's/ *[0-9][0-9]* *[a-zA-Z0-9][a-zA-Z0-9]* *\(.*\)/\1/g' | perl -pi -e 's/(.*)[ \t]*-[ \t]*.*/\1/g' | grep -v '^-' | grep -v '^[ \t]*[^\*].*' | grep -v '^[ \t][ \t]*[^*]' | grep -v '^[ -][ -]*$' | grep -v '^$' | sed 's/^* //g' | perl -0777 -pi -e 's/\n(.)/; \1/gs'`"
    [ $VERBOSE -gt 1 ] && echo "${_revision}: Description is $_itemDesc"
    _itemLine="$_itemLine,\"$_itemDesc\""

    # priority is unknown
    _itemLine="$_itemLine,5"

    # status is committed to the source repository
    _itemLine="$_itemLine,Committed"

    # get the assignee
    _itemAssigned="`echo \"$_annotate\" | grep -E \" *$_revision\" | grep '\- ' | grep -v '\-\-' | perl -pi -e 's/.* *- *([^-]*)/\1/g' | perl -0777 -pi -e 's/,/\n/g' | sed 's/^ *\(.*\) *$/\1/g' | sort | uniq | perl -0777 -pi -e 's/\n(.)/, \1/g'`"
    if test "x$_itemAssigned" = "x" ; then
	_itemAssigned="`echo \"$_annotate\" | grep -A 1 -E \" *$_revision\" | grep '\- ' | grep -v '\-\-' | perl -pi -e 's/.* *- *([^-]*)/\1/g' | perl -0777 -pi -e 's/,/\n/g' | sed 's/^ *\(.*\) *$/\1/g' | sort | uniq | perl -0777 -pi -e 's/\n(.)/, \1/g'`"
    fi
    [ $VERBOSE -gt 1 ] && echo "${_revision}: Assigned is $_itemAssigned"
    _itemLine="$_itemLine,\"$_itemAssigned\""

    # submitter is unknown (could try to extract from comments)
    _itemLine="$_itemLine,Unknown"

    # do not know date submitted
    _itemLine="$_itemLine,\"Unknown submission/request date\""

    # get the date
    _itemDateLastUpdated="`echo \"$_log\" | grep -C2 revision=\\"$_revision\\" | tail -1 | sed 's/.*\([0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]\)T\([0-9][0-9]:[0-9][0-9]\).*/\1 \2/'`"
    [ $VERBOSE -gt 1 ] && echo "${_revision}: Date Last Updated is $_itemDateLastUpdated"
    _itemLine="$_itemLine,$_itemDateLastUpdated"

    # resolution is patched
    _itemLine="$_itemLine,Patched"

    # comment is the log message
    _itemComment="`echo \"$_log\" | grep -A100 revision=\\"$_revision\\" | perl -0777 -pi -e 's/.*?<msg>(.*?)<\/msg>.*/\1/s' | sed 's/\"/\&quot;/g' | perl -0777 -pi -e 's/\n/\r\n/g'`"
    [ $VERBOSE -gt 1 ] && echo "${_revision}: Comment is $_itemComment"
    _itemLine="$_itemLine,\"$_itemComment\""

    # add the item to the output list
    _itemLines="$_itemLines
$_itemLine"
done


###
# format output
###
[ $VERBOSE -gt 0 ] && echo "FORMATTING OUTPUT"
# output a comma-separated value table with the following columns:
echo "TRACKER ID,CATEGORY,DESCRIPTION,PRIORITY,STATUS,ASSIGNED TO,SUBMITTER,DATE SUBMITTED,DATE LAST UPDATED,RESOLUTION,COMMENT" >> $OUTPUT
echo "$_itemLines" | tail -n +2 >> $OUTPUT


###
# clean up
###
[ $VERBOSE -gt 0 ] && echo "CLEANING UP"

# remove our session directory
if [ -d "$_stmp" ] ; then
    if [ "x`ls $_stmp`" = "x" ] ; then
	[ $VERBOSE -gt 1 ] && echo "rm -rf $_stmp"
	rm -rf "$_stmp"
    else
	echo "Session files found in $_stmp"
    fi

fi

# remove the tracker working dir if this is the last session
if [ "x`ls $_TTMP`" = "x" ] ; then
    [ $VERBOSE -gt 1 ] && echo "rmdir $_TTMP"
    rmdir "$_TTMP"
else
    [ $VERBOSE -gt 0 ] && echo "Old sessions remaining in $_TTMP"
fi

[ $VERBOSE -gt 0 ] && echo "Done."
exit 0

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
