#!/bin/sh
#                      E L A P S E D . S H
# BRL-CAD
#
# Copyright (c) 2004-2011 United States Government as represented by
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
# Compute the amount of time elapsed and format the results for
# printing.  Script takes three arguments: a number for the hour,
# minute, and seconds.  Presently does not support times that
# span days or multiple days.
#
# sh elapsed.sh 12 03 24
#   or
# sh elapsed.sh `date`
#   or
# some_date=`date -R` sh elapsed.sh $some_date
#   etc.
#
# Alternatively, the script can be used as a utility function in
# scripts.  With the --seconds argument, the number of seconds that
# have elapsed since the given date is output as a single number
# instead of "pretty printing" the elapsed time.  This is often useful
# for scripts that need to track elapsed time.  Another special mode
# is when you only pass a single number to the script, it is presumed
# to be an elapsed number of seconds that you simply wish to "pretty
# print".  The --seconds and single number mode complement each other.
#
# sh elapsed.sh --seconds $some_prev_date
#   or
# sh elapsed.sh 542
#   or
# sh elapsed.sh `sh elapsed.sh --seconds $some_prev_date`
#
# Author -
#   Christopher Sean Morrison
#
###

ARGS="$*"
ARG_1="$1"
ARG_2="$2"
ARG_4="$4"
ARG_5="$5"
CONFIG_TIME="$ARGS"

if test "x$ONLY_SECONDS" = "x" ; then
    ONLY_SECONDS=
fi
if test "x$DEBUG" = "x" ; then
    DEBUG=
fi

for arg in $ARGS ; do
    case "x$arg" in
	x-[sS]) ONLY_SECONDS=yes ; shift ;;
	x--[sS]) ONLY_SECONDS=yes ; shift ;;
	x-[sS][eE][cC][oO][nN][dD][sS]) ONLY_SECONDS=yes ; shift ;;
	x--[sS][eE][cC][oO][nN][dD][sS]) ONLY_SECONDS=yes ; shift ;;
	x-[dD]) DEBUG=yes ; shift ;;
	x--[dD]) DEBUG=yes ; shift ;;
	x-[dD][eE][bB][uU][gG]) DEBUG=yes ; shift ;;
	x--[dD][eE][bB][uU][gG]) DEBUG=yes ; shift ;;
	x-*)
	    echo "Usage: $0 [-s|--seconds] [-d|--debug] {RFC 2822 DATE | UNIX DATE | HOUR MIN SEC | SECONDS}" 1>&2
	    echo "Unrecognized option [$1]"
	    exit 1
	    ;;
	x*)
	    break
	    ;;
    esac
done

# done processing args, reset indices after shifting
ARGS="$*"
ARG_1="$1"
ARG_2="$2"
ARG_4="$4"
ARG_5="$5"
CONFIG_TIME="$ARGS"

if test "x$DEBUG" = "xyes" ; then
    echo "Debug output is enabled (DEBUG=[yes])"
    echo "ARGS=[$ARGS]"
    echo "ARG_1=[$ARG_1]"
    echo "ARG_2=[$ARG_2]"
    echo "ARG_4=[$ARG_4]"
    echo "ARG_5=[$ARG_5]"
    echo "CONFIG_TIME=[$CONFIG_TIME]"
    echo "ONLY_SECONDS=[$ONLY_SECONDS]"
fi

# force locale setting to C so things like date output as expected
LC_ALL=C

# make sure an argument is given
if test "x$CONFIG_TIME" = "x" ; then
    usage="Usage: $0 time"
    echo "$usage" 1>&2
    exit 1
fi

# if there is no second argument, assume it's just the time
if test "x$ARG_2" = "x" ; then
    CONFIG_TIME="`echo $ARGS | tr : ' '`"
fi

# check if this is a standard unix or iso date string based on length
if test `echo $ARGS | wc | awk '{print $2}'` -eq 6 ; then
    if test `echo $ARGS | awk '{print $4}' | tr : ' ' | wc | awk '{print $2}'` -eq 3 ; then
	if test "x$DEBUG" = "xyes" ; then
	    echo "ARGS appears to be a UNIX date string"
	fi
	CONFIG_TIME="`echo $ARGS | awk '{print $4}' | tr : ' '`"
    elif test `echo $ARGS | awk '{print $5}' | tr : ' ' | wc | awk '{print $2}'` -eq 3 ; then
	if test "x$DEBUG" = "xyes" ; then
	    echo "ARGS appears to be an RFC 2822 date string"
	fi
	CONFIG_TIME=`echo $ARGS | awk '{print $5}' | tr : ' '`
    else
	if test "x$DEBUG" = "xyes" ; then
	    echo "ARGS was not recognized as an RFC 2822 or UNIX date string"
	fi
    fi
fi

# parse the end time and convert to a seconds count
post_conf_time="`date '+%H %M %S'`"
post_hour="`echo $post_conf_time | awk '{print $1}'`"
post_min="`echo $post_conf_time | awk '{print $2}'`"
post_sec="`echo $post_conf_time | awk '{print $3}'`"
hour_seconds_after="`expr $post_hour \* 60 \* 60`"
min_seconds_after="`expr $post_min \* 60`"
total_post="`expr $hour_seconds_after + $min_seconds_after + $post_sec`"

if test "x$CONFIG_TIME" = "x" ; then
    CONFIG_TIME="$post_conf_time"
fi

# parse the start time and convert to a seconds count
pre_hour="`echo $CONFIG_TIME | awk '{print $1}'`"
pre_min="`echo $CONFIG_TIME | awk '{print $2}'`"
pre_sec="`echo $CONFIG_TIME | awk '{print $3}'`"

if test "x$pre_sec" = "x" ; then
    if test "x$pre_min" = "x" ; then
	# presume single value in CONFIG_TIME is elapsed seconds
	if test ! "x$pre_hour" = "x" ; then
	    pre_sec="`expr $post_sec - $pre_hour`"
	    pre_hour=""
	fi
    fi
fi

if test "x$pre_hour" = "x" ; then
    pre_hour="$post_hour"
fi
if test "x$pre_min" = "x" ; then
    pre_min="$post_min"
fi
if test "x$pre_sec" = "x" ; then
    pre_sec="$post_sec"
fi

if test "x$DEBUG" = "xyes" ; then
    echo "Parsed CONFIG_TIME=[$CONFIG_TIME]"
    echo "pre_hour=[$pre_hour]"
    echo "pre_min=[$pre_min]"
    echo "pre_sec=[$pre_sec]"
fi

hour_seconds_before="`expr $pre_hour \* 60 \* 60`"
min_seconds_before="`expr $pre_min \* 60`"
total_pre="`expr $hour_seconds_before + $min_seconds_before + $pre_sec`"

# if the end time is smaller than the start time, we have gone back in
# time so assume that the clock turned over a day.
if test $total_post -lt $total_pre ; then
    total_post="`expr $total_post + 86400`"
fi

# break out the elapsed time into seconds, minutes, and hours
sec_elapsed="`expr $total_post - $total_pre`"
# echo sec_elapsed is $total_post - $total_pre 1>&2

if test "x$DEBUG" = "xyes" ; then
    echo "Seconds elapsed is $sec_elapsed ($total_post - $total_pre)"
fi

# if we only need to report the number of seconds elapsed, then we're done
if test "x$ONLY_SECONDS" = "xyes" ; then
    echo "$sec_elapsed"
    exit 0
fi

min_elapsed="0"
if test ! "x$sec_elapsed" = "x0" ; then
    min_elapsed="`expr $sec_elapsed / 60`"
    sec_elapsed2="`expr $min_elapsed \* 60`"
    sec_elapsed="`expr $sec_elapsed - $sec_elapsed2`"
fi
hour_elapsed="0"
if test ! "x$min_elapsed" = "x0" ; then
    hour_elapsed="`expr $min_elapsed / 60`"
    min_elapsed2="`expr $hour_elapsed \* 60`"
    min_elapsed="`expr $min_elapsed - $min_elapsed2`"
fi

# generate a human-readable elapsed time message
time_elapsed=""
if test ! "x$hour_elapsed" = "x0" ; then
    if test "x$hour_elapsed" = "x1" ; then
	time_elapsed="$hour_elapsed hour"
    else
	time_elapsed="$hour_elapsed hours"
    fi
fi
if test ! "x$min_elapsed" = "x0" ; then
    if test ! "x$time_elapsed" = "x" ; then
	time_elapsed="${time_elapsed}, "
    fi
    time_elapsed="${time_elapsed}${min_elapsed}"
    if test "x$min_elapsed" = "x1" ; then
	time_elapsed="${time_elapsed} minute"
    else
	time_elapsed="${time_elapsed} minutes"
    fi
fi
if test ! "x$sec_elapsed" = "x0" ; then
    if test ! "x$time_elapsed" = "x" ; then
	time_elapsed="${time_elapsed}, "
    fi
    time_elapsed="${time_elapsed}${sec_elapsed}"
    if test "x$sec_elapsed" = "x1" ; then
	time_elapsed="${time_elapsed} second"
    else
	time_elapsed="${time_elapsed} seconds"
    fi
fi
if test "x$time_elapsed" = "x" ; then
    time_elapsed="0 seconds"
fi

# output the time elapsed
echo "$time_elapsed"

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
