#			M O U S E . T C L
#
#	TCL macros for selecting among the solids/objects being displayed.
#
#	Author -
#		Robert Parker
#

proc mouse_get_solid { x y } {
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
	mged_dialog .$id.editDialog $player_screen($id)\
		"Nothing was hit!"\
		"Nothing was hit!"\
		"" 0 OK
	return ""
    }

    if {[llength $paths] == 1} {
	return [lindex $paths 0]
    }

    _mged_press reject
    set top .mgs$id

    if [info exists player_screen($id)] {
	set screen $player_screen($id)
    } else {
	set screen [winfo screen $win]
    }

    create_listbox $top $screen Solid $paths "destroy $top"
    set mged_edit_menu($id) $top

    bind_listbox $top "<ButtonPress-1>"\
	    "set item \[get_listbox_entry %W %x %y\];\
	    new_solid_illum \$item"
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

proc mouse_get_solid_and_matrix { x y } {
    global player_screen
    global mged_edit_menu
    global mgsm_spath
    global mgsm_matrix

    set win [winset]
    set id [get_player_id_dm $win]

    set mgsm_spath [mouse_get_solid $x $y]

    if {$mgsm_spath == ""} {
	return ""
    }

    set mgsm_matrix -1
    set top .mgsm$id

    if [info exists player_screen($id)] {
	set screen $player_screen($id)
    } else {
	set screen [winfo screen $win]
    }
    regexp "\[^/\].*" $mgsm_spath match
    set path_components [split $match /]
    create_listbox $top $screen Matrix $path_components "_mged_press reject; destroy $top"
    set mged_edit_menu($id) $top

    bind_listbox $top "<ButtonPress-1>"\
	    "set item \[%W index @%x,%y\];\
	    _mged_press oill;\
	    _mged_ill \$mgsm_spath;\
	    _mged_matpick -n \$item"
    bind_listbox $top "<Double-1>"\
	    "set mgsm_matrix \[%W index @%x,%y\];\
	    destroy $top"
    bind_listbox $top "<ButtonRelease-1>"\
	    "%W selection clear 0 end; _mged_press reject"

    while {$mgsm_matrix == -1} {
	mged_update
    }

    return "$mgsm_spath $mgsm_matrix"
}
