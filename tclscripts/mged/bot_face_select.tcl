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
# Distribution Notice -
#	Re-distribution of this software is restricted, as described in
#       your "Statement of Terms and Conditions for the Release of
#       The BRL-CAD Package" agreement.
#
# Description -
#	rouintes for displaying a Tcl/Tk widget for selecting a BOT face
#

proc bot_face_select { face_list } {
    global bot_v1 bot_v2 bot_v3 mged_gui
    global tkPriv

    
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
			cad_dialog $tkPriv(cad_dialog) $screen "Internal Error"  "Face does not have 3 vertices" error 0 OK
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
