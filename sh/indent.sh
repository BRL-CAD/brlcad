#!/bin/sh
#                       I N D E N T . S H
# BRL-CAD
#
# Copyright (C) 2005 United States Government as represented by
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
# indent.sh runs emacs in batch mode to automatically indent source.
#
# Author -
#   Christopher Sean Morrison
#
# Source -
#   The U.S. Army Research Laboratory
#   Aberdeen Proving Ground, Maryland 21005-5068  USA
###

# locate ourselves for generating a file list
findgen="$1"

if [ ! -d "$findgen" ] ; then

    if [ -r "`dirname $0`/../configure.ac" ] ; then
	findgen="`dirname $0`/.."
    else
	for dir in . .. brlcad ; do
	    if [ -r "$dir/configure.ac" ] ; then
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

bir="batch-indent-region.el"
bir_dir="."
if [ -r "$bir_dir/$bir" ] ; then
    bir_dir="$findgen/misc"
elif [ -r "./$bir" ] ; then
    bir_dir="."
elif [ -r "../$bir" ] ; then
    bir_dir=".."
elif [ -r "./misc/$bir" ] ; then
    bir_dir="./misc"
elif [ -r "../misc/$bir" ] ; then
    bir_dir="../misc"
fi
if [ ! -f "$bir_dir/$bir" ] ; then
    echo "ERROR: Unable to find the batch-indent-region lisp script"
    echo "       Searched for $bir in:"
    echo "         $findgen/misc:.:..:misc:../misc"
    exit 1
fi

# generate a list of files to check, excluding directories that are
# not BRL-CAD sources, CVS, or start with a dot among other files.
# have to take care if including shell scripts; look for mistakes in
# here/now documents.
files="`find $findgen -type f -and \( \
                                     -name '*.c' -or \
                                     -name '*.h' -or \
                                     -name '*.tcl' -or \
                                     -name '*.tk' -or \
                                     -name '*.itcl' -or \
                                     -name '*.itk' -or \
                                     -name '*.pl' -or \
                                     -name '*.java' -or \
                                     -name '*.el' -or \
                                     -name '*.f' -or \
                                     -name '*.m4' \
                                   \) | \
        grep -v 'other/' | \
        grep -v 'doc/' | \
        grep -v 'CVS' | \
        grep -v 'misc/' | \
        grep -v \"$findgen/\.\" | \
        grep -v 'autom4te.cache' \
    `"

for file in $files ; do
    echo "=== BEGIN ===> $file"

    # sanity checks
    if [ ! -f "$file" ] ; then
	echo "."
	echo "WARNING: $file was lost, skipping"
	continue
    elif [ ! -r "$file" ] ; then
	echo "."
	echo "WARNING: $file is not readable"
	continue
    elif [ ! -w "$file" ] ; then
	echo "."
	echo "WARNING: $file is not writeable"
	continue
    fi

    emacs -batch -l $bir_dir/$bir -f batch-indent-region $file

    # done iteration over files
done

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
