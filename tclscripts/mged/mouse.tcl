#			M O U S E . T C L
#
#	TCL macros for selecting among the solids/objects being displayed.
#
#	Author -
#		Robert Parker
#

proc mouse_get_spath { x y } {
    global player_screen
    global mged_edit_menu
    global mgs_spath

    set win [winset]
    set id [get_player_id_dm $win]

    if {[info exists mged_edit_menu($id)] && \
	    [winfo exists $mged_edit_menu($id)]} {
	destroy $mged_edit_menu($id)
    }

    set mgs_spath ""
    set ray [mouse_shoot_ray $x $y]
    set paths [ray_get_info $ray in path]

    if {![llength $paths]} {
	mged_dialog .$id.spathDialog $player_screen($id)\
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

    if [info exists player_screen($id)] {
	set screen $player_screen($id)
    } else {
	set screen [winfo screen $win]
    }

    create_listbox $top $screen Solid $paths "destroy $top"
    set mged_edit_menu($id) $top

    bind_listbox $top "<ButtonPress-1>"\
	    "set item \[get_listbox_entry %W %x %y\];\
	    solid_illum \$item"
    bind_listbox $top "<Double-1>"\
	    "set mgs_spath \[get_listbox_entry %W %x %y\];\
	    destroy $top"
    bind_listbox $top "<ButtonRelease-1>"\
	    "%W selection clear 0 end; _mged_press reject"

    while {$mgs_spath == ""} {
	mged_update
    }

    return $mgs_spath
}

proc mouse_get_spath_and_pos { x y } {
    global player_screen
    global mged_edit_menu
    global mgsp_spath
    global mgsp_path_pos

    set win [winset]
    set id [get_player_id_dm $win]

    set mgsp_spath [mouse_get_spath $x $y]

    if {$mgsp_spath == ""} {
	return ""
    }

    set mgsp_path_pos -1
    set top .mgsp$id

    if [info exists player_screen($id)] {
	set screen $player_screen($id)
    } else {
	set screen [winfo screen $win]
    }
    regexp "\[^/\].*" $mgsp_spath match
    set path_components [split $match /]
    create_listbox $top $screen Matrix $path_components "destroy $top"
    set mged_edit_menu($id) $top

    bind_listbox $top "<ButtonPress-1>"\
	    "set item \[%W index @%x,%y\];\
	    _mged_press oill;\
	    _mged_ill \$mgsp_spath;\
	    _mged_matpick -n \$item"
    bind_listbox $top "<Double-1>"\
	    "set mgsp_path_pos \[%W index @%x,%y\];\
	    destroy $top"
    bind_listbox $top "<ButtonRelease-1>"\
	    "%W selection clear 0 end; _mged_press reject"

    while {$mgsp_path_pos == -1} {
	mged_update
    }

    return "$mgsp_spath $mgsp_path_pos"
}

proc mouse_get_comb { x y } {
    global player_screen
    global mged_edit_menu
    global mgc_comb

    set win [winset]
    set id [get_player_id_dm $win]

    if {[info exists mged_edit_menu($id)] && \
	    [winfo exists $mged_edit_menu($id)]} {
	destroy $mged_edit_menu($id)
    }

    set mgc_comb ""
    set ray [mouse_shoot_ray $x $y]
    set paths [ray_get_info $ray in path]
    if {![llength $paths]} {
	mged_dialog .$id.combDialog $player_screen($id)\
		"Nothing was hit!"\
		"Nothing was hit!"\
		"" 0 OK
	return ""
    }
    set combs [build_comb_list $paths]

    if {[llength $combs] == 1} {
	set mgc_comb [lindex $combs 0]
	return $mgc_comb
    }

    set top .mgc$id
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
	    "set mgc_comb \[%W get @%x,%y\];\
	    destroy $top"
    bind_listbox $top "<ButtonRelease-1>"\
	    "%W selection clear 0 end;\
	    _mged_press reject"

    while {$mgc_comb == ""} {
	mged_update
    }

    return $mgc_comb
}

proc mouse_solid_edit_select { x y } {
    global mged_players
    global mged_mouse_behavior

    set spath [mouse_get_spath $x $y]
    if {$spath == ""} {
	return
    }

    _mged_press sill
    _mged_ill $spath

    mged_apply_all "set mouse_behavior d"
    foreach id $mged_players {
	set mged_mouse_behavior($id) d
    }
}

proc mouse_object_edit_select { x y } {
    global mged_players
    global mged_mouse_behavior

    set spath_and_pos [mouse_get_spath_and_pos $x $y]
    if {[llength $spath_and_pos] != 2} {
	return
    }

    _mged_press oill
    _mged_ill [lindex $spath_and_pos 0]
    _mged_matpick [lindex $spath_and_pos 1]

    mged_apply_all "set mouse_behavior d"
    foreach id $mged_players {
	set mged_mouse_behavior($id) d
    }
}

proc mouse_comb_edit_select { x y } {
    global mged_players
    global mged_mouse_behavior
    global comb_name

    set comb [mouse_get_comb $x $y]
    if {$comb == ""} {
	return
    }

    set win [winset]
    set id [get_player_id_dm $win]
    init_comb $id
    set comb_name($id) $comb
    comb_load_defaults $id

    mged_apply_all "set mouse_behavior d"
    foreach id $mged_players {
	set mged_mouse_behavior($id) d
    }
}
