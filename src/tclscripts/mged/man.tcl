#!/usr/bin/env tclsh
#                   M A N . T C L
# BRL-CAD
#
# Copyright (c) 2004-2008 United States Government as represented by
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


proc man {cmdname} {
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


    if {![file exists [bu_brlcad_data "html/man1/en/$cmdname.html"]]} {
        return
    } else {
        set man_fd [open [bu_brlcad_data "html/man1/en/$cmdname.html"]]
        set man_data [read $man_fd]
        close $man_fd
        man_dialog $::tk::Priv(man_dialog) $mged_gui($_mgedFramebufferId,screen) $cmdname $man_data 0 OK
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
