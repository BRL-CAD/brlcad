# Author - Bob Parker

check_externs "_mged_attach _mged_aim _mged_add_view _mged_delete_view _mged_get_view _mged_goto_view _mged_next_view _mged_prev_view _mged_toggle_view"

if [info exists env(MGED_HTML_DIR)] {
        set mged_html_dir $env(MGED_HTML_DIR)
} else {
        set mged_html_dir [lindex $auto_path 0]/../html/mged
}

if ![info exists mged_players] {
    set mged_players ""
}

if ![info exists mged_collaborators] {
    set mged_collaborators ""
}

if ![info exists mged_default_gt] {
    set mged_default_gt [dm_best_name]
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

if ![info exists mged_default_comb] {
    set mged_default_comb 0
}

if ![info exists mged_default_vap] {
    winset nu
    switch $v_axes_pos {
	1 {
	    set mged_default_vap "Lower Left"
	}
	2 {
	    set mged_default_vap "Upper Left"
	}
	3 {
	    set mged_default_vap "Upper Right"
	}
	4 {
	    set mged_default_vap "Lower Right"
	}
	default {
	    set mged_default_vap Center
	}
    }
}

set do_tearoffs 0

proc openw args {
    global ia_cmd_prefix
    global ia_more_default
    global ia_font
    global ia_illum_label
    global mged_html_dir
    global mged_players
    global mged_collaborators
    global mged_display
    global player_screen
    global openw_usage
    global env
    global faceplate
    global transform
    global transform_what
    global rotate_about
    global rotate_about_what
    global coords
    global coord_type
    global rateknobs
    global rateknobs_on
    global adcflag
    global adcflag_on
    global edit_info
    global edit_info_on
    global multi_view
    global sliders_on
    global buttons_on
    global win_size
    global mged_top
    global mged_dmc
    global mged_active_dm
    global mged_small_dmc
    global mged_dm_loc
    global save_small_dmc
    global save_dm_loc
    global cmd_win
    global dm_win
    global status_bar
    global mged_default_id
    global mged_default_gt
    global mged_default_pane
    global mged_default_mvmode
    global mged_default_config
    global mged_default_display
    global mged_default_gdisplay
    global mged_default_comb
    global mged_default_vap
    global view_ring
    global view_axes_pos
    global view_axes
    global v_axes
    global model_axes
    global m_axes
    global edit_axes
    global e_axes
    global do_tearoffs

# set defaults
set save_id [lindex [cmd_get] 2]
set comb $mged_default_comb
set join_c 0
set dtype $mged_default_gt
set id ""
set scw 0
set sgw 0
set i 0

set screen $mged_default_display
set dscreen $mged_default_gdisplay

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

for {set i 0} {$i < $argc} {incr i} {
    set arg [lindex $args $i]
    if {$arg == "-config"} {
	incr i

	if {$i >= $argc} {
	    return [help openw]
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
	    return [help openw]
	}
    } elseif {$arg == "-d" || $arg == "-display"} {
# display string for everything but the graphics window
	incr i

	if {$i >= $argc} {
	    return [help openw]
	}

	set screen [lindex $args $i]
    } elseif {$arg == "-gd" || $arg == "-gdisplay"} {
# display string for graphics window
	incr i

	if {$i >= $argc} {
	    return [help openw]
	}

	set dscreen [lindex $args $i]
    } elseif {$arg == "-gt"} {
	incr i

	if {$i >= $argc} {
	    return [help openw]
	}

	set dtype [lindex $args $i]
    } elseif {$arg == "-id"} {
	incr i

	if {$i >= $argc} {
	    return [help openw]
	}

	set id [lindex $args $i]
    } elseif {$arg == "-j" || $arg == "-join"} { 
	set join_c 1
    } elseif {$arg == "-h" || $arg == "-help"} {
	return [help openw]
    } elseif {$arg == "-s" || $arg == "-sep"} {
	set comb 0
    } elseif {$arg == "-c" || $arg == "-comb"} {
	set comb 1
    } else {
	return [help openw]
    }
}

if {$id == "mged"} {
    return "openw: not allowed to use \"mged\" as id"
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

    if [catch { openmv $mged_top($id) $mged_dmc($id) $dtype $mv_size } result] {
	closew $id
	return $result
    }
} else {
    set mged_top($id) .top$id
    set mged_dmc($id) $mged_top($id)

    toplevel $mged_dmc($id) -screen $dscreen -relief sunken -borderwidth 2
    set win_size($id,big) [expr [winfo screenheight $mged_dmc($id)] - 24]
    set win_size($id,small) [expr $win_size($id,big) - 100]
    set win_size($id) $win_size($id,big)
    set mv_size [expr $win_size($id) / 2 - 4]
    if [catch { openmv $mged_top($id) $mged_dmc($id) $dtype $mv_size } result] {
	closew $id
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
.$id.m.file.m add command -label "Close" -underline 0 -command "closew $id"
.$id.m.file.m add command -label "Exit" -command quit -underline 0

menu .$id.m.file.m.cm_saveview -tearoff $do_tearoffs
.$id.m.file.m.cm_saveview add command -label "RT script" -command "init_rtScriptTool $id"
.$id.m.file.m.cm_saveview add command -label "Plot" -command "init_plotTool $id"
.$id.m.file.m.cm_saveview add command -label "PostScript" -command "init_psTool $id"

menubutton .$id.m.edit -text "Edit" -underline 0 -menu .$id.m.edit.m
menu .$id.m.edit.m -tearoff $do_tearoffs
.$id.m.edit.m add cascade -label "Add" -menu .$id.m.edit.m.cm_add
.$id.m.edit.m add command -label "Solid" -underline 0 -command "esolmenu"
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
	-command "cmd_set $id; press top"
.$id.m.view.m add command -label "Bottom" -underline 5\
	-command "cmd_set $id; press bottom"
.$id.m.view.m add command -label "Right" -underline 0\
	-command "cmd_set $id; press right"
.$id.m.view.m add command -label "Left" -underline 0\
	-command "cmd_set $id; press left"
.$id.m.view.m add command -label "Front" -underline 0\
	-command "cmd_set $id; press front"
.$id.m.view.m add command -label "Back" -underline 0\
	-command "cmd_set $id; press rear"
.$id.m.view.m add command -label "az35,el25" -underline 2\
	-command "cmd_set $id; press 35,25"
.$id.m.view.m add command -label "az45,el45" -underline 2\
	-command "cmd_set $id; press 45,45"
.$id.m.view.m add separator
.$id.m.view.m add command -label "Zoom In" -underline 5\
	-command "cmd_set $id; zoom 2"
.$id.m.view.m add command -label "Zoom Out" -underline 5\
	-command "cmd_set $id; zoom 0.5"
.$id.m.view.m add separator
.$id.m.view.m add command -label "Save" -underline 0\
	-command "cmd_set $id; press save"
.$id.m.view.m add command -label "Restore" -underline 1\
	-command "cmd_set $id; press restore"
.$id.m.view.m add separator
.$id.m.view.m add command -label "Reset Viewsize"\
	-underline 6 -command "cmd_set $id; press reset"
.$id.m.view.m add command -label "Zero" -underline 0\
	-command "cmd_set $id; knob zero"

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

menubutton .$id.m.options -text "Options" -underline 0 -menu .$id.m.options.m
menu .$id.m.options.m -tearoff $do_tearoffs
.$id.m.options.m add checkbutton -offvalue 0 -onvalue 1 -variable status_bar($id)\
	-label "Status Bar" -underline 0 -command "toggle_status_bar $id"
if {$comb} {
.$id.m.options.m add checkbutton -offvalue 0 -onvalue 1 -variable cmd_win($id)\
	-label "Command Window" -underline 0 -command "set_cmd_win $id"
.$id.m.options.m add checkbutton -offvalue 0 -onvalue 1 -variable dm_win($id)\
	-label "Graphics Window" -underline 0 -command "set_dm_win $id"
} 
.$id.m.options.m add checkbutton -offvalue 0 -onvalue 1 -variable rateknobs_on($id)\
	-label "Rateknobs" -underline 0 -command "set_rateknobs $id"
.$id.m.options.m add separator
.$id.m.options.m add cascade -label "Multi-Pane" -menu .$id.m.options.m.cm_mpane
.$id.m.options.m add cascade -label "Units" -menu .$id.m.options.m.cm_units
.$id.m.options.m add cascade -label "Constraint Coords" -menu .$id.m.options.m.cm_coord
#.$id.m.options.m add cascade -label "Rate/Abs" -menu .$id.m.options.m.cm_rate_type
.$id.m.options.m add cascade -label "Rotate About..." -menu .$id.m.options.m.cm_origin
.$id.m.options.m add cascade -label "Transform..." -menu .$id.m.options.m.cm_transform

menu .$id.m.options.m.cm_mpane -tearoff $do_tearoffs
.$id.m.options.m.cm_mpane add checkbutton -offvalue 0 -onvalue 1\
	-variable multi_view($id) -label "Multi Pane" -underline 0\
	-command "setmv $id"
.$id.m.options.m.cm_mpane add separator
.$id.m.options.m.cm_mpane add radiobutton -value ul -variable mged_dm_loc($id)\
	-label "Upper Left" -command "set_active_dm $id"
.$id.m.options.m.cm_mpane add radiobutton -value ur -variable mged_dm_loc($id)\
	-label "Upper Right" -command "set_active_dm $id"
.$id.m.options.m.cm_mpane add radiobutton -value ll -variable mged_dm_loc($id)\
	-label "Lower Left" -command "set_active_dm $id"
.$id.m.options.m.cm_mpane add radiobutton -value lr -variable mged_dm_loc($id)\
	-label "Lower right" -command "set_active_dm $id"
.$id.m.options.m.cm_mpane add radiobutton -value lv -variable mged_dm_loc($id)\
	-label "Last Visited" -command "do_last_visited $id"

menu .$id.m.options.m.cm_units -tearoff $do_tearoffs
.$id.m.options.m.cm_units add radiobutton -value um -variable mged_display(units)\
	-label "micrometers" -command "do_Units $id"
.$id.m.options.m.cm_units add radiobutton -value mm -variable mged_display(units)\
	-label "millimeters" -command "do_Units $id"
.$id.m.options.m.cm_units add radiobutton -value cm -variable mged_display(units)\
	-label "centimeters" -command "do_Units $id"
.$id.m.options.m.cm_units add radiobutton -value m -variable mged_display(units)\
	-label "meters" -command "do_Units $id"
.$id.m.options.m.cm_units add radiobutton -value km -variable mged_display(units)\
	-label "kilometers" -command "do_Units $id"
.$id.m.options.m.cm_units add separator
.$id.m.options.m.cm_units add radiobutton -value in -variable mged_display(units)\
	-label "inches" -command "do_Units $id"
.$id.m.options.m.cm_units add radiobutton -value ft -variable mged_display(units)\
	-label "feet" -command "do_Units $id"
.$id.m.options.m.cm_units add radiobutton -value yd -variable mged_display(units)\
	-label "yards" -command "do_Units $id"
.$id.m.options.m.cm_units add radiobutton -value mi -variable mged_display(units)\
	-label "miles" -command "do_Units $id"

menu .$id.m.options.m.cm_coord -tearoff $do_tearoffs
.$id.m.options.m.cm_coord add radiobutton -value m -variable coord_type($id)\
	-label "Model" -command "set_coords $id"
.$id.m.options.m.cm_coord add radiobutton -value v -variable coord_type($id)\
	-label "View" -command "set_coords $id"
.$id.m.options.m.cm_coord add radiobutton -value o -variable coord_type($id)\
	-label "Object" -command "set_coords $id" -state disabled

#menu .$id.m.options.m.cm_rate_type -tearoff $do_tearoffs
#.$id.m.options.m.cm_rate_type add checkbutton -offvalue 0 -onvalue 1\
#	-variable rateknobs -label "rateknobs"
#.$id.m.options.m.cm_rate_type add command -label "zero" -command "knob zero"
#.$id.m.options.m.cm_rate_type add separator
#.$id.m.options.m.cm_rate_type add radiobutton -value p -variable rate_type\
#	-label "Pos Rate" -command "set_rate_type $id"
#.$id.m.options.m.cm_rate_type add radiobutton -value v -variable rate_type\
#	-label "Vel Rate" -command "set_rate_type $id"
#.$id.m.options.m.cm_rate_type add command -label "Set Friction..." -underline 0 -command "set_friction $id"

menu .$id.m.options.m.cm_origin -tearoff $do_tearoffs
.$id.m.options.m.cm_origin add radiobutton -value v -variable rotate_about_what($id)\
	-label "View Center" -command "set_rotate_about $id"
.$id.m.options.m.cm_origin add radiobutton -value e -variable rotate_about_what($id)\
	-label "Eye" -command "set_rotate_about $id"
.$id.m.options.m.cm_origin add radiobutton -value m -variable rotate_about_what($id)\
	-label "Model Origin" -command "set_rotate_about $id"
.$id.m.options.m.cm_origin add radiobutton -value k -variable rotate_about_what($id)\
	-label "Key Point" -command "set_rotate_about $id" -state disabled

menu .$id.m.options.m.cm_transform -tearoff $do_tearoffs
.$id.m.options.m.cm_transform add radiobutton -value v -variable transform_what($id)\
	-label "View" -command "set_transform $id"
.$id.m.options.m.cm_transform add radiobutton -value a -variable transform_what($id)\
	-label "ADC" -command "set_transform $id"
.$id.m.options.m.cm_transform add radiobutton -value e -variable transform_what($id)\
	-label "Model Params" -command "set_transform $id" -state disabled

menubutton .$id.m.tools -text "Tools" -menu .$id.m.tools.m
menu .$id.m.tools.m -tearoff $do_tearoffs
.$id.m.tools.m add cascade -label "Axes" -menu .$id.m.tools.m.cm_axes
.$id.m.tools.m add cascade -label "View Axes Position" -menu .$id.m.tools.m.cm_vap
.$id.m.tools.m add separator
.$id.m.tools.m add checkbutton -offvalue 0 -onvalue 1 -variable buttons_on($id)\
	-label "Button Menu" -underline 0 -command "toggle_button_menu $id"
.$id.m.tools.m add checkbutton -offvalue 0 -onvalue 1 -variable adcflag_on($id)\
	 -label "Angle/Dist Cursor" -underline 0 -command "toggle_adc $id"
#.$id.m.tools.m add checkbutton -offvalue 0 -onvalue 1 -variable sliders_on($id)\
#	-label "Sliders" -underline 0 -command "toggle_sliders $id"
.$id.m.tools.m add checkbutton -offvalue 0 -onvalue 1 -variable edit_info_on($id)\
	-label "Edit Info" -underline 0 -command "toggle_edit_info $id"

menu .$id.m.tools.m.cm_axes -tearoff $do_tearoffs
.$id.m.tools.m.cm_axes add checkbutton -offvalue 0 -onvalue 1\
	-variable view_axes($id) -label "View" -command "set_view_axes $id"
.$id.m.tools.m.cm_axes add checkbutton -offvalue 0 -onvalue 1\
	-variable model_axes($id) -label "Model" -command "set_model_axes $id"
.$id.m.tools.m.cm_axes add checkbutton -offvalue 0 -onvalue 1\
	-variable edit_axes($id) -label "Edit" -command "set_edit_axes $id"

menu .$id.m.tools.m.cm_vap -tearoff $do_tearoffs
.$id.m.tools.m.cm_vap add radiobutton -variable view_axes_pos($id)\
	-label "Center" -command "set_v_axes_pos $id"
.$id.m.tools.m.cm_vap add radiobutton -variable view_axes_pos($id)\
	-label "Lower Left" -command "set_v_axes_pos $id"
.$id.m.tools.m.cm_vap add radiobutton -variable view_axes_pos($id)\
	-label "Upper Left" -command "set_v_axes_pos $id"
.$id.m.tools.m.cm_vap add radiobutton -variable view_axes_pos($id)\
	-label "Upper Right" -command "set_v_axes_pos $id"
.$id.m.tools.m.cm_vap add radiobutton -variable view_axes_pos($id)\
	-label "Lower Right" -command "set_v_axes_pos $id"

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

set dm_id $mged_top($id).ur

label .$id.status.cent -textvar mged_display($dm_id,center) -anchor w
label .$id.status.size -textvar mged_display($dm_id,size) -anchor w
label .$id.status.units -textvar mged_display(units) -anchor w -padx 4
label .$id.status.aet -textvar mged_display($dm_id,aet) -anchor w
label .$id.status.ang -textvar mged_display($dm_id,ang) -anchor w -padx 4
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

bind .$id.t <Return> {
    %W mark set insert {end - 1c};
    %W insert insert \n;
    ia_invoke %W;
    break;
}

bind .$id.t <Control-u> {
    %W delete promptEnd {promptEnd lineend}
    %W mark set insert promptEnd
}

bind .$id.t <Control-p> {
    %W delete promptEnd {promptEnd lineend}
    %W mark set insert promptEnd
    set id [get_player_id_t %W]
    cmd_set $id
    set result [catch hist_prev msg]
    if {$result==0} {
	%W insert insert [string range $msg 0 \
		[expr [string length $msg]-2]]
    }
    break
}

bind .$id.t <Control-n> {
    %W delete promptEnd {promptEnd lineend}
    %W mark set insert promptEnd
    set id [get_player_id_t %W]
    cmd_set $id
    set result [catch hist_next msg]
    if {$result==0} {
	%W insert insert [string range $msg 0 \
		[expr [string length $msg]-2]]
    }
    break
}

bind .$id.t <Control-c> {
    %W mark set insert {end - 1c};
    %W insert insert \n;
    ia_print_prompt %W "mged> "
    %W see insert
    set id [get_player_id_t %W]
    set ia_cmd_prefix($id) ""
    set ia_more_default($id) ""
}

bind .$id.t <Control-w> {
    set ti [%W search -backwards -regexp "\[ \t\]\[^ \t\]+\[ \t\]*" insert promptEnd]
    if [string length $ti] {
	%W delete "$ti + 1c" insert
    } else {
	%W delete promptEnd insert
    }
}

bind .$id.t <Control-a> {
    %W mark set insert promptEnd
    break
}

#bind .$id.t <Delete> {
#    catch {%W tag remove sel sel.first promptEnd}
#    if {[%W tag nextrange sel 1.0 end] == ""} {
#	if [%W compare insert < promptEnd] {
#	    break
#	}
#    }
#}
#
#bind .$id.t <BackSpace> {
#    catch {%W tag remove sel sel.first promptEnd}
#    if {[%W tag nextrange sel 1.0 end] == ""} {
#	if [%W compare insert <= promptEnd] {
#	    break
#	}
#    }
#}
#
#bind .$id.t <Control-d> {
#    if [%W compare insert < promptEnd] {
#	break
#    }
#}
#
#bind .$id.t <Control-k> {
#    if [%W compare insert < promptEnd] {
#	break
#    }
#}
#
#bind .$id.t <Control-t> {
#    if [%W compare insert < promptEnd] {
#	break
#    }
#}
#
#bind .$id.t <Meta-d> {
#    if [%W compare insert < promptEnd] {
#	break
#    }
#}
#
#bind .$id.t <Meta-BackSpace> {
#    if [%W compare insert <= promptEnd] {
#	break
#    }
#}
#
#bind .$id.t <Control-h> {
#    if [%W compare insert <= promptEnd] {
#	break
#    }
#}

set ia_cmd_prefix($id) ""
set ia_more_default($id) ""
ia_print_prompt .$id.t "mged> "

.$id.t tag configure bold -font -*-Courier-Bold-R-Normal-*-120-*-*-*-*-*-*
set ia_font -*-Courier-Medium-R-Normal-*-120-*-*-*-*-*-*
.$id.t configure -font $ia_font

#==============================================================================
# Pack windows
#==============================================================================

pack .$id.m.file .$id.m.edit .$id.m.view .$id.m.viewring .$id.m.options .$id.m.tools -side left
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
#trace variable adcflag w "set_adcflag $id"

winset $mged_active_dm($id)
set rateknobs_on($id) $rateknobs
set rotate_about_what($id) $rotate_about
set coord_type($id) $coords
set transform_what($id) $transform
if {$v_axes} {
    set view_axes($id) 1
} else {
    set view_axes($id) 0
}
set model_axes($id) $m_axes
set edit_axes($id) $e_axes
set view_axes_pos($id) $mged_default_vap

set_rateknobs $id
set_rotate_about $id
set_coords $id
set_transform $id
set_view_axes $id
set_v_axes_pos $id
set_model_axes $id
set_edit_axes $id

# reset current_cmd_list so that its cur_hist gets updated
cmd_set $save_id

# cause all 4 windows to share menu state
share_menu $mged_top($id).ul $mged_top($id).ur
share_menu $mged_top($id).ul $mged_top($id).ll
share_menu $mged_top($id).ul $mged_top($id).lr

do_rebind_keys $id

wm protocol $mged_top($id) WM_DELETE_WINDOW "closew $id"
wm title $mged_top($id) "MGED Interaction Window ($id) - Upper Right"
wm geometry $mged_top($id) -0+0
}

proc closew args {
    global mged_players
    global mged_collaborators
    global multi_view
    global sliders_on
    global buttons_on
    global edit_info
    global edit_info_on
    global mged_top
    global mged_dmc

    if { [llength $args] != 1 } {
	return [help closew]
    }

    set id [lindex $args 0]

    set i [lsearch -exact $mged_players $id]
    if { $i == -1 } {
	return "closecw: bad id - $id"
    }
    set mged_players [lreplace $mged_players $i $i]

    set i [lsearch -exact $mged_collaborators $id]
    if { $i != -1 } {
	cmd_set $id
	set cmd_list [cmd_get]
	set shared_id [lindex $cmd_list 1]
	qcs $id
	if {"$mged_top($id).ur" == "$shared_id"} {
	    reconfig_openw_all
	}
    }

    set multi_view($id) 0
    set sliders_on($id) 0
    set buttons_on($id) 0
    set edit_info_on($id) 0

    releasemv $id
    catch { cmd_close $id }
    catch { destroy .mmenu$id }
    catch { destroy .sliders$id }
    catch { destroy $mged_top($id) }
    catch { destroy .$id }
}

proc reconfig_openw { id } {
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

proc reconfig_openw_all {} {
    global mged_collaborators

    foreach id $mged_collaborators {
	reconfig_openw $id
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

proc toggle_sliders { id } {
    global sliders_on
    global scroll_enabled

    if [ winfo exists .sliders$id ] {
	cmd_set $id
	sliders off
	return
    }

    cmd_set $id
    sliders on
}

proc toggle_edit_info { id } {
    global player_screen
    global edit_info_on
    global edit_info_on

    if [ winfo exists .sei$id] {
	destroy .sei$id
	set edit_info_on($id) 0
	return
    }

    toplevel .sei$id -screen $player_screen($id)
    label .sei$id.l -bg black -fg yellow -textvar edit_info -font fixed
    pack .sei$id.l -expand 1 -fill both

    wm title .sei$id "$id\'s Edit Info"
    wm protocol .sei$id WM_DELETE_WINDOW "toggle_edit_info $id"
#    wm resizable .sei$id 0 0
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
	reconfig_openw $id
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
	reconfig_openw [lindex $args 0]
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
    global sliders_on
    global adcflag
    global adcflag_on
    global mged_display
    global view_ring
    global view_axes_pos
    global view_axes
    global v_axes
    global model_axes
    global m_axes
    global edit_axes
    global e_axes

    bind $mged_top($id).ul <Enter> "winset $mged_top($id).ul; focus $mged_top($id).ul"
    bind $mged_top($id).ur <Enter> "winset $mged_top($id).ur; focus $mged_top($id).ur"
    bind $mged_top($id).ll <Enter> "winset $mged_top($id).ll; focus $mged_top($id).ll"
    bind $mged_top($id).lr <Enter> "winset $mged_top($id).lr; focus $mged_top($id).lr"

    trace vdelete mged_display($mged_active_dm($id),fps) w "ia_changestate $id"

    set vloc [string range $mged_dm_loc($id) 0 0]
    set hloc [string range $mged_dm_loc($id) 1 1]
    set mged_active_dm($id) $mged_top($id).$mged_dm_loc($id)
    set mged_small_dmc($id) $mged_dmc($id).$vloc.$hloc

    trace variable mged_display($mged_active_dm($id),fps) w "ia_changestate $id"

    winset $mged_active_dm($id)
    set adcflag_on($id) $adcflag
    set view_ring($id) [get_view]
    if {$v_axes} {
	set view_axes($id) 1
    } else {
	set view_axes($id) 0
    }
    set view_axes_pos($id) [get_view_axes_pos $id]
    set model_axes($id) $m_axes
    set edit_axes($id) $e_axes

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
    bind $mged_top($id).ul <Enter> "winset $mged_top($id).ul; focus $mged_top($id).ul; sliders \[subst \$slidersflag\]"
    bind $mged_top($id).ur <Enter> "winset $mged_top($id).ur; focus $mged_top($id).ur; sliders \[subst \$slidersflag\]"
    bind $mged_top($id).ll <Enter> "winset $mged_top($id).ll; focus $mged_top($id).ll; sliders \[subst \$slidersflag\]"
    bind $mged_top($id).lr <Enter> "winset $mged_top($id).lr; focus $mged_top($id).lr; sliders \[subst \$slidersflag\]"
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
    global view_ring
    global mged_collaborators
    global mged_top

    if {$mged_active_dm($id) != "lv"} {
	winset $mged_active_dm($id)
    }

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
    global view_ring
    global mged_collaborators
    global mged_top

    if {$mged_active_dm($id) != "lv"} {
	winset $mged_active_dm($id)
    }

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
    global view_ring
    global mged_collaborators
    global mged_top

    if {$mged_active_dm($id) != "lv"} {
	winset $mged_active_dm($id)
    }

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
    global view_ring
    global mged_collaborators
    global mged_top

    if {$mged_active_dm($id) != "lv"} {
	winset $mged_active_dm($id)
    }

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
    global view_ring
    global mged_collaborators
    global mged_top

    if {$mged_active_dm($id) != "lv"} {
	winset $mged_active_dm($id)
    }

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
    global view_ring
    global mged_collaborators
    global mged_top

    if {$mged_active_dm($id) != "lv"} {
	winset $mged_active_dm($id)
    }

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

proc set_friction { id } {
}

proc set_coords { id } {
    global mged_top
    global mged_active_dm
    global coords
    global coord_type

    winset $mged_top($id).ul
    set coords $coord_type($id)

    winset $mged_top($id).ur
    set coords $coord_type($id)

    winset $mged_top($id).ll
    set coords $coord_type($id)

    winset $mged_top($id).lr
    set coords $coord_type($id)
}

proc set_rotate_about { id } {
    global mged_top
    global mged_active_dm
    global rotate_about
    global rotate_about_what

    winset $mged_top($id).ul
    set rotate_about $rotate_about_what($id)

    winset $mged_top($id).ur
    set rotate_about $rotate_about_what($id)

    winset $mged_top($id).ll
    set rotate_about $rotate_about_what($id)

    winset $mged_top($id).lr
    set rotate_about $rotate_about_what($id)
}

proc set_rate_type { id } {
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
    global transform_what

    winset $mged_top($id).ul
    set transform $transform_what($id)
    do_mouse_bindings $mged_top($id).ul

    winset $mged_top($id).ur
    set transform $transform_what($id)
    do_mouse_bindings $mged_top($id).ur

    winset $mged_top($id).ll
    set transform $transform_what($id)
    do_mouse_bindings $mged_top($id).ll

    winset $mged_top($id).lr
    set transform $transform_what($id)
    do_mouse_bindings $mged_top($id).lr
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
    global adcflag
    global adcflag_on

    set result [eval _mged_adc $args]
    set id [lindex [cmd_get] 2]

    if {[info exists mged_active_dm($id)]} {
	set adcflag_on($id) $adcflag
    }

    if {$transform == "a"} {
	do_mouse_bindings [winset]
    }

    return $result
}

proc toggle_adc { id } {
    cmd_set $id
    adc
}

proc set_adcflag { args } {
    global mged_active_dm
    global adcflag_on
    global adcflag

    set id [lindex $args 0]

    if {[winset] == $mged_active_dm($id)} {
	set adcflag_on($id) $adcflag
    }
}

proc set_rateknobs { id } {
    global mged_top
    global mged_active_dm
    global rateknobs
    global rateknobs_on

    winset $mged_top($id).ul
    set rateknobs $rateknobs_on($id)

    winset $mged_top($id).ur
    set rateknobs $rateknobs_on($id)

    winset $mged_top($id).ll
    set rateknobs $rateknobs_on($id)

    winset $mged_top($id).lr
    set rateknobs $rateknobs_on($id)
}

proc set_view_axes { id } {
    global mged_active_dm
    global view_axes
    global v_axes

    winset $mged_active_dm($id)
    set v_axes $view_axes($id)
}

proc set_model_axes { id } {
    global mged_active_dm
    global model_axes
    global m_axes

    winset $mged_active_dm($id)
    set m_axes $model_axes($id)
}

proc set_edit_axes { id } {
    global mged_active_dm
    global edit_axes
    global e_axes

    winset $mged_active_dm($id)
    set e_axes $edit_axes($id)
}

proc set_v_axes_pos { id } {
    global mged_active_dm
    global view_axes_pos
    global v_axes_pos
    global v_axes

    winset $mged_active_dm($id)
    switch $view_axes_pos($id) {
	"Lower Left" {
	    set v_axes_pos 1
	}
	"Upper Left" {
	    set v_axes_pos 2
	}
	"Upper Right" {
	    set v_axes_pos 3
	}
	"Lower Right" {
	    set v_axes_pos 4
	}
	default {
	    set v_axes_pos 0
	}
    }
}

proc get_view_axes_pos { id } {
    global mged_active_dm
    global view_axes_pos
    global v_axes_pos
    global v_axes

    winset $mged_active_dm($id)

    switch $v_axes_pos {
	1 {
	    return "Lower Left"
	}
	2 {
	    return "Upper Left"
	}
	3 {
	    return "Upper Right"
	}
	4 {
	    return "Lower Right"
	}
	default{
	    return Center
	}
    }
}