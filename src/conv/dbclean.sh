#!/bin/sh
#                      D B C L E A N . S H
# BRL-CAD
#
# Copyright (c) 1991-2004 United States Government as represented by
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

# This shell program compacts mged (.g) databases to remove spaces left
# by the kill/killall commands.  This is achieved by converting the
# database to ASCII and then back to .g format.
# Note: when the converters are unable to convert a solid and it is skipped,
# the conversions will go to completion and a new database --- without those
# solids --- results.  Caveat Emptor.

# Author: Susanne Muuss, J.D.
# Copyright (C), 1991, U.S. Army



# Test to see that two command line arguments are seen: the original
# database name, and a new database name.

if test "$#" -lt 2
then
	echo "Usage: $0 in.g out.g"
	exit 1
fi

# Test to check whether the name of the input database is the same
# as that of the output database.  If so, complain and abort to
# prevent loosing the original database.

if test $1 = $2
then
	echo " $0 ERROR: input and output name must be different"
	exit 1
fi

# Set a temp file to be in /usr/tmp and include the process id.  That
# way, the directory wherein dbclean is invoked need not be writable for
# the user, and also, it prevents a clash if two users compact the same
# database at the same time. Note: TMP=/user.... cannot be written with
# spaces around the equal sign or else the output from the first conversion
# goes to standard out.
# Trap sudden interrupts and clean up any debris left by dbclean at that
# time.

TMP=/usr/tmp/tmp.$$
echo $TMP
rm -f $2
trap "rm -f $TMP; exit 1"  1 2 3 15 

# Check to see that the output file is writeable.  If not, complain and
# abort

if touch $2
then
	eval
else
	echo "$0 ERROR: $2 permission denied"
	exit 1
fi

# Do the conversions with g2asc and asc2g.  If either conversion does
# not succeed, put out an error message and abort.  Finally, at the
# end of the conversion, remove the temporary file.

if g2asc < $1 > $TMP
then
	eval
else
	echo "$0 ERROR: g2asc failed on $1"
	rm -f $TMP
	exit 1
fi

if asc2g < $TMP > $2
then
	eval
else
	echo "$0 ERROR: asc2g failed on $2"
	rm -f $TMP $2
	exit 1
fi

# Now clean up after a successful conversion and exit.

rm -f $TMP
exit 0

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
