#			C O M B M E N U . T C L
#
#	Author -
#		Robert G. Parker
#
#	Description -
#		Tcl routines to specify a combination for editing
#		from among those currently being displayed.
#
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
check_externs "_mged_x _mged_press"

proc build_comb_menu_all {} {
    global player_screen
    global mged_players
    global mged_mouse_behavior
    global mouse_behavior
    global mged_edit_menu

    set win [winset]
    set id [get_player_id_dm $win]

    if {[info exists mged_edit_menu($id)] && \
	    [winfo exists $mged_edit_menu($id)]} {
	destroy $mged_edit_menu($id)
    }

    set paths [_mged_x -1]
    if {![llength $paths]} {
	cad_dialog .$id.combDialog $player_screen($id)\
		"No combinations are being displayed!"\
		"No combinations are being displayed!"\
		"" 0 OK
	return
    }

    _mged_press reject
    set combs [build_comb_list $paths]
    build_comb_menu $id $combs

    mged_apply_all "set mouse_behavior d"
    foreach id $mged_players {
	set mged_mouse_behavior($id) d
    }
}

proc ray_build_comb_menu { x y } {
    global player_screen
    global mged_players
    global mged_mouse_behavior
    global mouse_behavior
    global mged_edit_menu

    set win [winset]
    set id [get_player_id_dm $win]

    if {[info exists mged_edit_menu($id)] && \
	    [winfo exists $mged_edit_menu($id)]} {
	destroy $mged_edit_menu($id)
    }

    set ray [mouse_shoot_ray $x $y]
    set paths [ray_get_info $ray in path]

    _mged_press reject
    set combs [build_comb_list $paths]
    build_comb_menu $id $combs

    mged_apply_all "set mouse_behavior d"
    foreach id $mged_players {
	set mged_mouse_behavior($id) d
    }
}

proc build_comb_menu { id combs } {
    global player_screen
    global comb_name
    global mged_edit_menu

    if {[info exists mged_edit_menu($id)] && \
	    [winfo exists $mged_edit_menu($id)]} {
	destroy $mged_edit_menu($id)
    }

    set top .cm$id
    if [winfo exists $top] {
	destroy $top
    }

    if [info exists player_screen($id)] {
	set screen $player_screen($id)
    } else {
	set win [winset]
	set screen [winfo screen $win]
    }

    create_listbox $top $screen Combination $combs "destroy $top"
    set mged_edit_menu($id) $top

    bind_listbox $top "<ButtonPress-1>"\
	    "set comb \[%W get @%x,%y\];\
	    set spath \[comb_get_solid_path \$comb\];\
	    set path_pos \[comb_get_path_pos \$spath \$comb\];\
	    matrix_illum \$spath \$path_pos"
    bind_listbox $top "<Double-1>"\
	    "set comb_name($id) \[%W get @%x,%y\];\
	    comb_load_defaults $id;\
	    destroy $top"
    bind_listbox $top "<ButtonRelease-1>"\
	    "%W selection clear 0 end;\
	    _mged_press reject"
}

proc build_comb_list { paths } {
    set all_combs {}
    set combs {}

    foreach path $paths {
	regexp "\[^/\].*" $path match
	set path_components [split $match /]

# Append all path components except the last which is a solid
	set n [expr [llength $path_components] - 1]
	for { set i 0 } { $i < $n } { incr i } {
	    lappend all_combs [lindex $path_components $i]
	}
    }

    foreach comb $all_combs {
# Put $comb into combs if not already there
	if { [lsearch -exact $combs $comb] == -1 } {
	    lappend combs $comb
	}
    }

    return $combs
}

proc comb_get_solid_path { comb } {
    set paths [_mged_x -1]

    if {[llength $paths] == 0} {
	return ""
    }

    set path_index [lsearch $paths *$comb*]
    set spath [lindex $paths $path_index]

    return $spath
}

proc comb_get_path_pos { spath comb } {
    regexp "\[^/\].*" $spath match
    set path_components [split $match /]
    set path_pos [lsearch -exact $path_components $comb]

    return $path_pos
}
