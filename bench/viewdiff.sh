#!/bin/sh
#                      R E C H E C K . S H
# BRL-CAD
#
# Copyright (C) 2004-2005 United States Government as represented by
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
# A Shell script to view the differences of a benchmark run,
# displaying the results in a framebuffer window.
#
# The following environment variables will modify how the view diff
# behaves (in case you're not running from a source distribution, for
# example):
#
#   PIX - the directory containing the pix files to be compare
#   PIXDIFF - the name of a pix diff tool
#   PIXFB - the name of a pix to framebuffer tool (e.g. pix-fb)
#
# Authors -
#  Mike Muuss
#  Christopher Sean Morrison
#
#  @(#)$Header$

# Ensure /bin/sh
export PATH || (echo "This isn't sh."; sh $0 $*; kill $$)
path_to_this="`dirname $0`"

echo Looking for benchmark images ...
# find pix reference image directory if we do not already know where
# it is.  PIX environment variable overrides
if test "x${PIX}" = "x" ; then
    if test -f "$path_to_this/../pix/sphflake.pix" ; then
	echo ...found .pix image files in $path_to_this/../pix
	PIX="$path_to_this/../pix"
    elif test -f "$path_to_this/sphflake.pix" ; then
	echo ...found .pix image files in $path_to_this
	PIX="$path_to_this"
    elif test -f "$path_to_this/../share/brlcad/pix/sphflake.pix" ; then
	echo ...found .pix image files in $path_to_this/../share/brlcad/pix
	PIX="$path_to_this/../share/brlcad/pix"
    fi
else
    echo ...using $PIX from PIX environment variable setting
fi


echo Checking for pixel diff utility...
# find pixel diff utility
# PIXDIFF environment variable overrides
if test "x${PIXDIFF}" = "x" ; then
    if test -x $path_to_this/pixdiff ; then
	echo ...found $path_to_this/pixdiff
	PIXDIFF="$path_to_this/pixdiff"
    elif test -x $path_to_this/../src/util/pixdiff ; then
	echo ...found $path_to_this/../src/util/pixdiff
	PIXDIFF="$path_to_this/../src/util/pixdiff"
    else
	if test -f "$path_to_this/../src/util/pixdiff.c" ; then
	    echo ...need to build pixdiff

	    for compiler in $CC gcc cc ; do
		COMPILE="$compiler"

		if test "x$COMPILE" = "x" ; then
		    continue
		fi

		$COMPILE -o pixdiff "$path_to_this/../src/util/pixdiff.c"
		if test "x$?" = "x0" ; then
		    break
		fi
		if test -f "pixdiff" ; then
		    break;
		fi
	    done
	    
	    if test -f "pixdiff" ; then
		echo ...built pixdiff with $COMPILE -o pixdiff pixdiff.c
		PIXDIFF="./pixdiff"
	    fi
	fi
    fi
else
    echo ...using $PIXDIFF from PIXDIFF environment variable setting
fi


echo Checking for pix to framebuffer utility...
# find pix to framebuffer utility
# PIXFB environment variable overrides
if test "x${PIXFB}" = "x" ; then
    if test -x $path_to_this/pix-fb ; then
	echo ...found $path_to_this/pix-fb
	PIXFB="$path_to_this/pix-fb"
    elif test -x $path_to_this/../src/fb/pix-fb ; then
	echo ...found $path_to_this/../src/fb/pix-fb
	PIXFB="$path_to_this/../src/fb/pix-fb"
    else
	if test -f "$path_to_this/../src/fb/pix-fb.c" ; then
	    echo ...need to build pix-fb

	    for compiler in $CC gcc cc ; do
		COMPILE="$compiler"

		if test "x$COMPILE" = "x" ; then
		    continue
		fi

		$COMPILE -o pix-fb "$path_to_this/../src/fb/pix-fb.c"
		if test "x$?" = "x0" ; then
		    break
		fi
		if test -f "pix-fb" ; then
		    break;
		fi
	    done
	    
	    if test -f "pix-fb" ; then
		echo ...built pix-fb with $COMPILE -o pix-fb pix-fb.c
		PIXFB="./pix-fb"
	    fi
	fi
    fi
else
    echo ...using $PIXFB from PIXFB environment variable setting
fi

echo +++++ moss.pix
${PIXDIFF} ${PIX}/moss.pix moss.pix | ${PIXFB}

echo +++++ world.pix
${PIXDIFF} ${PIX}/world.pix world.pix  | ${PIXFB}

echo +++++ star.pix
${PIXDIFF} ${PIX}/star.pix star.pix  | ${PIXFB}

echo +++++ bldg391.pix
${PIXDIFF} ${PIX}/bldg391.pix bldg391.pix  | ${PIXFB}

echo +++++ m35.pix
${PIXDIFF} ${PIX}/m35.pix m35.pix  | ${PIXFB}

echo +++++ sphflake.pix
${PIXDIFF} ${PIX}/sphflake.pix sphflake.pix  | ${PIXFB}

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
