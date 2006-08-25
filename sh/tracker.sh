#!/bin/sh
#                      T R A C K E R . S H
# BRL-CAD
#
# Copyright (c) 2006 United States Government as represented by
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
# This script interacts with the Sourceforge website and extracts data
# from the available trackers.  The tracker data is conglomerated into
# a comma separate value file suitable for import into a spreadsheet
# application.
#
# Example use that provides progress status, includes no closed
# tracker items, and saves the data to a file named trackers.csv:
#
#   ./tracker.sh -v -o trackers.csv -c 0 brlcad
#
# Author -
#   Christopher Sean Morrison
#
# Source -
#   BRL-CAD Open Source
###

SELF="$0"
PROJECT="brlcad"
SITE="http://sourceforge.net"
OUTPUT="output.csv"
CLOSED="45"
DELETED="no"

###
# utility functions
###


# identify
#
# prints out identifying information for this utility.
identify ( ) {
    echo ""
    echo "**************************************************"
    echo "*            t  r  a  c  k  e  r                 *"
    echo "* A tool for extracting Sourceforge tracker data *"
    echo "*     Author: Christopher Sean Morrison          *"
    echo "*    Version 2006.3, BRL-CAD BSD License         *"
    echo "*              devs@brlcad.org                   *"
    echo "**************************************************"
    echo ""
}


# usage
#
# prints out basic application usage information including identify
# version information and an example use.
usage ( ) {
    identify
    printf "Usage: $SELF [-? | -h | --help] [-V | --version] [-v | --verbose] [-o | --output {file}] [-c | --closed {days}] [-d | --deleted] [project]\n\n"
    printf "\-h      \tthis usage help\n"
    printf "\-V      \tdisplay version information"
    printf "\-v      \tbasic progress output to console\n"
    printf "\-v -v   \tadds output of commands to console\n"
    printf "\-v -v -v\tadds output of tracker html content to files\n"
    printf "\-v -v -v -v\tadds output of tracker html content to console\n"
    printf "\-o file \tfile name to output comma-separate values (default: $OUTPUT)\n"
    printf "\-c days \toutput items closed within specified number of days (default: $CLOSED)\n"
    printf "\-d      \tinclude deleted items in the output\n"
    printf "project  \tSourceforge project to extract data from (default: $PROJECT)\n\n"
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
    if [ $# -ne 3 ] ; then
	_yer="`date +%Y`"
	_mon="`date +%m`"
	_day="`date +%d`"
    else
	_yer="$1"
	_mon="$2"
	_day="$3"
    fi

    if ! isint $_yer ; then
	echo "ERROR: $_yer is not a number"
	return 1
    fi
    if ! isint $_mon ; then
	echo "ERROR: $_mon is not a number"
	return 2
    fi
    if ! isint $_day ; then
	echo "ERROR: $_day is not a number"
	return 3
    fi
    [ $_yer -lt 1970 ] && echo "ERROR: $_yer is before the 1970 epoch" && return 4

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


# decode1 {string}
#
# makes a single pass over a given string, decoding a subset of ISO
# 10646 / ISO 8879 html entities and then providing the converted
# string as output.
decode1 ( ) {
    echo "$1" | sed \
	-e 's/&quot;/"/g' \
	-e 's/&\#0*34;/"/g' \
	-e 's/&amp;/\&/g' \
	-e 's/&\#0*38;/\&/g' \
	-e 's/&lt;/</g' \
	-e 's/&\#0*60;/</g' \
	-e 's/&gt;/>/g' \
	-e 's/&\#0*62;/>/g' \
	-e 's/&nbsp;/ /g' \
	-e 's/&\#160;/ /g' \
	-e 's/&auml;/ä/g' \
	-e 's/&ouml;/ö/g' \
	-e 's/&uuml;/ü/g' \
	-e 's/&szlig;/ß/g' \
	-e 's/&Auml;/Ä/g' \
	-e 's/&Ouml;/Ö/g' \
	-e 's/&Uuml;/Ü/g' \
	-e 's/&#322;/Ō/g' \
	-e 's/&#347;/ś/g' \
	-e 's/&\#0*39;/'\''/g'
    return 0;
}


# decode {string}
#
# makes multiple passes over an input string, decoding a subset of ISD
# 10646 / ISO 8879 html entities.  this allows patterns such as
# &amp;quot; to decode it to a single quote character instead of &quot;
decode ( ) {
    _old=""
    _new="$1"
    _limit=10
    while [ "x$_old" != "x$_new" -a $_limit -gt 0 ] ; do
	_old="$_new"
	_new="`decode1 \"$_old\"`"
	_limit=$(($_limit - 1))
    done
    echo "$_new"
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
	    [ $VERBOSE -gt 0 ] && echo "Exporting tracker data to $OUTPUT"
	    ;;
	x-c|x--c*)
	    shift
	    CLOSED="$1"
	    [ $VERBOSE -gt 0 ] && echo "Items closed within $CLOSED days will be exported"
	    ;;
	x-d|x--d*)
	    DELETED="yes"
	    [ $VERBOSE -gt 0 ] && echo "Deleted items will be exported"
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
	    [ $VERBOSE -gt 0 ] && echo "Extracting tracker data for project: $PROJECT"
    esac
    shift
done


# validate settings, semi-private variables
if ! isint $VERBOSE ; then
    echo "VERBOSE is somehow not a number!  Setting to 1."
    VERBOSE=1
fi

_SFURL="${SITE}/projects/${PROJECT}/"
_SID=$$
_TTMP=/tmp/tracker.sh

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

# make sure curl works
[ $VERBOSE -gt 1 ] && echo "curl --version > /dev/null 2>&1"
curl --version > /dev/null 2>&1
if [ $? -ne 2 ] ; then
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
# get the main project page
###
[ $VERBOSE -gt 0 ] && echo "GETTING THE MAIN PROJECT PAGE"

# get the main page data
# XXX does not presently deal with redirects like sf.net
[ $VERBOSE -gt 1 ] && echo "curl $_SFURL 2>/dev/null"
_main="`curl $_SFURL 2>/dev/null`"

# log the main page data
if [ $VERBOSE -gt 2 ] ; then
    echo "Wrote out html data for main project page $_SFURL to:"
    printf "\t$_stmp/${PROJECT}.html\n"
    echo "$_main" > "$_stmp/${PROJECT}.html"
fi
if [ $VERBOSE -gt 3 ] ; then
    echo "begin curl results for $_SFURL:"
    echo "$_main"
    echo "end of curl results for $_SFURL"
fi


###
# determine what trackers are available
###
[ $VERBOSE -gt 0 ] && echo "DETERMINING WHAT TRACKERS ARE AVAILABLE"

# extract the tracker page from the main page data
_trackerURL=`echo "$_main" | grep /tracker/ | grep Tracker | cut -d'"' -f2`
[ $VERBOSE -gt 1 ] && echo "tracker URL is $_trackerURL"

# extract the project group id from the tracker page url
_groupID=`echo "$_trackerURL" | cut -d= -f2`
[ $VERBOSE -gt 1 ] && echo "project group ID is $_groupID"

# get the tracker page data
[ $VERBOSE -gt 1 ] && echo "curl $SITE/$_trackerURL 2>/dev/null"
_tracker="`curl $SITE/$_trackerURL 2>/dev/null`"

# log the tracker page data
if [ $VERBOSE -gt 2 ] ; then
    echo "Wrote out html data for tracker $SITE/$_trackerURL to:"
    printf "\t$_stmp/${_groupID}.html\n"
    echo "$_tracker" > "$_stmp/${_groupID}.html"
fi
if [ $VERBOSE -gt 3 ] ; then
    echo "begin curl results for $SITE/$_trackerURL:"
    echo "$_tracker"
    echo "end curl results for $SITE/$_trackerURL:"
fi

# extract the tracker urls from the tracker page data
[ $VERBOSE -gt 1 ] && echo "echo \"\$_tracker\" | grep /tracker/ | grep browse | cut -d'\"' -f2 | sed 's/&amp;/\&/g'"
_trackers="`echo \"$_tracker\" | grep /tracker/ | grep browse | cut -d'\"' -f2 | sed 's/&amp;/\&/g'`"
if [ $VERBOSE -gt 1 ] ; then
    echo "Tracker URLS:"
    printf "\t%s\n" $_trackers
fi


###
# extract data for each tracker
###
[ $VERBOSE -gt 0 ] && echo "EXTRACTING DATA FOR EACH TRACKER"
_totalItemCount=0
_itemURLS=""
for _track in $_trackers ; do
    # extract the tracker ID from the url
    _trackID="`echo $_track | tr '&' '\n' | grep atid | head -1 | sed 's/.*=\([0-9][0-9]*\).*/\1/'`"
    [ $VERBOSE -gt 1 ] && echo "Processing tracker $_trackID"

    # get the individual tracker data
    _trackerTotalCount=0
    _trackerPageCount=0
    while [ $_trackerPageCount -ge 0 ] ; do

	[ $VERBOSE -gt 1 ] && echo "curl -d set=custom -d _status=100 -d offset=$_trackerPageCount $SITE/$_track 2>/dev/null"
	_trackData="`curl -d set=custom -d _status=100 -d offset=$_trackerPageCount $SITE/$_track 2>/dev/null`"

        # log the individual tracker group data
	if [ $VERBOSE -gt 2 ] ; then
	    echo "Wrote out html data for tracker group $SITE/$_track to:"
	    printf "\t$_stmp/${_trackID}.html\n"
	    echo "$_trackData" > "$_stmp/${_trackID}.html"
	fi
	if [ $VERBOSE -gt 3 ] ; then
	    echo "begin curl results for $SITE/$_track"
	    echo "$_trackData"
	    echo "end curl results for $SITE/$_track"
	fi

        # extract the tracker title from the individual tracker data
	_trackTitle="`echo \"$_trackData\" | grep '<title>' | cut -d: -f2 | sed 's/[ ]*<\/title>//' | sed 's/^[ ]*//'`"
	[ $VERBOSE -gt 0 ] && echo "Processing $_trackTitle (tracker $_trackID)"

        # get the item urls
	[ $VERBOSE -gt 1 ] && echo "echo \"\$_trackData\" | grep /tracker/ | grep detail | cut -d'\"' -f2 | sed 's/&amp;/\&/g'"
	_items="`echo \"$_trackData\" | grep /tracker/ | grep detail | cut -d'\"' -f2 | sed 's/&amp;/\&/g'`"
	if [ $VERBOSE -gt 1 ] ; then
	    echo "Item URLs for $_trackTitle:"
	    printf "\t%s\n" $_items
	fi
	_itemURLS="$_itemURLS $_items"

	# count how many we have
	_trackerPageCount=0
	for _item in $_items ; do
	    _trackerTotalCount=$(($_trackerTotalCount + 1))
	    _trackerPageCount=$(($_trackerPageCount + 1))
	done

	# SF.net provides in sets of 50, no sense pulling up the next
	# page if we got less than 50
	if [ $_trackerPageCount -lt 50 ] ; then
	    _trackerPageCount=-1
	else
	    _trackerPageCount=$_trackerTotalCount
	fi
    done  # while there are tracker items on the page

    # tally up our total item count so far
    [ $VERBOSE -gt 0 ] && echo "Found $_trackerTotalCount $_trackTitle"
    _totalItemCount=$(($_totalItemCount + $_trackerTotalCount))

done  # for each tracker

[ $VERBOSE -gt 0 ] && echo "Fount $_totalItemCount tracker items total"

###
# extract data for each tracker item
###
count=0
wrote=0
_itemLine=""
_itemLines=""
for _item in $_itemURLS ; do
    count=$(($count + 1))

    # extract the item ID from the url
    _itemID="`echo $_item | tr '&' '\n' | grep aid | head -1 | sed 's/.*=\([0-9][0-9]*\).*/\1/'`"
    _trackID="`echo $_item | tr '&' '\n' | grep atid | head -1 | sed 's/.*=\([0-9][0-9]*\).*/\1/'`"
    [ $VERBOSE -gt 1 ] && echo "Processing item $_itemID (tracker $_trackID)"
    _itemLine="$_itemID"

    # get the individual item data
    [ $VERBOSE -gt 1 ] && echo "curl $SITE/$_item 2>/dev/null"
    _itemData="`curl $SITE/$_item 2>/dev/null`"

    # log the individual item data
    if [ $VERBOSE -gt 2 ] ; then
	echo "Wrote out html data for item $SITE/$_item to:"
	printf "\t$_stmp/${_trackID}.${_itemID}.html\n"
	echo "$_itemData" > "$_stmp/${_trackID}.${_itemID}.html"
    fi
    if [ $VERBOSE -gt 3 ] ; then
	echo "begin curl results for $SITE/$_item"
	echo "$_itemData"
	echo "end curl results for $SITE/$_item"
    fi

    # extract the tracker title from the individual tracker data
    _itemTitle="`echo \"$_itemData\" | grep '<title>' | sed 's/.*<title>SourceForge.net: Detail: \(.*\)<\/title>.*/\1/'`"
    _itemTitle="`decode \"$_itemTitle\"`"
    [ $VERBOSE -gt 0 ] && echo "Processing $count of $_totalItemCount: $_itemTitle"

    # extract the type
    _itemType="`echo \"$_itemData\" | grep -C10 Summary | grep selected | grep -v View | grep -v Browse | head -1 | sed 's/.*<a[^>]*>\([^<]*\)<\/a>.*/\1/'`"
    [ $VERBOSE -gt 1 ] && echo "${_itemID}: Type is $_itemType"
    _itemLine="$_itemLine,$_itemType"

    # extract the description
    _itemDesc="`echo \"$_itemData\" | grep $_itemID | grep '<h2>' | sed 's/.*<h2>.*] \(.*\)<\/h2>/\1/'`"
    # clean up CSV output: replace double quote char with double double quotes, amp with &, quot with "", and #039 with '
    _itemDesc="`decode \"$_itemDesc\"`"
    _itemDesc="`echo \"$_itemDesc\" | sed 's/\"/\"\"/g'`"
    _itemDesc="`echo \"$_itemDesc\" | sed 's/^[[:space:]]*//' | sed 's/[[:space:]]$//'`"
    [ $VERBOSE -gt 1 ] && echo "${_itemID}: Description is $_itemDesc"
    _itemLine="$_itemLine,\"$_itemDesc\""

    # extract the priority
    _itemPriority="`echo \"$_itemData\" | grep -C5 'Priority:'`"
    _itemPriority="`echo $_itemPriority | sed 's/.*Priority: <a[^>]*>[^<]*<\/a><\/b> <br> \([0-9]\).*/\1/'`"
    [ $VERBOSE -gt 1 ] && echo "${_itemID}: Priority is $_itemPriority"
    _itemLine="$_itemLine,$_itemPriority"

    # extract the status
    _itemStatus="`echo \"$_itemData\" | grep -C5 'Status:'`"
    _itemStatus="`echo $_itemStatus | sed 's/.*Status: <a[^>]*>[^<]*<\/a><\/b> <br> \([[:alpha:]][[:alpha:]]*\)[^[:alpha:]].*/\1/'`"
    [ $VERBOSE -gt 1 ] && echo "${_itemID}: Status is $_itemStatus"
    _itemLine="$_itemLine,$_itemStatus"

    # extract the assigned
    _itemAssigned="`echo \"$_itemData\" | grep -C5 'Assigned To:'`"
    _itemAssigned="`echo $_itemAssigned | sed 's/.*Assigned To: <a[^>]*>[^<]*<\/a><\/b> <br> \([^<][^<]*\)<.*/\1/'`"
    _itemAssigned="`decode \"$_itemAssigned\"`"
    _itemAssigned="`echo \"$_itemAssigned\" | sed 's/^[[:space:]]*//' | sed 's/[[:space:]]$//'`"
    [ $VERBOSE -gt 1 ] && echo "${_itemID}: Assigned is $_itemAssigned"
    _itemLine="$_itemLine,\"$_itemAssigned\""

    # extract the submitter
    _itemSubmitter="`echo \"$_itemData\" | grep -C5 'Submitted By:'`"
    _itemSubmitter="`echo $_itemSubmitter | sed 's/.*Submitted By:<\/b> <br> \([^<][^<]*\)[[:space:]]*- <.*/\1/'`"
    _itemSubmitter="`decode \"$_itemSubmitter\"`"
    _itemSubmitter="`echo \"$_itemSubmitter\" | sed 's/^[[:space:]]*//' | sed 's/[[:space:]]$//'`"
    [ $VERBOSE -gt 1 ] && echo "${_itemID}: Submitter is $_itemSubmitter"
    _itemLine="$_itemLine,\"$_itemSubmitter\""

    # extract the date submitted
    _itemDateSubmitted="`echo \"$_itemData\" | grep -C5 'Date Submitted:'`"
    _itemDateSubmitted="`echo $_itemDateSubmitted | sed 's/.*Date Submitted:<\/b> <br> \([^<][^<]*\)<.*/\1/'`"
    [ $VERBOSE -gt 1 ] && echo "${_itemID}: Date Submitted is $_itemDateSubmitted"
    _itemLine="$_itemLine,$_itemDateSubmitted"

    # extract the date last updated
    _itemDateLastUpdated="`echo \"$_itemData\" | grep -C5 'Date Last Updated:'`"
    _itemDateLastUpdated="`echo $_itemDateLastUpdated | sed 's/.*Date Last Updated:<\/b> <br> \([^<][^<]*\)<.*/\1/'`"
    [ $VERBOSE -gt 1 ] && echo "${_itemID}: Date Last Updated is $_itemDateLastUpdated"
    _itemLine="$_itemLine,$_itemDateLastUpdated"

    # extract the resolution
    _itemResolution="`echo \"$_itemData\" | grep -C5 'Resolution:'`"
    _itemResolution="`echo $_itemResolution | sed 's/.*Resolution: <a[^>]*>[^<]*<\/a><\/b> <br> \([^<][^<]*\)<.*/\1/'`"
    [ $VERBOSE -gt 1 ] && echo "${_itemID}: Resolution is $_itemResolution"
    _itemLine="$_itemLine,$_itemResolution"

    # if the item is closed, was it closed within CLOSED number of days?
    if [ "x$_itemStatus" = "xClosed" -o "x$_itemStatus" = "xDeleted" ] ; then
	# item is closed or deleted.. figure out how many days ago
	_today="`days`"
	_y="`echo $_itemDateLastUpdated | cut -d- -f1`"
	_m="`echo $_itemDateLastUpdated | cut -d- -f2`"
	_d="`echo $_itemDateLastUpdated | cut -d- -f3 | awk '{print $1}'`"
	_closed="`days $_y $_m $_d`"

	[ $VERBOSE -gt 1 ] && echo "${_itemID} closed $_today - $_closed = $(($_today - $_closed)) days ago"
	if [ $(($_today - $_closed)) -le $CLOSED ] ; then
	    # if this is a deleted item, make sure output was requested
	    if [ "x$DELETED" = "xyes" -o "x$_itemStatus" = "xClosed" ] ; then
	        # add the item to the output list
		_itemLines="$_itemLines
$_itemLine"
		wrote=$(($wrote + 1))
	    fi
	fi
    else
	# add the item to the output list
	_itemLines="$_itemLines
$_itemLine"
	wrote=$(($wrote + 1))
    fi

done
[ $VERBOSE -gt 1 ] && echo "Processed ${count}: wrote $wrote, skipped $(($count - $wrote))"


###
# format output
###
[ $VERBOSE -gt 0 ] && echo "FORMATTING OUTPUT"
# output a comma-separated value table with the following columns:
echo "TRACKER ID,CATEGORY,DESCRIPTION,PRIORITY,STATUS,ASSIGNED TO,SUBMITTER,DATE SUBMITTED,DATE LAST UPDATED,RESOLUTION" >> $OUTPUT
echo "$_itemLines" | tail -n +2 >> $OUTPUT

# Notify where the output was saved regardless of verbosity (sent to stderr)
echo "Tracker data for $wrote items written to $OUTPUT" 1>&2


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
