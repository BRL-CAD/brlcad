 Author -
#	Bob Parker
#
# Source -
#	The U. S. Army Ballistic Research Laboratory
#	Aberdeen Proving Ground, Maryland  21005
#
# Distribution Notice -
#	Re-distribution of this software is restricted, as described in
#       your "Statement of Terms and Conditions for the Release of
#       The BRL-CAD Package" agreement.
#
# Copyright Notice -
#       This software is Copyright (C) 1995 by the United States Army
#       in all countries except the USA.  All rights reserved.
#
# Description -
#	MGED's Tcl/Tk interface.
#
# $Revision
#

check_externs "_mged_attach _mged_aim _mged_add_view _mged_delete_view _mged_get_view _mged_goto_view _mged_next_view _mged_prev_view _mged_toggle_view"

if [info exists env(MGED_HTML_DIR)] {
        set mged_html_dir $env(MGED_HTML_DIR)
} else {
        set mged_html_dir [lindex $auto_path 0]/../html/mged
}

if ![info exists dm_insert_char_flag] {
    set dm_insert_char_flag 0
}

if ![info exists mged_players] {
    set mged_players ""
}

if ![info exists mged_collaborators] {
    set mged_collaborators ""
}

if ![info exists mged_default_id] {
    set mged_default_id "id"
}

if ![info exists mged_default_pane] {
    set mged_default_pane "ur"
}

if ![info exists mged_default_mvmode] {
    set mged_default_mvmode 0
}

if ![info exists mged_default_config] {
    set mged_default_config g
}

if ![info exists mged_default_display] {
    if [info exists env(DISPLAY)] {
	set mged_default_display $env(DISPLAY)
    } else {
	set mged_default_display :0
    }
}

if ![info exists mged_default_gdisplay] {
    if [info exists env(DISPLAY)] {
	set mged_default_gdisplay $env(DISPLAY)
    } else {
	set mged_default_gdisplay :0
    }
}

if ![info exists mged_default_dt] {
    set mged_default_dt [dm_bestXType $mged_default_gdisplay]
}

if ![info exists mged_default_comb] {
    set mged_default_comb 0
}

if ![info exists mged_default_edit_style] {
    set mged_default_edit_style emacs
}

if ![info exists mged_gui_create] {
    set mged_gui_create gui_create_default
}

if ![info exists mged_default_gui_destroy] {
    set mged_gui_destroy gui_destroy_default
}

set do_tearoffs 0

proc gui_create_default { args } {
    global moveView
    global player_screen
    global mged_default_id
    global mged_default_dt
    global mged_default_pane
    global mged_default_mvmode
    global mged_default_config
    global mged_default_display
    global mged_default_gdisplay
    global mged_default_comb
    global mged_default_edit_style
    global mged_html_dir
    global mged_players
    global mged_collaborators
    global mged_display
    global mged_use_air
    global mged_listen
    global mged_fb
    global mged_fb_all
    global mged_fb_overlay
    global mged_rubber_band
    global mged_grid_draw
    global mged_grid_snap
    global mged_mouse_behavior
    global mged_qray_effects
    global mged_coords
    global mged_rotate_about
    global mged_transform
    global mged_rateknobs
    global mged_adc_draw
    global mged_v_axes_pos
    global mged_v_axes
    global mged_m_axes
    global mged_e_axes
    global mged_apply_to
    global mged_grid_control
    global mged_apply_list
    global mged_edit_style
    global mged_top
    global mged_dmc
    global mged_active_dm
    global mged_small_dmc
    global mged_dm_loc
    global save_small_dmc
    global save_dm_loc
    global ia_cmd_prefix
    global ia_more_default
    global ia_font
    global ia_illum_label
    global env
    global edit_info_pos
    global show_edit_info
    global multi_view
    global buttons_on
    global win_size
    global cmd_win
    global dm_win
    global status_bar
    global view_ring
    global do_tearoffs
    global freshline
    global scratchline
    global vi_delete_flag
    global vi_search_flag

# set defaults
    set save_id [lindex [cmd_get] 2]
    set comb $mged_default_comb
    set join_c 0
    set dtype $mged_default_dt
    set id ""
    set scw 0
    set sgw 0
    set i 0

    set screen $mged_default_display
    set gscreen $mged_default_gdisplay

    if {$mged_default_config == "b"} {
	set scw 1
	set sgw 1
    } elseif {$mged_default_config == "c"} {
	set scw 1
	set sgw 0
    } elseif {$mged_default_config == "g"} {
	set scw 0
	set sgw 1
    }

#==============================================================================
# PHASE 0: Process options
#==============================================================================
    set argc [llength $args]
    set dtype_set 0
    set screen_set 0
    set gscreen_set 0

for {set i 0} {$i < $argc} {incr i} {
    set arg [lindex $args $i]
    if {$arg == "-config"} {
	incr i

	if {$i >= $argc} {
	    return [help gui_create_default]
	}

	set arg [lindex $args $i]
	if {$arg == "b"} {
# show both command and graphics windows
	    set scw 1
	    set sgw 1
	} elseif { $arg == "c"} {
	    set scw 1
	    set sgw 0
	} elseif {$arg == "g"} {
# show display manager window
	    set scw 0
	    set sgw 1
	} else {
	    return [help gui_create_default]
	}
    } elseif {$arg == "-d" || $arg == "-display"} {
# display string for everything
	incr i

	if {$i >= $argc} {
	    return [help gui_create_default]
	}

	set screen [lindex $args $i]
	set screen_set 1
	if {!$gscreen_set} {
	    set gscreen $screen

	    if {!$dtype_set} {
		set dtype [dm_bestXType $gscreen]
	    }
	}
    } elseif {$arg == "-gd" || $arg == "-gdisplay"} {
# display string for graphics window
	incr i

	if {$i >= $argc} {
	    return [help gui_create_default]
	}

	set gscreen [lindex $args $i]
	set gscreen_set 1
	if {!$screen_set} {
	    set screen $gscreen
	}

	if {!$dtype_set} {
	    set dtype [dm_bestXType $gscreen]
	}
    } elseif {$arg == "-dt"} {
	incr i

	if {$i >= $argc} {
	    return [help gui_create_default]
	}

	set dtype [lindex $args $i]
	set dtype_set 1
    } elseif {$arg == "-id"} {
	incr i

	if {$i >= $argc} {
	    return [help gui_create_default]
	}

	set id [lindex $args $i]
    } elseif {$arg == "-j" || $arg == "-join"} { 
	set join_c 1
    } elseif {$arg == "-h" || $arg == "-help"} {
	return [help gui_create_default]
    } elseif {$arg == "-s" || $arg == "-sep"} {
	set comb 0
    } elseif {$arg == "-c" || $arg == "-comb"} {
	set comb 1
    } else {
	return [help gui_create_default]
    }
}

if {$comb} {
    set gscreen $screen
}

if {$id == "mged"} {
    return "gui_create_default: not allowed to use \"mged\" as id"
}

if {$id == ""} {
    for {set i 0} {1} {incr i} {
	set id [subst $mged_default_id]_$i
	if {[lsearch -exact $mged_players $id] == -1} {
	    break;
	}
    }
}

if {$scw == 0 && $sgw == 0} {
    set sgw 1
}
set cmd_win($id) $scw
set dm_win($id) $sgw
set status_bar($id) 1
set mged_apply_to($id) 0
set mged_grid_control($id) 0
set edit_info_pos($id) "+0+0"

if ![dm_validXType $gscreen $dtype] {
    return "gui_create_default: $gscreen does not support $dtype"
}

if { [info exists tk_strictMotif] == 0 } {
    loadtk $screen
}

#==============================================================================
# PHASE 1: Creation of main window
#==============================================================================
if [ winfo exists .$id ] {
    return "A session with the id \"$id\" already exists!"
}
	
toplevel .$id -screen $screen

lappend mged_players $id
set player_screen($id) $screen

#==============================================================================
# Create display manager window and menu
#==============================================================================
if {$comb} {
    set mged_top($id) .$id
    set mged_dmc($id) .$id.dmf

    frame $mged_dmc($id) -relief sunken -borderwidth 2
    set win_size($id,big) [expr [winfo screenheight $mged_dmc($id)] - 130]
    set win_size($id,small) [expr $win_size($id,big) - 80]
    set win_size($id) $win_size($id,big)
    set mv_size [expr $win_size($id) / 2 - 4]

    if [catch { openmv $id $mged_top($id) $mged_dmc($id) $screen $dtype $mv_size } result] {
	gui_destroy_default $id
	return $result
    }
} else {
    set mged_top($id) .top$id
    set mged_dmc($id) $mged_top($id)

    toplevel $mged_dmc($id) -screen $gscreen -relief sunken -borderwidth 2
    set win_size($id,big) [expr [winfo screenheight $mged_dmc($id)] - 24]
    set win_size($id,small) [expr $win_size($id,big) - 100]
    set win_size($id) $win_size($id,big)
    set mv_size [expr $win_size($id) / 2 - 4]
    if [catch { openmv $id $mged_top($id) $mged_dmc($id) $gscreen $dtype $mv_size } result] {
	gui_destroy_default $id
	return $result
    }
}

set mged_active_dm($id) $mged_top($id).$mged_default_pane
set vloc [string range $mged_default_pane 0 0]
set hloc [string range $mged_default_pane 1 1]
set mged_small_dmc($id) $mged_dmc($id).$vloc.$hloc
set mged_dm_loc($id) $mged_default_pane
set save_dm_loc($id) $mged_dm_loc($id)
set save_small_dmc($id) $mged_small_dmc($id)
set mged_apply_list($id) $mged_active_dm($id)

#==============================================================================
# PHASE 2: Construction of menu bar
#==============================================================================

frame .$id.m -relief raised -bd 1

menubutton .$id.m.file -text "File" -underline 0 -menu .$id.m.file.m
menu .$id.m.file.m -tearoff $do_tearoffs
.$id.m.file.m add command -label "New..." -underline 0 -command "do_New $id"
.$id.m.file.m add command -label "Open..." -underline 0 -command "do_Open $id"
.$id.m.file.m add command -label "Insert..." -underline 0 -command "do_Concat $id"
.$id.m.file.m add command -label "Extract..." -underline 2 -command "init_extractTool $id"
.$id.m.file.m add separator
.$id.m.file.m add command -label "Raytrace..." -underline 0 -command "init_Raytrace $id"
.$id.m.file.m add cascade -label "Save View As" -menu .$id.m.file.m.cm_saveview
.$id.m.file.m add separator
.$id.m.file.m add command -label "Close" -underline 0 -command "gui_destroy_default $id"
.$id.m.file.m add command -label "Exit" -command quit -underline 0

menu .$id.m.file.m.cm_saveview -tearoff $do_tearoffs
.$id.m.file.m.cm_saveview add command -label "RT script" -command "init_rtScriptTool $id"
.$id.m.file.m.cm_saveview add command -label "Plot" -command "init_plotTool $id"
.$id.m.file.m.cm_saveview add command -label "PostScript" -command "init_psTool $id"

menubutton .$id.m.edit -text "Edit" -underline 0 -menu .$id.m.edit.m
menu .$id.m.edit.m -tearoff $do_tearoffs
.$id.m.edit.m add cascade -label "Add" -menu .$id.m.edit.m.cm_add
.$id.m.edit.m add command -label "Solid" -underline 0 -command "build_esolmenu_all $id"
.$id.m.edit.m add command -label "Matrix" -underline 0 -command "press oill"
.$id.m.edit.m add command -label "Region" -underline 0 -command "init_red $id"
#.$id.m.edit.m add separator
#.$id.m.edit.m add command -label "Reject" -underline 0 -command "press reject" 
#.$id.m.edit.m add command -label "Accept" -underline 0 -command "press accept"

menu .$id.m.edit.m.cm_add -tearoff $do_tearoffs
.$id.m.edit.m.cm_add add command -label "Solid..." -command "solcreate $id"
#.$id.m.edit.m.cm_add add command -label "Combination..." -command "comb_create $id"
#.$id.m.edit.m.cm_add add command -label "Region..." -command "reg_create $id"
.$id.m.edit.m.cm_add add command -label "Instance..." -command "icreate $id"

menubutton .$id.m.view -text "View" -underline 0 -menu .$id.m.view.m
menu .$id.m.view.m -tearoff $do_tearoffs
.$id.m.view.m add command -label "Top" -underline 0\
	-command "mged_apply $id \"press top\""
.$id.m.view.m add command -label "Bottom" -underline 5\
	-command "mged_apply $id \"press bottom\""
.$id.m.view.m add command -label "Right" -underline 0\
	-command "mged_apply $id \"press right\""
.$id.m.view.m add command -label "Left" -underline 0\
	-command "mged_apply $id \"press left\""
.$id.m.view.m add command -label "Front" -underline 0\
	-command "mged_apply $id \"press front\""
.$id.m.view.m add command -label "Back" -underline 0\
	-command "mged_apply $id \"press rear\""
.$id.m.view.m add command -label "az35,el25" -underline 2\
	-command "mged_apply $id \"press 35,25\""
.$id.m.view.m add command -label "az45,el45" -underline 2\
	-command "mged_apply $id \"press 45,45\""
.$id.m.view.m add separator
.$id.m.view.m add command -label "Zoom In" -underline 5\
	-command "mged_apply $id \"zoom 2\""
.$id.m.view.m add command -label "Zoom Out" -underline 5\
	-command "mged_apply $id \"zoom 0.5\""
.$id.m.view.m add separator
.$id.m.view.m add command -label "Save" -underline 0\
	-command "mged_apply $id \"press save\""
.$id.m.view.m add command -label "Restore" -underline 1\
	-command "mged_apply $id \"press restore\""
.$id.m.view.m add separator
.$id.m.view.m add command -label "Reset Viewsize"\
	-underline 6 -command "mged_apply $id \"press reset\""
.$id.m.view.m add command -label "Zero" -underline 0\
	-command "mged_apply $id \"knob zero\""

menubutton .$id.m.viewring -text "ViewRing" -underline 4 -menu .$id.m.viewring.m
menu .$id.m.viewring.m -tearoff $do_tearoffs
.$id.m.viewring.m add command -label "Add View" -underline 0 -command "do_add_view $id"
.$id.m.viewring.m add cascade -label "Select View" -menu .$id.m.viewring.m.cm_select
.$id.m.viewring.m add cascade -label "Delete View" -menu .$id.m.viewring.m.cm_delete
.$id.m.viewring.m add command -label "Next View" -underline 0 -command "do_next_view $id"
.$id.m.viewring.m add command -label "Prev View" -underline 0 -command "do_prev_view $id"
.$id.m.viewring.m add command -label "Last View" -underline 0 -command "do_toggle_view $id"

menu .$id.m.viewring.m.cm_select -tearoff $do_tearoffs
do_view_ring_entries $id s
set view_ring($id) 1
menu .$id.m.viewring.m.cm_delete -tearoff $do_tearoffs
do_view_ring_entries $id d

#menubutton .$id.m.options -text "Options" -underline 0 -menu .$id.m.options.m
#menu .$id.m.options.m -tearoff $do_tearoffs

menubutton .$id.m.modes -text "Modes" -underline 0 -menu .$id.m.modes.m
menu .$id.m.modes.m -tearoff $do_tearoffs
.$id.m.modes.m add checkbutton -offvalue 0 -onvalue 1 -variable mged_rateknobs($id)\
	-label "Rateknobs" -underline 0 -command "mged_apply $id \"set rateknobs \$mged_rateknobs($id)\""
.$id.m.modes.m add checkbutton -offvalue 0 -onvalue 1 -variable mged_rubber_band($id)\
	-label "Draw Rubber Band" -underline 7 -command "mged_apply $id \"set rubber_band \$mged_rubber_band($id)\""
.$id.m.modes.m add checkbutton -offvalue 0 -onvalue 1 -variable mged_grid_draw($id)\
	-label "Draw Grid" -command "mged_apply $id \"set grid_draw \$mged_grid_draw($id)\""
.$id.m.modes.m add checkbutton -offvalue 0 -onvalue 1 -variable mged_grid_snap($id)\
	-label "Snap To Grid" -command "mged_apply $id \"set grid_snap \$mged_grid_snap($id)\""
.$id.m.modes.m add checkbutton -offvalue 0 -onvalue 1 -variable multi_view($id)\
	-label "Multi Pane" -underline 0 -command "setmv $id"
.$id.m.modes.m add checkbutton -offvalue 0 -onvalue 1 -variable mged_fb($id)\
	-label "Framebuffer" -underline 0 -command "set_fb $id"

menubutton .$id.m.settings -text "Settings" -underline 0 -menu .$id.m.settings.m
menu .$id.m.settings.m -tearoff $do_tearoffs
.$id.m.settings.m add cascade -label "Apply To..." -menu .$id.m.settings.m.cm_applyTo
.$id.m.settings.m add cascade -label "Mouse Behavior" -menu .$id.m.settings.m.cm_mb
.$id.m.settings.m add cascade -label "Transform..." -menu .$id.m.settings.m.cm_transform
.$id.m.settings.m add cascade -label "Constraint Coords" -menu .$id.m.settings.m.cm_coord
.$id.m.settings.m add cascade -label "Rotate About..." -menu .$id.m.settings.m.cm_origin
.$id.m.settings.m add cascade -label "Query Ray Effects" -menu .$id.m.settings.m.cm_qray
.$id.m.settings.m add cascade -label "Multi-Pane" -menu .$id.m.settings.m.cm_mpane
.$id.m.settings.m add cascade -label "Units" -menu .$id.m.settings.m.cm_units
.$id.m.settings.m add cascade -label "Framebuffer" -menu .$id.m.settings.m.cm_fb
.$id.m.settings.m add cascade -label "Grid" -menu .$id.m.settings.m.cm_grid
.$id.m.settings.m add cascade -label "View Axes Position" -menu .$id.m.settings.m.cm_vap
.$id.m.settings.m add cascade -label "Command Line Edit" -menu .$id.m.settings.m.cm_cle

menu .$id.m.settings.m.cm_applyTo -tearoff $do_tearoffs
.$id.m.settings.m.cm_applyTo add radiobutton -value 0 -variable mged_apply_to($id)\
	-label "Active Pane"
.$id.m.settings.m.cm_applyTo add radiobutton -value 1 -variable mged_apply_to($id)\
	-label "Local Panes"
.$id.m.settings.m.cm_applyTo add radiobutton -value 2 -variable mged_apply_to($id)\
	-label "Listed Panes"
.$id.m.settings.m.cm_applyTo add radiobutton -value 3 -variable mged_apply_to($id)\
	-label "All Panes"

menu .$id.m.settings.m.cm_mb -tearoff $do_tearoffs
.$id.m.settings.m.cm_mb add radiobutton -value p -variable mged_mouse_behavior($id)\
	-label "Paint Rectangle Area" -command "mged_apply $id \"set mouse_behavior \$mged_mouse_behavior($id)\""
.$id.m.settings.m.cm_mb add radiobutton -value r -variable mged_mouse_behavior($id)\
	-label "Raytrace Rectangle Area" -command "mged_apply $id \"set mouse_behavior \$mged_mouse_behavior($id)\""
.$id.m.settings.m.cm_mb add radiobutton -value z -variable mged_mouse_behavior($id)\
	-label "Zoom Rectangle Area" -command "mged_apply $id \"set mouse_behavior \$mged_mouse_behavior($id)\""
.$id.m.settings.m.cm_mb add radiobutton -value q -variable mged_mouse_behavior($id)\
	-label "Query Ray" -command "mged_apply $id \"set mouse_behavior \$mged_mouse_behavior($id)\""
.$id.m.settings.m.cm_mb add radiobutton -value s -variable mged_mouse_behavior($id)\
	-label "Solid Edit" -command "mged_apply $id \"set mouse_behavior \$mged_mouse_behavior($id)\""
.$id.m.settings.m.cm_mb add radiobutton -value o -variable mged_mouse_behavior($id)\
	-label "Object Edit" -command "mged_apply $id \"set mouse_behavior \$mged_mouse_behavior($id)\""
.$id.m.settings.m.cm_mb add radiobutton -value d -variable mged_mouse_behavior($id)\
	-label "Default" -command "mged_apply $id \"set mouse_behavior \$mged_mouse_behavior($id)\""

menu .$id.m.settings.m.cm_qray -tearoff $do_tearoffs
.$id.m.settings.m.cm_qray add radiobutton -value t -variable mged_qray_effects($id)\
	-label "Text" -command "mged_apply $id \"qray effects \$mged_qray_effects($id)\""
.$id.m.settings.m.cm_qray add radiobutton -value g -variable mged_qray_effects($id)\
	-label "Graphics" -command "mged_apply $id \"qray effects \$mged_qray_effects($id)\""
.$id.m.settings.m.cm_qray add radiobutton -value b -variable mged_qray_effects($id)\
	-label "both" -command "mged_apply $id \"qray effects \$mged_qray_effects($id)\""

menu .$id.m.settings.m.cm_mpane -tearoff $do_tearoffs
.$id.m.settings.m.cm_mpane add radiobutton -value ul -variable mged_dm_loc($id)\
	-label "Upper Left" -command "set_active_dm $id"
.$id.m.settings.m.cm_mpane add radiobutton -value ur -variable mged_dm_loc($id)\
	-label "Upper Right" -command "set_active_dm $id"
.$id.m.settings.m.cm_mpane add radiobutton -value ll -variable mged_dm_loc($id)\
	-label "Lower Left" -command "set_active_dm $id"
.$id.m.settings.m.cm_mpane add radiobutton -value lr -variable mged_dm_loc($id)\
	-label "Lower right" -command "set_active_dm $id"
.$id.m.settings.m.cm_mpane add radiobutton -value lv -variable mged_dm_loc($id)\
	-label "Last Visited" -command "do_last_visited $id"

menu .$id.m.settings.m.cm_units -tearoff $do_tearoffs
.$id.m.settings.m.cm_units add radiobutton -value um -variable mged_display(units)\
	-label "micrometers" -command "do_Units $id"
.$id.m.settings.m.cm_units add radiobutton -value mm -variable mged_display(units)\
	-label "millimeters" -command "do_Units $id"
.$id.m.settings.m.cm_units add radiobutton -value cm -variable mged_display(units)\
	-label "centimeters" -command "do_Units $id"
.$id.m.settings.m.cm_units add radiobutton -value m -variable mged_display(units)\
	-label "meters" -command "do_Units $id"
.$id.m.settings.m.cm_units add radiobutton -value km -variable mged_display(units)\
	-label "kilometers" -command "do_Units $id"
.$id.m.settings.m.cm_units add separator
.$id.m.settings.m.cm_units add radiobutton -value in -variable mged_display(units)\
	-label "inches" -command "do_Units $id"
.$id.m.settings.m.cm_units add radiobutton -value ft -variable mged_display(units)\
	-label "feet" -command "do_Units $id"
.$id.m.settings.m.cm_units add radiobutton -value yd -variable mged_display(units)\
	-label "yards" -command "do_Units $id"
.$id.m.settings.m.cm_units add radiobutton -value mi -variable mged_display(units)\
	-label "miles" -command "do_Units $id"

menu .$id.m.settings.m.cm_fb -tearoff $do_tearoffs
.$id.m.settings.m.cm_fb add radiobutton -value 1 -variable mged_fb_all($id)\
	-label "All" -command "mged_apply $id \"set fb_all \$mged_fb_all($id)\""
.$id.m.settings.m.cm_fb add radiobutton -value 0 -variable mged_fb_all($id)\
	-label "Rectangle Area" -command "mged_apply $id \"set fb_all \$mged_fb_all($id)\""
.$id.m.settings.m.cm_fb add separator
.$id.m.settings.m.cm_fb add radiobutton -value 1 -variable mged_fb_overlay($id)\
	-label "Overlay" -command "mged_apply $id \"set fb_overlay \$mged_fb_overlay($id)\""
.$id.m.settings.m.cm_fb add radiobutton -value 0 -variable mged_fb_overlay($id)\
	-label "Underlay" -command "mged_apply $id \"set fb_overlay \$mged_fb_overlay($id)\""
.$id.m.settings.m.cm_fb add separator
.$id.m.settings.m.cm_fb add checkbutton -offvalue 0 -onvalue 1 -variable mged_listen($id)\
	-label "Listen For Clients" -underline 0 -command "set_listen $id" -state disabled

menu .$id.m.settings.m.cm_grid -tearoff $do_tearoffs
.$id.m.settings.m.cm_grid add cascade -label "Spacing" -menu .$id.m.settings.m.cm_grid.cm_spacing
.$id.m.settings.m.cm_grid add cascade -label "Advanced" -menu .$id.m.settings.m.cm_grid.cm_adv

menu .$id.m.settings.m.cm_grid.cm_spacing -tearoff $do_tearoffs
.$id.m.settings.m.cm_grid.cm_spacing add command -label "Both" -underline 0\
	-command "do_grid_spacing $id b"
.$id.m.settings.m.cm_grid.cm_spacing add command -label "Horizontal" -underline 0\
	-command "do_grid_spacing $id h"
.$id.m.settings.m.cm_grid.cm_spacing add command -label "Vertical" -underline 0\
	-command "do_grid_spacing $id v"
.$id.m.settings.m.cm_grid.cm_spacing add command -label "Autosize" -underline 0\
	-command "grid_control_autosize $id; grid_spacing_apply $id b"

menu .$id.m.settings.m.cm_grid.cm_adv -tearoff $do_tearoffs
.$id.m.settings.m.cm_grid.cm_adv add command -label "Anchor" -underline 0\
	-command "do_grid_anchor $id"
.$id.m.settings.m.cm_grid.cm_adv add command -label "Color" -underline 0\
	-command "do_grid_color $id"

menu .$id.m.settings.m.cm_coord -tearoff $do_tearoffs
.$id.m.settings.m.cm_coord add radiobutton -value m -variable mged_coords($id)\
	-label "Model" -command "mged_apply $id \"set coords \$mged_coords($id)\""
.$id.m.settings.m.cm_coord add radiobutton -value v -variable mged_coords($id)\
	-label "View" -command "mged_apply $id \"set coords \$mged_coords($id)\""
.$id.m.settings.m.cm_coord add radiobutton -value o -variable mged_coords($id)\
	-label "Object" -command "mged_apply $id \"set coords \$mged_coords($id)\"" -state disabled

menu .$id.m.settings.m.cm_origin -tearoff $do_tearoffs
.$id.m.settings.m.cm_origin add radiobutton -value v -variable mged_rotate_about($id)\
	-label "View Center" -command "mged_apply $id \"set rotate_about \$mged_rotate_about($id)\""
.$id.m.settings.m.cm_origin add radiobutton -value e -variable mged_rotate_about($id)\
	-label "Eye" -command "mged_apply $id \"set rotate_about \$mged_rotate_about($id)\""
.$id.m.settings.m.cm_origin add radiobutton -value m -variable mged_rotate_about($id)\
	-label "Model Origin" -command "mged_apply $id \"set rotate_about \$mged_rotate_about($id)\""
.$id.m.settings.m.cm_origin add radiobutton -value k -variable mged_rotate_about($id)\
	-label "Key Point" -command "mged_apply $id \"set rotate_about \$mged_rotate_about($id)\"" -state disabled

menu .$id.m.settings.m.cm_transform -tearoff $do_tearoffs
.$id.m.settings.m.cm_transform add radiobutton -value v -variable mged_transform($id)\
	-label "View" -command "set_transform $id"
.$id.m.settings.m.cm_transform add radiobutton -value a -variable mged_transform($id)\
	-label "ADC" -command "set_transform $id"
.$id.m.settings.m.cm_transform add radiobutton -value e -variable mged_transform($id)\
	-label "Model Params" -command "set_transform $id" -state disabled

menu .$id.m.settings.m.cm_vap -tearoff $do_tearoffs
.$id.m.settings.m.cm_vap add radiobutton -value 0 -variable mged_v_axes_pos($id)\
	-label "Center" -command "mged_apply $id \"set v_axes_pos {0 0}\""
.$id.m.settings.m.cm_vap add radiobutton -value 1 -variable mged_v_axes_pos($id)\
	-label "Lower Left" -command "mged_apply $id \"set v_axes_pos {-1750 -1750}\""
.$id.m.settings.m.cm_vap add radiobutton -value 2 -variable mged_v_axes_pos($id)\
	-label "Upper Left" -command "mged_apply $id \"set v_axes_pos {-1750 1750}\""
.$id.m.settings.m.cm_vap add radiobutton -value 3 -variable mged_v_axes_pos($id)\
	-label "Upper Right" -command "mged_apply $id \"set v_axes_pos {1750 1750}\""
.$id.m.settings.m.cm_vap add radiobutton -value 4 -variable mged_v_axes_pos($id)\
	-label "Lower Right" -command "mged_apply $id \"set v_axes_pos {1750 -1750}\""

menu .$id.m.settings.m.cm_cle -tearoff $do_tearoffs
.$id.m.settings.m.cm_cle add radiobutton -value emacs -variable mged_edit_style($id)\
	-label "emacs" -command "set_text_key_bindings $id"
.$id.m.settings.m.cm_cle add radiobutton -value vi -variable mged_edit_style($id)\
	-label "vi" -command "set_text_key_bindings $id"

menubutton .$id.m.tools -text "Tools" -menu .$id.m.tools.m
menu .$id.m.tools.m -tearoff $do_tearoffs
.$id.m.tools.m add checkbutton -offvalue 0 -onvalue 1 -variable mged_adc_draw($id)\
	-label "Angle/Dist Cursor" -underline 0 -command "mged_apply $id \"adc draw \$mged_adc_draw($id)\""
.$id.m.tools.m add checkbutton -offvalue 0 -onvalue 1 -variable show_edit_info($id)\
	-label "Edit Info" -underline 0 -command "toggle_edit_info $id"
.$id.m.tools.m add checkbutton -offvalue 0 -onvalue 1 -variable status_bar($id)\
	-label "Status Bar" -underline 0 -command "toggle_status_bar $id"
if {$comb} {
.$id.m.tools.m add checkbutton -offvalue 0 -onvalue 1 -variable cmd_win($id)\
	-label "Command Window" -underline 0 -command "set_cmd_win $id"
.$id.m.tools.m add checkbutton -offvalue 0 -onvalue 1 -variable dm_win($id)\
	-label "Graphics Window" -underline 0 -command "set_dm_win $id"
} 
.$id.m.tools.m add checkbutton -offvalue 0 -onvalue 1 -variable buttons_on($id)\
	-label "Button Menu" -underline 0 -command "toggle_button_menu $id"
.$id.m.tools.m add checkbutton -offvalue 0 -onvalue 1 -variable mged_adc_control($id)\
	-label "ADC Control Panel" -command "init_adc_control $id"
.$id.m.tools.m add checkbutton -offvalue 0 -onvalue 1 -variable mged_grid_control($id)\
	-label "Grid Control Panel" -command "init_grid_control $id"
.$id.m.tools.m add checkbutton -offvalue 0 -onvalue 1 -variable mged_qray_control($id)\
	-label "Query Ray Control Panel" -command "init_qray_control $id"
.$id.m.tools.m add separator
.$id.m.tools.m add cascade -label "Axes" -menu .$id.m.tools.m.cm_axes

menu .$id.m.tools.m.cm_axes -tearoff $do_tearoffs
.$id.m.tools.m.cm_axes add checkbutton -offvalue 0 -onvalue 1\
	-variable mged_v_axes($id) -label "View" -command "mged_apply $id \"set v_axes \$mged_v_axes($id)\""
.$id.m.tools.m.cm_axes add checkbutton -offvalue 0 -onvalue 1\
	-variable mged_m_axes($id) -label "Model" -command "mged_apply $id \"set m_axes \$mged_m_axes($id)\""
.$id.m.tools.m.cm_axes add checkbutton -offvalue 0 -onvalue 1\
	-variable mged_e_axes($id) -label "Edit" -command "mged_apply $id \"set e_axes \$mged_e_axes($id)\""

menubutton .$id.m.help -text "Help" -menu .$id.m.help.m
menu .$id.m.help.m -tearoff $do_tearoffs
.$id.m.help.m add command -label "About" -command "do_About_MGED $id"
.$id.m.help.m add command -label "On Context" -underline 0\
	-command "on_context_help $id"
.$id.m.help.m add command -label "Commands..." -underline 0\
	-command "command_help $id"
.$id.m.help.m add command -label "Apropos..." -underline 0 -command "ia_apropos .$id $screen"

if [info exists env(WEB_BROWSER)] {
    if [ file exists $env(WEB_BROWSER) ] {
	set web_cmd "exec $env(WEB_BROWSER) -display $screen $mged_html_dir/index.html &"
    }
}

# Minimal attempt to locate a browser
if ![info exists web_cmd] {
    if [ file exists /usr/X11/bin/netscape ] {
	set web_cmd "exec /usr/X11/bin/netscape -display $screen $mged_html_dir/index.html &"
    } elseif [ file exists /usr/X11/bin/Mosaic ] {
	set web_cmd "exec /usr/X11/bin/Mosaic -display $screen $mged_html_dir/index.html &"
    } else {
	set web_cmd "ia_man .$id $screen"
    }
}
.$id.m.help.m add command -label "Manual..." -underline 0 -command $web_cmd

#==============================================================================
# PHASE 3: Bottom-row display
#==============================================================================

frame .$id.status
frame .$id.status.dpy
frame .$id.status.illum

label .$id.status.cent -textvar mged_display($mged_active_dm($id),center) -anchor w
label .$id.status.size -textvar mged_display($mged_active_dm($id),size) -anchor w
label .$id.status.units -textvar mged_display(units) -anchor w -padx 4
label .$id.status.aet -textvar mged_display($mged_active_dm($id),aet) -anchor w
label .$id.status.ang -textvar mged_display($mged_active_dm($id),ang) -anchor w -padx 4
label .$id.status.illum.label -textvar ia_illum_label($id)

#==============================================================================
# PHASE 4: Text widget for interaction
#==============================================================================

frame .$id.tf
if {$comb} {
    text .$id.t -width 10 -relief sunken -bd 2 -yscrollcommand ".$id.s set" -setgrid true
} else {
    text .$id.t -relief sunken -bd 2 -yscrollcommand ".$id.s set" -setgrid true
}
scrollbar .$id.s -relief flat -command ".$id.t yview"

bind .$id.t <Enter> "focus .$id.t"

set mged_edit_style($id) $mged_default_edit_style
set_text_key_bindings $id
set_text_button_bindings .$id.t

set ia_cmd_prefix($id) ""
set ia_more_default($id) ""
mged_print_prompt .$id.t "mged> "
.$id.t insert insert " "
beginning_of_line .$id.t
set moveView(.$id.t) 0
set freshline(.$id.t) 1
set scratchline(.$id.t) ""
set vi_delete_flag(.$id.t) 0
set vi_search_flag(.$id.t) 0

.$id.t tag configure sel -background #fefe8e
.$id.t tag configure result -foreground blue3
.$id.t tag configure oldcmd -foreground red3
.$id.t tag configure prompt -foreground red1

#==============================================================================
# Pack windows
#==============================================================================

pack .$id.m.file .$id.m.edit .$id.m.view .$id.m.viewring .$id.m.modes .$id.m.settings .$id.m.tools -side left
pack .$id.m.help -side right
pack .$id.m -side top -fill x

set multi_view($id) $mged_default_mvmode
setmv $id
pack $mged_active_dm($id) -in $mged_dmc($id)
if {$comb} {
    pack $mged_dmc($id) -side top -padx 2 -pady 2
}

pack .$id.status.cent .$id.status.size .$id.status.units .$id.status.aet\
	.$id.status.ang -in .$id.status.dpy -side left -anchor w
pack .$id.status.dpy -side top -anchor w -expand 1 -fill x
pack .$id.status.illum.label -side left -anchor w
pack .$id.status.illum -side top -anchor w -expand 1 -fill x
pack .$id.status -side bottom -anchor w -expand 1 -fill x

pack .$id.s -in .$id.tf -side right -fill y
pack .$id.t -in .$id.tf -side top -fill both -expand yes
pack .$id.tf -side top -fill both -expand yes

#==============================================================================
# PHASE 5: Creation of other auxilary windows
#==============================================================================

#==============================================================================
# PHASE 6: Further setup
#==============================================================================

cmd_init $id
setupmv $id
aim $id $mged_active_dm($id)

if {$comb} {
    set_dm_win $id
    set_cmd_win $id
}

if { $join_c } {
    jcs $id
}

trace variable mged_display($mged_active_dm($id),fps) w "ia_changestate $id"
update_mged_vars $id
set mged_qray_effects($id) [qray effects]

# reset current_cmd_list so that its cur_hist gets updated
cmd_set $save_id

# cause all 4 windows to share menu state
share_menu $mged_top($id).ul $mged_top($id).ur
share_menu $mged_top($id).ul $mged_top($id).ll
share_menu $mged_top($id).ul $mged_top($id).lr

do_rebind_keys $id
set_active_dm $id

wm protocol $mged_top($id) WM_DELETE_WINDOW "gui_destroy_default $id"
wm geometry $mged_top($id) -0+0
}

proc gui_destroy_default args {
    global mged_players
    global mged_collaborators
    global multi_view
    global buttons_on
    global edit_info
    global show_edit_info
    global mged_top
    global mged_dmc

    if { [llength $args] != 1 } {
	return [help gui_destroy_default]
    }

    set id [lindex $args 0]

    set i [lsearch -exact $mged_players $id]
    if { $i == -1 } {
	return "gui_destroy_default: bad id - $id"
    }
    set mged_players [lreplace $mged_players $i $i]

    set i [lsearch -exact $mged_collaborators $id]
    if { $i != -1 } {
	cmd_set $id
	set cmd_list [cmd_get]
	set shared_id [lindex $cmd_list 1]
	qcs $id
	if {"$mged_top($id).ur" == "$shared_id"} {
	    reconfig_all_gui_default
	}
    }

    set multi_view($id) 0
    set buttons_on($id) 0
    set show_edit_info($id) 0

    releasemv $id
    catch { cmd_close $id }
    catch { destroy .mmenu$id }
    catch { destroy .sliders$id }
    catch { destroy $mged_top($id) }
    catch { destroy .$id }
}

proc reconfig_gui_default { id } {
    global mged_display

    cmd_set $id
    set cmd_list [cmd_get]
    set shared_id [lindex $cmd_list 1]

    .$id.status.cent configure -textvar mged_display($shared_id,center)
    .$id.status.size configure -textvar mged_display($shared_id,size)
    .$id.status.units configure -textvar mged_display(units)

    .$id.status.aet configure -textvar mged_display($shared_id,aet)
    .$id.status.ang configure -textvar mged_display($shared_id,ang)

#    reconfig_mmenu $id
}

proc reconfig_all_gui_default {} {
    global mged_collaborators

    foreach id $mged_collaborators {
	reconfig_gui_default $id
    }
}

proc update_mged_vars { id } {
    global mged_active_dm
    global rateknobs
    global mged_rateknobs
    global mged_adc_draw
    global m_axes
    global mged_m_axes
    global v_axes
    global mged_v_axes
    global v_axes_pos
    global mged_v_axes_pos
    global e_axes
    global mged_e_axes
    global use_air
    global mged_use_air
    global listen
    global mged_listen
    global fb
    global mged_fb
    global fb_all
    global mged_fb_all
    global fb_overlay
    global mged_fb_overlay
    global rubber_band
    global mged_rubber_band
    global mouse_behavior
    global mged_mouse_behavior
    global coords
    global mged_coords
    global rotate_about
    global mged_rotate_about
    global transform
    global mged_transform
    global grid_draw
    global mged_grid_draw
    global grid_snap
    global mged_grid_snap

    winset $mged_active_dm($id)
    set mged_rateknobs($id) $rateknobs
    set mged_adc_draw($id) [adc draw]
    set mged_m_axes($id) $m_axes
    set mged_v_axes($id) $v_axes
    set mged_v_axes_pos($id) $v_axes_pos
    set mged_e_axes($id) $e_axes
    set mged_use_air($id) $use_air
    set mged_fb($id) $fb
    set mged_fb_all($id) $fb_all
    set mged_fb_overlay($id) $fb_overlay
    set mged_rubber_band($id) $rubber_band
    set mged_mouse_behavior($id) $mouse_behavior
    set mged_coords($id) $coords
    set mged_rotate_about($id) $rotate_about
    set mged_transform($id) $transform
    set mged_grid_draw($id) $grid_draw
    set mged_grid_snap($id) $grid_snap

    if {$mged_fb($id)} {
	.$id.m.settings.m.cm_fb entryconfigure 6 -state normal
	set mged_listen($id) $listen
    } else {
	.$id.m.settings.m.cm_fb entryconfigure 6 -state disabled
	set mged_listen($id) $listen
    }
}

proc toggle_button_menu { id } {
    global buttons_on

    if [ winfo exists .mmenu$id ] {
	destroy .mmenu$id
	set buttons_on($id) 0
	return
    }

    mmenu_init $id
}

proc toggle_edit_info { id } {
    global show_edit_info
    global mged_display

    if {$show_edit_info($id)} {
	if {$mged_display(state) == "SOL EDIT" || $mged_display(state) == "OBJ EDIT"} {
	    build_edit_info $id
	}
    } else {
	destroy_edit_info $id
    }
}

proc build_edit_info { id } {
    global player_screen
    global edit_info
    global edit_info_pos
    global show_edit_info

    if [winfo exists .sei$id] {
	return
    }

    if {$show_edit_info($id)} {
	toplevel .sei$id -screen $player_screen($id)
	label .sei$id.l -bg black -fg yellow -textvar edit_info -font fixed
	pack .sei$id.l -expand 1 -fill both

	wm title .sei$id "$id\'s Edit Info"
	wm protocol .sei$id WM_DELETE_WINDOW "destroy_edit_info $id"
	wm geometry .sei$id $edit_info_pos($id)
    }
}

proc destroy_edit_info { id } {
    global edit_info_pos
    global show_edit_info

    if ![winfo exists .sei$id] {
	return
    }

    regexp "\[-+\]\[0-9\]+\[-+\]\[0-9\]+" [wm geometry .sei$id] match
    set edit_info_pos($id) $match
    destroy .sei$id
}

# Join Mged Collaborative Session
proc jcs { id } {
    global mged_collaborators
    global mged_players
    global mged_active_dm
    global mged_top

    if { [lsearch -exact $mged_players $id] == -1 } {
	return "jcs: $id is not listed as an mged_player"
    }

    if { [lsearch -exact $mged_collaborators $id] != -1 } {
	return "jcs: $id is already in the collaborative session"
    }

    if [winfo exists $mged_active_dm($id)] {
	set nw $mged_top($id).ur
    } else {
	return "jcs: unrecognized pathname - $mged_active_dm($id)"
    }

    if [llength $mged_collaborators] {
	set cid [lindex $mged_collaborators 0]
	if [winfo exists $mged_top($cid).ur] {
	    set ow $mged_top($cid).ur
	} else {
	    return "jcs: me thinks the session is corrupted"
	}

	catch { share_view $ow $nw }
	reconfig_gui_default $id
    }

    lappend mged_collaborators $id
}

# Quit Mged Collaborative Session
proc qcs { id } {
    global mged_collaborators
    global mged_active_dm

    set i [lsearch -exact $mged_collaborators $id]
    if { $i == -1 } {
	return "qcs: bad id - $id"
    }

    if [winfo exists $mged_active_dm($id)] {
	set w $mged_active_dm($id)
    } else {
	return "qcs: unrecognized pathname - $mged_active_dm($id)"
    }

    catch {unshare_view $w}
    set mged_collaborators [lreplace $mged_collaborators $i $i]
}

# Print Collaborative Session participants
proc pcs {} {
    global mged_collaborators

    return $mged_collaborators
}

# Print Mged Players
proc pmp {} {
    global mged_players

    return $mged_players
}

proc aim args {
    if { [llength $args] == 2 } {
	if ![winfo exists .[lindex $args 0]] {
	    return
	}
    }

    set result [eval _mged_aim $args]
    
    if { [llength $args] == 2 } {
	reconfig_gui_default [lindex $args 0]
    }

    return $result
}


proc set_active_dm { id } {
    global mged_top
    global mged_dmc
    global mged_active_dm
    global mged_small_dmc
    global mged_dm_loc
    global save_dm_loc
    global save_small_dmc
    global multi_view
    global win_size
    global mged_display
    global view_ring

    # unhighlight
    $mged_small_dmc($id) configure -bg #d9d9d9

    trace vdelete mged_display($mged_active_dm($id),fps) w "ia_changestate $id"

    set vloc [string range $mged_dm_loc($id) 0 0]
    set hloc [string range $mged_dm_loc($id) 1 1]
    set mged_active_dm($id) $mged_top($id).$mged_dm_loc($id)
    set mged_small_dmc($id) $mged_dmc($id).$vloc.$hloc

    # highlight
    $mged_small_dmc($id) configure -bg yellow

    trace variable mged_display($mged_active_dm($id),fps) w "ia_changestate $id"

    update_mged_vars $id
    set view_ring($id) [get_view]

    aim $id $mged_active_dm($id)

    if {!$multi_view($id)} {
# unpack previously active dm
	pack forget $mged_top($id).$save_dm_loc($id)

# resize and repack previously active dm into smaller frame
	winset $mged_top($id).$save_dm_loc($id)
	set mv_size [expr $win_size($id) / 2 - 4]
	dm size $mv_size $mv_size
	pack $mged_top($id).$save_dm_loc($id) -in $save_small_dmc($id)

	setmv $id
    }
    set save_dm_loc($id) $mged_dm_loc($id)
    set save_small_dmc($id) $mged_small_dmc($id)

    do_view_ring_entries $id s
    do_view_ring_entries $id d

    if {$mged_dm_loc($id) == "ul"} {
	wm title $mged_top($id) "MGED Interaction Window ($id) - Upper Left"
    } elseif {$mged_dm_loc($id) == "ur"} {
	wm title $mged_top($id) "MGED Interaction Window ($id) - Upper Right"
    } elseif {$mged_dm_loc($id) == "ll"} {
	wm title $mged_top($id) "MGED Interaction Window ($id) - Lower Left"
    } elseif {$mged_dm_loc($id) == "lr"} {
	wm title $mged_top($id) "MGED Interaction Window ($id) - Lower Right"
    }
}

proc do_last_visited { id } {
    global mged_top
    global slidersflag

    unaim $id
    wm title $mged_top($id) "MGED Interaction Window ($id) - Last Visited"
}

proc set_cmd_win { id } {
    global cmd_win
    global win_size

    if {$cmd_win($id)} {
	set win_size($id) $win_size($id,small)
	setmv $id
	pack .$id.tf -side top -fill both -expand yes
    } else {
	pack forget .$id.tf
	set win_size($id) $win_size($id,big)
	setmv $id
    }
}

proc set_dm_win { id } {
    global dm_win
    global cmd_win
    global mged_dmc

    if {$dm_win($id)} {
	if {[winfo ismapped .$id.tf]} {
	    pack $mged_dmc($id) -side top -before .$id.tf -padx 2 -pady 2
	    .$id.t configure -width 10 -height 10
	} else {
	    pack $mged_dmc($id) -side top -padx 2 -pady 2
	}
    } else {
	pack forget $mged_dmc($id)
	.$id.t configure -width 80 -height 85
    }
}

proc do_add_view { id } {
    global mged_active_dm
    global mged_dm_loc
    global view_ring
    global mged_collaborators
    global mged_top

#    if {$mged_dm_loc($id) != "lv"} {
#	winset $mged_active_dm($id)
#    }
    winset $mged_active_dm($id)

    _mged_add_view

    set i [lsearch -exact $mged_collaborators $id]
    if {$i != -1 && "$mged_top($id).ur" == "$mged_active_dm($id)"} {
	foreach cid $mged_collaborators {
	    if {"$mged_top($cid).ur" == "$mged_active_dm($cid)"} {
		do_view_ring_entries $cid s
		do_view_ring_entries $cid d
		winset $mged_active_dm($cid)
		set view_ring($cid) [get_view]
	    }
	}
    } else {
	do_view_ring_entries $id s
	do_view_ring_entries $id d
	set view_ring($id) [get_view]
    }
}

proc do_delete_view { id vid } {
    global mged_active_dm
    global mged_dm_loc
    global view_ring
    global mged_collaborators
    global mged_top

#    if {$mged_dm_loc($id) != "lv"} {
#	winset $mged_active_dm($id)
#    }
    winset $mged_active_dm($id)

    _mged_delete_view $vid

    set i [lsearch -exact $mged_collaborators $id]
    if { $i != -1 && "$mged_top($id).ur" == "$mged_active_dm($id)"} {
	foreach cid $mged_collaborators {
	    if {"$mged_top($cid).ur" == "$mged_active_dm($cid)"} {
		do_view_ring_entries $cid s
		do_view_ring_entries $cid d
		winset $mged_active_dm($cid)
		set view_ring($cid) [get_view]
	    }
	}
    } else {
	do_view_ring_entries $id s
	do_view_ring_entries $id d
	set view_ring($id) [get_view]
    }
}

proc do_goto_view { id vid } {
    global mged_active_dm
    global mged_dm_loc
    global view_ring
    global mged_collaborators
    global mged_top

#    if {$mged_dm_loc($id) != "lv"} {
#	winset $mged_active_dm($id)
#    }
    winset $mged_active_dm($id)

    _mged_goto_view $vid

    set i [lsearch -exact $mged_collaborators $id]
    if { $i != -1 && "$mged_top($id).ur" == "$mged_active_dm($id)"} {
	foreach cid $mged_collaborators {
	    if {"$mged_top($cid).ur" == "$mged_active_dm($cid)"} {
		set view_ring($cid) $vid
	    }
	}
    } else {
	set view_ring($id) $vid
    }
}

proc do_next_view { id } {
    global mged_active_dm
    global mged_dm_loc
    global view_ring
    global mged_collaborators
    global mged_top

#    if {$mged_dm_loc($id) != "lv"} {
#	winset $mged_active_dm($id)
#    }
    winset $mged_active_dm($id)

    _mged_next_view

    set i [lsearch -exact $mged_collaborators $id]
    if { $i != -1 && "$mged_top($id).ur" == "$mged_active_dm($id)"} {
	foreach cid $mged_collaborators {
	    if {"$mged_top($cid).ur" == "$mged_active_dm($cid)"} {
		winset $mged_active_dm($cid)
		set view_ring($cid) [get_view]
	    }
	}
    } else {
	set view_ring($id) [get_view]
    }
}

proc do_prev_view { id } {
    global mged_active_dm
    global mged_dm_loc
    global view_ring
    global mged_collaborators
    global mged_top

#    if {$mged_dm_loc($id) != "lv"} {
#	winset $mged_active_dm($id)
#    }
    winset $mged_active_dm($id)

    _mged_prev_view

    set i [lsearch -exact $mged_collaborators $id]
    if { $i != -1 && "$mged_top($id).ur" == "$mged_active_dm($id)"} {
	foreach cid $mged_collaborators {
	    if {"$mged_top($cid).ur" == "$mged_active_dm($cid)"} {
		winset $mged_active_dm($cid)
		set view_ring($cid) [get_view]
	    }
	}
    } else {
	set view_ring($id) [get_view]
    }
}

proc do_toggle_view { id } {
    global mged_active_dm
    global mged_dm_loc
    global view_ring
    global mged_collaborators
    global mged_top

#    if {$mged_dm_loc($id) != "lv"} {
#	winset $mged_active_dm($id)
#    }
    winset $mged_active_dm($id)

    _mged_toggle_view

    set i [lsearch -exact $mged_collaborators $id]
    if { $i != -1 && "$mged_top($id).ur" == "$mged_active_dm($id)"} {
	foreach cid $mged_collaborators {
	    if {"$mged_top($cid).ur" == "$mged_active_dm($cid)"} {
		winset $mged_active_dm($cid)
		set view_ring($cid) [get_view]
	    }
	}
    } else {
	set view_ring($id) [get_view]
    }
}

proc do_view_ring_entries { id m } {
    global view_ring

    set views [get_view -a]
    set llen [llength $views]

    if {$m == "s"} {
	set w .$id.m.viewring.m.cm_select
	$w delete 0 end
	for {set i 0} {$i < $llen} {incr i} {
	    $w add radiobutton -value [lindex $views $i] -variable view_ring($id)\
		    -label [lindex $views $i] -command "do_goto_view $id [lindex $views $i]"
	}
    } elseif {$m == "d"} {
	set w .$id.m.viewring.m.cm_delete
	$w delete 0 end
	for {set i 0} {$i < $llen} {incr i} {
	    $w add command -label [lindex $views $i]\
		    -command "do_delete_view $id [lindex $views $i]"
	}
    } else {
	puts "Usage: do_view_ring_entries w s|d"
    }
}

proc toggle_status_bar { id } {
    global status_bar

    if {$status_bar($id)} {
	pack .$id.status -side bottom -anchor w

	if {[winfo ismapped .$id.tf]} {
	    .$id.t configure -height 6
	}
    } else {
	pack forget .$id.status
	.$id.t configure -height 9
    }
}

proc set_transform { id } {
    global mged_top
    global mged_active_dm
    global transform
    global mged_transform

    winset $mged_top($id).ul
    set transform $mged_transform($id)
    do_mouse_bindings $mged_top($id).ul

    winset $mged_top($id).ur
    set transform $mged_transform($id)
    do_mouse_bindings $mged_top($id).ur

    winset $mged_top($id).ll
    set transform $mged_transform($id)
    do_mouse_bindings $mged_top($id).ll

    winset $mged_top($id).lr
    set transform $mged_transform($id)
    do_mouse_bindings $mged_top($id).lr

    winset $mged_active_dm($id)
}

proc do_rebind_keys { id } {
    global mged_top

    bind $mged_top($id).ul <Control-n> "winset $mged_top($id).ul; do_next_view $id" 
    bind $mged_top($id).ur <Control-n> "winset $mged_top($id).ur; do_next_view $id" 
    bind $mged_top($id).ll <Control-n> "winset $mged_top($id).ll; do_next_view $id" 
    bind $mged_top($id).lr <Control-n> "winset $mged_top($id).lr; do_next_view $id" 

    bind $mged_top($id).ul <Control-p> "winset $mged_top($id).ul; do_prev_view $id" 
    bind $mged_top($id).ur <Control-p> "winset $mged_top($id).ur; do_prev_view $id" 
    bind $mged_top($id).ll <Control-p> "winset $mged_top($id).ll; do_prev_view $id" 
    bind $mged_top($id).lr <Control-p> "winset $mged_top($id).lr; do_prev_view $id" 

    bind $mged_top($id).ul <Control-t> "winset $mged_top($id).ul; do_toggle_view $id" 
    bind $mged_top($id).ur <Control-t> "winset $mged_top($id).ur; do_toggle_view $id" 
    bind $mged_top($id).ll <Control-t> "winset $mged_top($id).ll; do_toggle_view $id" 
    bind $mged_top($id).lr <Control-t> "winset $mged_top($id).lr; do_toggle_view $id" 
}

proc adc { args } {
    global mged_active_dm
    global transform
    global mged_adc_draw

    set result [eval _mged_adc $args]
    set id [lindex [cmd_get] 2]

    if { ![llength $args] } {
	if {[info exists mged_active_dm($id)]} {
	    set mged_adc_draw($id) [adc draw]
	}

	if {$transform == "a"} {
	    do_mouse_bindings [winset]
	}
    }

    return $result
}

proc mged_apply { id cmd } {
    global mged_active_dm
    global mged_dm_loc
    global mged_apply_to

    if {$mged_apply_to($id) == 1} {
	mged_apply_local $id $cmd
    } elseif {$mged_apply_to($id) == 2} {
	mged_apply_using_list $id $cmd
    } elseif {$mged_apply_to($id) == 3} {
	mged_apply_all $cmd
    } else {
	if {$mged_dm_loc($id) != "lv"} {
	    winset $mged_active_dm($id)
	}

	catch [list uplevel #0 $cmd]
    }
}

proc mged_apply_local { id cmd } {
    global mged_top
    global mged_active_dm

    winset $mged_top($id).ul
    catch [list uplevel #0 $cmd]

    winset $mged_top($id).ur
    catch [list uplevel #0 $cmd]

    winset $mged_top($id).ll
    catch [list uplevel #0 $cmd]

    winset $mged_top($id).lr
    catch [list uplevel #0 $cmd] msg

    winset $mged_active_dm($id)

    return $msg
}

proc mged_apply_using_list { id cmd } {
    global mged_apply_list

    foreach dm $mged_apply_list($id) {
	winset $dm
	catch [list uplevel #0 $cmd] msg
    }

    return $msg
}

proc mged_apply_all { cmd } {
    foreach dm [get_dm_list] {
	winset $dm
	catch [list uplevel #0 $cmd] msg
    }

    return $msg
}

proc set_listen { id } {
    global listen
    global mged_listen

    mged_apply $id "set listen \$mged_listen($id)"

# In case things didn't work.
    set mged_listen($id) $listen
}

proc set_fb { id } {
    global fb
    global mged_fb
    global listen
    global mged_listen

    mged_apply $id "set fb \$mged_fb($id)"

    if {$mged_fb($id)} {
	.$id.m.settings.m.cm_fb entryconfigure 6 -state normal
	set mged_listen($id) 1
	mged_apply $id "set listen \$mged_listen($id)"
    } else {
	.$id.m.settings.m.cm_fb entryconfigure 6 -state disabled
	set mged_listen($id) 0
    }
}
