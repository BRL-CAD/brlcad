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

    set win [winset]
    set id [get_player_id_dm $win]

    if {[info exists mged_gui($id,edit_menu)] && \
	    [winfo exists $mged_gui($id,edit_menu)]} {
	destroy $mged_gui($id,edit_menu)
    }

    set paths [_mged_x -1]
    if {![llength $paths]} {
	cad_dialog .$id.editDialog $mged_gui($id,screen)\
		"No solids are being displayed!"\
		"No solids are being displayed!"\
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

    set win [winset]
    set id [get_player_id_dm $win]

    if {[info exists mged_gui($id,edit_menu)] && \
	    [winfo exists $mged_gui($id,edit_menu)]} {
	destroy $mged_gui($id,edit_menu)
    }

    set ray [mouse_shoot_ray $x $y]
    set paths [ray_get_info $ray in path]

    if {![llength $paths]} {
	cad_dialog .$id.editDialog $mged_gui($id,screen)\
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
		build_solid_menu s $id $paths
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

    create_listbox $top $screen Solid $rpaths "destroy $top"
    set mged_gui($id,edit_menu) $top

    bind_listbox $top "<B1-Motion>"\
	    "set item \[get_listbox_entry %W %x %y\];\
	    solid_illum \$item"
    bind_listbox $top "<ButtonPress-1>" "doubleClickHack %W %x %y %t $id $type junkpath"
    bind_listbox $top "<ButtonRelease-1>" "%W selection clear 0 end; _mged_press reject"
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

    bind_listbox $top "<B1-Motion>"\
	    "set path_pos \[%W index @%x,%y\];\
	    matrix_illum $path \$path_pos"
    bind_listbox $top "<ButtonPress-1>" "doubleClickHack %W %x %y %t $id m $path"
    bind_listbox $top "<ButtonRelease-1>"\
	    "%W selection clear 0 end;\
	    _mged_press reject"
}

proc doubleClickHack {w x y t id type path} {
    global mged_gui
    global mged_default

    switch $type {
	s -
	o {
	    set item [get_listbox_entry $w $x $y]
	}
	m {
	    set item [$w index @$x,$y]
	}
    }

    if {$mged_gui($id,lastButtonPress) == 0 ||
        $mged_gui($id,lastItem) != $item ||
        [expr {abs($mged_gui($id,lastButtonPress) - $t) > $mged_default(doubleClickTol)}]} {

	switch $type {
	    s -
	    o {
		solid_illum $item
	    }
	    m {
		matrix_illum $path $item
	    }
	}
    } else {
	switch $type {
	    s {
		_mged_sed -i 1 $item
		destroy [winfo parent $w]
	    }
	    o {
		destroy [winfo parent $w]
		build_matrix_menu $id $item
	    }
	    m {
		_mged_press oill
		_mged_ill -i 1 $path
		_mged_matpick $item
		destroy [winfo parent $w]
	    }
	}
    }

    set mged_gui($id,lastButtonPress) $t
    set mged_gui($id,lastItem) $item
}
