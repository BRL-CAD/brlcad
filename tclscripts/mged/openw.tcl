# Author -
#	Bob Parker
#
# Source -
#	The U. S. Army Research Laboratory
#	Aberdeen Proving Ground, Maryland  21005
#
# Distribution Notice -
#	Re-distribution of this software is restricted, as described in
#       your "Statement of Terms and Conditions for the Release of
#       The BRL-CAD Package" agreement.
#
# Copyright Notice -
#       This software is Copyright (C) 1998 by the United States Army
#       in all countries except the USA.  All rights reserved.
#
# Description -
#	MGED's default Tcl/Tk interface.
#
# $Revision
#

check_externs "_mged_attach _mged_aim _mged_add_view _mged_delete_view _mged_get_view _mged_goto_view _mged_next_view _mged_prev_view _mged_toggle_view"

if [info exists env(MGED_HTML_DIR)] {
        set mged_html_dir $env(MGED_HTML_DIR)
} else {
        set mged_html_dir [lindex $auto_path 0]/../html/mged
}

if ![info exists bob_testing] {
    set bob_testing 0
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

if ![info exists mged_default_num_lines] {
    set mged_default_num_lines 10
}

if ![info exists player_screen(mged)] {
    set player_screen(mged) $mged_default_display
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
    global mged_default_num_lines
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
    global mged_num_lines
    global save_small_dmc
    global save_dm_loc
    global ia_cmd_prefix
    global ia_more_default
    global ia_font
    global ia_illum_label
    global env
    global edit_info_pos
    global show_edit_info
    global mged_multi_view
    global buttons_on
    global win_size
    global mged_show_cmd
    global mged_show_dm
    global mged_show_status
    global view_ring
    global do_tearoffs
    global freshline
    global scratchline
    global vi_overwrite_flag
    global vi_change_flag
    global vi_delete_flag
    global vi_search_flag
    global vi_search_char
    global vi_search_dir
    global dm_insert_char_flag
    global mged_comb
    global bob_testing

    if {$mged_default_dt == ""} {
	set mged_default_dt [dm_bestXType $mged_default_gdisplay]
    }

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

set mged_comb($id) $comb
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
set mged_show_cmd($id) $scw
set mged_show_dm($id) $sgw
set mged_show_status($id) 1
set mged_apply_to($id) 0
set mged_grid_control($id) 0
set edit_info_pos($id) "+0+0"
set mged_num_lines($id) $mged_default_num_lines

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
	
toplevel .$id -screen $screen -menu .$id.menubar

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

menu .$id.menubar -tearoff $do_tearoffs
.$id.menubar add cascade -label "File" -underline 0 -menu .$id.menubar.file
.$id.menubar add cascade -label "Edit" -underline 0 -menu .$id.menubar.edit
.$id.menubar add cascade -label "Create" -underline 0 -menu .$id.menubar.create
.$id.menubar add cascade -label "View" -underline 0 -menu .$id.menubar.view
.$id.menubar add cascade -label "ViewRing" -underline 4 -menu .$id.menubar.viewring
.$id.menubar add cascade -label "Settings" -underline 0 -menu .$id.menubar.settings
.$id.menubar add cascade -label "Modes" -underline 0 -menu .$id.menubar.modes
.$id.menubar add cascade -label "Tools" -underline 0 -menu .$id.menubar.tools
.$id.menubar add cascade -label "Help" -underline 0 -menu .$id.menubar.help

menu .$id.menubar.file -tearoff $do_tearoffs
.$id.menubar.file add command -label "New..." -underline 0 -command "do_New $id"
.$id.menubar.file add command -label "Open..." -underline 0 -command "do_Open $id"
.$id.menubar.file add command -label "Insert..." -underline 0 -command "do_Concat $id"
.$id.menubar.file add command -label "Extract..." -underline 1 -command "init_extractTool $id"
.$id.menubar.file add separator
.$id.menubar.file add command -label "Raytrace..." -underline 0 -command "init_Raytrace $id"
.$id.menubar.file add cascade -label "Save View As" -underline 0 -menu .$id.menubar.file.saveview
.$id.menubar.file add separator
.$id.menubar.file add cascade -label "Preferences" -underline 0 -menu .$id.menubar.file.pref
.$id.menubar.file add separator
.$id.menubar.file add command -label "Close" -underline 0 -command "gui_destroy_default $id"
.$id.menubar.file add command -label "Exit" -underline 0 -command quit

menu .$id.menubar.file.saveview -tearoff $do_tearoffs
.$id.menubar.file.saveview add command -label "RT script" -underline 0\
	-command "init_rtScriptTool $id"
.$id.menubar.file.saveview add command -label "Plot" -underline 1\
	-command "init_plotTool $id"
.$id.menubar.file.saveview add command -label "PostScript" -underline 0\
	-command "init_psTool $id"

menu .$id.menubar.file.pref -tearoff $do_tearoffs
.$id.menubar.file.pref add cascade -label "Units" -underline 0\
	-menu .$id.menubar.file.pref.units
.$id.menubar.file.pref add cascade -label "Command Line Edit" -underline 0\
	-menu .$id.menubar.file.pref.cle
.$id.menubar.file.pref add cascade -label "View Axes Position" -underline 0\
	-menu .$id.menubar.file.pref.vap
.$id.menubar.file.pref add cascade -label "Special Characters" -underline 0\
	-menu .$id.menubar.file.pref.special_chars

menu .$id.menubar.file.pref.units -tearoff $do_tearoffs
.$id.menubar.file.pref.units add radiobutton -value um -variable mged_display(units)\
	-label "micrometers" -underline 4 -command "do_Units $id"
.$id.menubar.file.pref.units add radiobutton -value mm -variable mged_display(units)\
	-label "millimeters" -underline 2 -command "do_Units $id"
.$id.menubar.file.pref.units add radiobutton -value cm -variable mged_display(units)\
	-label "centimeters" -underline 0 -command "do_Units $id"
.$id.menubar.file.pref.units add radiobutton -value m -variable mged_display(units)\
	-label "meters" -underline 0 -command "do_Units $id"
.$id.menubar.file.pref.units add radiobutton -value km -variable mged_display(units)\
	-label "kilometers" -underline 0 -command "do_Units $id"
.$id.menubar.file.pref.units add separator
.$id.menubar.file.pref.units add radiobutton -value in -variable mged_display(units)\
	-label "inches" -underline 0 -command "do_Units $id"
.$id.menubar.file.pref.units add radiobutton -value ft -variable mged_display(units)\
	-label "feet" -underline 0 -command "do_Units $id"
.$id.menubar.file.pref.units add radiobutton -value yd -variable mged_display(units)\
	-label "yards" -underline 0 -command "do_Units $id"
.$id.menubar.file.pref.units add radiobutton -value mi -variable mged_display(units)\
	-label "miles" -underline 3 -command "do_Units $id"

menu .$id.menubar.file.pref.cle -tearoff $do_tearoffs
.$id.menubar.file.pref.cle add radiobutton -value emacs -variable mged_edit_style($id)\
	-label "emacs" -underline 0 -command "set_text_key_bindings $id"
.$id.menubar.file.pref.cle add radiobutton -value vi -variable mged_edit_style($id)\
	-label "vi" -underline 0 -command "set_text_key_bindings $id"

menu .$id.menubar.file.pref.vap -tearoff $do_tearoffs
.$id.menubar.file.pref.vap add radiobutton -value 0 -variable mged_v_axes_pos($id)\
	-label "Center" -underline 0\
	-command "mged_apply $id \"set v_axes_pos {0 0}\""
.$id.menubar.file.pref.vap add radiobutton -value 1 -variable mged_v_axes_pos($id)\
	-label "Lower Left" -underline 2\
	-command "mged_apply $id \"set v_axes_pos {-1750 -1750}\""
.$id.menubar.file.pref.vap add radiobutton -value 2 -variable mged_v_axes_pos($id)\
	-label "Upper Left" -underline 6\
	-command "mged_apply $id \"set v_axes_pos {-1750 1750}\""
.$id.menubar.file.pref.vap add radiobutton -value 3 -variable mged_v_axes_pos($id)\
	-label "Upper Right" -underline 6\
	-command "mged_apply $id \"set v_axes_pos {1750 1750}\""
.$id.menubar.file.pref.vap add radiobutton -value 4 -variable mged_v_axes_pos($id)\
	-label "Lower Right" -underline 3\
	-command "mged_apply $id \"set v_axes_pos {1750 -1750}\""

menu .$id.menubar.file.pref.special_chars -tearoff $do_tearoffs
.$id.menubar.file.pref.special_chars add radiobutton -value 0 -variable glob_compat_mode\
	-label "Tcl Evaluation" -underline 0
.$id.menubar.file.pref.special_chars add radiobutton -value 1 -variable glob_compat_mode\
	-label "Object Name Matching" -underline 0

menu .$id.menubar.edit -tearoff $do_tearoffs
.$id.menubar.edit add command -label "Solid..." -underline 0 \
	-command "winset \$mged_active_dm($id); build_edit_menu_all s"
.$id.menubar.edit add command -label "Matrix..." -underline 0 \
	-command "winset \$mged_active_dm($id); build_edit_menu_all o"
.$id.menubar.edit add command -label "Combination..." -underline 0 \
	-command "init_comb $id"

menu .$id.menubar.create -tearoff $do_tearoffs
.$id.menubar.create add cascade\
	-label "Solid" -underline 0 -menu .$id.menubar.create.solid
.$id.menubar.create add command\
	-label "Instance Creation Panel..." -underline 0 -command "icreate $id"

menu .$id.menubar.create.solid -tearoff $do_tearoffs
.$id.menubar.create.solid add command\
	-label "Solid Creation Panel..." -underline 0 -command "solcreate $id"
.$id.menubar.create.solid add separator
set make_solid_types [_mged_make -t]
foreach solid_type $make_solid_types {
    .$id.menubar.create.solid add command -label "$solid_type..."\
	    -command "init_solid_create $id $solid_type"
}

menu .$id.menubar.view -tearoff $do_tearoffs
.$id.menubar.view add command -label "Top" -underline 0\
	-command "mged_apply $id \"press top\""
.$id.menubar.view add command -label "Bottom" -underline 0\
	-command "mged_apply $id \"press bottom\""
.$id.menubar.view add command -label "Right" -underline 0\
	-command "mged_apply $id \"press right\""
.$id.menubar.view add command -label "Left" -underline 0\
	-command "mged_apply $id \"press left\""
.$id.menubar.view add command -label "Front" -underline 0\
	-command "mged_apply $id \"press front\""
.$id.menubar.view add command -label "Back" -underline 3\
	-command "mged_apply $id \"press rear\""
.$id.menubar.view add command -label "az35,el25" -underline 2\
	-command "mged_apply $id \"press 35,25\""
.$id.menubar.view add command -label "az45,el45" -underline 2\
	-command "mged_apply $id \"press 45,45\""
.$id.menubar.view add separator
.$id.menubar.view add command -label "Zoom In" -underline 5\
	-command "mged_apply $id \"zoom 2\""
.$id.menubar.view add command -label "Zoom Out" -underline 5\
	-command "mged_apply $id \"zoom 0.5\""
.$id.menubar.view add separator
.$id.menubar.view add command -label "Save" -underline 0\
	-command "mged_apply $id \"press save\""
.$id.menubar.view add command -label "Restore" -underline 1\
	-command "mged_apply $id \"press restore\""
.$id.menubar.view add separator
.$id.menubar.view add command -label "Default"\
	-underline 6 -command "mged_apply $id \"press reset\""
.$id.menubar.view add command -label "Multipane Defaults"\
	-underline 6 -command "set_default_views $id"
.$id.menubar.view add command -label "Zero" -underline 0\
	-command "mged_apply $id \"knob zero\""

menu .$id.menubar.viewring -tearoff $do_tearoffs
.$id.menubar.viewring add command -label "Add View" -underline 0 -command "do_add_view $id"
.$id.menubar.viewring add cascade -label "Select View" -underline 0 -menu .$id.menubar.viewring.select
.$id.menubar.viewring add cascade -label "Delete View" -underline 0 -menu .$id.menubar.viewring.delete
.$id.menubar.viewring add command -label "Next View" -underline 0 -command "do_next_view $id"
.$id.menubar.viewring add command -label "Prev View" -underline 0 -command "do_prev_view $id"
.$id.menubar.viewring add command -label "Last View" -underline 0 -command "do_toggle_view $id"

menu .$id.menubar.viewring.select -tearoff $do_tearoffs
do_view_ring_entries $id s
set view_ring($id) 1
menu .$id.menubar.viewring.delete -tearoff $do_tearoffs
do_view_ring_entries $id d

menu .$id.menubar.settings -tearoff $do_tearoffs
.$id.menubar.settings add cascade -label "Mouse Behavior" -underline 0\
	-menu .$id.menubar.settings.mouse_behavior
.$id.menubar.settings add cascade -label "Transform" -underline 0\
	-menu .$id.menubar.settings.transform
.$id.menubar.settings add cascade -label "Constraint Coords" -underline 0\
	-menu .$id.menubar.settings.coord
.$id.menubar.settings add cascade -label "Rotate About" -underline 0\
	-menu .$id.menubar.settings.origin
.$id.menubar.settings add cascade -label "Active Pane" -underline 0\
	-menu .$id.menubar.settings.mpane
.$id.menubar.settings add cascade -label "Apply To" -underline 1\
	-menu .$id.menubar.settings.applyTo
.$id.menubar.settings add cascade -label "Query Ray Effects" -underline 0\
	-menu .$id.menubar.settings.qray
.$id.menubar.settings add cascade -label "Grid" -underline 0\
	-menu .$id.menubar.settings.grid
.$id.menubar.settings add cascade -label "Grid Spacing" -underline 5\
	-menu .$id.menubar.settings.grid_spacing
.$id.menubar.settings add cascade -label "Framebuffer" -underline 0\
	-menu .$id.menubar.settings.fb

menu .$id.menubar.settings.applyTo -tearoff $do_tearoffs
.$id.menubar.settings.applyTo add radiobutton -value 0 -variable mged_apply_to($id)\
	-label "Active Pane" -underline 0
.$id.menubar.settings.applyTo add radiobutton -value 1 -variable mged_apply_to($id)\
	-label "Local Panes" -underline 0
.$id.menubar.settings.applyTo add radiobutton -value 2 -variable mged_apply_to($id)\
	-label "Listed Panes" -underline 1
.$id.menubar.settings.applyTo add radiobutton -value 3 -variable mged_apply_to($id)\
	-label "All Panes" -underline 4

menu .$id.menubar.settings.mouse_behavior -tearoff $do_tearoffs
.$id.menubar.settings.mouse_behavior add radiobutton -value d -variable mged_mouse_behavior($id)\
	-label "Default" -underline 0\
	-command "mged_apply $id \"set mouse_behavior \$mged_mouse_behavior($id)\""
.$id.menubar.settings.mouse_behavior add radiobutton -value p -variable mged_mouse_behavior($id)\
	-label "Paint Rectangle Area" -underline 0\
	-command "mged_apply $id \"set mouse_behavior \$mged_mouse_behavior($id)\""
.$id.menubar.settings.mouse_behavior add radiobutton -value r -variable mged_mouse_behavior($id)\
	-label "Raytrace Rectangle Area" -underline 0\
	-command "mged_apply $id \"set mouse_behavior \$mged_mouse_behavior($id)\""
.$id.menubar.settings.mouse_behavior add radiobutton -value z -variable mged_mouse_behavior($id)\
	-label "Zoom Rectangle Area" -underline 0\
	-command "mged_apply $id \"set mouse_behavior \$mged_mouse_behavior($id)\""
.$id.menubar.settings.mouse_behavior add radiobutton -value q -variable mged_mouse_behavior($id)\
	-label "Query Ray" -underline 0\
	-command "mged_apply $id \"set mouse_behavior \$mged_mouse_behavior($id)\""
.$id.menubar.settings.mouse_behavior add radiobutton -value s -variable mged_mouse_behavior($id)\
	-label "Solid Edit Ray" -underline 0\
	-command "mged_apply $id \"set mouse_behavior \$mged_mouse_behavior($id)\""
.$id.menubar.settings.mouse_behavior add radiobutton -value o -variable mged_mouse_behavior($id)\
	-label "Object Edit Ray" -underline 0\
	-command "mged_apply $id \"set mouse_behavior \$mged_mouse_behavior($id)\""
.$id.menubar.settings.mouse_behavior add radiobutton -value c -variable mged_mouse_behavior($id)\
	-label "Combination Edit Ray" -underline 0\
	-command "mged_apply $id \"set mouse_behavior \$mged_mouse_behavior($id)\""

menu .$id.menubar.settings.qray -tearoff $do_tearoffs
.$id.menubar.settings.qray add radiobutton -value t -variable mged_qray_effects($id)\
	-label "Text" -underline 0\
	-command "mged_apply $id \"qray effects \$mged_qray_effects($id)\""
.$id.menubar.settings.qray add radiobutton -value g -variable mged_qray_effects($id)\
	-label "Graphics" -underline 0\
	-command "mged_apply $id \"qray effects \$mged_qray_effects($id)\""
.$id.menubar.settings.qray add radiobutton -value b -variable mged_qray_effects($id)\
	-label "both" -underline 0\
	-command "mged_apply $id \"qray effects \$mged_qray_effects($id)\""

menu .$id.menubar.settings.mpane -tearoff $do_tearoffs
.$id.menubar.settings.mpane add radiobutton -value ul -variable mged_dm_loc($id)\
	-label "Upper Left" -underline 6\
	-command "set_active_dm $id"
.$id.menubar.settings.mpane add radiobutton -value ur -variable mged_dm_loc($id)\
	-label "Upper Right" -underline 6\
	-command "set_active_dm $id"
.$id.menubar.settings.mpane add radiobutton -value ll -variable mged_dm_loc($id)\
	-label "Lower Left" -underline 2\
	-command "set_active_dm $id"
.$id.menubar.settings.mpane add radiobutton -value lr -variable mged_dm_loc($id)\
	-label "Lower Right" -underline 3\
	-command "set_active_dm $id"

menu .$id.menubar.settings.fb -tearoff $do_tearoffs
.$id.menubar.settings.fb add radiobutton -value 1 -variable mged_fb_all($id)\
	-label "All" -underline 0\
	-command "mged_apply $id \"set fb_all \$mged_fb_all($id)\""
.$id.menubar.settings.fb add radiobutton -value 0 -variable mged_fb_all($id)\
	-label "Rectangle Area" -underline 0\
	-command "mged_apply $id \"set fb_all \$mged_fb_all($id)\""
.$id.menubar.settings.fb add separator
.$id.menubar.settings.fb add radiobutton -value 1 -variable mged_fb_overlay($id)\
	-label "Overlay" -underline 0\
	-command "mged_apply $id \"set fb_overlay \$mged_fb_overlay($id)\""
.$id.menubar.settings.fb add radiobutton -value 0 -variable mged_fb_overlay($id)\
	-label "Underlay" -underline 0\
	-command "mged_apply $id \"set fb_overlay \$mged_fb_overlay($id)\""
.$id.menubar.settings.fb add separator
.$id.menubar.settings.fb add checkbutton -offvalue 0 -onvalue 1 -variable mged_fb($id)\
	-label "Framebuffer Active" -underline 0\
	-command "set_fb $id; update_Raytrace $id"
.$id.menubar.settings.fb add checkbutton -offvalue 0 -onvalue 1 -variable mged_listen($id)\
	-label "Listen For Clients" -underline 0\
	-command "set_listen $id" -state disabled

menu .$id.menubar.settings.grid -tearoff $do_tearoffs
.$id.menubar.settings.grid add command -label "Anchor..." -underline 0\
	-command "do_grid_anchor $id"
.$id.menubar.settings.grid add command -label "Color..." -underline 0\
	-command "do_grid_color $id"
.$id.menubar.settings.grid add cascade -label "Spacing" -underline 1\
	-menu .$id.menubar.settings.grid.spacing
.$id.menubar.settings.grid add separator
.$id.menubar.settings.grid add checkbutton -offvalue 0 -onvalue 1 -variable mged_grid_snap($id)\
	-label "Snap To Grid" -underline 0\
	-command "mged_apply $id \"set grid_snap \$mged_grid_snap($id)\""
.$id.menubar.settings.grid add checkbutton -offvalue 0 -onvalue 1 -variable mged_grid_draw($id)\
	-label "Draw Grid" -underline 0\
	-command "mged_apply $id \"set grid_draw \$mged_grid_draw($id)\""

menu .$id.menubar.settings.grid.spacing -tearoff $do_tearoffs
.$id.menubar.settings.grid.spacing add command -label "Autosize" -underline 0\
	-command "grid_control_autosize $id; grid_spacing_apply $id b"
.$id.menubar.settings.grid.spacing add command -label "Arbitrary..." -underline 1\
	-command "do_grid_spacing $id b"
.$id.menubar.settings.grid.spacing add separator
.$id.menubar.settings.grid.spacing add command -label "micrometer" -underline 4\
	-command "set_grid_spacing $id micrometer"
.$id.menubar.settings.grid.spacing add command -label "millimeter" -underline 2\
	-command "set_grid_spacing $id millimeter"
.$id.menubar.settings.grid.spacing add command -label "centimeter" -underline 0\
	-command "set_grid_spacing $id centimeter"
.$id.menubar.settings.grid.spacing add command -label "decimeter" -underline 0\
	-command "set_grid_spacing $id decimeter"
.$id.menubar.settings.grid.spacing add command -label "meter" -underline 0\
	-command "set_grid_spacing $id meter"
.$id.menubar.settings.grid.spacing add command -label "kilometer" -underline 0\
	-command "set_grid_spacing $id kilometer"
.$id.menubar.settings.grid.spacing add separator
.$id.menubar.settings.grid.spacing add command -label "1/10 inch" -underline 0\
	-command "set_grid_spacing $id \"1/10 inch\""
.$id.menubar.settings.grid.spacing add command -label "1/4 inch" -underline 2\
	-command "set_grid_spacing $id \"1/4 inch\""
.$id.menubar.settings.grid.spacing add command -label "1/2 inch" -underline 2\
	-command "set_grid_spacing $id \"1/2 inch\""
.$id.menubar.settings.grid.spacing add command -label "inch" -underline 0\
	-command "set_grid_spacing $id inch"
.$id.menubar.settings.grid.spacing add command -label "foot" -underline 0\
	-command "set_grid_spacing $id foot"
.$id.menubar.settings.grid.spacing add command -label "yard" -underline 0\
	-command "set_grid_spacing $id yard"
.$id.menubar.settings.grid.spacing add command -label "mile" -underline 0\
	-command "set_grid_spacing $id mile"

menu .$id.menubar.settings.grid_spacing -tearoff $do_tearoffs
.$id.menubar.settings.grid_spacing add command -label "Autosize" -underline 0\
	-command "grid_control_autosize $id; grid_spacing_apply $id b"
.$id.menubar.settings.grid_spacing add command -label "Arbitrary..." -underline 1\
	-command "do_grid_spacing $id b"
.$id.menubar.settings.grid_spacing add separator
.$id.menubar.settings.grid_spacing add command -label "micrometer" -underline 4\
	-command "set_grid_spacing $id micrometer"
.$id.menubar.settings.grid_spacing add command -label "millimeter" -underline 2\
	-command "set_grid_spacing $id millimeter"
.$id.menubar.settings.grid_spacing add command -label "centimeter" -underline 0\
	-command "set_grid_spacing $id centimeter"
.$id.menubar.settings.grid_spacing add command -label "decimeter" -underline 0\
	-command "set_grid_spacing $id decimeter"
.$id.menubar.settings.grid_spacing add command -label "meter" -underline 0\
	-command "set_grid_spacing $id meter"
.$id.menubar.settings.grid_spacing add command -label "kilometer" -underline 0\
	-command "set_grid_spacing $id kilometer"
.$id.menubar.settings.grid_spacing add separator
.$id.menubar.settings.grid_spacing add command -label "1/10 inch" -underline 0\
	-command "set_grid_spacing $id \"1/10 inch\""
.$id.menubar.settings.grid_spacing add command -label "1/4 inch" -underline 2\
	-command "set_grid_spacing $id \"1/4 inch\""
.$id.menubar.settings.grid_spacing add command -label "1/2 inch" -underline 2\
	-command "set_grid_spacing $id \"1/2 inch\""
.$id.menubar.settings.grid_spacing add command -label "inch" -underline 0\
	-command "set_grid_spacing $id inch"
.$id.menubar.settings.grid_spacing add command -label "foot" -underline 0\
	-command "set_grid_spacing $id foot"
.$id.menubar.settings.grid_spacing add command -label "yard" -underline 0\
	-command "set_grid_spacing $id yard"
.$id.menubar.settings.grid_spacing add command -label "mile" -underline 0\
	-command "set_grid_spacing $id mile"

menu .$id.menubar.settings.coord -tearoff $do_tearoffs
.$id.menubar.settings.coord add radiobutton -value m -variable mged_coords($id)\
	-label "Model" -underline 0\
	-command "mged_apply $id \"set coords \$mged_coords($id)\""
.$id.menubar.settings.coord add radiobutton -value v -variable mged_coords($id)\
	-label "View" -underline 0\
	-command "mged_apply $id \"set coords \$mged_coords($id)\""
.$id.menubar.settings.coord add radiobutton -value o -variable mged_coords($id)\
	-label "Object" -underline 0\
	-command "mged_apply $id \"set coords \$mged_coords($id)\"" -state disabled

menu .$id.menubar.settings.origin -tearoff $do_tearoffs
.$id.menubar.settings.origin add radiobutton -value v -variable mged_rotate_about($id)\
	-label "View Center" -underline 0\
	-command "mged_apply $id \"set rotate_about \$mged_rotate_about($id)\""
.$id.menubar.settings.origin add radiobutton -value e -variable mged_rotate_about($id)\
	-label "Eye" -underline 0\
	-command "mged_apply $id \"set rotate_about \$mged_rotate_about($id)\""
.$id.menubar.settings.origin add radiobutton -value m -variable mged_rotate_about($id)\
	-label "Model Origin" -underline 0\
	-command "mged_apply $id \"set rotate_about \$mged_rotate_about($id)\""
.$id.menubar.settings.origin add radiobutton -value k -variable mged_rotate_about($id)\
	-label "Key Point" -underline 0\
	-command "mged_apply $id \"set rotate_about \$mged_rotate_about($id)\"" -state disabled

menu .$id.menubar.settings.transform -tearoff $do_tearoffs
.$id.menubar.settings.transform add radiobutton -value v -variable mged_transform($id)\
	-label "View" -underline 0\
	-command "set_transform $id"
.$id.menubar.settings.transform add radiobutton -value a -variable mged_transform($id)\
	-label "ADC" -underline 0\
	-command "set_transform $id"
.$id.menubar.settings.transform add radiobutton -value e -variable mged_transform($id)\
	-label "Model Params" -underline 0\
	-command "set_transform $id" -state disabled

menu .$id.menubar.modes -tearoff $do_tearoffs
.$id.menubar.modes add checkbutton -offvalue 0 -onvalue 1 -variable mged_rateknobs($id)\
	-label "Rateknobs" -underline 0\
	-command "mged_apply $id \"set rateknobs \$mged_rateknobs($id)\""
.$id.menubar.modes add checkbutton -offvalue 0 -onvalue 1 -variable mged_grid_snap($id)\
	-label "Snap To Grid" -underline 0\
	-command "mged_apply $id \"set grid_snap \$mged_grid_snap($id)\""
.$id.menubar.modes add separator
.$id.menubar.modes add checkbutton -offvalue 0 -onvalue 1 -variable mged_grid_draw($id)\
	-label "Draw Grid" -underline 0\
	-command "mged_apply $id \"set grid_draw \$mged_grid_draw($id)\""
.$id.menubar.modes add checkbutton -offvalue 0 -onvalue 1 -variable mged_rubber_band($id)\
	-label "Persistent Rubber Band" -underline 0\
	-command "mged_apply $id \"set rubber_band \$mged_rubber_band($id)\""
.$id.menubar.modes add checkbutton -offvalue 0 -onvalue 1 -variable mged_adc_draw($id)\
	-label "Angle/Dist Cursor" -underline 0 \
	-command "mged_apply $id \"adc draw \$mged_adc_draw($id)\""
.$id.menubar.modes add checkbutton -offvalue 0 -onvalue 1 -variable mged_faceplate($id)\
	-label "Faceplate" -underline 7\
	-command "mged_apply $id \"set faceplate \$mged_faceplate($id)\""
.$id.menubar.modes add cascade -label "Axes" -underline 1\
	-menu .$id.menubar.modes.axes
.$id.menubar.modes add separator
.$id.menubar.modes add checkbutton -offvalue 0 -onvalue 1 -variable mged_fb($id)\
	-label "Framebuffer Active" -underline 0 \
	-command "set_fb $id; update_Raytrace $id"
.$id.menubar.modes add checkbutton -offvalue 0 -onvalue 1 -variable mged_listen($id)\
	-label "Listen For Clients" -underline 0\
	-command "set_listen $id" -state disabled
.$id.menubar.modes add separator
.$id.menubar.modes add checkbutton -offvalue 0 -onvalue 1 -variable mged_multi_view($id)\
	-label "Multipane" -underline 0 -command "setmv $id"
.$id.menubar.modes add checkbutton -offvalue 0 -onvalue 1 -variable show_edit_info($id)\
	-label "Edit Info" -underline 0\
	-command "toggle_edit_info $id"
.$id.menubar.modes add checkbutton -offvalue 0 -onvalue 1 -variable mged_show_status($id)\
	-label "Status Bar" -underline 7\
	-command "toggle_status_bar $id"
if {$comb} {
.$id.menubar.modes add checkbutton -offvalue 0 -onvalue 1 -variable mged_show_cmd($id)\
	-label "Command Window" -underline 0\
	-command "set_cmd_win $id"
.$id.menubar.modes add checkbutton -offvalue 0 -onvalue 1 -variable mged_show_dm($id)\
	-label "Graphics Window" -underline 0\
	-command "set_dm_win $id"
} 

menu .$id.menubar.modes.axes -tearoff $do_tearoffs
.$id.menubar.modes.axes add checkbutton -offvalue 0 -onvalue 1\
	-variable mged_v_axes($id) -label "View" -underline 0\
	-command "mged_apply $id \"set v_axes \$mged_v_axes($id)\""
.$id.menubar.modes.axes add checkbutton -offvalue 0 -onvalue 1\
	-variable mged_m_axes($id) -label "Model" -underline 0\
	-command "mged_apply $id \"set m_axes \$mged_m_axes($id)\""
.$id.menubar.modes.axes add checkbutton -offvalue 0 -onvalue 1\
	-variable mged_e_axes($id) -label "Edit" -underline 0\
	-command "mged_apply $id \"set e_axes \$mged_e_axes($id)\""

menu .$id.menubar.tools -tearoff $do_tearoffs
.$id.menubar.tools add command -label "ADC Control Panel..." -underline 0\
	-command "init_adc_control $id"
.$id.menubar.tools add command -label "Grid Control Panel..." -underline 0\
	-command "init_grid_control $id"
.$id.menubar.tools add command -label "Query Ray Control Panel..." -underline 0\
	-command "init_qray_control $id"
.$id.menubar.tools add command -label "Combination Edit Panel..." -underline 0\
	-command "init_comb $id"
.$id.menubar.tools add separator
.$id.menubar.tools add checkbutton -offvalue 0 -onvalue 1 -variable buttons_on($id)\
	-label "Button Menu..." -underline 0\
	-command "toggle_button_menu $id"

menu .$id.menubar.help -tearoff $do_tearoffs
.$id.menubar.help add command -label "About" -underline 0\
	-command "do_About_MGED $id"
.$id.menubar.help add command -label "On Context" -underline 0\
	-command "on_context_help $id"
.$id.menubar.help add command -label "Commands..." -underline 0\
	-command "command_help $id"
.$id.menubar.help add command -label "Apropos..." -underline 1\
	-command "ia_apropos .$id $screen"

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
.$id.menubar.help add command -label "Manual..." -underline 0 -command $web_cmd

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
if {$bob_testing} {
    text .$id.t -height $mged_num_lines($id) -relief sunken -bd 2 -yscrollcommand ".$id.s set"
} else {
    if {$comb} {
	text .$id.t -width 10 -relief sunken -bd 2 -yscrollcommand ".$id.s set" -setgrid true
    } else {
	text .$id.t -relief sunken -bd 2 -yscrollcommand ".$id.s set" -setgrid true
    }
}
scrollbar .$id.s -relief flat -command ".$id.t yview"

bind .$id.t <Enter> "focus .$id.t"

set mged_edit_style($id) $mged_default_edit_style
set dm_insert_char_flag(.$id.t) 0
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
set vi_overwrite_flag(.$id.t) 0
set vi_change_flag(.$id.t) 0
set vi_delete_flag(.$id.t) 0
set vi_search_flag(.$id.t) 0
set vi_search_char(.$id.t) ""
set vi_search_dir(.$id.t) ""

.$id.t tag configure sel -background #fefe8e
.$id.t tag configure result -foreground blue3
.$id.t tag configure oldcmd -foreground red3
.$id.t tag configure prompt -foreground red1

#==============================================================================
# Pack windows
#==============================================================================

if { $bob_testing } {
    grid $mged_active_dm($id) -in $mged_dmc($id) -sticky "nsew" -row 0 -column 0
} else {
    pack $mged_active_dm($id) -in $mged_dmc($id)

    if {$comb} {
	pack $mged_dmc($id) -side top -padx 2 -pady 2
    }
}

set mged_multi_view($id) $mged_default_mvmode
#setmv $id

if { $bob_testing } {
    if {$comb} {
	grid $mged_dmc($id) -sticky nsew -row 0 -column 0
    }

    grid .$id.t .$id.s -in .$id.tf -sticky "nsew"
    grid columnconfigure .$id.tf 0 -weight 1
    grid columnconfigure .$id.tf 1 -weight 0
    grid rowconfigure .$id.tf 0 -weight 1
    grid .$id.tf -sticky "nsew" -row 1 -column 0

    grid .$id.status.cent .$id.status.size .$id.status.units .$id.status.aet\
	    .$id.status.ang x -in .$id.status.dpy -sticky "ew"
    grid columnconfigure .$id.status.dpy 0 -weight 0
    grid columnconfigure .$id.status.dpy 1 -weight 0
    grid columnconfigure .$id.status.dpy 2 -weight 0
    grid columnconfigure .$id.status.dpy 3 -weight 0
    grid columnconfigure .$id.status.dpy 4 -weight 0
    grid columnconfigure .$id.status.dpy 5 -weight 1
    grid rowconfigure .$id.status.dpy 0 -weight 0
    grid .$id.status.dpy -sticky "ew"
    grid .$id.status.illum.label x -sticky "ew"
    grid columnconfigure .$id.status.illum 0 -weight 0
    grid columnconfigure .$id.status.illum 1 -weight 1
    grid rowconfigure .$id.status.illum 0 -weight 0
    grid .$id.status.illum -sticky "w"
    grid columnconfigure .$id.status 0 -weight 1
    grid rowconfigure .$id.status 0 -weight 0
    grid rowconfigure .$id.status 1 -weight 0
    grid rowconfigure .$id.status 2 -weight 0
    grid .$id.status -sticky "ew"

    grid columnconfigure .$id 0 -weight 1
    if {$comb} {
	grid rowconfigure .$id 0 -weight 1
	grid rowconfigure .$id 1 -weight 0
	grid rowconfigure .$id 2 -weight 0
    } else {
	grid rowconfigure .$id 0 -weight 1
	grid rowconfigure .$id 1 -weight 0
    }
} else {
    pack .$id.status.cent .$id.status.size .$id.status.units .$id.status.aet\
	    .$id.status.ang -in .$id.status.dpy -side left -anchor w
    pack .$id.status.dpy -side top -anchor w -expand 1 -fill x
    pack .$id.status.illum.label -side left -anchor w
    pack .$id.status.illum -side top -anchor w -expand 1 -fill x
    pack .$id.status -side bottom -anchor w -expand 1 -fill x

    pack .$id.s -in .$id.tf -side right -fill y
    pack .$id.t -in .$id.tf -side top -fill both -expand yes
    pack .$id.tf -side top -fill both -expand yes
}

#==============================================================================
# PHASE 5: Creation of other auxilary windows
#==============================================================================
mview_build_menubar $id

#==============================================================================
# PHASE 6: Further setup
#==============================================================================

if {[info procs mged_MenuFirstEntry] == ""} {
    # trigger the Tcl source file to be loaded
    mged_MenuFirstEntry ""
}

cmd_init $id
setupmv $id
aim $id $mged_active_dm($id)

if {$comb} {
#    set_dm_win $id
#    set_cmd_win $id
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
bind $mged_dmc($id) <Configure> "setmv $id"

set dbname [_mged_opendb]
set_wm_title $id $dbname

wm protocol $mged_top($id) WM_DELETE_WINDOW "gui_destroy_default $id"
wm geometry $mged_top($id) -0+0
}

proc gui_destroy_default args {
    global mged_players
    global mged_collaborators
    global mged_multi_view
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

    set mged_multi_view($id) 0
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
    global faceplate
    global mged_faceplate

    winset $mged_active_dm($id)
    set mged_rateknobs($id) $rateknobs
    set mged_adc_draw($id) [adc draw]
    set mged_m_axes($id) $m_axes
    set mged_v_axes($id) $v_axes
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
    set mged_faceplate($id) $faceplate

    if {$mged_fb($id)} {
	.$id.menubar.settings.fb entryconfigure 7 -state normal
	set mged_listen($id) $listen
    } else {
	.$id.menubar.settings.fb entryconfigure 7 -state disabled
	set mged_listen($id) $listen
    }

    set_mged_v_axes_pos $id
}

proc set_mged_v_axes_pos { id } {
    global v_axes_pos
    global mged_v_axes_pos

    set hpos [lindex $v_axes_pos 0]
    set vpos [lindex $v_axes_pos 1]

    if {$hpos == 0 && $vpos == 0} {
	set mged_v_axes_pos($id) 0
    } elseif {$hpos < 0 && $vpos < 0} {
	set mged_v_axes_pos($id) 1
    } elseif {$hpos < 0 && $vpos > 0} {
	set mged_v_axes_pos($id) 2
    } elseif {$hpos > 0 && $vpos > 0} {
	set mged_v_axes_pos($id) 3
    } else {
	set mged_v_axes_pos($id) 4
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
    global mged_multi_view
    global win_size
    global mged_display
    global view_ring
    global bob_testing

    if { 1 } {
	set vloc [string range $mged_dm_loc($id) 0 0]
	set hloc [string range $mged_dm_loc($id) 1 1]
	set tmp_dm $mged_top($id).$mged_dm_loc($id)

# Nothing to do
	if { $tmp_dm == $mged_active_dm($id) } {
	    return
	}

	trace vdelete mged_display($mged_active_dm($id),fps) w "ia_changestate $id"

	# unhighlight
	$mged_small_dmc($id) configure -bg #d9d9d9

	set mged_active_dm($id) $tmp_dm
	set mged_small_dmc($id) $mged_dmc($id).$vloc.$hloc
    } else {
	# unhighlight
	$mged_small_dmc($id) configure -bg #d9d9d9
	
	trace vdelete mged_display($mged_active_dm($id),fps) w "ia_changestate $id"

	set vloc [string range $mged_dm_loc($id) 0 0]
	set hloc [string range $mged_dm_loc($id) 1 1]
	set mged_active_dm($id) $mged_top($id).$mged_dm_loc($id)
	set mged_small_dmc($id) $mged_dmc($id).$vloc.$hloc
    }

    # highlight
    $mged_small_dmc($id) configure -bg yellow

    trace variable mged_display($mged_active_dm($id),fps) w "ia_changestate $id"

    update_mged_vars $id
    set view_ring($id) [get_view]

    aim $id $mged_active_dm($id)

    if {!$mged_multi_view($id)} {
# unpack previously active dm
	if { $bob_testing } {
	    grid forget $mged_top($id).$save_dm_loc($id)
	} else {
	    pack forget $mged_top($id).$save_dm_loc($id)
	}

# resize and repack previously active dm into smaller frame
	winset $mged_top($id).$save_dm_loc($id)
	set mv_size [expr $win_size($id) / 2 - 4]
	dm size $mv_size $mv_size

	if { $bob_testing } {
	    grid $mged_top($id).$save_dm_loc($id) -in $save_small_dmc($id) -sticky "nsew"
	} else {
	    pack $mged_top($id).$save_dm_loc($id) -in $save_small_dmc($id)
	}

	setmv $id
    }
    set save_dm_loc($id) $mged_dm_loc($id)
    set save_small_dmc($id) $mged_small_dmc($id)

    do_view_ring_entries $id s
    do_view_ring_entries $id d

    update_Raytrace $id

    set dbname [_mged_opendb]
    set_wm_title $id $dbname
}

proc set_wm_title { id dbname } {
    global mged_top
    global mged_dmc
    global mged_dm_loc

    if {$mged_top($id) == $mged_dmc($id)} {
	if {$mged_dm_loc($id) == "ul"} {
	    wm title $mged_top($id) "MGED 5.0 Graphics Window ($id) - $dbname - Upper Left"
	    wm title .$id "MGED 5.0 Command Window ($id) - $dbname - Upper Left"
	} elseif {$mged_dm_loc($id) == "ur"} {
	    wm title $mged_top($id) "MGED 5.0 Graphics Window ($id) - $dbname - Upper Right"
	    wm title .$id "MGED 5.0 Command Window ($id) - $dbname - Upper Right"
	} elseif {$mged_dm_loc($id) == "ll"} {
	    wm title $mged_top($id) "MGED 5.0 Graphics Window ($id) - $dbname - Lower Left"
	    wm title .$id "MGED 5.0 Command Window ($id) - $dbname - Lower Left"
	} elseif {$mged_dm_loc($id) == "lr"} {
	    wm title $mged_top($id) "MGED 5.0 Graphics Window ($id) - $dbname - Lower Right"
	    wm title .$id "MGED 5.0 Command Window ($id) - $dbname - Lower Right"
	}
    } else {
	if {$mged_dm_loc($id) == "ul"} {
	    wm title $mged_top($id) "MGED 5.0 Command Window ($id) - $dbname - Upper Left"
	} elseif {$mged_dm_loc($id) == "ur"} {
	    wm title $mged_top($id) "MGED 5.0 Command Window ($id) - $dbname - Upper Right"
	} elseif {$mged_dm_loc($id) == "ll"} {
	    wm title $mged_top($id) "MGED 5.0 Command Window ($id) - $dbname - Lower Left"
	} elseif {$mged_dm_loc($id) == "lr"} {
	    wm title $mged_top($id) "MGED 5.0 Command Window ($id) - $dbname - Lower Right"
	}
    }
}

proc set_cmd_win { id } {
    global mged_show_cmd
    global mged_show_dm
    global win_size
    global bob_testing

    if { $mged_show_cmd($id) } {
	if { $bob_testing } {
	    if { $mged_show_dm($id) } {
		grid .$id.tf -sticky nsew -row 1 -column 0
	    } else {
		grid .$id.tf -sticky nsew -row 0 -column 0
	    }
#	    setmv $id
	} else {
	    set win_size($id) $win_size($id,small)
	    setmv $id
	    pack .$id.tf -side top -fill both -expand yes
	}
    } else {
	if {$bob_testing} {
	    grid forget .$id.tf
#	    setmv $id
	} else {
	    pack forget .$id.tf
	    set win_size($id) $win_size($id,big)
	    setmv $id
	}
    }
}

proc set_dm_win { id } {
    global mged_top
    global mged_show_dm
    global mged_show_cmd
    global mged_show_status
    global mged_dmc
    global mged_num_lines
    global bob_testing

    if { $mged_show_dm($id) } {
	if { $bob_testing } {
	    if { $mged_show_cmd($id) } {
		grid forget .$id.tf
		.$id.t configure -height $mged_num_lines($id)
		update
	    }

	    grid $mged_dmc($id) -sticky nsew -row 0 -column 0
	    setmv $id

	    if { $mged_show_cmd($id) } {
		grid .$id.tf -sticky nsew -row 1 -column 0
		update
		.$id.t see end
	    }
	} else {
	    if {[winfo ismapped .$id.tf]} {
		pack $mged_dmc($id) -side top -before .$id.tf -padx 2 -pady 2
		.$id.t configure -width 10 -height 10
	    } else {
		pack $mged_dmc($id) -side top -padx 2 -pady 2
	    }
	}
    } else {
	if { $bob_testing } {
	    grid forget $mged_dmc($id)

	    set mged_show_cmd($id) 1
	    set_cmd_win $id
	    set tmp_font [lindex [.$id.t configure -font] 4]
	    set fh [font metrics $tmp_font -linespace]
	    set h [winfo height $mged_top($id)]

	    if { $mged_show_status($id) } {
		set h [expr $h - [winfo height .$id.status]]
	    }

	    set nlines [expr $h / $fh]
	    .$id.t configure -height $nlines
	} else {
	    pack forget $mged_dmc($id)
	    .$id.t configure -width 80 -height 85
	}
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
	set w .$id.menubar.viewring.select
	$w delete 0 end
	for {set i 0} {$i < $llen} {incr i} {
	    $w add radiobutton -value [lindex $views $i] -variable view_ring($id)\
		    -label [lindex $views $i] -command "do_goto_view $id [lindex $views $i]"
	}
    } elseif {$m == "d"} {
	set w .$id.menubar.viewring.delete
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
    global mged_show_status

    if {$mged_show_status($id)} {
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
    default_mouse_bindings $mged_top($id).ul

    winset $mged_top($id).ur
    set transform $mged_transform($id)
    default_mouse_bindings $mged_top($id).ur

    winset $mged_top($id).ll
    set transform $mged_transform($id)
    default_mouse_bindings $mged_top($id).ll

    winset $mged_top($id).lr
    set transform $mged_transform($id)
    default_mouse_bindings $mged_top($id).lr

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
	    default_mouse_bindings [winset]
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
	winset $mged_active_dm($id)
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
    global mged_fb
    global listen
    global mged_listen

    mged_apply $id "set fb \$mged_fb($id)"

    if {$mged_fb($id)} {
	.$id.menubar.settings.fb entryconfigure 7 -state normal
	.$id.menubar.modes entryconfigure 10 -state normal
	set mged_listen($id) 1
	mged_apply $id "set listen \$mged_listen($id)"
    } else {
	.$id.menubar.settings.fb entryconfigure 7 -state disabled
	.$id.menubar.modes entryconfigure 10 -state disabled
	set mged_listen($id) 0
    }
}

proc new_db_callback { dbname } {
    global mged_players

    foreach id $mged_players {
	set_wm_title $id $dbname
    }
}
