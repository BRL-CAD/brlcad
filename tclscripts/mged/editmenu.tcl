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

if ![info exists mged_default_display] {
    if [info exists env(DISPLAY)] {
	set mged_default_display $env(DISPLAY)
    } else {
	set mged_default_display :0
    }
}

if ![info exists player_screen(mged)] {
    set player_screen(mged) $mged_default_display
}

#	Ensure that all commands that this script uses without defining
#	are provided by the calling application
check_externs "_mged_x _mged_press _mged_who _mged_ill"

proc build_edit_menu_all { type } {
    global player_screen
    global mged_players
    global mged_mouse_behavior
    global mouse_behavior
    global mged_active_dm
    global mged_edit_menu

    set win [winset]
    set id [get_player_id_dm $win]

    if {[info exists mged_edit_menu($id)] && \
	    [winfo exists $mged_edit_menu($id)]} {
	destroy $mged_edit_menu($id)
    }

    set paths [_mged_x -1]
    if {![llength $paths]} {
	cad_dialog .$id.editDialog $player_screen($id)\
		"No solids are being displayed!"\
		"No solids are being displayed!"\
		"" 0 OK
	return
    }

    _mged_press reject
    build_solid_menu $type $id $paths

    mged_apply_all "set mouse_behavior d"
    foreach id $mged_players {
	set mged_mouse_behavior($id) d
    }
}

proc ray_build_edit_menu { type x y } {
    global player_screen
    global mged_players
    global mged_mouse_behavior
    global mouse_behavior
    global mged_active_dm
    global mged_edit_menu

    set win [winset]
    set id [get_player_id_dm $win]

    if {[info exists mged_edit_menu($id)] && \
	    [winfo exists $mged_edit_menu($id)]} {
	destroy $mged_edit_menu($id)
    }

    set ray [mouse_shoot_ray $x $y]
    set paths [ray_get_info $ray in path]

    if {![llength $paths]} {
	cad_dialog .$id.editDialog $player_screen($id)\
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
		_mged_ill [lindex $paths 0]
	    } elseif {[llength $paths] > 1} {
		build_solid_menu s $id $paths
	    }
	}
	o {
	    if {[llength $paths] == 1} {
		_mged_press oill
		_mged_ill [lindex $paths 0]
		build_matrix_menu $id [lindex $paths 0]
	    } elseif {[llength $paths] > 1} {
		build_solid_menu o $id $paths
	    }
	}
	default {
	    return "ray_build_edit_menu: bad type - $type"
	}
    }

    mged_apply_all "set mouse_behavior d"
    foreach id $mged_players {
	set mged_mouse_behavior($id) d
    }
}

proc build_solid_menu { type id paths } {
    global player_screen
    global mged_edit_menu

    if {[info exists mged_edit_menu($id)] && \
	    [winfo exists $mged_edit_menu($id)]} {
	destroy $mged_edit_menu($id)
    }

    set top .em$id
    if [winfo exists $top] {
	destroy $top
    }

    if [info exists player_screen($id)] {
	set screen $player_screen($id)
    } else {
	set win [winset]
	set screen [winfo screen $win]
    }

    create_listbox $top $screen Solid $paths "destroy $top"
    set mged_edit_menu($id) $top

    switch $type {
	s {
	    bind_listbox $top "<ButtonPress-1>"\
		    "set item \[get_listbox_entry %W %x %y\];\
		    solid_illum \$item; break"
	    bind_listbox $top "<Double-1>"\
		    "set item \[get_listbox_entry %W %x %y\];\
		    _mged_press sill;\
		    _mged_ill \$item;\
		    destroy $top; break"
	}
	o {
	    bind_listbox $top "<ButtonPress-1>"\
		    "set item \[get_listbox_entry %W %x %y\];\
		    solid_illum \$item; break"
	    bind_listbox $top "<Double-1>"\
		    "set item \[get_listbox_entry %W %x %y\];\
		    build_matrix_menu $id \$item;\
		    destroy $top; break"
	}
	default {
	    destroy $top
	    return "build_solid_menu: bad type - $type"
	}
    }

    bind_listbox $top "<ButtonRelease-1>" "%W selection clear 0 end; _mged_press reject; break"
}

proc build_matrix_menu { id path } {
    global player_screen
    global mged_edit_menu

    if {[info exists mged_edit_menu($id)] && \
	    [winfo exists $mged_edit_menu($id)]} {
	destroy $mged_edit_menu($id)
    }

    set top .mm$id
    if [winfo exists $top] {
	destroy $top
    }

    if [info exists player_screen($id)] {
	set screen $player_screen($id)
    } else {
	set win [winset]
	set screen [winfo screen $win]
    }

    regexp "\[^/\].*" $path match
    set path_components [split $match /]
    create_listbox $top $screen Matrix $path_components "_mged_press reject; destroy $top"
    set mged_edit_menu($id) $top

    bind_listbox $top "<ButtonPress-1>"\
	    "set path_pos \[%W index @%x,%y\];\
	    matrix_illum $path \$path_pos; break"
    bind_listbox $top "<Double-1>"\
	    "set path_pos \[%W index @%x,%y\];\
	    _mged_press oill;\
	    _mged_ill $path;\
	    _mged_matpick \$path_pos;\
	    destroy $top; break"
    bind_listbox $top "<ButtonRelease-1>"\
	    "%W selection clear 0 end;\
	    _mged_press reject; break"
}
