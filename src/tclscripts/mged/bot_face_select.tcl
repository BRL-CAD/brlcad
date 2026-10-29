#             B O T _ F A C E _ S E L E C T . T C L
# BRL-CAD
#
# Copyright (c) 2004-2026 United States Government as represented by
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
# Description -
#	routines for displaying a Tcl/Tk widget for selecting a BOT face
#

namespace eval ::brlcad::mged::bot_face {

namespace export init

proc apply { face } {
    if { [llength $face] != 3 } {
	return -code error "BOT face does not have 3 vertices"
    }

    ::brlcad::mged p {*}$face
    ::brlcad::mged get_solid_keypoint
    ::brlcad::mged refresh
}

proc init { face_list } {
    global mged_gui
    global ::tk::Priv


    set win [::brlcad::mged winset]
    set id [get_player_id_dm $win]
    if {$id == "mged"} {
	mouse_init_mged_gui
    }

    set w .bot_face_select

    if { [winfo exists $w] } { catch {destroy $w} }

    if [info exists mged_gui($id,screen)] {
	set screen $mged_gui($id,screen)
    } else {
	set screen [winfo screen $win]
    }

    set face_list_len [llength $face_list]
    if { $face_list_len == 0 } {
	abort $w
	return
    }

    if { $face_list_len == 1 } {
	set face [lindex $face_list 0]
	if { [llength $face] != 3 } {
	    abort $w
	    cad_dialog $::tk::Priv(cad_dialog) $screen "Internal Error"  "Face does not have 3 vertices" error 0 OK
	    return
	}
	apply $face
	return
    }

    set abort_command [list ::brlcad::mged::bot_face::abort $w]
    create_listbox $w $screen "BOT Triangle" $face_list $abort_command
    wm protocol $w WM_DELETE_WINDOW $abort_command
    bind_listbox $w "<B1-Motion>" \
	[list ::brlcad::mged::bot_face::select_entry %W %x %y]
    bind_listbox $w "<ButtonPress-1>" \
	[list ::lbdcHack %W %x %y %t $id bf junkpath]
    hoc_register_data $w.listbox "BOT Triangle Select" {
	{summary "Use your left mouse button to highlight entries in this list.
Each entry shows the three vertex numbers that make up a single triangle.
Double click to select a triangle for editing."}
    }
}

proc abort { w } {
    ::brlcad::mged get_solid_keypoint
    ::brlcad::mged refresh
    catch {destroy $w}
}

proc select_entry {window x y} {
    apply [get_listbox_entry $window $x $y]
}

}

proc bot_face_select {face_list} {
    tailcall ::brlcad::mged::bot_face::init $face_list
}

proc bot_face_sel_apply {face} {
    tailcall ::brlcad::mged::bot_face::apply $face
}

proc bot_face_sel_abort {window} {
    tailcall ::brlcad::mged::bot_face::abort $window
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
