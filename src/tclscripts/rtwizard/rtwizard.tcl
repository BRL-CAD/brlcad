#!/bin/sh
#                    R T W I Z A R D . T C L
# BRL-CAD
#
# Copyright (c) 2006-2010 United States Government as represented by
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
exec bwish "$0" "$@"

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
