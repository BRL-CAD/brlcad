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

check_externs "_mged_attach _mged_tie _mged_view_ring"

if [info exists env(MGED_HTML_DIR)] {
        set mged_html_dir $env(MGED_HTML_DIR)
} else {
        set mged_html_dir [lindex $auto_path 0]/../html/mged
}

if ![info exists mged_color_scheme] {
    color_scheme_init
}

if ![info exists use_grid_gm] {
    set use_grid_gm 1
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

# Set the class bindings for use with help. This requires the
# widget to register its data using hoc_register_data. Also, for now,
# the Menu class binding is being handled in tclscripts/menu_override.tcl.
#
if ![info exists tk_version] {
    loadtk
}
bind Entry <ButtonPress-3><ButtonRelease-3> "hoc_callback %W %X %Y"
bind Label <ButtonPress-3><ButtonRelease-3> "hoc_callback %W %X %Y"
bind Text <ButtonPress-3><ButtonRelease-3> "hoc_callback %W %X %Y"
bind Button <ButtonPress-3><ButtonRelease-3> "hoc_callback %W %X %Y"
bind Checkbutton <ButtonPress-3><ButtonRelease-3> "hoc_callback %W %X %Y"
bind Menubutton <ButtonPress-3><ButtonRelease-3> "hoc_callback %W %X %Y"
bind Radiobutton <ButtonPress-3><ButtonRelease-3> "hoc_callback %W %X %Y"

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
    global mged_grid
    global mged_mouse_behavior
    global mged_qray_effects
    global mged_coords
    global mged_rotate_about
    global mged_transform
    global mged_rateknobs
    global mged_adc_draw
    global mged_axes
    global mged_apply_to
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
    global use_grid_gm
    global mged_color_scheme

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
set mged_comb($id) $comb
set mged_show_cmd($id) $scw
set mged_show_dm($id) $sgw
set mged_show_status($id) 1
set mged_apply_to($id) 0
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

menu .$id.menubar.file -title "File" -tearoff $do_tearoffs
.$id.menubar.file add command -label "New..." -underline 0 -command "do_New $id"
hoc_register_menu_data "File" "New..." "New Database"\
	{ { summary "Open a new database. Note - the database\nmust not already exist." }
          { see_also opendb} }
.$id.menubar.file add command -label "Open..." -underline 0 -command "do_Open $id"
hoc_register_menu_data "File" "Open..." "Open Database"\
	{ { summary "Open an existing database" }
          { see_also opendb } }
.$id.menubar.file add command -label "Insert..." -underline 0 -command "do_Concat $id"
hoc_register_menu_data "File" "Insert..." "Insert Database"\
	{ { summary "Insert another database into the current database." }
          { see_also dbconcat } }
.$id.menubar.file add command -label "Extract..." -underline 1 -command "init_extractTool $id"
hoc_register_menu_data "File" "Extract..." "Extract Objects"\
	{ { summary "Tool for extracting objects out of the current database." }
          { see_also keep } }
.$id.menubar.file add command -label "g2asc..." -underline 0 -command "init_g2asc $id"
hoc_register_menu_data "File" "g2asc..." "Convert to Ascii"\
	{ { summary "Convert the current database into an ascii file." } }
.$id.menubar.file add separator
.$id.menubar.file add command -label "Raytrace..." -underline 0 -command "init_Raytrace $id"
hoc_register_menu_data "File" "Raytrace..." "Raytrace View"\
	{ { summary "Tool for raytracing the current view." }
          { see_also rt } }
.$id.menubar.file add cascade -label "Save View As" -underline 0 -menu .$id.menubar.file.saveview
.$id.menubar.file add separator
.$id.menubar.file add cascade -label "Preferences" -underline 0 -menu .$id.menubar.file.pref
.$id.menubar.file add separator
.$id.menubar.file add command -label "Close" -underline 0 -command "gui_destroy_default $id"
hoc_register_menu_data "File" "Close" "Close Window"\
	{ { summary "Close this graphical user interface." }
          { see_also } }
.$id.menubar.file add command -label "Exit" -underline 0 -command _mged_quit
hoc_register_menu_data "File" "Exit" "Exit MGED"\
	{ { summary "Exit MGED." }
          { see_also "exit q quit" } }

menu .$id.menubar.file.saveview -title "Save View As" -tearoff $do_tearoffs
.$id.menubar.file.saveview add command -label "RT script..." -underline 0\
	-command "init_rtScriptTool $id"
hoc_register_menu_data "Save View As" "RT script..." "RT Script File"\
	{ { summary "Save the current view as a RT script file." }
          { see_also saveview } }
.$id.menubar.file.saveview add command -label "Plot..." -underline 1\
	-command "init_plotTool $id"
hoc_register_menu_data "Save View As" "Plot..." "Plot File"\
	{ { summary "Save the current view as a Plot file." }
          { see_also pl } }
.$id.menubar.file.saveview add command -label "PostScript..." -underline 0\
	-command "init_psTool $id"
hoc_register_menu_data "Save View As" "PostScript..." "PostScript File"\
	{ { summary "Save the current view as a PostScript file." }
          { see_also ps } }

menu .$id.menubar.file.pref -title "Preferences" -tearoff $do_tearoffs
.$id.menubar.file.pref add cascade -label "Units" -underline 0\
	-menu .$id.menubar.file.pref.units
.$id.menubar.file.pref add cascade -label "Command Line Edit" -underline 0\
	-menu .$id.menubar.file.pref.cle
.$id.menubar.file.pref add cascade -label "Special Characters" -underline 0\
	-menu .$id.menubar.file.pref.special_chars
.$id.menubar.file.pref add command -label "Color Schemes..." -underline 6\
	-command "color_scheme_build $id \"Color Schemes\" [list $mged_color_scheme(primary_map)]\
	\"Faceplate Colors\" [list $mged_color_scheme(secondary_map)]"
hoc_register_menu_data "Preferences" "Color Schemes..." "Color Schemes"\
	{ { summary "Tool for setting colors." }
          { see_also "rset" } }

menu .$id.menubar.file.pref.units -title "Units" -tearoff $do_tearoffs
.$id.menubar.file.pref.units add radiobutton -value um -variable mged_display(units)\
	-label "micrometers" -underline 4 -command "do_Units $id"
hoc_register_menu_data "Units" "micrometers" "Unit of measure - Micrometers"\
	{ { summary "Set the unit of measure to micrometers.\n1 micrometer = 1/1,000,000 meters" }
          { see_also "units" } }
.$id.menubar.file.pref.units add radiobutton -value mm -variable mged_display(units)\
	-label "millimeters" -underline 2 -command "do_Units $id"
hoc_register_menu_data "Units" "millimeters" "Unit of measure - Millimeters"\
	{ { summary "Set the unit of measure to millimeters.\n1 millimeter = 1/1000 meters" }
          { see_also "units" } }
.$id.menubar.file.pref.units add radiobutton -value cm -variable mged_display(units)\
	-label "centimeters" -underline 0 -command "do_Units $id"
hoc_register_menu_data "Units" "centimeters" "Unit of measure - Centimeters"\
	{ { summary "Set the unit of measure to centimeters.n\t1 centimeter = 1/100 meters" }
          { see_also "units" } }
.$id.menubar.file.pref.units add radiobutton -value m -variable mged_display(units)\
	-label "meters" -underline 0 -command "do_Units $id"
hoc_register_menu_data "Units" "meters" "Unit of measure - Meters"\
	{ { summary "Set the unit of measure to meters." }
          { see_also "units" } }
.$id.menubar.file.pref.units add radiobutton -value km -variable mged_display(units)\
	-label "kilometers" -underline 0 -command "do_Units $id"
hoc_register_menu_data "Units" "kilometers" "Unit of measure - Kilometers"\
	{ { summary "Set the unit of measure to kilometers.\n 1 kilometer = 1000 meters" }
          { see_also "units" } }
.$id.menubar.file.pref.units add separator
.$id.menubar.file.pref.units add radiobutton -value in -variable mged_display(units)\
	-label "inches" -underline 0 -command "do_Units $id"
hoc_register_menu_data "Units" "inches" "Unit of measure - Inches"\
	{ { summary "Set the unit of measure to inches.\n 1 inch = 25.4 mm" }
          { see_also "units" } }
.$id.menubar.file.pref.units add radiobutton -value ft -variable mged_display(units)\
	-label "feet" -underline 0 -command "do_Units $id"
hoc_register_menu_data "Units" "feet" "Unit of measure - Feet"\
	{ { summary "Set the unit of measure to feet.\n 1 foot = 12 inches" }
          { see_also "units" } }
.$id.menubar.file.pref.units add radiobutton -value yd -variable mged_display(units)\
	-label "yards" -underline 0 -command "do_Units $id"
hoc_register_menu_data "Units" "yards" "Unit of measure - Yards"\
	{ { summary "Set the unit of measure to yards.\n 1 yard = 36 inches" }
          { see_also "" } }
.$id.menubar.file.pref.units add radiobutton -value mi -variable mged_display(units)\
	-label "miles" -underline 3 -command "do_Units $id"
hoc_register_menu_data "Units" "miles" "Unit of measure - Miles"\
	{ { summary "Set the unit of measure to miles.\n 1 mile = 5280 feet" }
          { see_also "units" } }

menu .$id.menubar.file.pref.cle -title "Command Line Edit" -tearoff $do_tearoffs
.$id.menubar.file.pref.cle add radiobutton -value emacs -variable mged_edit_style($id)\
	-label "emacs" -underline 0 -command "set_text_key_bindings $id"
hoc_register_menu_data "Command Line Edit" "emacs" "Emacs"\
	{ { summary "Set the command line edit mode to emacs." } }
.$id.menubar.file.pref.cle add radiobutton -value vi -variable mged_edit_style($id)\
	-label "vi" -underline 0 -command "set_text_key_bindings $id"
hoc_register_menu_data "Command Line Edit" "vi" "Vi"\
	{ { summary "Set the command line edit mode to vi." } }

menu .$id.menubar.file.pref.special_chars -title "Special Characters" -tearoff $do_tearoffs
.$id.menubar.file.pref.special_chars add radiobutton -value 0 -variable glob_compat_mode\
	-label "Tcl Evaluation" -underline 0
hoc_register_menu_data "Special Characters" "Tcl Evaluation" "Tcl Evaluation"\
	{ { summary "Set the command interpretation mode to Tcl mode." } }
.$id.menubar.file.pref.special_chars add radiobutton -value 1 -variable glob_compat_mode\
	-label "Object Name Matching" -underline 0
hoc_register_menu_data "Special Characters" "Object Name Matching" "Object Name Matching"\
	{ { summary\
"Set the command interpretation mode to MGED object name matching.\n\
In this mode, globbing is performed against MGED database objects."\
        } }

menu .$id.menubar.edit -title "Edit" -tearoff $do_tearoffs
.$id.menubar.edit add command -label "Solid Selection..." -underline 0 \
	-command "winset \$mged_active_dm($id); build_edit_menu_all s"
hoc_register_menu_data "Edit" "Solid Selection..." "Solid Selection"\
	{ { summary "A tool for selecting a solid to edit." } }
.$id.menubar.edit add command -label "Matrix Selection..." -underline 0 \
	-command "winset \$mged_active_dm($id); build_edit_menu_all o"
hoc_register_menu_data "Edit" "Matrix Selection..." "Matrix Selection"\
	{ { summary "A tool for selecting a matrix to edit." } }
.$id.menubar.edit add separator
.$id.menubar.edit add command -label "Solid Editor..." -underline 6 \
	-command "init_edit_solid $id"
hoc_register_menu_data "Edit" "Solid Editor..." "Solid Editor"\
	{ { summary "A tool for editing/creating solids." } }
.$id.menubar.edit add command -label "Combination Editor..." -underline 0 \
	-command "init_comb $id"
hoc_register_menu_data "Edit" "Combination Editor..." "Combination Editor"\
	{ { summary "A tool for editing/creating combinations." } }


menu .$id.menubar.create -title "Create" -tearoff $do_tearoffs
.$id.menubar.create add cascade\
	-label "Make Solid" -underline 0 -menu .$id.menubar.create.solid
#.$id.menubar.create add command\
#	-label "Instance Creation Panel..." -underline 0 -command "icreate $id"
.$id.menubar.create add command -label "Solid Editor..." -underline 0 \
	-command "init_edit_solid $id"
hoc_register_menu_data "Create" "Solid Editor..." "Solid Editor"\
	{ { summary "A tool for editing/creating solids." } }
.$id.menubar.create add command -label "Combination Editor..." -underline 0 \
	-command "init_comb $id"
hoc_register_menu_data "Create" "Combination Editor..." "Combination Editor"\
	{ { summary "A tool for editing/creating combinations." } }

menu .$id.menubar.create.solid -title "Make Solid" -tearoff $do_tearoffs
set make_solid_types [_mged_make -t]
foreach solid_type $make_solid_types {
    .$id.menubar.create.solid add command -label "$solid_type..."\
	    -command "init_solid_create $id $solid_type"

    set ksl {}
    lappend ksl "summary \"Make a $solid_type using the values found in the tcl variable solid_data(attr,$solid_type).\"" "see_also \"make, in\""
    hoc_register_menu_data "Make Solid" "$solid_type..." "Make a $solid_type" $ksl
}

menu .$id.menubar.view -title "View" -tearoff $do_tearoffs
.$id.menubar.view add command -label "Top" -underline 0\
	-command "mged_apply $id \"press top\""
hoc_register_menu_data "View" "Top" "Top View"\
	{ { summary "View of the top (i.e. azimuth = 270, elevation = 90). " }
          { see_also "press, ae, view, viewset, viewget" } }
.$id.menubar.view add command -label "Bottom" -underline 0\
	-command "mged_apply $id \"press bottom\""
hoc_register_menu_data "View" "Bottom" "Bottom View"\
	{ { summary "View of the bottom (i.e. azimuth = 270 , elevation = -90)." }
          { see_also "press, ae, view, viewset, viewget" } }
.$id.menubar.view add command -label "Right" -underline 0\
	-command "mged_apply $id \"press right\""
hoc_register_menu_data "View" "Right" "Right View"\
	{ { summary "View of the right side (i.e. azimuth = 270, elevation = 0)." }
          { see_also "press, ae, view, viewset, viewget" } }
.$id.menubar.view add command -label "Left" -underline 0\
	-command "mged_apply $id \"press left\""
hoc_register_menu_data "View" "Left" "Left View"\
	{ { summary "View of the left side (i.e. azimuth = 90, elevation = 0)." }
          { see_also "press, ae, view, viewset, viewget" } }
.$id.menubar.view add command -label "Front" -underline 0\
	-command "mged_apply $id \"press front\""
hoc_register_menu_data "View" "Front" "Front View"\
	{ { summary "View of the front (i.e. azimuth = 0, elevation = 0)." }
          { see_also "press, ae, view, viewset, viewget" } }
.$id.menubar.view add command -label "Back" -underline 3\
	-command "mged_apply $id \"press rear\""
hoc_register_menu_data "View" "Back" "Back View"\
	{ { summary "View of the back (i.e. azimuth = 180, elevation = 0)." }
          { see_also "press, ae, view, viewset, viewget" } }
.$id.menubar.view add command -label "az35,el25" -underline 2\
	-command "mged_apply $id \"press 35,25\""
hoc_register_menu_data "View" "az35,el25" "View - 35,25"\
	{ { summary "View with an azimuth of 35 and an elevation of 25." }
          { see_also "press, ae, view, viewset, viewget" } }
.$id.menubar.view add command -label "az45,el45" -underline 2\
	-command "mged_apply $id \"press 45,45\""
hoc_register_menu_data "View" "az45,el45" "View - 45,45"\
	{ { summary "View with an azimuth of 45 and an elevation of 45." }
          { see_also "press, ae, view, viewset, viewget" } }
.$id.menubar.view add separator
.$id.menubar.view add command -label "Zoom In" -underline 5\
	-command "mged_apply $id \"zoom 2\""
hoc_register_menu_data "View" "Zoom In" "Zoom In"\
	{ { summary "Zoom in by a factor of 2." }
          { see_also "sca, zoom" } }
.$id.menubar.view add command -label "Zoom Out" -underline 5\
	-command "mged_apply $id \"zoom 0.5\""
hoc_register_menu_data "View" "Zoom Out" "Zoom Out"\
	{ { summary "Zoom out by a factor of 2." }
          { see_also "sca, zoom" } }
.$id.menubar.view add separator
.$id.menubar.view add command -label "Default"\
	-underline 0 -command "mged_apply $id \"press reset\""
hoc_register_menu_data "View" "Default" "Default View"\
	{ { summary "Same as top (i.e. azimuth = 270, elevation = 90)." }
          { see_also "press, ae, view, viewset, viewget" } }
.$id.menubar.view add command -label "Multipane Defaults"\
	-underline 0 -command "set_default_views $id"
hoc_register_menu_data "View" "Multipane Defaults" "Multipane Default Views"\
	{ { summary "Sets the view of all four panes to their defaults.\n\n\
\tupper left\taz = 90, el = 0\n\
\tupper right\taz = 35, el = 25\n\
\tlower left\taz = 0, el = 0\n\
\tlower right\taz = 90, el = 0" }
          { see_also "press, ae, view, viewset, viewget" } }
.$id.menubar.view add command -label "Zero" -underline 0\
	-command "mged_apply $id \"knob zero\""
hoc_register_menu_data "View" "Zero" "Zero Knobs"\
	{ { summary "Stop all rate transformations." }
          { see_also "knob, press" } }

menu .$id.menubar.viewring -title "ViewRing" -tearoff $do_tearoffs
.$id.menubar.viewring add command -label "Add View" -underline 0 -command "view_ring_add $id"
hoc_register_menu_data "ViewRing" "Add View" "Add View"\
	{ { summary "Add a view to the view ring." }
          { see_also "view_ring" } }
.$id.menubar.viewring add cascade -label "Select View" -underline 0 -menu .$id.menubar.viewring.select
.$id.menubar.viewring add cascade -label "Delete View" -underline 0 -menu .$id.menubar.viewring.delete
.$id.menubar.viewring add command -label "Next View" -underline 0 -command "view_ring_next $id"
hoc_register_menu_data "ViewRing" "Next View" "Next View"\
	{ { summary "Go to the next view on the view ring.\nControl-n can also be used." }
          { see_also "view_ring" } }
.$id.menubar.viewring add command -label "Prev View" -underline 0 -command "view_ring_prev $id"
hoc_register_menu_data "ViewRing" "Prev View" "Previous View"\
	{ { summary "Go to the previous view on the view ring.\nControl-p can also be used." }
          { see_also "view_ring" } }
.$id.menubar.viewring add command -label "Last View" -underline 0 -command "view_ring_toggle $id"
hoc_register_menu_data "ViewRing" "Last View" "Last View"\
	{ { summary "Go to the last view. This can be used to toggle\nbetween two views. Control-t can also be used." }
          { see_also "view_ring" } }

menu .$id.menubar.viewring.select -title "Select View" -tearoff $do_tearoffs\
	-postcommand "do_view_ring_labels $id"
do_view_ring_entries $id s
set view_ring($id) 1
menu .$id.menubar.viewring.delete -title "Delete View" -tearoff $do_tearoffs\
	-postcommand "do_view_ring_labels $id"
do_view_ring_entries $id d

menu .$id.menubar.settings -title "Settings" -tearoff $do_tearoffs
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
#.$id.menubar.settings add cascade -label "Pane Background Color" -underline 5\
#	-menu .$id.menubar.settings.bgColor
.$id.menubar.settings add cascade -label "View Axes Position" -underline 0\
	-menu .$id.menubar.settings.vap

menu .$id.menubar.settings.applyTo -title "Apply To" -tearoff $do_tearoffs
.$id.menubar.settings.applyTo add radiobutton -value 0 -variable mged_apply_to($id)\
	-label "Active Pane" -underline 0
hoc_register_menu_data "Apply To" "Active Pane" "Active Pane"\
	{ { summary "Set the \"Apply To\" mode such that the user's\n\
interaction with the GUI is applied to the active pane." } }
.$id.menubar.settings.applyTo add radiobutton -value 1 -variable mged_apply_to($id)\
	-label "Local Panes" -underline 0
hoc_register_menu_data "Apply To" "Local Panes" "Local Panes"\
	{ { summary "Set the \"Apply To\" mode such that the user's\n\
interaction with the GUI is applied to all panes\n\
local to this instance of the GUI." } }
.$id.menubar.settings.applyTo add radiobutton -value 2 -variable mged_apply_to($id)\
	-label "Listed Panes" -underline 1
hoc_register_menu_data "Apply To" "Listed Panes" "Listed Panes"\
	{ { summary "Set the \"Apply To\" mode such that the user's\n\
interaction with the GUI is applied to all panes\n\
listed in the Tcl variable mged_apply_list(id)\n\
(Note - id refers to the GUI's id)." } }
.$id.menubar.settings.applyTo add radiobutton -value 3 -variable mged_apply_to($id)\
	-label "All Panes" -underline 4
hoc_register_menu_data "Apply To" "All Panes" "All Panes"\
	{ { summary "Set the \"Apply To\" mode such that the user's\n\
interaction with the GUI is applied to all panes." } }

menu .$id.menubar.settings.mouse_behavior -title "Mouse Behavior" -tearoff $do_tearoffs
.$id.menubar.settings.mouse_behavior add radiobutton -value d -variable mged_mouse_behavior($id)\
	-label "Default" -underline 0\
	-command "mged_apply $id \"set mouse_behavior \$mged_mouse_behavior($id); refresh\""
hoc_register_menu_data "Mouse Behavior" "Default" "Default Mouse Behavior"\
	{ { summary "Enter the default MGED mouse behavior.\n\n\
Mouse Button\tBehavior\n\
     1\tZoom out by a factor of 2\n\
     2\tCenter View\n\
     3\tZoom in by a factor of 2" }
          { see_also "rset, vars" } }
.$id.menubar.settings.mouse_behavior add radiobutton -value p -variable mged_mouse_behavior($id)\
	-label "Paint Rectangle Area" -underline 0\
	-command "mged_apply $id \"set mouse_behavior \$mged_mouse_behavior($id); refresh\""
hoc_register_menu_data "Mouse Behavior" "Paint Rectangle Area" "Paint Rectangle Area"\
	{ { summary "Enter paint rectangle mode.\n\
If the framebuffer is active, the rectangular area as\n\
specified by the user is painted with the contents of the\n\
framebuffer. Otherwise, only the rectangle is drawn.\n\n\
Mouse Button\tBehavior\n\
     1\tZoom out by a factor of 2\n\
     2\tDraw paint rectangle\n\
     3\tZoom in by a factor of 2" }
          { see_also "rset, vars" } }
.$id.menubar.settings.mouse_behavior add radiobutton -value r -variable mged_mouse_behavior($id)\
	-label "Raytrace Rectangle Area" -underline 0\
	-command "mged_apply $id \"set mouse_behavior \$mged_mouse_behavior($id); refresh\""
hoc_register_menu_data "Mouse Behavior" "Raytrace Rectangle Area" "Raytrace Rectangle Area"\
	{ { summary "Enter raytrace rectangle mode.\n\
If the framebuffer is active, the rectangular area as\n\
specified by the user is raytraced. The rectangular area\n\
is also painted with the contents of the framebuffer.
Otherwise, only the rectangle is drawn.\n\n\
Mouse Button\tBehavior\n\
     1\tZoom out by a factor of 2\n\
     2\tDraw raytrace rectangle\n\
     3\tZoom in by a factor of 2" }
          { see_also "rset, vars" } }
.$id.menubar.settings.mouse_behavior add radiobutton -value z -variable mged_mouse_behavior($id)\
	-label "Zoom Rectangle Area" -underline 0\
	-command "mged_apply $id \"set mouse_behavior \$mged_mouse_behavior($id); refresh\""
hoc_register_menu_data "Mouse Behavior" "Zoom Rectangle Area" "Zoom Rectangle Area"\
	{ { summary "Enter zoom rectangle mode.\n\
The rectangular area as specified by the user is used\n\
to zoom the view.\n\n\
Mouse Button\tBehavior\n\
     1\tZoom out by a factor of 2\n\
     2\tDraw zoom rectangle\n\
     3\tZoom in by a factor of 2" }
          { see_also "rset, vars" } }
.$id.menubar.settings.mouse_behavior add radiobutton -value q -variable mged_mouse_behavior($id)\
	-label "Query Ray" -underline 0\
	-command "mged_apply $id \"set mouse_behavior \$mged_mouse_behavior($id); refresh\""
hoc_register_menu_data "Mouse Behavior" "Query Ray" "Query Ray"\
	{ { summary "Enter query ray mode.\n\
In this mode, the mouse is used to fire rays. The data\n\
from the fired rays can be viewed textually, graphically\n\
or both.\n\n\
Mouse Button\tBehavior\n\
     1\tZoom out by a factor of 2\n\
     2\tFire query ray\n\
     3\tZoom in by a factor of 2" }
          { see_also "nirt, qray, rset, vars" } }
.$id.menubar.settings.mouse_behavior add radiobutton -value s -variable mged_mouse_behavior($id)\
	-label "Solid Edit Ray" -underline 0\
	-command "mged_apply $id \"set mouse_behavior \$mged_mouse_behavior($id); refresh\""
hoc_register_menu_data "Mouse Behavior" "Solid Edit Ray" "Solid Edit Ray"\
	{ { summary "Enter solid edit ray mode.\n\
In this mode, the mouse is used to fire rays for selecting\n\
a solid to edit. If more than one solid was hit, a menu of the\n\
hit solids is presented. The user then selects a solid\n\
to edit from this menu. If a single solid was hit, it\n\
is selected for editing. If no solids were hit, a dialog is\n\
popped up saying that nothing was hit. The user must then fire another
ray to continue selecting a solid. When a solid is finally\n\
selected, solid edit mode is entered. When this happens, the mouse\n\
behavior mode is set to default mode. Note - When selecting items\n\
from a menu, a left buttonpress highlights the solid in question\n\
until the button is released. To select a solid, double click with\n\
the left mouse button.\n\n\
Mouse Button\tBehavior\n\
     1\tZoom out by a factor of 2\n\
     2\tFire solid edit ray\n\
     3\tZoom in by a factor of 2" }
          { see_also "nirt, qray, rset, sed, vars" } }
.$id.menubar.settings.mouse_behavior add radiobutton -value m -variable mged_mouse_behavior($id)\
	-label "Matrix Edit Ray" -underline 0\
	-command "mged_apply $id \"set mouse_behavior \$mged_mouse_behavior($id); refresh\""
hoc_register_menu_data "Mouse Behavior" "Matrix Edit Ray" "Matrix Edit Ray"\
	{ { summary "Enter matrix edit ray mode.\n\
In this mode, the mouse is used to fire rays for selecting\n\
a matrix to edit. If more than one solid was hit, a menu of the\n\
hit solids is presented. The user then selects a solid\n\
from this menu. If a single solid was hit, that solid is selected.\n\
If no solids were hit, a dialog is popped up saying that nothing was hit.\n\
The user must then fire another ray to continue selecting a matrix to edit.\n\
When a solid is finally selected, the user is presented with a menu consisting\n\
of the path components of the selected solid. From this menu, the\n\
user selects a path component. This component determines which\n\
matrix will be edited. After selecting the path component, object/matrix\n\
edit mode is entered. When this happens, the mouse behavior mode\n\
is set to default mode. Note - When selecting items\n\
from a menu, a left buttonpress highlights the solid/matrix in question\n\
until the button is released. To select a solid/matrix, double click with\n\
the left mouse button.\n\n\
Mouse Button\tBehavior\n\
     1\tZoom out by a factor of 2\n\
     2\tFire matrix edit ray\n\
     3\tZoom in by a factor of 2" }
          { see_also "rset, vars" } }
.$id.menubar.settings.mouse_behavior add radiobutton -value c -variable mged_mouse_behavior($id)\
	-label "Combination Edit Ray" -underline 0\
	-command "mged_apply $id \"set mouse_behavior \$mged_mouse_behavior($id); refresh\""
hoc_register_menu_data "Mouse Behavior" "Combination Edit Ray" "Combination Edit Ray"\
	{ { summary "Enter combination edit ray mode.\n\
In this mode, the mouse is used to fire rays for selecting\n\
a combination to edit. If more than one combination was hit, a menu of the\n\
hit combinations is presented. The user then selects a combination\n\
from this menu. If a single combination was hit, that combination is selected.\n\
If no combinations were hit, a dialog is popped up saying that nothing was hit.\n\
The user must then fire another ray to continue selecting a combination to edit.\n\
When a combination is finally selected, the combination edit tool is presented\n\
and initialized with the values of the selected combination. When this happens,\n\
the mouse behavior mode is set to default mode. Note - When selecting items\n\
from a menu, a left buttonpress highlights the combination in question\n\
until the button is released. To select a combination, double click with\n\
the left mouse button.\n\n\
Mouse Button\t\t\tBehavior\n\
\t1\t\tZoom out by a factor of 2\n\
\t2\t\tFire combination edit ray\n\
\t3\t\tZoom in by a factor of 2" }
          { see_also "rset, vars" } }

menu .$id.menubar.settings.qray -title "Query Ray Effects" -tearoff $do_tearoffs
.$id.menubar.settings.qray add radiobutton -value t -variable mged_qray_effects($id)\
	-label "Text" -underline 0\
	-command "mged_apply $id \"qray effects \$mged_qray_effects($id)\""
hoc_register_menu_data "Query Ray Effects" "Text" "Query Ray Effects - Text"\
	{ { summary "Set qray effects mode to 'text'. In this mode,\n\
only textual output is used to represent the results\n\
of firing a query ray." }
          { see_also "qray" } }
.$id.menubar.settings.qray add radiobutton -value g -variable mged_qray_effects($id)\
	-label "Graphics" -underline 0\
	-command "mged_apply $id \"qray effects \$mged_qray_effects($id)\""
hoc_register_menu_data "Query Ray Effects" "Graphics" "Query Ray Effects - Graphics"\
	{ { summary "Set qray effects mode to 'graphics'. In this mode,\n\
only graphical output is used to represent the results\n\
of firing a query ray." }
          { see_also "qray" } }
.$id.menubar.settings.qray add radiobutton -value b -variable mged_qray_effects($id)\
	-label "both" -underline 0\
	-command "mged_apply $id \"qray effects \$mged_qray_effects($id)\""
hoc_register_menu_data "Query Ray Effects" "both" "Query Ray Effects - Both"\
	{ { summary "Set qray effects mode to 'both'. In this mode,\n\
both textual and graphical output is used to represent the results\n\
of firing a query ray." }
          { see_also "qray" } }

menu .$id.menubar.settings.mpane -title "Active Pane" -tearoff $do_tearoffs
.$id.menubar.settings.mpane add radiobutton -value ul -variable mged_dm_loc($id)\
	-label "Upper Left" -underline 6\
	-command "set_active_dm $id"
hoc_register_menu_data "Active Pane" "Upper Left" "Active Pane - Upper Left"\
	{ { summary "Set the active pane to be the upper left pane.\n\
Any interaction with the GUI that affects a pane/\"display manager\"\n\
will be directed at the upper left pane." } }
.$id.menubar.settings.mpane add radiobutton -value ur -variable mged_dm_loc($id)\
	-label "Upper Right" -underline 6\
	-command "set_active_dm $id"
hoc_register_menu_data "Active Pane" "Upper Right" "Active Pane - Upper Right"\
	{ { summary "Set the active pane to be the upper right pane.\n\
Any interaction with the GUI that affects a pane/\"display manager\"\n\
will be directed at the upper right pane." } }
.$id.menubar.settings.mpane add radiobutton -value ll -variable mged_dm_loc($id)\
	-label "Lower Left" -underline 2\
	-command "set_active_dm $id"
hoc_register_menu_data "Active Pane" "Lower Left" "Active Pane - Lower Left"\
	{ { summary "Set the active pane to be the lower left pane.\n\
Any interaction with the GUI that affects a pane/\"display manager\"\n\
will be directed at the lower left pane." } }
.$id.menubar.settings.mpane add radiobutton -value lr -variable mged_dm_loc($id)\
	-label "Lower Right" -underline 3\
	-command "set_active_dm $id"
hoc_register_menu_data "Active Pane" "Lower Right" "Active Pane - Lower Right"\
	{ { summary "Set the active pane to be the lower right pane.\n\
Any interaction with the GUI that affects a pane/\"geometry window\"\n\
will be directed at the lower right pane." } }

menu .$id.menubar.settings.fb -title "Framebuffer" -tearoff $do_tearoffs
.$id.menubar.settings.fb add radiobutton -value 1 -variable mged_fb_all($id)\
	-label "All" -underline 0\
	-command "mged_apply $id \"set fb_all \$mged_fb_all($id)\""
hoc_register_menu_data "Framebuffer" "All" "Framebuffer - All"\
	{ { summary "Use the entire pane for the framebuffer." }
          { see_also "rset, vars" } }
.$id.menubar.settings.fb add radiobutton -value 0 -variable mged_fb_all($id)\
	-label "Rectangle Area" -underline 0\
	-command "mged_apply $id \"set fb_all \$mged_fb_all($id)\""
hoc_register_menu_data "Framebuffer" "Rectangle Area" "Framebuffer - Rectangle Area"\
	{ { summary "Use the rectangle area for the framebuffer." }
          { see_also "rset, vars" } }
.$id.menubar.settings.fb add separator
.$id.menubar.settings.fb add radiobutton -value 1 -variable mged_fb_overlay($id)\
	-label "Overlay" -underline 0\
	-command "mged_apply $id \"set fb_overlay \$mged_fb_overlay($id)\""
hoc_register_menu_data "Framebuffer" "Overlay" "Framebuffer - Overlay"\
	{ { summary "Put the framebuffer in overlay mode. In this mode,\n\
the framebuffer data is placed in the pane after\n\
the geometry is drawn (i.e. the framebuffer data is\n\
is drawn on top of the geometry)." }
          { see_also "rset, vars" } }
.$id.menubar.settings.fb add radiobutton -value 0 -variable mged_fb_overlay($id)\
	-label "Underlay" -underline 0\
	-command "mged_apply $id \"set fb_overlay \$mged_fb_overlay($id)\""
hoc_register_menu_data "Framebuffer" "Underlay" "Framebuffer - Underlay"\
	{ { summary "Put the framebuffer in underlay mode. In this mode,\n\
the framebuffer data is placed in the pane before\n\
the geometry is drawn (i.e. the geometry is drawn on\n\
top of the framebuffer data." }
          { see_also "rset, vars" } }
.$id.menubar.settings.fb add separator
.$id.menubar.settings.fb add checkbutton -offvalue 0 -onvalue 1 -variable mged_fb($id)\
	-label "Framebuffer Active" -underline 0\
	-command "set_fb $id; update_Raytrace $id"
hoc_register_menu_data "Framebuffer" "Framebuffer Active" "Framebuffer Active"\
	{ { summary "This activates/deactivates the framebuffer." }
          { see_also "rset, vars" } }
.$id.menubar.settings.fb add checkbutton -offvalue 0 -onvalue 1 -variable mged_listen($id)\
	-label "Listen For Clients" -underline 0\
	-command "set_listen $id" -state disabled
hoc_register_menu_data "Framebuffer" "Listen For Clients" "Listen For Clients"\
	{ { summary "This activates/deactivates listening for clients.\n\
If the framebuffer is listening for clients, new data can\n\
be passed into the framebuffer. Otherwise, the framebuffer\n\
is write protected. Actually, it is also read protected with
one exception. MGED can still read the framebuffer data." }
          { see_also "rset, vars" } }

menu .$id.menubar.settings.vap -title "View Axes Position" -tearoff $do_tearoffs
.$id.menubar.settings.vap add radiobutton -value 0 -variable mged_axes($id,view_pos)\
	-label "Center" -underline 0\
	-command "mged_apply $id \"rset ax view_pos 0 0\""
hoc_register_menu_data "View Axes Position" "Center" "View Axes Position - Center"\
	{ { summary "Place the view axes in the center of the active pane." }
          { see_also "rset" } }
.$id.menubar.settings.vap add radiobutton -value 1 -variable mged_axes($id,view_pos)\
	-label "Lower Left" -underline 2\
	-command "mged_apply $id \"rset ax view_pos -1750 -1750\""
hoc_register_menu_data "View Axes Position" "Lower Left" "View Axes Position - Lower Left"\
	{ { summary "Place the view axes in the lower left corner of the active pane." }
          { see_also "rset" } }
.$id.menubar.settings.vap add radiobutton -value 2 -variable mged_axes($id,view_pos)\
	-label "Upper Left" -underline 6\
	-command "mged_apply $id \"rset ax view_pos -1750 1750\""
hoc_register_menu_data "View Axes Position" "Upper Left" "View Axes Position - Upper Left"\
	{ { summary "Place the view axes in the upper left corner of the active pane." }
          { see_also "rset" } }
.$id.menubar.settings.vap add radiobutton -value 3 -variable mged_axes($id,view_pos)\
	-label "Upper Right" -underline 6\
	-command "mged_apply $id \"rset ax view_pos 1750 1750\""
hoc_register_menu_data "View Axes Position" "Upper Right" "View Axes Position - Upper Right"\
	{ { summary "Place the view axes in the upper right corner of the active pane." }
          { see_also "rset" } }
.$id.menubar.settings.vap add radiobutton -value 4 -variable mged_axes($id,view_pos)\
	-label "Lower Right" -underline 3\
	-command "mged_apply $id \"rset ax view_pos 1750 -1750\""
hoc_register_menu_data "View Axes Position" "Lower Right" "View Axes Position - Lower Right"\
	{ { summary "Place the view axes in the lower right corner of the active pane." }
          { see_also "rset" } }

menu .$id.menubar.settings.grid -title "Grid" -tearoff $do_tearoffs
.$id.menubar.settings.grid add command -label "Anchor..." -underline 0\
	-command "do_grid_anchor $id"
hoc_register_menu_data "Grid" "Anchor..." "Grid Anchor"\
	{ { summary "Pops up the grid anchor entry dialog." }
          { see_also "rset" } }
.$id.menubar.settings.grid add cascade -label "Spacing" -underline 1\
	-menu .$id.menubar.settings.grid.spacing
.$id.menubar.settings.grid add separator
.$id.menubar.settings.grid add checkbutton -offvalue 0 -onvalue 1 -variable mged_grid($id,draw)\
	-label "Draw Grid" -underline 0\
	-command "mged_apply $id \"rset grid draw \$mged_grid($id,draw)\""
hoc_register_menu_data "Grid" "Draw Grid" "Draw Grid"\
	{ { summary "Toggle drawing the grid. The grid is\n\
a lattice of points over the pane (geometry window).\n\
The regular spacing between the points gives the user\n\
accurate visual cues regarding dimension. This spacing\n\
can be set by the user." }
          { see_also "rset" } }
.$id.menubar.settings.grid add checkbutton -offvalue 0 -onvalue 1 -variable mged_grid($id,snap)\
	-label "Snap To Grid" -underline 0\
	-command "mged_apply $id \"rset grid snap \$mged_grid($id,snap)\""
hoc_register_menu_data "Grid" "Snap To Grid" "Snap To Grid"\
	{ { summary "Toggle grid snapping. When snapping to grid,\n\
the internal routines that use the mouse pointer location,\n\
move/snap that location to the nearest grid point. This gives\n\
the user high accuracy with the mouse for transforming the\n\
view or editing solids/matrices." }
          { see_also "rset" } }

menu .$id.menubar.settings.grid.spacing -title "Grid Spacing" -tearoff $do_tearoffs
.$id.menubar.settings.grid.spacing add command -label "Autosize" -underline 0\
	-command "grid_spacing_autosize $id; grid_spacing_apply $id b"
hoc_register_menu_data "Grid Spacing" "Autosize" "Grid Spacing - Autosize"\
	{ { summary "Set the grid spacing according to the current view size.\n\
The number of ticks will be between 20 and 200 in user units.\n\
The major spacing will be set to 10 ticks per major." }
          { see_also "rset" } }
.$id.menubar.settings.grid.spacing add command -label "Arbitrary..." -underline 1\
	-command "do_grid_spacing $id b"
hoc_register_menu_data "Grid Spacing" "Arbitrary..." "Grid Spacing - Arbitrary"\
	{ { summary "Pops up the grid spacing entry dialog. The user can\n\
use this to set both the horizontal and vertical tick spacing." }
          { see_also "rset" } }
.$id.menubar.settings.grid.spacing add separator
.$id.menubar.settings.grid.spacing add command -label "micrometer" -underline 4\
	-command "set_grid_spacing $id micrometer 1"
hoc_register_menu_data "Grid Spacing" "micrometer" "Grid Spacing - micrometer"\
	{ { summary "Set the horizontal and vertical tick spacing\n\
to 1 micrometer." }
          { see_also "rset" } }
.$id.menubar.settings.grid.spacing add command -label "millimeter" -underline 2\
	-command "set_grid_spacing $id millimeter 1"
hoc_register_menu_data "Grid Spacing" "millimeter" "Grid Spacing - millimeter"\
	{ { summary "Set the horizontal and vertical tick spacing\n\
to 1 millimeter." }
          { see_also "rset" } }
.$id.menubar.settings.grid.spacing add command -label "centimeter" -underline 0\
	-command "set_grid_spacing $id centimeter 1"
hoc_register_menu_data "Grid Spacing" "centimeter" "Grid Spacing - centimeter"\
	{ { summary "Set the horizontal and vertical tick spacing\n\
to 1 centimeter." }
          { see_also "rset" } }
.$id.menubar.settings.grid.spacing add command -label "decimeter" -underline 0\
	-command "set_grid_spacing $id decimeter 1"
hoc_register_menu_data "Grid Spacing" "decimeter" "Grid Spacing - decimeter"\
	{ { summary "Set the horizontal and vertical tick spacing\n\
to 1 decimeter." }
          { see_also "rset" } }
.$id.menubar.settings.grid.spacing add command -label "meter" -underline 0\
	-command "set_grid_spacing $id meter 1"
hoc_register_menu_data "Grid Spacing" "meter" "Grid Spacing - meter"\
	{ { summary "Set the horizontal and vertical tick spacing\n\
to 1 meter." }
          { see_also "rset" } }
.$id.menubar.settings.grid.spacing add command -label "kilometer" -underline 0\
	-command "set_grid_spacing $id kilometer 1"
hoc_register_menu_data "Grid Spacing" "kilometer" "Grid Spacing - kilometer"\
	{ { summary "Set the horizontal and vertical tick spacing\n\
to 1 kilometer." }
          { see_also "rset" } }
.$id.menubar.settings.grid.spacing add separator
.$id.menubar.settings.grid.spacing add command -label "1/10 inch" -underline 0\
	-command "set_grid_spacing $id \"1/10 inch\" 1"
hoc_register_menu_data "Grid Spacing" "1/10 inch" "Grid Spacing - 1/10 inch"\
	{ { summary "Set the horizontal and vertical tick spacing\n\
to 1/10 inches." }
          { see_also "rset" } }
.$id.menubar.settings.grid.spacing add command -label "1/4 inch" -underline 2\
	-command "set_grid_spacing $id \"1/4 inch\" 1"
hoc_register_menu_data "Grid Spacing" "1/4 inch" "Grid Spacing - 1/4 inch"\
	{ { summary "Set the horizontal and vertical tick spacing\n\
to 1/4 inches." }
          { see_also "rset" } }
.$id.menubar.settings.grid.spacing add command -label "1/2 inch" -underline 2\
	-command "set_grid_spacing $id \"1/2 inch\" 1"
hoc_register_menu_data "Grid Spacing" "1/2 inch" "Grid Spacing - 1/2 inch"\
	{ { summary "Set the horizontal and vertical tick spacing\n\
to 1/2 inches." }
          { see_also "rset" } }
.$id.menubar.settings.grid.spacing add command -label "inch" -underline 0\
	-command "set_grid_spacing $id inch 1"
hoc_register_menu_data "Grid Spacing" "inch" "Grid Spacing - inch"\
	{ { summary "Set the horizontal and vertical tick spacing\n\
to 1 inch." }
          { see_also "rset" } }
.$id.menubar.settings.grid.spacing add command -label "foot" -underline 0\
	-command "set_grid_spacing $id foot 1"
hoc_register_menu_data "Grid Spacing" "foot" "Grid Spacing - foot"\
	{ { summary "Set the horizontal and vertical tick spacing\n\
to 1 foot." }
          { see_also "rset" } }
.$id.menubar.settings.grid.spacing add command -label "yard" -underline 0\
	-command "set_grid_spacing $id yard 1"
hoc_register_menu_data "Grid Spacing" "yard" "Grid Spacing - yard"\
	{ { summary "Set the horizontal and vertical tick spacing\n\
to 1 yard." }
          { see_also "rset" } }
.$id.menubar.settings.grid.spacing add command -label "mile" -underline 0\
	-command "set_grid_spacing $id mile 1"
hoc_register_menu_data "Grid Spacing" "mile" "mile"\
	{ { summary "Set the horizontal and vertical tick spacing\n\
to 1 mile." }
          { see_also "rset" } }

#
# Don't need to register menu data here. Already being done above.
#
menu .$id.menubar.settings.grid_spacing -title "Grid Spacing" -tearoff $do_tearoffs
.$id.menubar.settings.grid_spacing add command -label "Autosize" -underline 0\
	-command "grid_spacing_autosize $id; grid_spacing_apply $id b"
.$id.menubar.settings.grid_spacing add command -label "Arbitrary..." -underline 1\
	-command "do_grid_spacing $id b"
.$id.menubar.settings.grid_spacing add separator
.$id.menubar.settings.grid_spacing add command -label "micrometer" -underline 4\
	-command "set_grid_spacing $id micrometer 1"
.$id.menubar.settings.grid_spacing add command -label "millimeter" -underline 2\
	-command "set_grid_spacing $id millimeter 1"
.$id.menubar.settings.grid_spacing add command -label "centimeter" -underline 0\
	-command "set_grid_spacing $id centimeter 1"
.$id.menubar.settings.grid_spacing add command -label "decimeter" -underline 0\
	-command "set_grid_spacing $id decimeter 1"
.$id.menubar.settings.grid_spacing add command -label "meter" -underline 0\
	-command "set_grid_spacing $id meter 1"
.$id.menubar.settings.grid_spacing add command -label "kilometer" -underline 0\
	-command "set_grid_spacing $id kilometer 1"
.$id.menubar.settings.grid_spacing add separator
.$id.menubar.settings.grid_spacing add command -label "1/10 inch" -underline 0\
	-command "set_grid_spacing $id \"1/10 inch\" 1"
.$id.menubar.settings.grid_spacing add command -label "1/4 inch" -underline 2\
	-command "set_grid_spacing $id \"1/4 inch\" 1"
.$id.menubar.settings.grid_spacing add command -label "1/2 inch" -underline 2\
	-command "set_grid_spacing $id \"1/2 inch\" 1"
.$id.menubar.settings.grid_spacing add command -label "inch" -underline 0\
	-command "set_grid_spacing $id inch 1"
.$id.menubar.settings.grid_spacing add command -label "foot" -underline 0\
	-command "set_grid_spacing $id foot 1"
.$id.menubar.settings.grid_spacing add command -label "yard" -underline 0\
	-command "set_grid_spacing $id yard 1"
.$id.menubar.settings.grid_spacing add command -label "mile" -underline 0\
	-command "set_grid_spacing $id mile 1"

menu .$id.menubar.settings.coord -title "Constraint Coords" -tearoff $do_tearoffs
.$id.menubar.settings.coord add radiobutton -value m -variable mged_coords($id)\
	-label "Model" -underline 0\
	-command "mged_apply $id \"set coords \$mged_coords($id)\""
hoc_register_menu_data "Constraint Coords" "Model" "Constraint Coords - Model"\
	{ { summary "Constrained transformations will use model coordinates." }
          { see_also "rset, vars" } }
.$id.menubar.settings.coord add radiobutton -value v -variable mged_coords($id)\
	-label "View" -underline 0\
	-command "mged_apply $id \"set coords \$mged_coords($id)\""
hoc_register_menu_data "Constraint Coords" "View" "Constraint Coords - View"\
	{ { summary "Constrained transformations will use view coordinates." }
          { see_also "rset, vars" } }
.$id.menubar.settings.coord add radiobutton -value o -variable mged_coords($id)\
	-label "Object" -underline 0\
	-command "mged_apply $id \"set coords \$mged_coords($id)\"" -state disabled
hoc_register_menu_data "Constraint Coords" "Object" "Constraint Coords - Object"\
	{ { summary "Constrained transformations will use object coordinates." }
          { see_also "rset, vars" } }

menu .$id.menubar.settings.origin -title "Rotate About" -tearoff $do_tearoffs
.$id.menubar.settings.origin add radiobutton -value v -variable mged_rotate_about($id)\
	-label "View Center" -underline 0\
	-command "mged_apply $id \"set rotate_about \$mged_rotate_about($id)\""
hoc_register_menu_data "Rotate About" "View Center" "Rotate About - View Center"\
	{ { summary "Set the center of rotation to be about the view center." }
          { see_also "rset, vars" } }
.$id.menubar.settings.origin add radiobutton -value e -variable mged_rotate_about($id)\
	-label "Eye" -underline 0\
	-command "mged_apply $id \"set rotate_about \$mged_rotate_about($id)\""
hoc_register_menu_data "Rotate About" "Eye" "Rotate About - Eye"\
	{ { summary "Set the center of rotation to be about the eye." }
          { see_also "rset, vars" } }
.$id.menubar.settings.origin add radiobutton -value m -variable mged_rotate_about($id)\
	-label "Model Origin" -underline 0\
	-command "mged_apply $id \"set rotate_about \$mged_rotate_about($id)\""
hoc_register_menu_data "Rotate About" "Model Origin" "Rotate About - Model Origin"\
	{ { summary "Set the center of rotation to be about the model origin." }
          { see_also "rset, vars" } }
.$id.menubar.settings.origin add radiobutton -value k -variable mged_rotate_about($id)\
	-label "Key Point" -underline 0\
	-command "mged_apply $id \"set rotate_about \$mged_rotate_about($id)\"" -state disabled
hoc_register_menu_data "Rotate About" "Key Point" "Rotate About - Key Point"\
	{ { summary "Set the center of rotation to be about the key point." }
          { see_also "rset, vars" } }

menu .$id.menubar.settings.transform -title "Transform" -tearoff $do_tearoffs
.$id.menubar.settings.transform add radiobutton -value v -variable mged_transform($id)\
	-label "View" -underline 0\
	-command "set_transform $id"
hoc_register_menu_data "Transform" "View" "Transform - View"\
	{ { summary "Set the transform mode to VIEW. When in VIEW mode, the mouse\n\
can be used to transform the view. This is the default." }
          { see_also "rset, vars" } }
.$id.menubar.settings.transform add radiobutton -value a -variable mged_transform($id)\
	-label "ADC" -underline 0\
	-command "set_transform $id"
hoc_register_menu_data "Transform" "ADC" "Transform - ADC"\
	{ { summary "Set the transform mode to ADC. When in ADC mode, the mouse\n\
can be used to transform the angle distance cursor while\n\
the angle distance cursor is active. If the angle distance cursor\n\
is not active, the behavior is the same as VIEW mode." }
          { see_also "rset, vars" } }
.$id.menubar.settings.transform add radiobutton -value e -variable mged_transform($id)\
	-label "Model Params" -underline 0\
	-command "set_transform $id" -state disabled
hoc_register_menu_data "Transform" "Object Params" "Transform - Object Params"\
	{ { summary "Set the transform mode to OBJECT PARAMS. When in OBJECT PARAMS\n\
mode, the mouse can be used to transform the object parameters." }
          { see_also "rset, vars" } }

menu .$id.menubar.modes -title "Modes" -tearoff $do_tearoffs
.$id.menubar.modes add checkbutton -offvalue 0 -onvalue 1 -variable mged_grid($id,draw)\
	-label "Draw Grid" -underline 0\
	-command "mged_apply $id \"rset grid draw \$mged_grid($id,draw)\""
hoc_register_menu_data "Modes" "Draw Grid" "Draw Grid"\
	{ { summary "Toggle drawing the grid. The grid is\n\
a lattice of points over the pane (geometry window).\n\
The regular spacing between the points gives the user\n\
accurate visual cues regarding dimension. This spacing\n\
can be set by the user." }
          { see_also "rset" } }
.$id.menubar.modes add checkbutton -offvalue 0 -onvalue 1 -variable mged_grid($id,snap)\
	-label "Snap To Grid" -underline 0\
	-command "mged_apply $id \"rset grid snap \$mged_grid($id,snap)\""
hoc_register_menu_data "Modes" "Snap To Grid" "Snap To Grid"\
	{ { summary "Toggle grid snapping. When snapping to grid,\n\
the internal routines that use the mouse pointer location,\n\
move/snap that location to the nearest grid point. This gives\n\
the user high accuracy with the mouse for transforming the\n\
view or editing solids/matrices." }
          { see_also "rset" } }
.$id.menubar.modes add separator
.$id.menubar.modes add checkbutton -offvalue 0 -onvalue 1 -variable mged_fb($id)\
	-label "Framebuffer Active" -underline 0 \
	-command "set_fb $id; update_Raytrace $id"
hoc_register_menu_data "Modes" "Framebuffer Active" "Framebuffer Active"\
	{ { summary "This activates/deactivates the framebuffer." }
          { see_also "rset, vars" } }
.$id.menubar.modes add checkbutton -offvalue 0 -onvalue 1 -variable mged_listen($id)\
	-label "Listen For Clients" -underline 0\
	-command "set_listen $id" -state disabled
hoc_register_menu_data "Modes" "Listen For Clients" "Listen For Clients"\
	{ { summary "This activates/deactivates listening for clients.\n\
If the framebuffer is listening for clients, new data can\n\
be passed into the framebuffer. Otherwise, the framebuffer\n\
is write protected. Actually, it is also read protected with
one exception. MGED can still read the framebuffer data." }
          { see_also "rset, vars" } }
.$id.menubar.modes add separator
.$id.menubar.modes add checkbutton -offvalue 0 -onvalue 1 -variable mged_rubber_band($id)\
	-label "Persistent Rubber Band" -underline 0\
	-command "mged_apply $id \"rset rb draw \$mged_rubber_band($id)\""
hoc_register_menu_data "Modes" "Persistent Rubber Band" "Persistent Rubber Band"\
	{ { summary "Toggle drawing the rubber band while idle." }
          { see_also "rset" } }
.$id.menubar.modes add checkbutton -offvalue 0 -onvalue 1 -variable mged_adc_draw($id)\
	-label "Angle/Dist Cursor" -underline 0 \
	-command "mged_apply $id \"adc draw \$mged_adc_draw($id)\""
hoc_register_menu_data "Modes" "Angle/Dist Cursor" "Angle/Dist Cursor"\
	{ { summary "Toggle drawing the angle distance cursor." }
          { see_also "adc" } }
.$id.menubar.modes add checkbutton -offvalue 0 -onvalue 1 -variable mged_faceplate($id)\
	-label "Faceplate" -underline 7\
	-command "mged_apply $id \"set faceplate \$mged_faceplate($id)\""
hoc_register_menu_data "Modes" "Faceplate" "Faceplate"\
	{ { summary "Toggle drawing the MGED faceplate." }
          { see_also "rset, vars" } }
.$id.menubar.modes add cascade -label "Axes" -underline 1\
	-menu .$id.menubar.modes.axes
.$id.menubar.modes add separator
.$id.menubar.modes add checkbutton -offvalue 0 -onvalue 1 -variable mged_multi_view($id)\
	-label "Multipane" -underline 0 -command "setmv $id"
hoc_register_menu_data "Modes" "Multipane" "Multipane"\
	{ { summary "Toggle between multipane and single pane mode. In\n\
multipane mode there are four panes, each with its own state." } }
.$id.menubar.modes add checkbutton -offvalue 0 -onvalue 1 -variable show_edit_info($id)\
	-label "Edit Info" -underline 0\
	-command "toggle_edit_info $id"
hoc_register_menu_data "Modes" "Edit Info" "Edit Info"\
	{ { summary "Toggle display of edit information.\n\
If in solid edit state, the edit information is\n\
displayed in the internal solid editor. This editor,\n\
as its name implies, can be used to edit the solid\n\
as well as view its contents. If in object edit\n\
state, the object information is displayed in a\n\
dialog box." } }
.$id.menubar.modes add checkbutton -offvalue 0 -onvalue 1 -variable mged_show_status($id)\
	-label "Status Bar" -underline 7\
	-command "toggle_status_bar $id"
hoc_register_menu_data "Modes" "Status Bar" "Status Bar"\
	{ { summary "Toggle display of the command window's status bar." } }
if {$comb} {
.$id.menubar.modes add checkbutton -offvalue 0 -onvalue 1 -variable mged_show_cmd($id)\
	-label "Command Window" -underline 0\
	-command "set_cmd_win $id"
hoc_register_menu_data "Modes" "Command Window" "Command Window"\
	{ { summary "Toggle display of the command window." } }
.$id.menubar.modes add checkbutton -offvalue 0 -onvalue 1 -variable mged_show_dm($id)\
	-label "Graphics Window" -underline 0\
	-command "set_dm_win $id"
hoc_register_menu_data "Modes" "Graphics Window" "Graphics Window"\
	{ { summary "Toggle display of the graphics window." } }
} 
.$id.menubar.modes add separator
.$id.menubar.modes add checkbutton -offvalue 0 -onvalue 1 -variable mged_rateknobs($id)\
	-label "Rateknobs" -underline 0\
	-command "mged_apply $id \"set rateknobs \$mged_rateknobs($id)\""
hoc_register_menu_data "Modes" "Rateknobs" "Rate Knobs"\
	{ { summary "Toggle rate knob mode. When in rate knob mode,\n\
transformation with the mouse becomes rate based.\n\
For example, if the user rotates the view about the\n\
X axis, the view continues to rotate about the X axis\n\
until the rate rotation is stopped." }
          { see_also "knob" } }

menu .$id.menubar.modes.axes -title "Axes" -tearoff $do_tearoffs
.$id.menubar.modes.axes add checkbutton -offvalue 0 -onvalue 1\
	-variable mged_axes($id,view_draw) -label "View" -underline 0\
	-command "mged_apply $id \"rset ax view_draw \$mged_axes($id,view_draw)\""
hoc_register_menu_data "Axes" "View" "View Axes"\
	{ { summary "Toggle display of the view axes. The view\n\
axes are used to give the user visual cues indicating\n\
the current view of model space. These axes are drawn\n\
the same as the model axes, except that the view axes'\n\
position is fixed in view space. This position as\n\
well as other characteristics can be set by the user." }
          { see_also "rset" } }
.$id.menubar.modes.axes add checkbutton -offvalue 0 -onvalue 1\
	-variable mged_axes($id,model_draw) -label "Model" -underline 0\
	-command "mged_apply $id \"rset ax model_draw \$mged_axes($id,model_draw)\""
hoc_register_menu_data "Axes" "Model" "Model Axes"\
	{ { summary "Toggle display of the model axes. The model\n\
axes are used to give the user visual cues indicating\n\
the current view of model space. The model axes are by default\n\
located at the model origin and are fixed in model space.\n\
So, if the user transforms the view, the model axes will potentially
move in view space. The model axes position as well\n\
as other characteristics can be set by the user." }
          { see_also "rset" } }
.$id.menubar.modes.axes add checkbutton -offvalue 0 -onvalue 1\
	-variable mged_axes($id,edit_draw) -label "Edit" -underline 0\
	-command "mged_apply $id \"rset ax edit_draw \$mged_axes($id,edit_draw)\""
hoc_register_menu_data "Axes" "Edit" "Edit Axes"\
	{ { summary "Toggle display of the edit axes. The edit\n\
axes are used to give the user visual cues indicating\n\
how the edit is progressing. They consist of a pair of axes.\n\
One remains unmoved, while the other moves to indicate\n\
what has changed." }
          { see_also "rset" } }

menu .$id.menubar.tools -title "Tools" -tearoff $do_tearoffs
.$id.menubar.tools add command -label "ADC Control Panel..." -underline 0\
	-command "init_adc_control $id"
hoc_register_menu_data "Tools" "ADC Control Panel..." "ADC Control Panel"\
	{ { summary "Tool for controlling the angle distance cursor." }
          { see_also "adc" } }
.$id.menubar.tools add command -label "Grid Control Panel..." -underline 0\
	-command "init_grid_control $id"
hoc_register_menu_data "Tools" "Grid Control Panel..." "Grid Control Panel"\
	{ { summary "Tool for setting grid parameters." }
          { see_also "rset" } }
.$id.menubar.tools add command -label "Query Ray Control Panel..." -underline 0\
	-command "init_qray_control $id"
hoc_register_menu_data "Tools" "Query Ray Control Panel..." "Query Ray Control Panel"\
	{ { summary "Tool for setting query ray parameters." } }
.$id.menubar.tools add command -label "Raytrace Control Panel..." -underline 0\
	-command "init_Raytrace $id"
hoc_register_menu_data "Tools" "Raytrace Control Panel..." "Raytrace Control Panel"\
	{ { summary "Tool for raytracing the current view." }
          { see_also rt } }
.$id.menubar.tools add separator
.$id.menubar.tools add command -label "Solid Editor..." -underline 0\
	-command "init_edit_solid $id"
hoc_register_menu_data "Tools" "Solid Editor..." "Solid Editor"\
	{ { summary "Tool for creating/editing solids." } }
.$id.menubar.tools add command -label "Combination Editor..." -underline 0\
	-command "init_comb $id"
hoc_register_menu_data "Tools" "Combination Editor..." "Combination Editor"\
	{ { summary "Tool for creating/editing combinations." } }
.$id.menubar.tools add command -label "Color Editor..." -underline 1\
	-command "cadColorWidget tool .$id colorEditTool\
	-title \"Color Editor\"\
	-initialcolor black"
hoc_register_menu_data "Tools" "Color Editor..." "Color Editor"\
	{ { summary "Tool for displaying colors." } }
#.$id.menubar.tools add separator
#.$id.menubar.tools add checkbutton -offvalue 0 -onvalue 1 -variable buttons_on($id)\
#	-label "Classic Menu Tool..." -underline 0\
#	-command "toggle_button_menu $id"

menu .$id.menubar.help -title "Help" -tearoff $do_tearoffs
.$id.menubar.help add command -label "About" -underline 0\
	-command "do_About_MGED $id"
hoc_register_menu_data "Help" "About" "About MGED"\
	{ { summary "Information about MGED" } }
#.$id.menubar.help add command -label "On Context" -underline 0\
#	-command "on_context_help $id"
.$id.menubar.help add command -label "Commands..." -underline 0\
	-command "command_help $id"
hoc_register_menu_data "Help" "Commands..." "Commands"\
	{ { summary "Tool for getting information on MGED's commands." }
          { see_also "?, help" } }
.$id.menubar.help add command -label "Apropos..." -underline 1\
	-command "ia_apropos .$id $screen"
hoc_register_menu_data "Help" "Apropos..." "Apropos"\
	{ { summary "Tool for searching for information about\n\
MGED's commands." }
          { see_also "apropos" } }

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
hoc_register_menu_data "Help" "Manual..." "Manual"\
	{ { summary "Tool for browsing the online MGED manual." } }

#==============================================================================
# PHASE 3: Bottom-row display
#==============================================================================

frame .$id.status
frame .$id.status.dpy
frame .$id.status.illum

label .$id.status.cent -textvar mged_display($mged_active_dm($id),center) -anchor w
hoc_register_data .$id.status.cent "View Center"\
	{ { summary "These numbers indicate the view center in\nmodel coordinates (local units)." }
          { see_also "center, view, viewget, viewset" } }
label .$id.status.size -textvar mged_display($mged_active_dm($id),size) -anchor w
hoc_register_data .$id.status.size "View Size"\
	{ { summary "This number indicates the view size (local units)." }
          { see_also size} }
label .$id.status.units -textvar mged_display(units) -anchor w -padx 4
hoc_register_data .$id.status.units "Units"\
	{ { summary "This indicates the local units.     " }
          { see_also units} }
label .$id.status.aet -textvar mged_display($mged_active_dm($id),aet) -anchor w
hoc_register_data .$id.status.aet "View Orientation"\
	{ { summary "These numbers indicate the view orientation using azimuth,\nelevation and twist." }
        { see_also "ae, view, viewget, viewset" } }
label .$id.status.ang -textvar mged_display($mged_active_dm($id),ang) -anchor w -padx 4
hoc_register_data .$id.status.ang "Rateknobs"\
	{ { summary "These numbers give some indication of\nrate of rotation about the x,y,z axes." }
        { see_also knob} }
label .$id.status.illum.label -textvar ia_illum_label($id)
hoc_register_data .$id.status.illum.label "Status Area"\
	{ { summary "This area is for displaying either the frames per second,\nthe illuminated path or the keypoint during an edit." } }

#==============================================================================
# PHASE 4: Text widget for interaction
#==============================================================================

frame .$id.tf
if {$use_grid_gm} {
    if {$comb} {
	text .$id.t -height $mged_num_lines($id) -relief sunken -bd 2 -yscrollcommand ".$id.s set"
    } else {
	text .$id.t -relief sunken -bd 2 -yscrollcommand ".$id.s set"
    }
} else {
    if {$comb} {
	text .$id.t -width 10 -relief sunken -bd 2 -yscrollcommand ".$id.s set" -setgrid true
    } else {
	text .$id.t -relief sunken -bd 2 -yscrollcommand ".$id.s set" -setgrid true
    }
}
scrollbar .$id.s -relief flat -command ".$id.t yview"

bind .$id.t <Enter> "focus .$id.t; break"
hoc_register_data .$id.t "Command Window" { { summary "Enter commands here!" } }

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

if { $use_grid_gm } {
    grid $mged_active_dm($id) -in $mged_dmc($id) -sticky "nsew" -row 0 -column 0
} else {
    pack $mged_active_dm($id) -in $mged_dmc($id)

    if {$comb} {
	pack $mged_dmc($id) -side top -padx 2 -pady 2
    }
}

set mged_multi_view($id) $mged_default_mvmode

if { $use_grid_gm } {
    if { $comb } {
	if { $mged_show_dm($id) } {
	    grid $mged_dmc($id) -sticky nsew -row 0 -column 0
	}
    }

    grid .$id.t .$id.s -in .$id.tf -sticky "nsew"
    grid columnconfigure .$id.tf 0 -weight 1
    grid columnconfigure .$id.tf 1 -weight 0
    grid rowconfigure .$id.tf 0 -weight 1

    if { !$comb || ($comb && $mged_show_cmd($id)) } {
	grid .$id.tf -sticky "nsew" -row 1 -column 0
    }

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

    if { $mged_show_status($id) } {
	grid .$id.status -sticky "ew" -row 2 -column 0
    }

    grid columnconfigure .$id 0 -weight 1
    if { $comb } {
	grid rowconfigure .$id 0 -weight 1
	grid rowconfigure .$id 1 -weight 0
	grid rowconfigure .$id 2 -weight 0
    } else {
	grid rowconfigure .$id 0 -weight 0
	grid rowconfigure .$id 1 -weight 1
	grid rowconfigure .$id 2 -weight 0
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

if {[info procs cad_MenuFirstEntry] == ""} {
    # trigger the Tcl source file to be loaded
    cad_MenuFirstEntry ""
}

cmd_init $id
setupmv $id
tie $id $mged_active_dm($id)
rset cs mode 1

if { $join_c } {
    jcs $id
}

trace variable mged_display($mged_active_dm($id),fps) w "ia_changestate $id"
update_mged_vars $id
set mged_qray_effects($id) [qray effects]

# reset current_cmd_list so that its cur_hist gets updated
cmd_set $save_id

# cause all 4 windows to share menu state
share m $mged_top($id).ul $mged_top($id).ur
share m $mged_top($id).ul $mged_top($id).ll
share m $mged_top($id).ul $mged_top($id).lr

do_rebind_keys $id
bind $mged_dmc($id) <Configure> "setmv $id"

# Throw away key events
bind $mged_top($id) <KeyPress> { break }

set dbname [_mged_opendb]
set_wm_title $id $dbname

# Force display manager windows to update their respective color schemes
mged_apply_local $id "rset cs mode \[rset cs mode\]"

wm protocol $mged_top($id) WM_DELETE_WINDOW "gui_destroy_default $id"
wm geometry $mged_top($id) -0+0
#set width [winfo screenwidth $mged_top($id)]
#set height [winfo screenheight $mged_top($id)]
#wm geometry $mged_top($id) $width\x$height+8+40

if { $comb } {
    if { !$mged_show_dm($id) } {
	update
	set_dm_win $id
    }
} else {
    # Position command window in upper left corner
    wm geometry .$id +8+32
    update

    # Prevent command window from resizing itself as labels change
    set geometry [wm geometry .$id]
    wm geometry .$id $geometry
}

set num_players [llength $mged_players]
switch $num_players {
    1 {
	.$id.menubar.file entryconfigure 11 -state disabled
    }
    2 {
	set id [lindex $mged_players 0]
	.$id.menubar.file entryconfigure 11 -state normal
    }
}
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

    if { [llength $mged_players] == 1 } {
	set id [lindex $mged_players 0]
	.$id.menubar.file entryconfigure 11 -state disabled
    }
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
    global mouse_behavior
    global mged_mouse_behavior
    global coords
    global mged_coords
    global rotate_about
    global mged_rotate_about
    global transform
    global mged_transform
    global faceplate
    global mged_faceplate
    global mged_adc_draw
    global mged_axes
    global mged_grid
    global mged_rubber_band

    winset $mged_active_dm($id)
    set mged_rateknobs($id) $rateknobs
    set mged_adc_draw($id) [adc draw]
    set mged_axes($id,model_draw) [rset ax model_draw]
    set mged_axes($id,view_draw) [rset ax view_draw]
    set mged_axes($id,edit_draw) [rset ax edit_draw]
    set mged_use_air($id) $use_air
    set mged_fb($id) $fb
    set mged_fb_all($id) $fb_all
    set mged_fb_overlay($id) $fb_overlay
    set mged_rubber_band($id) [rset rb draw]
    set mged_mouse_behavior($id) $mouse_behavior
    set mged_coords($id) $coords
    set mged_rotate_about($id) $rotate_about
    set mged_transform($id) $transform
    set mged_grid($id,draw) [rset grid draw]
    set mged_grid($id,snap) [rset grid snap]
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
    global mged_axes

    set view_pos [rset ax view_pos]
    set hpos [lindex $view_pos 0]
    set vpos [lindex $view_pos 1]

    if {$hpos == 0 && $vpos == 0} {
	set mged_axes($id,view_pos) 0
    } elseif {$hpos < 0 && $vpos < 0} {
	set mged_axes($id,view_pos) 1
    } elseif {$hpos < 0 && $vpos > 0} {
	set mged_axes($id,view_pos) 2
    } elseif {$hpos > 0 && $vpos > 0} {
	set mged_axes($id,view_pos) 3
    } else {
	set mged_axes($id,view_pos) 4
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
	if {$mged_display(state) == "SOL EDIT"} {
	    init_edit_solid_int $id
	} elseif {$mged_display(state) == "OBJ EDIT"} {
	    build_edit_info $id
	}
    } else {
	if {$mged_display(state) == "SOL EDIT"} {
	    esolint_destroy $id
	} elseif {$mged_display(state) == "OBJ EDIT"} {
	    destroy_edit_info $id
	}
    }
}

proc build_edit_info { id } {
    global player_screen
    global edit_info_pos
    global show_edit_info

    if [winfo exists .sei$id] {
	raise .sei$id
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

	catch { share view $ow $nw }
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

    catch {share -u view $w}
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

proc tie args {
    if { [llength $args] == 2 } {
	if {[lindex $args 0] == "-u"} {
	    set i 1
	} else {
	    set i 0
	}

	if ![winfo exists .[lindex $args $i]] {
	    return
	}
    }

    set result [eval _mged_tie $args]
    
    if { [llength $args] == 2 } {
	reconfig_gui_default [lindex $args $i]
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
    global use_grid_gm

    set vloc [string range $mged_dm_loc($id) 0 0]
    set hloc [string range $mged_dm_loc($id) 1 1]
    set tmp_dm $mged_top($id).$mged_dm_loc($id)

# Nothing to do
    if { $tmp_dm == $mged_active_dm($id) } {
	return
    }

    trace vdelete mged_display($mged_active_dm($id),fps) w "ia_changestate $id"

    # make inactive
    winset $mged_active_dm($id)
    rset cs mode 0

    set mged_active_dm($id) $tmp_dm
    set mged_small_dmc($id) $mged_dmc($id).$vloc.$hloc

    # make active
    winset $mged_active_dm($id)
    rset cs mode 1

    trace variable mged_display($mged_active_dm($id),fps) w "ia_changestate $id"

    update_mged_vars $id
    set view_ring($id) [view_ring get]

    tie $id $mged_active_dm($id)

    if {!$mged_multi_view($id)} {
# unpack previously active dm
	if { $use_grid_gm } {
	    grid forget $mged_top($id).$save_dm_loc($id)
	} else {
	    pack forget $mged_top($id).$save_dm_loc($id)
	}

# resize and repack previously active dm into smaller frame
	winset $mged_top($id).$save_dm_loc($id)
	set mv_size [expr $win_size($id) / 2 - 4]
	dm size $mv_size $mv_size

	if { $use_grid_gm } {
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
    global mged_dmc
    global mged_show_cmd
    global mged_show_dm
    global mged_num_lines
    global win_size
    global use_grid_gm

    if { $mged_show_cmd($id) } {
	if { $use_grid_gm } {
	    if { $mged_show_dm($id) } {
		.$id.t configure -height $mged_num_lines($id)
		grid .$id.tf -sticky nsew -row 1 -column 0
#		update
#		setmv $id
	    } else {
		grid .$id.tf -sticky nsew -row 0 -column 0
	    }
	} else {
	    set win_size($id) $win_size($id,small)
	    setmv $id
	    pack .$id.tf -side top -fill both -expand yes
	}
    } else {
	if {$use_grid_gm} {
	    grid forget .$id.tf

	    if { !$mged_show_dm($id) } {
		set mged_show_dm($id) 1
		grid $mged_dmc($id) -sticky nsew -row 0 -column 0
		update
		setmv $id
	    }
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
    global use_grid_gm

    if { $mged_show_dm($id) } {
	if { $use_grid_gm } {
	    if { $mged_show_cmd($id) } {
		grid forget .$id.tf
		.$id.t configure -height $mged_num_lines($id)
		update
	    }

	    grid $mged_dmc($id) -sticky nsew -row 0 -column 0

	    if { $mged_show_cmd($id) } {
		grid .$id.tf -sticky nsew -row 1 -column 0
		update
		.$id.t see end
	    }

	    setmv $id
	} else {
	    if {[winfo ismapped .$id.tf]} {
		pack $mged_dmc($id) -side top -before .$id.tf -padx 2 -pady 2
		.$id.t configure -width 10 -height 10
	    } else {
		pack $mged_dmc($id) -side top -padx 2 -pady 2
	    }
	}
    } else {
	if { $use_grid_gm } {
	    grid forget $mged_dmc($id)

	    set mged_show_cmd($id) 1
	    set_cmd_win $id
	    set fh [get_font_height .$id.t]
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

proc view_ring_add { id } {
    global mged_active_dm
    global mged_dm_loc
    global view_ring
    global mged_collaborators
    global mged_top

#    if {$mged_dm_loc($id) != "lv"} {
#	winset $mged_active_dm($id)
#    }
    winset $mged_active_dm($id)

    _mged_view_ring add

    set i [lsearch -exact $mged_collaborators $id]
    if {$i != -1 && "$mged_top($id).ur" == "$mged_active_dm($id)"} {
	foreach cid $mged_collaborators {
	    if {"$mged_top($cid).ur" == "$mged_active_dm($cid)"} {
		do_view_ring_entries $cid s
		do_view_ring_entries $cid d
		winset $mged_active_dm($cid)
		set view_ring($cid) [view_ring get]
	    }
	}
    } else {
	do_view_ring_entries $id s
	do_view_ring_entries $id d
	set view_ring($id) [view_ring get]
    }
}

proc view_ring_delete { id vid } {
    global mged_active_dm
    global mged_dm_loc
    global view_ring
    global mged_collaborators
    global mged_top

#    if {$mged_dm_loc($id) != "lv"} {
#	winset $mged_active_dm($id)
#    }
    winset $mged_active_dm($id)

    _mged_view_ring delete $vid

    set i [lsearch -exact $mged_collaborators $id]
    if { $i != -1 && "$mged_top($id).ur" == "$mged_active_dm($id)"} {
	foreach cid $mged_collaborators {
	    if {"$mged_top($cid).ur" == "$mged_active_dm($cid)"} {
		do_view_ring_entries $cid s
		do_view_ring_entries $cid d
		winset $mged_active_dm($cid)
		set view_ring($cid) [view_ring get]
	    }
	}
    } else {
	do_view_ring_entries $id s
	do_view_ring_entries $id d
	set view_ring($id) [view_ring get]
    }
}

proc view_ring_goto { id vid } {
    global mged_active_dm
    global mged_dm_loc
    global view_ring
    global mged_collaborators
    global mged_top

#    if {$mged_dm_loc($id) != "lv"} {
#	winset $mged_active_dm($id)
#    }
    winset $mged_active_dm($id)

    _mged_view_ring goto $vid

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

proc view_ring_next { id } {
    global mged_active_dm
    global mged_dm_loc
    global view_ring
    global mged_collaborators
    global mged_top

#    if {$mged_dm_loc($id) != "lv"} {
#	winset $mged_active_dm($id)
#    }
    winset $mged_active_dm($id)

    _mged_view_ring next

    set i [lsearch -exact $mged_collaborators $id]
    if { $i != -1 && "$mged_top($id).ur" == "$mged_active_dm($id)"} {
	foreach cid $mged_collaborators {
	    if {"$mged_top($cid).ur" == "$mged_active_dm($cid)"} {
		winset $mged_active_dm($cid)
		set view_ring($cid) [view_ring get]
	    }
	}
    } else {
	set view_ring($id) [view_ring get]
    }
}

proc view_ring_prev { id } {
    global mged_active_dm
    global mged_dm_loc
    global view_ring
    global mged_collaborators
    global mged_top

#    if {$mged_dm_loc($id) != "lv"} {
#	winset $mged_active_dm($id)
#    }
    winset $mged_active_dm($id)

    _mged_view_ring prev

    set i [lsearch -exact $mged_collaborators $id]
    if { $i != -1 && "$mged_top($id).ur" == "$mged_active_dm($id)"} {
	foreach cid $mged_collaborators {
	    if {"$mged_top($cid).ur" == "$mged_active_dm($cid)"} {
		winset $mged_active_dm($cid)
		set view_ring($cid) [view_ring get]
	    }
	}
    } else {
	set view_ring($id) [view_ring get]
    }
}

proc view_ring_toggle { id } {
    global mged_active_dm
    global mged_dm_loc
    global view_ring
    global mged_collaborators
    global mged_top

#    if {$mged_dm_loc($id) != "lv"} {
#	winset $mged_active_dm($id)
#    }
    winset $mged_active_dm($id)

    _mged_view_ring toggle

    set i [lsearch -exact $mged_collaborators $id]
    if { $i != -1 && "$mged_top($id).ur" == "$mged_active_dm($id)"} {
	foreach cid $mged_collaborators {
	    if {"$mged_top($cid).ur" == "$mged_active_dm($cid)"} {
		winset $mged_active_dm($cid)
		set view_ring($cid) [view_ring get]
	    }
	}
    } else {
	set view_ring($id) [view_ring get]
    }
}

proc do_view_ring_entries { id m } {
    global view_ring

    set views [view_ring get -a]
    set llen [llength $views]

    if {$m == "s"} {
	set w .$id.menubar.viewring.select
	$w delete 0 end
	for {set i 0} {$i < $llen} {incr i} {
	    $w add radiobutton -value [lindex $views $i] -variable view_ring($id)\
		    -label [lindex $views $i] -command "view_ring_goto $id [lindex $views $i]"
	}
    } elseif {$m == "d"} {
	set w .$id.menubar.viewring.delete
	$w delete 0 end
	for {set i 0} {$i < $llen} {incr i} {
	    $w add command -label [lindex $views $i]\
		    -command "view_ring_delete $id [lindex $views $i]"
	}
    } else {
	puts "Usage: do_view_ring_entries w s|d"
    }
}

proc do_view_ring_labels { id } {
    global mged_active_dm
    global view_ring

    winset $mged_active_dm($id)
    set save_view [view_ring get]
    set views [view_ring get -a]
    set llen [llength $views]

    set ws .$id.menubar.viewring.select
    set wd .$id.menubar.viewring.delete
    for {set i 0} {$i < $llen} {incr i} {
	view_ring goto [lindex $views $i]
	set aet [view aet]
	set aet [format "az=%.2f el=%.2f tw=%.2f"\
		[lindex $aet 0] [lindex $aet 1] [lindex $aet 2]]
	set center [view center]
	set center [format "cent=(%.3f %.3f %.3f)"\
		[lindex $center 0] [lindex $center 1] [lindex $center 2]]
	set size [format "size=%.3f" [view size]]
	$ws entryconfigure $i -label "$center $size $aet"
	$wd entryconfigure $i -label "$center $size $aet"
    }

    view_ring goto $save_view
}

proc toggle_status_bar { id } {
    global mged_show_status
    global use_grid_gm

    if {$mged_show_status($id)} {
	if { $use_grid_gm } {
	    grid .$id.status -sticky ew -row 2 -column 0
	} else {
	    pack .$id.status -side bottom -anchor w
	}
    } else {
	if { $use_grid_gm } {
	    grid forget .$id.status
	} else {
	    pack forget .$id.status
	}
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

    bind $mged_top($id).ul <Control-n> "winset $mged_top($id).ul; view_ring_next $id; break" 
    bind $mged_top($id).ur <Control-n> "winset $mged_top($id).ur; view_ring_next $id; break" 
    bind $mged_top($id).ll <Control-n> "winset $mged_top($id).ll; view_ring_next $id; break" 
    bind $mged_top($id).lr <Control-n> "winset $mged_top($id).lr; view_ring_next $id; break" 

    bind $mged_top($id).ul <Control-p> "winset $mged_top($id).ul; view_ring_prev $id; break" 
    bind $mged_top($id).ur <Control-p> "winset $mged_top($id).ur; view_ring_prev $id; break" 
    bind $mged_top($id).ll <Control-p> "winset $mged_top($id).ll; view_ring_prev $id; break" 
    bind $mged_top($id).lr <Control-p> "winset $mged_top($id).lr; view_ring_prev $id; break" 

    bind $mged_top($id).ul <Control-t> "winset $mged_top($id).ul; view_ring_toggle $id; break" 
    bind $mged_top($id).ur <Control-t> "winset $mged_top($id).ur; view_ring_toggle $id; break" 
    bind $mged_top($id).ll <Control-t> "winset $mged_top($id).ll; view_ring_toggle $id; break" 
    bind $mged_top($id).lr <Control-t> "winset $mged_top($id).lr; view_ring_toggle $id; break" 
}

proc adc { args } {
    global mged_active_dm
    global transform
    global mged_adc_draw

    set result [eval _mged_adc $args]
    set id [lindex [cmd_get] 2]

    if { ![llength $args] } {
	if {[info exists mged_active_dm($id)]} {
	    set mged_adc_draw($id) [_mged_adc draw]
	}

	if {$transform == "a"} {
	    default_mouse_bindings [winset]
	}
    }

    return $result
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
	.$id.menubar.modes entryconfigure 4 -state normal
	set mged_listen($id) 1
	mged_apply $id "set listen \$mged_listen($id)"
    } else {
	.$id.menubar.settings.fb entryconfigure 7 -state disabled
	.$id.menubar.modes entryconfigure 4 -state disabled
	set mged_listen($id) 0
    }
}

proc get_font_height { w } {
    if { [winfo class $w] != "Text" } {
	return 0
    }

    set tmp_font [lindex [$w configure -font] 4]

    return [font metrics $tmp_font -linespace]
}

proc get_cmd_win_height { id } {
    global mged_num_lines

    set fh [get_font_height .$id.t]

    return [expr $fh * $mged_num_lines($id)]
}

proc choosePaneColor { id } {
    global mged_top
    global mged_active_dm

    winset $mged_active_dm($id)
    set rgb_str [_mged_dm bg]
    set rgb [format "#%02x%02x%02x" [lindex $rgb_str 0] [lindex $rgb_str 1] [lindex $rgb_str 2]]

    set parent $mged_top($id)
    set child pane_color

    cadColorWidget dialog $parent $child\
	    -title "Pane Color"\
	    -initialcolor $rgb\
	    -ok "pane_color_ok $id $parent.$child"\
	    -cancel "cadColorWidget_destroy $parent.$child"
}

proc pane_color_ok { id w } {
    upvar #0 $w data

    mged_apply $id "dm bg $data(red) $data(green) $data(blue)"

    destroy $w
    unset data
}

proc chooseWireframeHighlightColor { id } {
    global wireframe_highlight_color
    global mged_top
    global mged_active_dm

    winset $mged_active_dm($id)
    set rgb_str $wireframe_highlight_color
    set rgb [format "#%02x%02x%02x" [lindex $wireframe_highlight_color 0]\
	    [lindex $wireframe_highlight_color 1] [lindex $wireframe_highlight_color 2]]

    set parent $mged_top($id)
    set child wire_hl_color

    cadColorWidget dialog $parent $child\
	    -title "Wireframe Highlight Color"\
	    -initialcolor $rgb\
	    -ok "wireframeHighlightColor_ok $parent.$child"\
	    -cancel "cadColorWidget_destroy $parent.$child"
}

proc wireframeHighlightColor_ok { w } {
    global wireframe_highlight_color
    upvar #0 $w data

    set wireframe_highlight_color "$data(red) $data(green) $data(blue)"

    destroy $w
    unset data
}

proc chooseDefaultWireframeColor { id } {
    global default_wireframe_color
    global mged_top
    global mged_active_dm

    winset $mged_active_dm($id)
    set rgb [format "#%02x%02x%02x" [lindex $default_wireframe_color 0]\
	    [lindex $default_wireframe_color 1] [lindex $default_wireframe_color 2]]

    set parent $mged_top($id)
    set child def_wire_color

    cadColorWidget dialog $parent $child\
	    -title "Default Wireframe Color"\
	    -initialcolor $rgb\
	    -ok "defaultWireframeColor_ok $parent.$child"\
	    -cancel "cadColorWidget_destroy $parent.$child"
}

proc defaultWireframeColor_ok { w } {
    global default_wireframe_color
    upvar #0 $w data

    set default_wireframe_color "$data(red) $data(green) $data(blue)"

    destroy $w
    unset data
}
