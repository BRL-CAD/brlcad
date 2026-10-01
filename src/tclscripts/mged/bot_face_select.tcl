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

proc bot_face_sel_apply { face } {
    if { [llength $face] != 3 } {
	return -code error "BOT face does not have 3 vertices"
    }

    _mged_p {*}$face
    _mged_get_solid_keypoint
    _mged_refresh
}

proc bot_face_select { face_list } {
    global mged_gui
    global ::tk::Priv


    set win [winset]
    set id [get_player_id_dm $win]
    if {$id == "mged"} {
	mouse_init_mged_gui
    }

    set w .bot_face_select

    if { [winfo exists $w] } { catch "destroy $w" }

    if [info exists mged_gui($id,screen)] {
	set screen $mged_gui($id,screen)
    } else {
	set screen [winfo screen $win]
    }

    set face_list_len [llength $face_list]
    if { $face_list_len == 0 } {
	bot_face_sel_abort $w
	return
    }

    if { $face_list_len == 1 } {
	set face [lindex $face_list 0]
	if { [llength $face] != 3 } {
	    bot_face_sel_abort $w
	    cad_dialog $::tk::Priv(cad_dialog) $screen "Internal Error"  "Face does not have 3 vertices" error 0 OK
	    return
	}
	bot_face_sel_apply $face
	return
    }

    create_listbox $w $screen "BOT Triangle" $face_list "bot_face_sel_abort $w"
    wm protocol $w WM_DELETE_WINDOW [list bot_face_sel_abort $w]
    bind_listbox $w "<B1-Motion>"\
	"bot_face_sel_apply \[get_listbox_entry %W %x %y\]"
    bind_listbox $w "<ButtonPress-1>" \
	"lbdcHack %W %x %y %t $id bf junkpath"
    hoc_register_data $w.listbox "BOT Triangle Select" {
	{summary "Use your left mouse button to highlight entries in this list.
Each entry shows the three vertex numbers that make up a single triangle.
Double click to select a triangle for editing."}
    }
}

proc bot_face_sel_abort { w } {
    _mged_get_solid_keypoint
    _mged_refresh
    catch {destroy $w}
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
