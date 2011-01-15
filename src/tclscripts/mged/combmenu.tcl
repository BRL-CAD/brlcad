#                    C O M B M E N U . T C L
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
#	Author -
#		Robert G. Parker
#
#	Description -
#		Tcl routines to specify a combination for editing
#		from among all combinations in the database or from
#		among those currently being displayed.
#
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
check_externs "_mged_x _mged_press"

proc build_comb_menu_all_displayed {} {
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

    set paths [_mged_x -1]
    if {![llength $paths]} {
	cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen)\
	    "No combinations are being displayed!"\
	    "No combinations are being displayed!"\
	    "" 0 OK
	return
    }

    _mged_press reject
    set combs [build_comb_list $paths]
    build_comb_menu $id $combs

    mged_apply_all [winset] "set mouse_behavior d"
    foreach id $mged_players {
	set mged_gui($id,mouse_behavior) d
    }
}

proc ray_build_comb_menu { x y } {
    global mged_players
    global mged_gui
    global mouse_behavior

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

    _mged_press reject
    set combs [build_comb_list $paths]
    build_comb_menu $id $combs

    mged_apply_all [winset] "set mouse_behavior d"
    foreach id $mged_players {
	set mged_gui($id,mouse_behavior) d
    }
}

proc build_comb_menu { id combs } {
    global comb_control
    global mged_gui

    if {[info exists mged_gui($id,edit_menu)] && \
	    [winfo exists $mged_gui($id,edit_menu)]} {
	destroy $mged_gui($id,edit_menu)
    }

    set top .cm$id
    if [winfo exists $top] {
	destroy $top
    }

    if [info exists mged_gui($id,screen)] {
	set screen $mged_gui($id,screen)
    } else {
	set win [winset]
	set screen [winfo screen $win]
    }

    create_listbox $top $screen Combination $combs "destroy $top"
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
	    "set comb_control($id,name) \[%W get @%x,%y\];\
	    comb_reset $id;\
	    destroy $top"
    } else {
	bind_listbox $top "<ButtonPress-1>" \
	    "lbdcHack %W %x %y %t $id c2 junkpath"
    }
    bind_listbox $top "<ButtonRelease-1>"\
	"%W selection clear 0 end;\
	    _mged_press reject"
}

proc build_comb_menu_all_regions {} {
    set win [winset]
    set id [get_player_id_dm $win]

    set combs [_mged_ls -r]
    build_comb_menu2 $id $combs
}

proc build_comb_menu_all {} {
    set win [winset]
    set id [get_player_id_dm $win]

    set combs [_mged_ls -c]
    build_comb_menu2 $id $combs
}

proc build_comb_menu2 { id combs } {
    global comb_control
    global mged_gui

    if {[info exists mged_gui($id,edit_menu)] && \
	    [winfo exists $mged_gui($id,edit_menu)]} {
	destroy $mged_gui($id,edit_menu)
    }

    set top .cm$id
    if [winfo exists $top] {
	destroy $top
    }

    if [info exists mged_gui($id,screen)] {
	set screen $mged_gui($id,screen)
    } else {
	set win [winset]
	set screen [winfo screen $win]
    }

    create_listbox $top $screen Combination $combs "destroy $top"
    set mged_gui($id,edit_menu) $top

    if 0 {
	bind_listbox $top "<Double-1>"\
	    "set comb_control($id,name) \[%W get @%x,%y\];\
	    comb_reset $id;\
	    destroy $top"
    } else {
	bind_listbox $top "<ButtonPress-1>" \
	    "lbdcHack %W %x %y %t $id c3 junkpath"
    }
    bind_listbox $top "<ButtonRelease-1>"\
	"%W selection clear 0 end"
}

proc build_comb_list { paths } {
    set all_combs {}
    set combs {}

    foreach path $paths {
	# remove leading /'s
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

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
