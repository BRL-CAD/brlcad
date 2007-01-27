#!/bin/sh
#			    W S . S H
# BRL-CAD
#
# Copyright (c) 2007 United States Government as represented by
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
# The purpose of this script is to update source files in a project so
# that they have more consistent whitespace (aka "ws") formatting.
#
# The list of things the script can perform are as follows:
#
#   a) removes whitespace on lines that only have whitespace.
#
#   b) removes whitespace at the end of any line.
#
#   c) removes successive blanks lines so there are no more than two
#      in a row anywhere in a given file.  WS_SUCCESSIVE=2
#
#   d) removes all blank lines at the end of a file.
#
#   e) ensures that there is a newline at the end of file.
#
#   f) expand embedded tabs to spaces. (DISABLED BY DEFAULT)
#
#   g) unexpands leading whitespace and tabs, inserting tabs at the
#      default system tabstops.	 WS_TABSTOPS=8
#
#   *) show progress.  WS_PROGRESS=yes
#
#   *) keep backups if a change is made.  WS_BACKUPS=yes
#
# By default, the script will perform all of the above actions.	 You
# can customize the set of changes the script performs by setting the
# WS environment variable to the letter(s) of the task to run.	There
# are also several other WS_* variables that can be set to modify
# default behavior settings.
#
# Examples:
#   # only remove blank lines and whitespace at end of lines
#   WS=abd ./ws.sh some_file
#
#   # reduce collections of blank lines to no more than 4 in a row
#   WS_SUCCESSIVE=2 WS=ac ./ws.sh some_file
#
#   # run all tasks on all files in current directory without backups
#   WS_BACKUPS=no find . -type f -exec ./ws.sh {} \;
#
# Author -
#   Christopher Sean Morrison
#
# Source -
#   BRL-CAD Open Source
###

files="$*"

# default settings
if [ "x$WS" = "x" ] ; then
    # run everything
    # NOTE: step f is disabled
    WS=abcdeghijklmnopqrstuvwxyz
fi
if [ "x$WS_SUCCESSIVE" = "x" ] ; then
    # how many blank lines in a row (max)
    WS_SUCCESSIVE=2
fi
if [ ! "x$WS_TABSTOPS" = "x" ] ; then
    echo "\
Overriding the default *system* tabstop of 8 is highly discouraged for
cross-platform and cross-software compatibility reasons.  Display of
tabs is generally best left at the default, and instead modifying your
software's indentation level or tab *display* width instead.

This script will not change the indentation levels of your file.
There are other/better tools for that job.
"
else
    # default tabtops for inserting real tabs
    WS_TABSTOPS=8
fi
if [ "x$WS_PROGRESS" = "x" ] ; then
    # show progress
    WS_PROGRESS=yes
fi
if [ "x$WS_BACKUPS" = "x" ] ; then
    # keep backups
    WS_BACKUPS=yes
fi


findgen="."
if [ "x$files" = "x" ] ; then
    # locate ourselves for generating a file list
    if [ -f "`dirname $0`/../configure.ac" ] ; then
	findgen="`dirname $0`/.."
    else
	for dir in . .. brlcad ; do
	    if [ -f "$dir/configure.ac" ] ; then
		findgen="$dir"
		break
	    fi
	done
    fi

    # sanity check
    if [ ! -d "$findgen/sh" ] ; then
	echo "ERROR: Unable to find our path relative to configure.ac"
	exit 1
    fi
fi

# generate a list of files to check, excluding directories that are
# not BRL-CAD sources, CVS, or start with a dot among other files.
# have to take care if including shell scripts; look for mistakes in
# here/now documents.  this is, if no file arguments were provided.
if [ "x$files" = "x" ] ; then
    files="`find $findgen -type f -and \( \
	    -name '*.[0-9n]' -or \
	    -name '*.[aA][cC]' -or \
	    -name '*.[aA][mM]' -or \
	    -name '*.[cC]' -or \
	    -name '*.[cC]++' -or \
	    -name '*.[cC][cC]' -or \
	    -name '*.[cC][pP]' -or \
	    -name '*.[cC][pP][pP]' -or \
	    -name '*.[cC][xX][xX]' -or \
	    -name '*.[dD][sS][pP]' -or \
	    -name '*.[eE][lL]' -or \
	    -name '*.[fF]' -or \
	    -name '*.[fF][mM][tT]' -or \
	    -name '*.[hH]' -or \
	    -name '*.[hH][hH]' -or \
	    -name '*.[iI][tT][cC][lL]' -or \
	    -name '*.[iI][tT][kK]' -or \
	    -name '*.[jJ][aA][vV][aA]' -or \
	    -name '*.[mM]' -or \
	    -name '*.[mM]4' -or \
	    -name '*.[mM][mM]' -or \
	    -name '*.[pP][lL]' -or \
	    -name '*.[sS][hH]' -or \
	    -name '*.[tT][cC][lL]' -or \
	    -name '*.[tT][kK]' -or \
	    -name '*.[tT][xX][tT]' -or \
	    -name 'AUTHORS*' -or \
	    -name 'COPYING*' -or \
	    -name 'DEVINFO*' -or \
	    -name 'HACKING*' -or \
	    -name 'NEWS*' -or \
	    -name 'README*' -or \
	    -name 'TODO*' \
	    \) | \
	    grep -v '/other/' | \
	    grep -v '/CVS/' | \
	    grep -v '/.deps/' | \
	    grep -v '/.libs/' | \
	    grep -v 'autom4te.cache' | \
	    grep -v 'aclocal.m4' | \
	    grep -v '.ws.bak' | \
	    grep -v '.ws.expand' | \
	    grep -v '.ws.new' \
	  `"
fi

# successive lines step c preparation
count=0
regex=""
while [ $count -lt $WS_SUCCESSIVE ] ; do
    regex="$regex\n"
    count=$((count + 1))
done
step_c_regex="'s/\n$regex\n*/\n$regex/g'"

# tab expansion step g preparation
count=1
regex1=""
regex2=""
while [ $count -lt $WS_TABSTOPS ] ; do
    regex1="$regex1 ?"
    regex2="$regex2 "
    count=$((count + 1))
done
step_g_regex1="'s/^([\t]*)$regex1\t/\1\t/g'"
step_g_regex2="'s/^([\t]*)[ ]$regex2/\1\t/g'"


# begin processing files
for file in $files ; do

    # show progress
    if [ "x$WS_PROGRESS" = "xyes" ] ; then
	echo -n "$file ... "
    fi

    # sanity checks
    if [ -d "$file" ] ; then
	echo ""
	echo "WARNING: $file is a directory, skipping"
	continue
    elif [ ! -f "$file" ] ; then
	echo ""
	echo "WARNING: $file does not exist, skipping"
	continue
    elif [ ! -r "$file" ] ; then
	echo ""
	echo "WARNING: $file is not readable, skipping"
	continue
    elif [ ! -w "$file" ] ; then
	echo ""
	echo "WARNING: $file is not writeable, skipping"
	continue
    fi

    # process a backup until we're sure a change is made
    if [ -f "$file.ws.new" ] ; then
	echo ""
	echo "WARNING: $file.ws.new was in the way (overwritten)"
	rm -f "$file.ws.new"
    fi
    cp "$file" "$file.ws.new"

    # BEGIN WORK STEPS
    steps="`echo $WS | sed 's/\(.\)/\1 /g'`"
    for step in $steps ; do
	case x$step in
	    x[aA])
		# remove whitespace one lines with only whitespace
	        if [ "x$WS_PROGRESS" = "xyes" ] ; then
		    echo -n "a"
		fi
		perl -pi -e 's/^[ \t]*$//g' "$file.ws.new"
		;;
	    x[bB])
		# remove whitespace at end of all lines
	        if [ "x$WS_PROGRESS" = "xyes" ] ; then
		    echo -n "b"
		fi
		perl -pi -e 's/[ \t]*$//g' "$file.ws.new"
		;;
	    x[cC])
		# remove successive blank lines
	        if [ "x$WS_PROGRESS" = "xyes" ] ; then
		    echo -n "c"
		fi
		cmd="perl -0777 -pi -e $step_c_regex \"$file.ws.new\""
		eval "$cmd"
		;;
	    x[dD])
		# remove all blank lines from end of file
	        if [ "x$WS_PROGRESS" = "xyes" ] ; then
		    echo -n "d"
		fi
		perl -0777 -pi -e 's/\n\n*$/\n/' "$file.ws.new"
		;;
	    x[eE])
		# ensure there is a trailing newline
	        if [ "x$WS_PROGRESS" = "xyes" ] ; then
		    echo -n "e"
		fi
		perl -0777 -pi -e 's/\([^\n]\)$/\1\n/' "$file.ws.new"
		;;
	    x[fF])
 		# convert embedded tabs to spaces
	        if [ "x$WS_PROGRESS" = "xyes" ] ; then
		    echo -n "f"
		fi
		if [ -f "$file.ws.expand" ] ; then
		    echo "WARNING: $file.ws.expand was in the way (overwritten)"
		    rm -f "$file.ws.expand"
		fi
		# copy before redirect so permissions are retained
		cp -p "$file.ws.new" "$file.ws.expand"
		expand -t $WS_TABSTOPS "$file.ws.new" > "$file.ws.expand"
		mv "$file.ws.expand" "$file.ws.new"
		;;
	    x[gG])
 		# convert leading whitespace and tabs, insert tabs
	        if [ "x$WS_PROGRESS" = "xyes" ] ; then
		    echo -n "g"
		fi
		cmd1="perl -pi -e $step_g_regex1 \"$file.ws.new\""
		cmd2="perl -pi -e $step_g_regex2 \"$file.ws.new\""

		# regex woes, nasty hack -- iterate to clean up
		# indentation one tab at a time.
	        count=0
		while [ $count -lt 10 ] ; do
		    eval "$cmd1"
		    eval "$cmd2"
		    count="$((count + 1))"
		done
		;;
	esac

    done

    # if the file changed, move it into place and keep a backup
    filediff="`diff $file $file.ws.new`"
    if [ "x$filediff" = "x" ] ; then
	if [ "x$WS_PROGRESS" = "xyes" ] ; then
	    echo ""
	fi
	rm -f "$file.ws.new"
    else
	if [ "x$WS_PROGRESS" = "xyes" ] ; then
	    echo " ... FILE CHANGED"
	fi
	if [ "x$WS_BACKUPS" = "xyes" ] ; then
	    if [ -f "$file.ws.bak" ] ; then
		echo ""
		echo "WARNING: $file.ws.bak was in the way (overwritten)"
		rm -f "$file.ws.bak"
	    fi
	    mv "$file" "$file.ws.bak"
	fi
	mv "$file.ws.new" "$file"
    fi

done # iteration over files

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
