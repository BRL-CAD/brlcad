#                    E D I T M E N U . T C L
# BRL-CAD
#
# Copyright (c) 2004-2011 United States Government as represented by
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
#			E D I T M E N U . T C L
#
#	TCL macros for MGED(1) to specify a solid/obj_path for editing
#	from among those currently being displayed.
#
#	Authors -
#		Paul Tanenbaum
#		Robert Parker
#

if ![info exists mged_default(display)] {
    if [info exists env(DISPLAY)] {
	set mged_default(display) $env(DISPLAY)
    } else {
	set mged_default(display) :0
    }
}

if ![info exists mged_gui(mged,screen)] {
    set mged_gui(mged,screen) $mged_default(display)
}

#	Ensure that all commands that this script uses without defining
#	are provided by the calling application
check_externs "_mged_x _mged_press _mged_who _mged_ill"

proc build_edit_menu_all { type } {
    global mged_players
    global mged_gui
    global mouse_behavior
    global ::tk::Priv

    if ![info exists mged_players] {
	return
    }

    set win [winset]
    set id [get_player_id_dm $win]

    if {[opendb] == ""} {
	cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen) "No database." \
	    "No database has been opened!" info 0 OK
	return
    }

    if {[info exists mged_gui($id,edit_menu)] && \
	    [winfo exists $mged_gui($id,edit_menu)]} {
	destroy $mged_gui($id,edit_menu)
    }

    set paths [_mged_x -1]
    if {![llength $paths]} {
	cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen)\
	    "No primitives are being displayed!"\
	    "No primitives are being displayed!"\
	    "" 0 OK
	return
    }

    _mged_press reject
    build_solid_menu $type $id $paths

    mged_apply_all [winset] "set mouse_behavior d"
    foreach id $mged_players {
	set mged_gui($id,mouse_behavior) d
    }
}

proc ray_build_edit_menu { type x y } {
    global mged_players
    global mged_gui
    global mouse_behavior
    global ::tk::Priv

    if ![info exists mged_players] {
	return
    }

    set win [winset]
    set id [get_player_id_dm $win]

    if {[info exists mged_gui($id,edit_menu)] && \
	    [winfo exists $mged_gui($id,edit_menu)]} {
	destroy $mged_gui($id,edit_menu)
    }

    set ray [mouse_shoot_ray $x $y]
    set paths [ray_get_info $ray in path]

    if {![llength $paths]} {
	cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen)\
	    "Nothing was hit!"\
	    "Nothing was hit!"\
	    "" 0 OK
	return
    }

    _mged_press reject

    switch $type {
	s {
	    if {[llength $paths] == 1} {
		_mged_press sill
		_mged_ill -i 1 [lindex $paths 0]
	    } elseif {[llength $paths] > 1} {
		build_solid_menu s1 $id $paths
	    }
	}
	o {
	    if {[llength $paths] == 1} {
		_mged_press oill
		_mged_ill -i 1 [lindex $paths 0]
		build_matrix_menu $id [lindex $paths 0]
	    } elseif {[llength $paths] > 1} {
		build_solid_menu o $id $paths
	    }
	}
	default {
	    return "ray_build_edit_menu: bad type - $type"
	}
    }

    mged_apply_all [winset] "set mouse_behavior d"
    foreach id $mged_players {
	set mged_gui($id,mouse_behavior) d
    }
}

proc build_solid_menu { type id paths } {
    global mged_gui

    if {[info exists mged_gui($id,edit_menu)] && \
	    [winfo exists $mged_gui($id,edit_menu)]} {
	destroy $mged_gui($id,edit_menu)
    }

    set top .em$id
    if [winfo exists $top] {
	destroy $top
    }

    if [info exists mged_gui($id,screen)] {
	set screen $mged_gui($id,screen)
    } else {
	set win [winset]
	set screen [winfo screen $win]
    }

    # reverse paths
    for {set i [expr [llength $paths] - 1]} {0 <= $i} {incr i -1} {
	lappend rpaths [lindex $paths $i]
    }

    create_listbox $top $screen Primitive $rpaths "destroy $top"
    set mged_gui($id,edit_menu) $top

    bind_listbox $top <B1-Motion> \
	"set item \[get_listbox_entry %W %x %y\];\
	    solid_illum \$item"
    bind_listbox $top "<ButtonPress-1>" \
	"lbdcHack %W %x %y %t $id $type junkpath"
    bind_listbox $top "<ButtonRelease-1>" \
	"%W selection clear 0 end; _mged_press reject"
}

proc build_matrix_menu { id path } {
    global mged_gui

    if {[info exists mged_gui($id,edit_menu)] && \
	    [winfo exists $mged_gui($id,edit_menu)]} {
	destroy $mged_gui($id,edit_menu)
    }

    set top .mm$id
    if [winfo exists $top] {
	destroy $top
    }

    if [info exists mged_gui($id,screen)] {
	set screen $mged_gui($id,screen)
    } else {
	set win [winset]
	set screen [winfo screen $win]
    }

    regexp "\[^/\].*" $path match
    set path_components [split $match /]
    create_listbox $top $screen Matrix $path_components "_mged_press reject; destroy $top"
    set mged_gui($id,edit_menu) $top

    bind_listbox $top "<B1-Motion>" \
	"set path_pos \[%W index @%x,%y\];\
	    matrix_illum $path \$path_pos"
    bind_listbox $top "<ButtonPress-1>" \
	"lbdcHack %W %x %y %t $id m1 $path"
    bind_listbox $top "<ButtonRelease-1>" \
	"%W selection clear 0 end; _mged_press reject"
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
