#                       M O U S E . T C L
# BRL-CAD
#
# Copyright (C) 1995-2005 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#
###
#
# Author -
#	Robert Parker
#
# Source -
#	The U. S. Army Ballistic Research Laboratory
#	Aberdeen Proving Ground, Maryland  21005
#  
#
#
# Description -
#       Mouse routines.

proc mouse_get_spath { x y } {
    global mged_gui
    global ::tk::Priv

    set win [winset]
    set id [get_player_id_dm $win]
    if {$id == "mged"} {
	mouse_init_mged_gui
    }

    if {[info exists mged_gui($id,edit_menu)] && \
	    [winfo exists $mged_gui($id,edit_menu)]} {
	destroy $mged_gui($id,edit_menu)
    }

    set mged_gui($id,mgs_path) ""
    set ray [mouse_shoot_ray $x $y]
    set paths [ray_get_info $ray in path]

    if {![llength $paths]} {
	cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen)\
		"Nothing was hit!"\
		"Nothing was hit!"\
		"" 0 OK
	return ""
    }

    if {[llength $paths] == 1} {
	return [lindex $paths 0]
    }

    set top .mgs$id
    if [winfo exists $top] {
	destroy $top
    }

    if [info exists mged_gui($id,screen)] {
	set screen $mged_gui($id,screen)
    } else {
	set screen [winfo screen $win]
    }

    create_listbox $top $screen Primitive $paths "mouse_spath_destroy $id $top"
    set mged_gui($id,edit_menu) $top

    bind_listbox $top "<B1-Motion>"\
	    "set item \[get_listbox_entry %W %x %y\];\
	    solid_illum \$item"
if 0 {
    bind_listbox $top "<ButtonPress-1>"\
	    "set item \[get_listbox_entry %W %x %y\];\
	    solid_illum \$item"
    bind_listbox $top "<Double-1>"\
	    "set mged_gui($id,mgs_path) \[get_listbox_entry %W %x %y\];\
	    destroy $top"
} else {
    bind_listbox $top "<ButtonPress-1>" \
	    "lbdcHack %W %x %y %t $id s2 junkpath"
}
    bind_listbox $top "<ButtonRelease-1>" \
	    "%W selection clear 0 end; _mged_press reject"

    wm protocol $top WM_DELETE_WINDOW "mouse_spath_destroy $id $top"

    while {$mged_gui($id,mgs_path) == ""} {
	mged_update 0
    }

    return $mged_gui($id,mgs_path)
}

proc mouse_spath_destroy { id top } {
    global mged_gui

    set mged_gui($id,mgs_path) " "
    destroy $top
}

proc mouse_get_spath_and_pos { x y } {
    global mged_gui

    set win [winset]
    set id [get_player_id_dm $win]
    if {$id == "mged"} {
	mouse_init_mged_gui
    }

    set mged_gui($id,mgs_path) [mouse_get_spath $x $y]

    if {$mged_gui($id,mgs_path) == "" || $mged_gui($id,mgs_path) == " "} {
	return ""
    }

    set mged_gui($id,mgs_pos) -1
    set top .mgsp$id

    if [info exists mged_gui($id,screen)] {
	set screen $mged_gui($id,screen)
    } else {
	set screen [winfo screen $win]
    }
    regexp "\[^/\].*" $mged_gui($id,mgs_path) match
    set path_components [split $match /]
    create_listbox $top $screen Matrix $path_components "mouse_spath_and_pos_destroy $id $top"
    set mged_gui($id,edit_menu) $top

    bind_listbox $top "<B1-Motion>"\
	    "set item \[%W index @%x,%y\];\
	    _mged_press reject;\
	    _mged_press oill;\
	    _mged_ill -i 1 \$mged_gui($id,mgs_path);\
	    _mged_matpick -n \$item"
if 0 {
    bind_listbox $top "<ButtonPress-1>"\
	    "set item \[%W index @%x,%y\];\
	    _mged_press oill;\
	    _mged_ill -i 1 \$mged_gui($id,mgs_path);\
	    _mged_matpick -n \$item"
    bind_listbox $top "<Double-1>"\
	    "set mged_gui($id,mgs_pos) \[%W index @%x,%y\];\
	    destroy $top"
} else {
    bind_listbox $top "<ButtonPress-1>" \
	    "lbdcHack %W %x %y %t $id m2 \$mged_gui($id,mgs_path)"
}
    bind_listbox $top "<ButtonRelease-1>" \
	    "%W selection clear 0 end; _mged_press reject"

    wm protocol $top WM_DELETE_WINDOW "mouse_spath_and_pos_destroy $id $top"

    while {$mged_gui($id,mgs_pos) == -1} {
	mged_update 0
    }

    return "$mged_gui($id,mgs_path) $mged_gui($id,mgs_pos)"
}

proc mouse_spath_and_pos_destroy { id top } {
    global mged_gui

    set mged_gui($id,mgs_pos) -2
    set mged_gui($id,mgs_path) ""
    destroy $top
}

proc mouse_get_comb { x y } {
    global mged_gui
    global ::tk::Priv

    set win [winset]
    set id [get_player_id_dm $win]
    if {$id == "mged"} {
	mouse_init_mged_gui
    }

    if {[info exists mged_gui($id,edit_menu)] && \
	    [winfo exists $mged_gui($id,edit_menu)]} {
	destroy $mged_gui($id,edit_menu)
    }

    set mged_gui($id,mgc_comb) ""
    set ray [mouse_shoot_ray $x $y]
    set paths [ray_get_info $ray in path]
    if {![llength $paths]} {
	cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen)\
		"Nothing was hit!"\
		"Nothing was hit!"\
		"" 0 OK
	return ""
    }
    set combs [build_comb_list $paths]

    if {[llength $combs] == 1} {
	set mged_gui($id,mgc_comb) [lindex $combs 0]
	return $mged_gui($id,mgc_comb)
    }

    set top .mgc$id
    if [winfo exists $top] {
	destroy $top
    }

    if [info exists mged_gui($id,screen)] {
	set screen $mged_gui($id,screen)
    } else {
	set win [winset]
	set screen [winfo screen $win]
    }

    create_listbox $top $screen Combination $combs "mouse_comb_destroy $id $top"
    set mged_gui($id,edit_menu) $top

    bind_listbox $top "<B1-Motion>"\
	    "set comb \[%W get @%x,%y\];\
	    set spath \[comb_get_solid_path \$comb\];\
	    set path_pos \[comb_get_path_pos \$spath \$comb\];\
	    matrix_illum \$spath \$path_pos"
if 0 {
    bind_listbox $top "<ButtonPress-1>"\
	    "set comb \[%W get @%x,%y\];\
	    set spath \[comb_get_solid_path \$comb\];\
	    set path_pos \[comb_get_path_pos \$spath \$comb\];\
	    matrix_illum \$spath \$path_pos"
    bind_listbox $top "<Double-1>"\
	    "set mged_gui($id,mgc_comb) \[%W get @%x,%y\];\
	    destroy $top"
} else {
    bind_listbox $top "<ButtonPress-1>" \
	    "lbdcHack %W %x %y %t $id c1 junkpath"
}
    bind_listbox $top "<ButtonRelease-1>"\
	    "%W selection clear 0 end;\
	    _mged_press reject"

    wm protocol $top WM_DELETE_WINDOW "mouse_comb_destroy $id $top"

    while {$mged_gui($id,mgc_comb) == ""} {
	mged_update 0
    }

    return $mged_gui($id,mgc_comb)
}

proc mouse_comb_destroy { id top } {
    global mged_gui

    set mged_gui($id,mgc_comb) " "
    destroy $top
}

proc mouse_solid_edit_select { x y } {
    global mged_players
    global mged_gui

    if {[opendb] == ""} {
	return
    }

    set spath [mouse_get_spath $x $y]
    if {$spath == "" || $spath == " "} {
	return
    }

    _mged_sed -i 1 $spath

    mged_apply_all [winset] "set mouse_behavior d"
    foreach id $mged_players {
	set mged_gui($id,mouse_behavior) d
    }
}

proc mouse_matrix_edit_select { x y } {
    global mged_players
    global mged_gui

    if {[opendb] == ""} {
	return
    }

    set spath_and_pos [mouse_get_spath_and_pos $x $y]
    if {[llength $spath_and_pos] != 2} {
	return
    }

    _mged_press oill
    _mged_ill -i 1 [lindex $spath_and_pos 0]
    _mged_matpick [lindex $spath_and_pos 1]

    mged_apply_all [winset] "set mouse_behavior d"
    foreach id $mged_players {
	set mged_gui($id,mouse_behavior) d
    }
}

proc mouse_rt_obj_select { x y } {
    global mged_players
    global mged_gui
    global rt_control
    global port

    if {[opendb] == ""} {
	return
    }

    set win [winset]
    set id [get_player_id_dm $win]
    if {$id == "mged"} {
	mouse_init_mged_gui
    }

    set spath_and_pos [mouse_get_spath_and_pos $x $y]
    if {[llength $spath_and_pos] != 2} {
	return
    }

    set spath [lindex $spath_and_pos 0]
    set sindex [lindex $spath_and_pos 1]

    # remove leading /
    if {[string index $spath 0] == "/"} {
	set spath [string range $spath 1 end]
    }

    set components [split $spath /]
    if ![llength $components] {
	return
    }

    set component [lindex $components 0]
    for {set i 1} {$i <= $sindex} {incr i} {
	set component $component/[lindex $components $i]
    }

    rt_init_vars $id $win

    switch $rt_control($id,omode) {
	all
            -
	one {
	    rt_olist_set $id $component
	    do_Raytrace $id
	}
	several {
	    rt_olist_add $id $component
	}
    }

    return
}

proc mouse_comb_edit_select { x y } {
    global mged_players
    global mged_gui
    global comb_control

    if {[opendb] == ""} {
	return
    }

    set win [winset]
    set id [get_player_id_dm $win]
    if {$id == "mged"} {
	mouse_init_mged_gui
    }

    set comb [mouse_get_comb $x $y]
    if {$comb == "" || $comb == " "} {
	return
    }

    init_comb $id
    set comb_control($id,name) $comb
    comb_reset $id

    mged_apply_all [winset] "set mouse_behavior d"
    foreach id $mged_players {
	set mged_gui($id,mouse_behavior) d
    }
}

##
#
# Hack to use new mouse utilities with an arbitrary 
# display manager window.
#
proc mouse_init_mged_gui {} {
    global mged_gui

    set mged_gui(mged,active_dm) ""
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
