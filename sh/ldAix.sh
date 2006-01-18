#!/bin/sh
#                        L D A I X . S H
# BRL-CAD
#
# Copyright (c) 2004-2006 United States Government as represented by
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
# ldAix ldCmd ldArg ldArg ...
#
# This shell script provides a wrapper for ld under AIX in order to
# create the .exp file required for linking.  Its arguments consist
# of the name and arguments that would normally be provided to the
# ld command.  This script extracts the names of the object files
# from the argument list, creates a .exp file describing all of the
# symbols exported by those files, and then invokes "ldCmd" to
# perform the real link.
#
# RCS: @(#) $Id$

# Extract from the arguments the names of all of the object files.

args=$*
ofiles=""
for i do
    x=`echo $i | grep '[^.].o$'`
    if test "$x" != ""; then
	ofiles="$ofiles $i"
    fi
done

# Extract the name of the object file that we're linking.
outputFile=`echo $args | sed -e 's/.*-o \([^ ]*\).*/\1/'`

# Create the export file from all of the object files, using nm followed
# by sed editing.  Here are some tricky aspects of this:
#
# 1. Nm produces different output under AIX 4.1 than under AIX 3.2.5;
#    the following statements handle both versions.
# 2. Use the -g switch to nm instead of -e under 4.1 (this shows just
#    externals, not statics;  -g isn't available under 3.2.5, though).
# 3. Use the -X32_64 switch to nm on AIX-4+ to handle 32 or 64bit compiles.
# 4. Eliminate lines that end in ":": these are the names of object
#    files (relevant in 4.1 only).
# 5. Eliminate entries with the "U" key letter;  these are undefined
#    symbols (relevant in 4.1 only).
# 6. Eliminate lines that contain the string "0|extern" preceded by space;
#    in 3.2.5, these are undefined symbols (address 0).
# 7. Eliminate lines containing the "unamex" symbol.  In 3.2.5, these
#    are also undefined symbols.
# 8. If a line starts with ".", delete the leading ".", since this will
#    just cause confusion later.
# 9. Eliminate everything after the first field in a line, so that we're
#    left with just the symbol name.

nmopts="-g -C"
osver=`uname -v`
if test $osver -eq 3; then
  nmopts="-e"
fi
if test $osver -gt 3; then
  nmopts="$nmopts -X32_64"
fi
rm -f lib.exp
echo "#! $outputFile" >lib.exp
/usr/ccs/bin/nm $nmopts -h $ofiles | sed -e '/:$/d' -e '/ U /d' -e '/[ 	]0|extern/d' -e '/unamex/d' -e 's/^\.//' -e 's/[ 	|].*//' | sort | uniq >>lib.exp

# If we're linking a .a file, then link all the objects together into a
# single file "shr.o" and then put that into the archive.  Otherwise link
# the object files directly into the .a file.

outputFile=`echo $args | sed -e 's/.*-o \([^ ]*\).*/\1/'`
noDotA=`echo $outputFile | sed -e '/\.a$/d'`
echo "noDotA=\"$noDotA\""
if test "$noDotA" = "" ; then
    linkArgs=`echo $args | sed -e 's/-o .*\.a /-o shr.o /'`
    echo $linkArgs
    eval $linkArgs
    echo ar cr $outputFile shr.o
    ar cr $outputFile shr.o
    rm -f shr.o
else
    eval $args
fi

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
