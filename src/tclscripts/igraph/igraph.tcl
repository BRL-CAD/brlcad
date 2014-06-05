#!/bin/sh
#                   I G R A P H . T C L
# BRL-CAD
#
# Copyright (c) 2012-2014 United States Government as represented by
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
# This is the Graph Editor main script.  It manages the loading
# and unloading of a GeometryBrowser object into mged automatically.
#
# The trailing backslash forces tcl to skip the next line \
exec bwish "$0" "$@"


package require GraphEditor

### # All GraphEditor stuff is in the GraphEditor namespace
proc igraph { } {
    set ge .igraph

    # see if the window is already open.  If so, just raise it up.
    if [ winfo exists $ge ] {
	raise $ge
	return
    }

    # just to quell the tk name returned and report fatal errors
    if [ catch { GraphEditor $ge } geName ] {
	puts $geName
    }
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
