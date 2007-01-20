#!/bin/sh
#                      D B C L E A N . S H
# BRL-CAD
#
# Copyright (c) 1991-2007 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of the
# License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this file; see the file named COPYING for more
# information.
###

# This shell program compacts mged (.g) databases to remove spaces left
# by the kill/killall commands.  This is achieved by converting the
# database to ASCII and then back to .g format.
# Note: when the converters are unable to convert a solid and it is skipped,
# the conversions will go to completion and a new database --- without those
# solids --- results.  Caveat Emptor.

# Author: Susanne Muuss, J.D.


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
