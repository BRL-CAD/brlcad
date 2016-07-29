#!/bin/sh
#                      C H E C K . T C L
# BRL-CAD
#
# Copyright (c) 2016 United States Government as represented by
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
# This is the Geometry Checker main script.  It manages the loading
# and unloading of a GeometryChecker object into mged automatically.
#
# The trailing backslash forces tcl to skip the next line \
exec bwish "$0" "$@"


package require GeometryChecker

# All GeometryChecker stuff is in the GeometryChecker namespace
proc check { } {
    global mged_gui
    global ::tk::Priv
    global mged_players

    # determine the framebuffer window id
    if { [ catch { set mged_players } _mgedFramebufferId ] } {
	puts $_mgedFramebufferId
	puts "assuming default mged framebuffer id: id_0"
	set _mgedFramebufferId "id_0"
    }
    # just in case there are more than one returned
    set _mgedFramebufferId [ lindex $_mgedFramebufferId 0 ]

    set gc .$_mgedFramebufferId.checker

    # see if the window is already open.  If so, just raise it up.
    if [ winfo exists $gc ] {
	raise $gc
	return
    }

    # just to quell the tk name returned and report fatal errors
    if [ catch { GeometryChecker $gc } gbName ] {
	puts $gbName
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
