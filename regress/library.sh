#                      L I B R A R Y . S H
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

# save the precious args
ARGS="$*"
ARG0="$0"
ARG1="$1"
NAME_OF_THIS=`basename $ARG0`
PATH_TO_THIS=`dirname $ARG0`
THIS="$PATH_TO_THIS/$NAME_OF_THIS"
export ARGS ARG0 ARG1 NAME_OF_THIS PATH_TO_THIS THIS

LD_LIBRARY_PATH=../src/other/tcl/unix:../src/other/tk/unix:$LD_LIBRARY_PATH
DYLD_LIBRARY_PATH=../src/other/tcl/unix:../src/other/tk/unix:$DYLD_LIBRARY_PATH
export LD_LIBRARY_PATH DYLD_LIBRARY_PATH


ensearch ( ) {
    ensearch_file="$1"
    ensearch_dirs="$ARG1/src $PATH_TO_THIS/../src ../src/$1 $ARG1/src/$1 $PATH_TO_THIS/../src/$1 ../bin ../src ../src/util ../src/conv ../src/conv/iges ../src/gtools ../src/rt ../bench"

    if test "x$ensearch_file" = "x" ; then
	# nothing to do
	echo "ERROR: no file name specified" 1>&2
	echo ":"
	return
    fi

    for dir in $ensearch_dirs ; do
	ensearch_binary="$dir/$ensearch_file"
	echo "Searching for $ensearch_binary" 1>&2
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
