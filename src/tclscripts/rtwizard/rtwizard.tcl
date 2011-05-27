#!/bin/sh
#                    R T W I Z A R D . T C L
# BRL-CAD
#
# Copyright (c) 2006-2011 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# version 2.1 as published by the Free Software Foundation.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#
###
#
# rtwizard wrapper script to make things seem normal
#
# The trailing backslash forces tcl to skip the next line \
STARTUP_HOME=`dirname $0`/..
#\
export STARTUP_HOME
# restart using bwish \
WISH="bwish"
#\
for ish in bwish bwish_d ; do
# see if we're installed \
    if test -f ${STARTUP_HOME}/bin/$ish ; then
#\
	WISH="${STARTUP_HOME}/bin/$ish"
#\
	break;
#\
    fi
# see if we're not installed yet \
    if test -f ${STARTUP_HOME}/bwish/$ish ; then
#\
	WISH="${STARTUP_HOME}/bwish/$ish"
#\
	break;
#\
    fi
#\
done
#\
exec "$WISH" "$0" "$@"

#
# Begin Tcl here!
#
foreach i $auto_path {
    set wizpath [ file join $i {..} tclscripts rtwizard RaytraceWizard.tcl ]
    if { [file exists $wizpath] } {
	source $wizpath
	exit
    }
}

puts "rtwizard not found in any of $auto_path"

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
