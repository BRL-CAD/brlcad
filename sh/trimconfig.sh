#                   T R I M C O N F I G . S H
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
# review the function and header checks in configure.ac and make sure
# that they're all actually being used in the source code somewhere.
# this script reports any functions and headers that can probably be
# removed from configure.ac to reduce the number of configure checks.
#
#   ./trimconfig.sh configure.ac
#
###

SELF="$0"
ARGS="$*"

#srcfiles="-name \\\\*\\\\.c -o -name \\\\*\\\\.h -o -name \\\\*\\\\.ac -o -name \\\\*\\\\.cxx -o -name \\\\*\\\\.cpp -o -name \\\\*\\\\.m"
srcfiles="-name '*.c' -o -name '*.h' -o -name '*.ac' -o -name '*.cxx' -o -name '*.cpp' -o -name '*.m'"
excludes="-regex '.*src/other.*'"

if [ "x$ARGS" = "x" ] ; then
    echo "Usage: $SELF /path/to/some/configure.ac"
    exit 1
fi

echo "t r i m c o n f i g . s h"
echo "========================="

echo "This is a script to determine what checks can be removed from a"
echo "configure template."

if [ "x$DEBUG" = "x" ] ; then
    echo "Set DEBUG environment variable to report where checks are found in the"
    echo "sources. (e.g. DEBUG=1 $SELF $ARGS)"
fi
if [ "x$SKIP_FUNCTIONS" = "x" ] ; then
    echo "Set SKIP_FUNCTIONS environment variable to skip AC_CHECK_FUNCS checks"
fi
if [ "x$SKIP_HEADERS" = "x" ] ; then
    echo "Set SKIP_HEADERS environment variable to skip AC_CHECK_HEADERS checks"
fi
if [ "x$SKIP_DEFINES" = "x" ] ; then
    echo "Set SKIP_DEFINES environment variable to skip AC_DEFINE checks"
fi

echo

for cac in $ARGS ; do
    if [ ! -f "$cac" ] ; then
	echo "ERROR: $cac does not exist"
	exit 2
    fi

    if [ "x`grep AC_INIT \"$cac\"`" = "x" ] ; then
	echo "ERROR: $cac does not seem to be a configure template, no AC_INIT found"
	exit 3
    fi

    dir="`dirname $cac`"
    if [ ! -d "$dir" ] ; then
	echo "ERROR: Unable to find the source directory [$dir] for $cac"
	exit 4
    fi

    echo "Processing $cac ... please wait ..."

    cacsrc="`cat \"$cac\" | perl -0777 -lape 's/\\\\\\n//g'`"

    cacconf="`echo \"$cacsrc\" | grep AM_CONFIG_HEADER | grep -v '#'`"
    cacconf="`echo \"$cacconf\" | perl -lape 's/.*AM_CONFIG_HEADER\([[:space:]]*\[?[[:space]]*(.*)/\1/g'`"
    cacconf="`echo \"$cacconf\" | perl -lape 's/[[:space:]]*\]*\)+//g'`"
    cacconf="`echo $cacconf | perl -lape 's/[[:space:]]+/ /g'`"

    if [ "x$SKIP_FUNCTIONS" = "x" ] ; then

	cacfuncs="`echo \"$cacsrc\" | grep AC_CHECK_FUNCS | grep -v '#'`"
	cacfuncs="`echo \"$cacfuncs\" | perl -lape 's/.*AC_CHECK_FUNCS\([[:space:]]*\[?[[:space:]]*(.*)/\1/g'`"
	cacfuncs="`echo \"$cacfuncs\" | perl -lape 's/[[:space:]]*\]*\)+//g'`"
	cacfuncs="`echo $cacfuncs | perl -lape 's/[[:space:]]+/ /g'`"
	
	echo
	echo "Functions being checked"
	echo "-----------------------"
	echo "$cacfuncs"
	echo
	
	for func in $cacfuncs ; do
	    upfunc="`echo $func | tr \"a-z-\" \"A-Z_\"`"
	    upfunc="HAVE_$upfunc"
	    printf "Searching for $upfunc ... "
	    cmd="find $dir -type f \( `echo $srcfiles` \) -not \( `echo $excludes` \) -exec grep \"$upfunc\" {} /dev/null \; | grep -v \"$upfunc[A-Z_]\""
	    found="`eval $cmd`"
	    for conf in $cacconf ; do
		found="`echo \"$found\" | grep -v $conf`"
	    done
	    if [ "x$found" = "x" ] ; then
		echo "NOT FOUND"
	    else
		cnt="`echo \"$found\" | wc -l | awk '{print $1}'`"
		printf "found (%d use" $cnt
		if [ $cnt -eq 1 ] ; then
		    printf ")\n"
		else
		    printf "s)\n"
		fi
	    fi
	    if [ ! "x$DEBUG" = "x" ] ; then
		echo "$found"
		echo
	    fi
	done

    fi # SKIP_FUNCTIONS


    if [ "x$SKIP_HEADERS" = "x" ] ; then

	cacheads="`echo \"$cacsrc\" | grep AC_CHECK_HEADERS | grep -v '#'`"
	cacheads="`echo \"$cacheads\" | perl -lape 's/.*AC_CHECK_HEADERS\([[:space:]]*\[?[[:space:]]*(.*)/\1/g'`"
	cacheads="`echo \"$cacheads\" | perl -lape 's/[[:space:]]*\]*\)+//g'`"
	cacheads="`echo $cacheads | perl -lape 's/[[:space:]]+/ /g'`"
	
	echo
	echo "Headers being checked"
	echo "---------------------"
	echo "$cacheads"
	echo
	
	for head in $cacheads ; do
	    uphead="`echo $head | tr \"a-z-/.\" \"A-Z___\"`"
	    uphead="HAVE_$uphead"
	    printf "Searching for $uphead ... "
	    cmd="find $dir -type f \( `echo $srcfiles` \) -not \( `echo $excludes` \) -exec grep \"$uphead\" {} /dev/null \; | grep -v \"$uphead[A-Z_]\""
	    found="`eval $cmd`"
	    for conf in $cacconf ; do
		found="`echo \"$found\" | grep -v $conf`"
	    done
	    if [ "x$found" = "x" ] ; then
		echo "NOT FOUND"
	    else
		cnt="`echo \"$found\" | wc -l | awk '{print $1}'`"
		printf "found (%d use" $cnt
		if [ $cnt -eq 1 ] ; then
		    printf ")\n"
		else
		    printf "s)\n"
		fi
	    fi
	    if [ ! "x$DEBUG" = "x" ] ; then
		echo "$found"
		echo
	    fi
	done
	
    fi # SKIP_HEADERS

    if [ "x$SKIP_DEFINES" = "x" ] ; then

	cacdefs="`echo \"$cacsrc\" | grep AC_DEFINE | grep -v '#'`"
	cacdefs="`echo \"$cacdefs\" | perl -lape 's/.*(AC_DEFINE|AC_DEFINE_UNQUOTED)\([[:space:]]*\[?[[:space:]]*(.*)/\2/g'`"
	cacdefs="`echo \"$cacdefs\" | perl -lape 's/[[:space:]]*[,\]\)].*//g'`"
	cacdefs="`echo $cacdefs | perl -lape 's/[[:space:]]+/ /g'`"
	
	echo
	echo "Defines being checked"
	echo "---------------------"
	echo "$cacdefs"
	echo
	
	for def in $cacdefs ; do
	    printf "Searching for $def ... "
	    cmd="find $dir -type f \( `echo $srcfiles` \) -not \( `echo $excludes` \) -exec grep \"$def\" {} /dev/null \; | grep -v \"$def[A-Z_]\""
	    found="`eval $cmd`"
	    for conf in $cacconf ; do
		found="`echo \"$found\" | grep -v $conf`"
	    done
	    if [ "x$found" = "x" ] ; then
		echo "NOT FOUND"
	    else
		cnt="`echo \"$found\" | wc -l | awk '{print $1}'`"
		printf "found (%d use" $cnt
		if [ $cnt -eq 1 ] ; then
		    printf ")\n"
		else
		    printf "s)\n"
		fi
	    fi
	    if [ ! "x$DEBUG" = "x" ] ; then
		echo "$found"
		echo
	    fi
	done

    fi # SKIP_DEFINES

    echo "... done processing $cac"
done

# Local Variables:
# tab-width: 8
# mode: sh
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
