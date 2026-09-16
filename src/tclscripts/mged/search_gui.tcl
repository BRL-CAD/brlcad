#                    S E A R C H _ G U I . T C L
# BRL-CAD
#
# Copyright (c) 2026 United States Government as represented by
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

proc init_search_gui {id} {
    global mged_gui

    set top .$id.search_gui

    if {[winfo exists $top]} {
	wm deiconify $top
	raise $top
	return
    }

    toplevel $top -screen $mged_gui($id,screen)
    wm title $top "Search Geometry ($id)"

    label $top.placeholder -text "Search interface placeholder" -padx 24 -pady 18
    button $top.dismiss -text "Dismiss" -command [list destroy $top]

    grid $top.placeholder -row 0 -column 0 -sticky nsew
    grid $top.dismiss -row 1 -column 0 -pady {0 10}
    grid columnconfigure $top 0 -weight 1
    grid rowconfigure $top 0 -weight 1
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
