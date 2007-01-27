#                        L I S T . T C L
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
#			L I S T . T C L
#
#  Author -
#	Robert G. Parker
#
#  Description -
#	Tcl routines for utilizing Tcl's listbox.
#

proc create_listbox { top screen type items abort_cmd } {
    toplevel $top -screen $screen
    frame $top.frame
    listbox $top.listbox -xscrollcommand "$top.hscrollbar set" -yscrollcommand "$top.vscrollbar set"
    foreach word $items {
	$top.listbox insert end $word
    }
    # right justify
    $top.listbox xview 1000
    scrollbar $top.hscrollbar -orient horizontal -command "$top.listbox xview"
    scrollbar $top.vscrollbar -command "$top.listbox yview"
    button $top.abortB -text "Abort $type Selection" \
	-command "$abort_cmd"

    grid $top.listbox $top.vscrollbar -sticky "nsew" -in $top.frame
    grid $top.hscrollbar x -sticky "nsew" -in $top.frame
    grid $top.frame -sticky "nsew" -padx 8 -pady 8
    grid $top.abortB -padx 8 -pady 8
    grid columnconfigure $top.frame 0 -weight 1
    grid rowconfigure $top.frame 0 -weight 1
    grid columnconfigure $top 0 -weight 1
    grid rowconfigure $top 0 -weight 1

    place_near_mouse $top
    wm protocol $top WM_DELETE_WINDOW "catch { destroy $top }"
    wm title $top "$type Selection Menu"
}

proc bind_listbox {top event action} {
    bind $top.listbox $event "$action"
}

proc get_listbox_entry {w x y} {
    if ![winfo exists $w] {
	return
    }

    $w selection clear 0 end
    $w selection set @$x,$y

    return [$w get @$x,$y]
}


## - lbdcHack
#
# Listbox double click hack.
#
proc lbdcHack {w x y t id type path} {
    global mged_gui
    global mged_default
    global comb_control
    global bot_v1 bot_v2 bot_v3

    # set item from listbox selection @$x,$y
    switch $type {
	s1 -
	s2 -
	o -
	c1 -
	c2 -
	c3 -
	bf {
	    set item [get_listbox_entry $w $x $y]
	}
	m1 -
	m2 {
	    set item [$w index @$x,$y]
	}
	default {
	    error "lbdcHack: bad type - $type"
	}
    }

    if {$mged_gui($id,lastButtonPress) == 0 ||
	$mged_gui($id,lastItem) != $item ||
	[expr {abs($mged_gui($id,lastButtonPress) - $t) > $mged_default(doubleClickTol)}]} {

	switch $type {
	    s1 -
	    s2 -
	    o {
		solid_illum $item
	    }
	    c1 -
	    c2 {
		set spath [comb_get_solid_path $item]
		set path_pos [comb_get_path_pos $spath $item]
		matrix_illum $spath $path_pos
	    }
	    c3 {
	    }
	    m1 -
	    m2 {
		matrix_illum $path $item
	    }
	    bf {
		set bot_v1 [lindex $item 0];
		set bot_v2 [lindex $item 1];
		set bot_v3 [lindex $item 2];
		get_solid_keypoint;
		refresh;
	    }
	}
    } else {
	switch $type {
	    s1 {
		_mged_sed -i 1 $item
	    }
	    s2 {
		set mged_gui($id,mgs_path) $item
	    }
	    o {
		bind $w <ButtonRelease-1> \
			"destroy [winfo parent $w]; build_matrix_menu $id $item; break"
		set mged_gui($id,lastButtonPress) $t
		set mged_gui($id,lastItem) $item
		return
	    }
	    c1 {
		set mged_gui($id,mgc_comb) $item
	    }
	    c2 -
	    c3 {
		set comb_control($id,name) $item
		comb_reset $id
	    }
	    m1 {
		_mged_press oill
		_mged_ill -i 1 $path
		_mged_matpick $item
	    }
	    m2 {
		set mged_gui($id,mgs_pos) $item
	    }
	    bf {
		set bot_v1 [lindex $item 0];
		set bot_v2 [lindex $item 1];
		set bot_v3 [lindex $item 2];
		get_solid_keypoint;
		refresh;
	    }
	}

	bind $w <ButtonRelease-1> \
		"destroy [winfo parent $w]; break"
    }

    set mged_gui($id,lastButtonPress) $t
    set mged_gui($id,lastItem) $item
    return
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
