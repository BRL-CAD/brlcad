#!/bin/sh
#                     L I N K R O O T . S H
# BRL-CAD
#
# Copyright (c) 2007-2011 United States Government as represented by
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
# Script used to create or recreate symbolink links in an installation
# root directory where there are multiple versions installed in the
# format:
#          /usr/brlcad/rel-7.6.2
#          /usr/brlcad/rel-7.8.0
#          /usr/brlcad/rel-7.10.0
#
# In the base root directory (e.g. /usr/brlcad), there are symbolic
# links that point to the "stable" version.  This script creates those
# links including updating the stable link itself, which points to a
# given version.
#
# With the links in place, users only need to ensure that the base
# root directories are in their path. (e.g. /usr/brlcad/bin)
#
# Usage:
#          ./linkroot.sh base_root stable_release
#
# Example:
#          ./linkroot.sh /usr/brlcad /usr/brlcad/rel-7.10.0
#
######################################################################

THIS="$0"

usage ( ) {
    __copyright="`grep Copyright $THIS | head -${HEAD_N}1 | awk '{print $4}'`"
    if [ "x$__copyright" = "x" ] ; then
	__copyright="`date +%Y`"
    fi

    cat <<EOF
    Usage: $0 base_root stable_release

    base_root is a base directory that will contain your links
    stable_release is the root directory to link "stable"

    linkroot.sh installation support script by Christopher Sean Morrison
    revised 3-clause BSD-license, copyright (c) $__copyright
EOF
    exit 1
}


###
# make sure base root is valid
###
BASE="$1"
if [ "x$BASE" = "x" ] ; then
    echo "ERROR: must specify a base root directory"
    usage "$THIS" 1>&2
fi
if [ ! -e "$BASE" ] ; then
    echo "ERROR: the base root path does not exist [$BASE]"
    usage "$THIS" 1>&2
fi
if [ ! -d "$BASE" ] ; then
    echo "ERROR: the base root path is not a directory [$BASE]"
    usage "$THIS" 1>&2
fi


###
# make sure stable is valid
###
STABLE="$2"
if [ "x$STABLE" = "x" ] ; then
    echo "ERROR: must specify a stable root directory"
    usage "$THIS" 1>&2
fi
if [ ! -e "$STABLE" ] ; then
    echo "ERROR: the stable root path does not exist [$STABLE]"
    usage "$THIS" 1>&2
fi
if [ ! -d "$STABLE" ] ; then
    echo "ERROR: the stable root path is not a directory [$STABLE]"
    usage "$THIS" 1>&2
fi


###
# make sure there aren't more arguments following
###
if [ $# -gt 2 ] ; then
    echo "ERROR: too many command-line arguments encountered"
    usage "$THIS" 1>&2
fi


###
# validate that they are not one in the same
###
if [ "$BASE" -ef "$STABLE" ] ; then
    echo "ERROR: STABLE refers to the same directory as BASE.  Nothing to do."
    exit 2
fi


###
# validate that there isn't a conflicting dir or file in the way
###
for dir in "$BASE/bin" "$BASE/include" "$BASE/lib" "$BASE/man" "$BASE/share" ; do
    if [ -e "$dir" ] ; then
	if [ -f "$dir" ] ; then
	    echo "ERROR: $dir is a file in the way, cannot create symlink"
	    exit 2
	fi
	if [ -d "$dir" ] ; then
	    echo "ERROR: $dir is a directory in the way, cannot create symlink"
	    exit 2
	fi
	if [ ! -L "$dir" ] ; then
	    echo "WARNING: $dir exists but is not a symbolic link"
	fi
    fi
done


###
# done validation, summarize
###
echo "###"
echo "#   BASE = $BASE"
echo "# STABLE = $STABLE"
echo "###"


###
# create links to stable
###
cmd='ln -s stable/bin     "$BASE/bin"'
echo "$cmd"
eval "$cmd"
cmd='ln -s stable/include "$BASE/include"'
echo "$cmd"
eval "$cmd"
cmd='ln -s stable/lib     "$BASE/lib"'
echo "$cmd"
eval "$cmd"
cmd='ln -s stable/man     "$BASE/man"'
echo "$cmd"
eval "$cmd"
cmd='ln -s stable/share   "$BASE/share"'
echo "$cmd"
eval "$cmd"


###
# create stable link
###
if [ "$BASE/`basename $STABLE`" -ef "$STABLE" ] ; then
    cmd='ln -s "`basename $STABLE`"      "$BASE/stable"'
else
    cmd='ln -s "$STABLE"      "$BASE/stable"'
fi
echo "$cmd"
eval "$cmd"


# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
