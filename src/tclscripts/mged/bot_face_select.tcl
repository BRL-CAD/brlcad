#             B O T _ F A C E _ S E L E C T . T C L
# BRL-CAD
#
# Copyright (c) 2004-2007 United States Government as represented by
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
#			B O T _ F A C E _ S E L E C T . T C L
#
# Author -
#	John Anderson
#
# Source -
#	The U. S. Army Research Laboratory
#	Aberdeen Proving Ground, Maryland  21005
#
#
# Description -
#	rouintes for displaying a Tcl/Tk widget for selecting a BOT face
#

proc bot_face_select { face_list } {
    global bot_v1 bot_v2 bot_v3 mged_gui
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
		set bot_v1 -1
		set bot_v2 -1
		set bot_v3 -1
		return
	}

	if { $face_list_len == 1 } {
		set face [lindex $face_list 0]
		if { [llength $face] != 3 } {
			set bot_v1 -1
			set bot_v2 -1
			set bot_v3 -1
			cad_dialog $::tk::Priv(cad_dialog) $screen "Internal Error"  "Face does not have 3 vertices" error 0 OK
			return
		}
		set bot_v1 [lindex $face 0]
		set bot_v2 [lindex $face 1]
		set bot_v3 [lindex $face 2]
		get_solid_keypoint
		return
	}

	create_listbox $w $screen "BOT Triangle" $face_list "bot_face_sel_abort $w"
	bind_listbox $w "<B1-Motion>"\
		"set face \[get_listbox_entry %W %x %y\];\
		set bot_v1 \[lindex \$face 0\];\
		set bot_v2 \[lindex \$face 1\];\
		set bot_v3 \[lindex \$face 2\];\
		get_solid_keypoint;\
		refresh"
if 0 {
	bind_listbox $w "<ButtonPress-1>"\
		"set face \[get_listbox_entry %W %x %y\];\
		set bot_v1 \[lindex \$face 0\];\
		set bot_v2 \[lindex \$face 1\];\
		set bot_v3 \[lindex \$face 2\];\
		get_solid_keypoint;\
		refresh"
	bind_listbox $w "<Double-1>"\
		"set face \[get_listbox_entry %W %x %y\];\
		set bot_v1 \[lindex \$face 0\];\
		set bot_v2 \[lindex \$face 1\];\
		set bot_v3 \[lindex \$face 2\];\
		get_solid_keypoint;\
		refresh;\
		destroy $w"
} else {
    bind_listbox $w "<ButtonPress-1>" \
	    "lbdcHack %W %x %y %t $id bf junkpath"
}
	hoc_register_data $w.listbox "BOT Triangle Select" {
		{summary "Use your left mouse button to highlight entries in this list.
Each entry show the three vertex numbers that make up a single triangle.
Double click to select a triangle for editing."}
	}
}

proc bot_face_sel_abort { w } {
	global bot_v1 bot_v2 bot_v3

	set bot_v1 -1
	set bot_v2 -1
	set bot_v3 -1
	get_solid_keypoint
	catch "destroy  $w"
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
