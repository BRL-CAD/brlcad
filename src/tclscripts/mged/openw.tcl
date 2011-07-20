#                       O P E N W . T C L
# BRL-CAD
#
# Copyright (c) 1998-2011 United States Government as represented by
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
# Description -
#	MGED's default Tcl/Tk interface.
#

check_externs "_mged_attach _mged_tie _mged_view_ring"

set mged_Priv(arb8) {arb8 arb7 arb6 arb5 arb4 rpp}
set mged_Priv(cones) {rcc rec rhc rpc tec tgc trc}
set mged_Priv(ellipsoids) {ehy ell ell1 epa sph}

# grip is purposely not a part of the other_prims list or any
# other prim list because it's not considered geometry
set mged_Priv(other_prims) {ars dsp eto extrude half metaball part pipe sketch tor}

# weak edit support for these primitives
set mged_Priv(weak_prims) {bot nmg}

if {![info exists tk_version]} {
    loadtk
}

if {![info exists mged_default(max_views)]} {
    set mged_default(max_views) 10
}

if {![info exists mged_default(rot_factor)]} {
    set mged_default(rot_factor) 1
}

if {![info exists mged_default(tran_factor)]} {
    set mged_default(tran_factor) 0.01
}

set mged_default(html_dir) [file normalize [file join [bu_brlcad_data "html"] manuals mged]]
if {![file exists $mged_default(html_dir)]} {
    set mged_default(html_dir) [file normalize [file join [bu_brlcad_data "doc"] html manuals mged]]
}

if {[info exists env(MGED_HTML_DIR)]} {
    set mged_html_dir $env(MGED_HTML_DIR)
} else {
    set mged_html_dir $mged_default(html_dir)
}

if {![info exists mged_default(web_browser)]} {
    if { ($::tcl_platform(platform) == "windows") && [file exists "C:/Program Files/Internet Explorer/iexplore.exe"] } {
	set mged_default(web_browser) "C:/Program Files/Internet Explorer/iexplore.exe"
    } else {
	set mged_default(web_browser) /usr/bin/netscape
    }
}

if {![info exists mged_color_scheme]} {
    color_scheme_init
}

if {![info exists mged_players]} {
    set mged_players {}
}

if {![info exists mged_collaborators]} {
    set mged_collaborators {}
}

if {![info exists mged_default(ggeom)]} {
    set mged_default(ggeom) -0+0
}

if {![info exists mged_default(geom)]} {
    # Position command window in upper left corner
    set mged_default(geom) +8+32
}

if {![info exists mged_default(id)]} {
    set mged_default(id) "id"
}

if {![info exists mged_default(pane)]} {
    set mged_default(pane) "ur"
}

if {![info exists mged_default(multi_pane)]} {
    set mged_default(multi_pane) 0
}

if {![info exists mged_default(config)]} {
    set mged_default(config) b
}

if {[info exists env(DISPLAY)]} {
    set mged_default(display) $env(DISPLAY)
} else {
    if {![info exists mged_default(display)]} {
	set mged_default(display) :0
    }
    set env(DISPLAY) $mged_default(display)
}

if {![info exists mged_default(gdisplay)]} {
    set mged_default(gdisplay) $mged_default(display)
}

if {![info exists mged_default(dm_type)]} {
    set mged_default(dm_type) [dm_bestXType $mged_default(gdisplay)]
}

if {![info exists mged_default(comb)]} {
    set mged_default(comb) 0
}

if {![info exists mged_default(edit_style)]} {
    set mged_default(edit_style) emacs
}

if {![info exists mged_default(num_lines)]} {
    set mged_default(num_lines) 10
}

# XXX For the moment, disable tearoff menus
if {![info exists mged_default(tearoff_menus)]} {
    set mged_default(tearoff_menus) 0
} else {
    set mged_default(tearoff_menus) 0
}

if {![info exists mged_gui(mged,screen)]} {
    set mged_gui(mged,screen) $mged_default(display)
}

if {![info exists mged_gui(databaseDir)]} {
    set mged_gui(databaseDir) [pwd]
}

if {![info exists mged_gui(loadScriptDir)]} {
    set mged_gui(loadScriptDir) [pwd]
}

if {![info exists mged_default(font_init)]} {
    font_init
}

if {![info exists mged_default(status_bar)]} {
    set mged_default(status_bar) 1
}

if {![info exists mged_default(faceplate)]} {
    set mged_default(faceplate) 0
}

if {![info exists mged_default(orig_gui)]} {
    set mged_default(orig_gui) 0
}

if {![info exists mged_default(zclip)]} {
    set mged_default(zclip) 0
}

if {![info exists mged_default(zbuffer)]} {
    set mged_default(zbuffer) 1
}

if {![info exists mged_default(lighting)]} {
    set mged_default(lighting) 0
}

if {![info exists mged_default(perspective_mode)]} {
    set mged_default(perspective_mode) 0
}

if {![info exists mged_default(max_text_lines)]} {
    set mged_default(max_text_lines) 10000
}

if {![info exists mged_default(doubleClickTol)]} {
    set mged_default(doubleClickTol) 500
}

if {[info exists env(WEB_BROWSER)]} {
    if {[file exists $env(WEB_BROWSER)]} {
	set mged_browser $env(WEB_BROWSER)
    }
}

if {![info exists mged_browser]} {
    if {[file exists $mged_default(web_browser)]} {
	set mged_browser $mged_default(web_browser)
    } else {
	if { ($::tcl_platform(os) == "Windows NT") && [file exists "C:/Program Files/Internet Explorer/iexplore.exe"] } {
	    set mged_browser "C:/Program Files/Internet Explorer/iexplore.exe"
	} elseif {[info exists env(PATH)]} {
	    set pathlist [split $env(PATH) :]
	    foreach path $pathlist {
		if {[file exists $path/netscape]} {
		    set mged_browser $path/netscape
		    break;
		} elseif { [file exists $path/mozilla] } {
		    set mged_browser $path/mozilla
		    break;
		} elseif { [file exists $path/firefox] } {
		    set mged_browser $path/firefox
		    break;
		} elseif { ($::tcl_platform(os) == "Darwin") && [file exists $path/open] } {
		    set mged_browser $path/open
		    break;
		}
	    }
	}

	if {![info exists mged_browser]} {
	    set mged_browser "Browser not found, check your PATH"
	}
    }
}

# Call this routine to initialize the "solid_data" array
solid_data_init

##
# Set the class bindings for use with help. This requires the
# widget to register its data using hoc_register_data. Also, for now,
# the Menu class binding is being handled in tclscripts/menu_override.tcl.
#
bind Entry <ButtonPress-3><ButtonRelease-3> "hoc_callback %W %X %Y"
bind Label <ButtonPress-3><ButtonRelease-3> "hoc_callback %W %X %Y"
bind Text <ButtonPress-3><ButtonRelease-3> "hoc_callback %W %X %Y"
bind Button <ButtonPress-3><ButtonRelease-3> "hoc_callback %W %X %Y"
bind Checkbutton <ButtonPress-3><ButtonRelease-3> "hoc_callback %W %X %Y"
bind Menubutton <ButtonPress-3><ButtonRelease-3> "hoc_callback %W %X %Y"
bind Radiobutton <ButtonPress-3><ButtonRelease-3> "hoc_callback %W %X %Y"
bind Canvas <ButtonPress-3><ButtonRelease-3> "hoc_callback %W %X %Y"
bind Frame <ButtonPress-3><ButtonRelease-3> "hoc_callback %W %X %Y"
bind Listbox <ButtonPress-3><ButtonRelease-3> "hoc_callback %W %X %Y"
bind Scale <ButtonPress-3><ButtonRelease-3> "hoc_callback %W %X %Y"

# This causes cad_dialog to use mged_wait instead of tkwait
set ::tk::Priv(wait_cmd) mged_wait

# Used throughout the GUI as the dialog window name.
# This helps prevent window clutter.
set ::tk::Priv(cad_dialog) .mged_dialog

proc gui { args } {
    global tmp_hoc
    global mged_gui
    global mged_default
    global mged_html_dir
    global mged_browser
    global mged_players
    global mged_collaborators
    global mged_display
    global env
    global view_ring
    global vi_state
    global mged_color_scheme
    global mged_Priv
    global tcl_platform
    global mged_cmds

    # list of commands for use in tab expansion
    set mged_cmds [?]

    # configure the stdout chanel for this platform
    # this is supposedly done automatically by Tcl, but not
    switch $::tcl_platform(platform) {
	"macintosh" -
	"unix" {
	    fconfigure stdout -translation lf
	}
	"windows" {
	    fconfigure stdout -translation crlf
	}
    }

    # set defaults
    set save_id [cmd_win get]
    set comb $mged_default(comb)
    set join_c 0
    set dtype $mged_default(dm_type)
    set id ""
    set scw 0
    set sgw 0
    set i 0

    set screen $mged_default(display)
    set gscreen $mged_default(gdisplay)

    if {$mged_default(config) == "b"} {
	set scw 1
	set sgw 1
    } elseif {$mged_default(config) == "c"} {
	set scw 1
	set sgw 0
    } elseif {$mged_default(config) == "g"} {
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
		return [help gui]
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
		return [help gui]
	    }
	} elseif {$arg == "-d" || $arg == "-display"} {
	    # display string for everything
	    incr i

	    if {$i >= $argc} {
		return [help gui]
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
		return [help gui]
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
		return [help gui]
	    }

	    set dtype [lindex $args $i]
	    set dtype_set 1
	} elseif {$arg == "-id"} {
	    incr i

	    if {$i >= $argc} {
		return [help gui]
	    }

	    set id [lindex $args $i]
	} elseif {$arg == "-j" || $arg == "-join"} {
	    set join_c 1
	} elseif {$arg == "-h" || $arg == "-help"} {
	    return [help gui]
	} elseif {$arg == "-s" || $arg == "-sep"} {
	    set comb 0
	} elseif {$arg == "-c" || $arg == "-comb"} {
	    set comb 1
	} else {
	    return [help gui]
	}
    }

    if {$comb} {
	set gscreen $screen
    }

    if {$id == "mged"} {
	return "gui: not allowed to use \"mged\" as id"
    }

    if {$id == ""} {
	for {set i 0} {1} {incr i} {
	    set id [subst $mged_default(id)]_$i
	    if {[lsearch -exact $mged_players $id] == -1} {
		break;
	    }
	}
    }

    if {$scw == 0 && $sgw == 0} {
	set sgw 1
    }

    set mged_gui($id,comb) $comb
    set mged_gui($id,show_cmd) $scw
    set mged_gui($id,show_dm) $sgw
    set mged_gui($id,show_status) $mged_default(status_bar)
    set mged_gui($id,apply_to) 0
    set mged_gui($id,edit_info_pos) "+0+0"
    set mged_gui($id,num_lines) $mged_default(num_lines)
    set mged_gui($id,multi_pane) $mged_default(multi_pane)
    set mged_gui($id,dm_loc) $mged_default(pane)
    set mged_gui($id,dtype) $dtype
    set mged_gui($id,lastButtonPress) 0
    set mged_gui($id,lastItem) ""

    if {![dm_validXType $gscreen $dtype]} {
	set dtype [dm_bestXType $gscreen]
    }

    if { [info exists tk_strictMotif] == 0 } {
	loadtk $screen
    }

    #==============================================================================
    # PHASE 1: Creation of main window
    #==============================================================================
    if {[ winfo exists .$id ]} {
	return "A session with the id \"$id\" already exists!"
    }

    toplevel .$id -screen $screen -menu .$id.menubar

    lappend mged_players $id
    set mged_gui($id,screen) $screen

    #==============================================================================
    # Create display manager window and menu
    #==============================================================================
    if {$comb} {
	set mged_gui($id,top) .$id
	set mged_gui($id,dmc) .$id.dmf

	frame $mged_gui($id,dmc) -relief sunken -borderwidth 2

	if {[catch { openmv $id $mged_gui($id,top) $mged_gui($id,dmc) $screen $dtype } result]} {
	    gui_destroy $id
	    return $result
	}
    } else {
	set mged_gui($id,top) .top$id
	set mged_gui($id,dmc) $mged_gui($id,top)

	toplevel $mged_gui($id,dmc) -screen $gscreen -relief sunken -borderwidth 2

	if {[catch { openmv $id $mged_gui($id,top) $mged_gui($id,dmc) $gscreen $dtype } result]} {
	    gui_destroy $id
	    return $result
	}
    }

    set mged_gui($id,active_dm) $mged_gui($id,top).$mged_default(pane)
    set mged_gui($id,apply_list) $mged_gui($id,active_dm)

    #==============================================================================
    # PHASE 2: Construction of menu bar
    #==============================================================================
    menu .$id.menubar -tearoff $mged_default(tearoff_menus)
    .$id.menubar add cascade -label "File" -underline 0 -menu .$id.menubar.file
    .$id.menubar add cascade -label "Edit" -underline 0 -menu .$id.menubar.edit
    .$id.menubar add cascade -label "Create" -underline 0 -menu .$id.menubar.create
    .$id.menubar add cascade -label "View" -underline 0 -menu .$id.menubar.view
    .$id.menubar add cascade -label "ViewRing" -underline 4 -menu .$id.menubar.viewring
    .$id.menubar add cascade -label "Settings" -underline 0 -menu .$id.menubar.settings
    .$id.menubar add cascade -label "Modes" -underline 0 -menu .$id.menubar.modes
    .$id.menubar add cascade -label "Misc" -underline 1 -menu .$id.menubar.misc
    .$id.menubar add cascade -label "Tools" -underline 0 -menu .$id.menubar.tools
    .$id.menubar add cascade -label "Help" -underline 0 -menu .$id.menubar.help

    menu .$id.menubar.file -title "File" -tearoff $mged_default(tearoff_menus)
    .$id.menubar.file add command -label "New..." -underline 0 -command "do_New $id"
    hoc_register_menu_data "File" "New..." "New Database"\
	{ { summary "Open a new database. Note - the database\nmust not already exist." }
	    { see_also opendb} }
    .$id.menubar.file add command -label "Open..." -underline 0 -command "do_Open $id"
    hoc_register_menu_data "File" "Open..." "Open Database"\
	{ { summary "Open an existing database." }
	    { see_also opendb } }
    .$id.menubar.file add separator
    .$id.menubar.file add cascade -label "Import" -underline 0 -menu .$id.menubar.file.import
    .$id.menubar.file add cascade -label "Export" -underline 0 -menu .$id.menubar.file.export

    .$id.menubar.file add separator
    .$id.menubar.file add command -label "Load Script..." -underline 0 \
	-command "do_LoadScript $id"
    hoc_register_menu_data "File" "Load Script..." "Load Script"\
	{ { summary "Browse directories for a Tcl script file to load." } { see_also "loadview, source" } }
    .$id.menubar.file add separator
    .$id.menubar.file add command -label "Raytrace" -underline 0 -command "init_Raytrace $id"
    hoc_register_menu_data "File" "Raytrace" "Raytrace View"\
	{ { summary "Tool for raytracing the current view." }
	    { see_also rt } }
    .$id.menubar.file add cascade -label "Render View" -menu .$id.menubar.file.renderview
    .$id.menubar.file add separator
    .$id.menubar.file add cascade -label "Preferences" -underline 0 -menu .$id.menubar.file.pref
    .$id.menubar.file add separator
    .$id.menubar.file add command -label "Create/Update .mgedrc" -underline 0 \
	-command "update_mgedrc"
    hoc_register_menu_data "File" "Create/Update .mgedrc" "Create/Update .mgedrc"\
	{ { summary "Create the .mgedrc startup file with default variable settings, or update to current settings." }
	    { see_also } }
    .$id.menubar.file add command -label "Clear Command Window" -underline 14 \
	-command ".$id.t delete 1.0 end; mged_print_prompt .$id.t {mged> }; .$id.t insert insert \" \"; beginning_of_line .$id.t; .$id.t edit reset;"
    hoc_register_menu_data "File" "Clear Command Window" "Delete all text from command window"\
	{ { summary "Delete all text from command window" } see_also }
    .$id.menubar.file add command -label "Exit" -underline 1 -command _mged_quit
    hoc_register_menu_data "File" "Exit" "Exit MGED"\
	{ { summary "Exit MGED." }
	    { see_also "exit q quit" } }

    menu .$id.menubar.file.import -title "Import" -tearoff $mged_default(tearoff_menus)
    .$id.menubar.file.import add command -label "Ascii Database" -underline 0 -command "init_asc2g $id"
    hoc_register_menu_data "Import" "Ascii Database" "g2asc Ascii Database" { { summary "Import a database in ascii format using asc2g" } { see_also asc2g } }
    .$id.menubar.file.import add command -label "Binary Database" -underline 0 -command "do_Concat $id"
    hoc_register_menu_data "Import" "Insert Database" "Insert Database"	{ { summary "Insert another database into the current database." }	{ see_also dbconcat } }

    menu .$id.menubar.file.export -title "Export" -tearoff $mged_default(tearoff_menus)
    .$id.menubar.file.export add command -label "Ascii Database" -underline 0 -command "init_g2asc $id"
    hoc_register_menu_data "Export" "Ascii Database" "g2asc Ascii Database" { { summary "Export the current database in ascii format using g2asc" } { see_also g2asc } }
    .$id.menubar.file.export add command -label "Database Objects" -underline 0 -command "init_extractTool $id"
    hoc_register_menu_data "Export" "Extract Objects" "Extract Objects" { { summary "Tool for extracting objects out of the current database." } { see_also keep } }

    menu .$id.menubar.file.renderview -title "Render View" -tearoff $mged_default(tearoff_menus)
    .$id.menubar.file.renderview add command -label "RT Script..." -underline 0\
	-command "init_rtScriptTool $id"
    hoc_register_menu_data "Render View" "RT Script..." "RT Script File"\
	{ { summary "Save the current view as an RT script file." }
	    { see_also saveview } }
    .$id.menubar.file.renderview add command -label "Plot..." -underline 1\
	-command "init_plotTool $id"
    hoc_register_menu_data "Render View As" "Plot..." "Plot File"\
	{ { summary "Render the current view to a Plot file." }
	    { see_also pl } }
    .$id.menubar.file.renderview add command -label "PostScript..." -underline 0\
	-command "init_psTool $id"
    hoc_register_menu_data "Render View As" "PostScript..." "PostScript File"\
	{ { summary "Render the current view to a PostScript file." }
	    { see_also ps } }

    menu .$id.menubar.file.pref -title "Preferences" -tearoff $mged_default(tearoff_menus)
    .$id.menubar.file.pref add cascade -label "Units" -underline 0\
	-menu .$id.menubar.file.pref.units
    .$id.menubar.file.pref add cascade -label "Command Line Edit" -underline 13\
	-menu .$id.menubar.file.pref.cle
    .$id.menubar.file.pref add cascade -label "Special Characters" -underline 0\
	-menu .$id.menubar.file.pref.special_chars
    .$id.menubar.file.pref add command -label "Color Schemes" -underline 0\
	-command "color_scheme_build $id \"Color Schemes\" [list $mged_color_scheme(primary_map)]\
			\"Faceplate Colors\" [list $mged_color_scheme(secondary_map)]"
    hoc_register_menu_data "Preferences" "Color Schemes" "Color Schemes"\
	{ { summary "Tool for setting colors." }
	    { see_also "rset" } }
    .$id.menubar.file.pref add command -label "Fonts" -underline 0 \
	-command "font_scheme_init $id"
    hoc_register_menu_data "Preferences" "Fonts" "Fonts" \
	{ { summary "Tool for creating/configuring named fonts." }
	    { see_also "font" } }

    menu .$id.menubar.file.pref.units -title "Units" -tearoff $mged_default(tearoff_menus)
    .$id.menubar.file.pref.units add radiobutton -value um -variable mged_display(units)\
	-label "Micrometers" -underline 4 -command "do_Units $id"
    hoc_register_menu_data "Units" "Micrometers" "Unit of Measure - Micrometers"\
	{ { summary "Set the unit of measure to micrometers.\n1 micrometer = 1/1,000,000 meters" }
	    { see_also "units" } }
    .$id.menubar.file.pref.units add radiobutton -value mm -variable mged_display(units)\
	-label "Millimeters" -underline 2 -command "do_Units $id"
    hoc_register_menu_data "Units" "Millimeters" "Unit of Measure - Millimeters"\
	{ { summary "Set the unit of measure to millimeters.\n1 millimeter = 1/1000 meters" }
	    { see_also "units" } }
    .$id.menubar.file.pref.units add radiobutton -value cm -variable mged_display(units)\
	-label "Centimeters" -underline 0 -command "do_Units $id"
    hoc_register_menu_data "Units" "Centimeters" "Unit of Measure - Centimeters"\
	{ { summary "Set the unit of measure to centimeters.\n\t1 centimeter = 1/100 meters" }
	    { see_also "units" } }
    .$id.menubar.file.pref.units add radiobutton -value m -variable mged_display(units)\
	-label "Meters" -underline 0 -command "do_Units $id"
    hoc_register_menu_data "Units" "Meters" "Unit of Measure - Meters"\
	{ { summary "Set the unit of measure to meters." }
	    { see_also "units" } }
    .$id.menubar.file.pref.units add radiobutton -value km -variable mged_display(units)\
	-label "Kilometers" -underline 0 -command "do_Units $id"
    hoc_register_menu_data "Units" "Kilometers" "Unit of Measure - Kilometers"\
	{ { summary "Set the unit of measure to kilometers.\n 1 kilometer = 1000 meters" }
	    { see_also "units" } }
    .$id.menubar.file.pref.units add separator
    .$id.menubar.file.pref.units add radiobutton -value in -variable mged_display(units)\
	-label "Inches" -underline 0 -command "do_Units $id"
    hoc_register_menu_data "Units" "Inches" "Unit of Measure - Inches"\
	{ { summary "Set the unit of measure to inches.\n 1 inch = 25.4 mm" }
	    { see_also "units" } }
    .$id.menubar.file.pref.units add radiobutton -value ft -variable mged_display(units)\
	-label "Feet" -underline 0 -command "do_Units $id"
    hoc_register_menu_data "Units" "Feet" "Unit of Measure - Feet"\
	{ { summary "Set the unit of measure to feet.\n 1 foot = 12 inches" }
	    { see_also "units" } }
    .$id.menubar.file.pref.units add radiobutton -value yd -variable mged_display(units)\
	-label "Yards" -underline 0 -command "do_Units $id"
    hoc_register_menu_data "Units" "Yards" "Unit of Measure - Yards"\
	{ { summary "Set the unit of measure to yards.\n 1 yard = 36 inches" }
	    { see_also "" } }
    .$id.menubar.file.pref.units add radiobutton -value mi -variable mged_display(units)\
	-label "Miles" -underline 3 -command "do_Units $id"
    hoc_register_menu_data "Units" "Miles" "Unit of Measure - Miles"\
	{ { summary "Set the unit of measure to miles.\n 1 mile = 5280 feet" }
	    { see_also "units" } }

    menu .$id.menubar.file.pref.cle -title "Command Line Edit" -tearoff $mged_default(tearoff_menus)
    .$id.menubar.file.pref.cle add radiobutton -value emacs -variable mged_gui($id,edit_style)\
	-label "Emacs" -underline 0 -command "set_text_key_bindings $id"
    hoc_register_menu_data "Command Line Edit" "Emacs" "Command Line Edit - Emacs"\
	{ { synopsis "Set the command line edit mode to emacs." }
	    { description "
	\tBackSpace\tbackward delete char
	\tDelete\t\tbackward delete char
	\tLeft\t\tbackward char
	\tRight\t\tforward char
	\tUp\t\tprevious command
	\tDown\t\tnext command
	\tHome\t\tbeginning of line
	\tEnd\t\tend of line
	\tControl-a\tbeginning of line
	\tControl-b\tbackward char
	\tControl-c\tinterrupt command
	\tControl-d\tdelete char
	\tControl-e\tend of line
	\tControl-f\t\tforward char
	\tControl-h\tbackward delete char
	\tControl-k\tdelete end of line
	\tControl-n\tnext command
	\tControl-p\tprevious command
	\tControl-t\t\ttranspose
	\tControl-u\tdelete line
	\tControl-w\tbackward delete word" }
	}
    .$id.menubar.file.pref.cle add radiobutton -value vi -variable mged_gui($id,edit_style)\
	-label "Vi" -underline 0 -command "set_text_key_bindings $id"
    hoc_register_menu_data "Command Line Edit" "Vi" "Command Line Edit - Vi"\
	{ { synopsis "Set the command line edit mode to vi." }
	    { description "
	\t************ VI Insert Mode ************
	\tEscape\t\tcommand
	\tLeft\t\tbackward char, command
	\tRight\t\tforward char, command
	\tBackSpace\tbackward delete char

	\t************ VI Command Mode ************
	\tLeft\t\tbackward char
	\tRight\t\tforward char
	\tBackSpace\tbackward char
	\tSpace\t\tforward char
	\tA\t\tend of line, insert (i.e. append to end of line)
	\tC\t\tdelete to end of line, insert
	\tD\t\tdelete to end of line
	\tF\t\tsearch backward char
	\tI\t\tbeginning of line, insert
	\tR\t\toverwrite
	\tX\t\tbackward delete char
	\t0\t\tbeginning of line
	\t$\t\tend of line
	\t;\t\tcontinue search in same direction
	\t,\t\tcontinue search in opposite direction
	\ta\t\tforward char, insert (i.e. append)
	\tb\t\tbackward word
	\tc\t\tchange
	\td\t\tdelete
	\te\t\tend of word
	\tf\t\tsearch forward char
	\th\t\tbackward char
	\ti\t\tinsert
	\tj\t\tnext command
	\tk\t\tprevious command
	\tl\t\tforward char
	\tr\t\treplace char
	\ts\t\tdelete char, insert
	\tw\t\tforward word
	\tx\t\tdelete char

	\t************ Both ************
	\tDelete\t\tbackward delete char
	\tUp\t\tprevious command
	\tDown\t\tnext command
	\tHome\t\tbeginning of line
	\tEnd\t\tend of line
	\tControl-a\tbeginning of line
	\tControl-b\tbackward char
	\tControl-c\tinterrupt command
	\tControl-e\tend of line
	\tControl-f\t\tforward char
	\tControl-h\tbackward delete char
	\tControl-k\tdelete end of line
	\tControl-n\tnext command
	\tControl-p\tprevious command
	\tControl-t\t\ttranspose
	\tControl-u\tdelete to beginning of line
	\tControl-w\tbackward delete word" }
	}

    menu .$id.menubar.file.pref.special_chars -title "Special Characters" -tearoff $mged_default(tearoff_menus)
    .$id.menubar.file.pref.special_chars add radiobutton -value 0 -variable glob_compat_mode\
	-label "Tcl Evaluation" -underline 0
    hoc_register_menu_data "Special Characters" "Tcl Evaluation" "Tcl Evaluation"\
	{ { summary "Set the command interpretation mode to Tcl mode.
In this mode, globbing is not performed against
MGED database objects. Rather, the command string
is passed, unmodified, to the Tcl interpreter." } }
    .$id.menubar.file.pref.special_chars add radiobutton -value 1 -variable glob_compat_mode\
	-label "Object Name Matching" -underline 0
    hoc_register_menu_data "Special Characters" "Object Name Matching" "Object Name Matching"\
	{ { summary "Set the command interpretation mode to MGED object
name matching. In this mode, globbing is performed
against MGED database objects."\
	    } }

    menu .$id.menubar.edit -title "Edit" -tearoff $mged_default(tearoff_menus)
    .$id.menubar.edit add command -label "Primitive Selection..." -underline 0 \
	-command "winset \$mged_gui($id,active_dm); build_edit_menu_all s1"
    hoc_register_menu_data "Edit" "Primitive Selection..." "Primitive Selection"\
	{ { summary "A tool for selecting a primitive to edit." } }
    .$id.menubar.edit add command -label "Matrix Selection..." -underline 0 \
	-command "winset \$mged_gui($id,active_dm); build_edit_menu_all o"
    hoc_register_menu_data "Edit" "Matrix Selection..." "Matrix Selection"\
	{ { summary "A tool for selecting a matrix to edit." } }
    .$id.menubar.edit add separator
    .$id.menubar.edit add command -label "Primitive Editor" -underline 6 \
	-command "init_edit_solid $id"
    hoc_register_menu_data "Edit" "Primitive Editor" "Primitive Editor"\
	{ { summary "A tool for editing/creating primitives." } }
    .$id.menubar.edit add command -label "Combination Editor" -underline 0 \
	-command "init_comb $id"
    hoc_register_menu_data "Edit" "Combination Editor" "Combination Editor"\
	{ { summary "A tool for editing/creating combinations." } }
    .$id.menubar.edit add command -label "Attribute Editor" -underline 0 \
	-command "Attr_editor::start_editor $id"
    .$id.menubar.edit add command -label "Browse Geometry" -underline 0 -command "geometree"

    menu .$id.menubar.create -title "Create" -tearoff $mged_default(tearoff_menus)

    .$id.menubar.create add cascade\
	-label "Arbs" -underline 0 -menu .$id.menubar.create.arb8
    .$id.menubar.create add cascade\
	-label "Cones & Cylinders" -underline 0 -menu .$id.menubar.create.cones
    .$id.menubar.create add cascade\
	-label "Ellipsoids" -underline 0 -menu .$id.menubar.create.ell
    .$id.menubar.create add separator

    menu .$id.menubar.create.arb8 -title "Arb8" -tearoff $mged_default(tearoff_menus)
    menu .$id.menubar.create.cones -title "Cones & Cyls" -tearoff $mged_default(tearoff_menus)
    menu .$id.menubar.create.ell -title "Ellipses" -tearoff $mged_default(tearoff_menus)

    # populate Arb8 menu
    foreach ptype $mged_Priv(arb8) {
	.$id.menubar.create.arb8 add command -label "$ptype..."\
	    -command "init_solid_create $id $ptype"

	set ksl {}
	lappend ksl "summary \"Make an $ptype.\"" "see_also \"make, in\""
	hoc_register_menu_data "Arb8" "$ptype..." "Make an $ptype" $ksl
    }

    .$id.menubar.create.arb8 add separator

    # separate the arbn from the other arb representations
    set ptype arbn

    .$id.menubar.create.arb8 add command -foreground firebrick4 -label "$ptype..."\
	-command "init_solid_create $id $ptype"
    set ksl {}
    lappend ksl "summary \"Make an $ptype (limited edit capabilities).\"" "see_also \"make, in\""
    hoc_register_menu_data "Arb8" "$ptype..." "Make an $ptype" $ksl


    # populate "Cones & Cyls" menu
    foreach ptype $mged_Priv(cones) {
	.$id.menubar.create.cones add command -label "$ptype..."\
	    -command "init_solid_create $id $ptype"

	set ksl {}
	lappend ksl "summary \"Make a $ptype.\"" "see_also \"make, in\""
	hoc_register_menu_data "Cones & Cyls" "$ptype..." "Make a $ptype" $ksl
    }

    # populate Ellipses menu
    foreach ptype $mged_Priv(ellipsoids) {
	.$id.menubar.create.ell add command -label "$ptype..."\
	    -command "init_solid_create $id $ptype"

	set ksl {}
	lappend ksl "summary \"Make a $ptype.\"" "see_also \"make, in\""
	hoc_register_menu_data "Ellipses" "$ptype..." "Make a $ptype" $ksl
    }

    # populate the Create menu with other_prims
    foreach ptype $mged_Priv(other_prims) {
	if {$ptype == "dsp"} {
	    .$id.menubar.create add command -label "dsp..."\
		-command "dsp_create $id"

	    set ksl {}
	    lappend ksl "summary \"Make a $ptype.\"" "see_also \"make, in\""
	    hoc_register_menu_data "Create" "dsp..." "Make a dsp solid" $ksl
	} else {
	    .$id.menubar.create add command -label "$ptype..."\
		-command "init_solid_create $id $ptype"

	    set ksl {}
	    lappend ksl "summary \"Make a $ptype.\"" "see_also \"make, in\""
	    hoc_register_menu_data "Create" "$ptype..." "Make a $ptype" $ksl
	}
    }

    # separate weak prims from other entries
    .$id.menubar.create add separator

    # populate the remainder of the Create menu with weak_prims
    foreach ptype $mged_Priv(weak_prims) {
	.$id.menubar.create add command -foreground firebrick4 -label "$ptype..."\
	    -command "init_solid_create $id $ptype"

	set ksl {}
	lappend ksl "summary \"Make a $ptype (limited edit capability).\"" "see_also \"make, in\""
	hoc_register_menu_data "Create" "$ptype..." "Make a $ptype" $ksl
    }

# separate weak primitives from non-geometric 'binary' objects
.$id.menubar.create add separator

# binary object
set ptype "binunif"
.$id.menubar.create add command -label "$ptype..."\
    -command "binunif_create $id"

set ksl {}
lappend ksl "summary \"Make a $ptype.\"" "see_also \"make, in\""
hoc_register_menu_data "Create" "$ptype..." "Make a $ptype" $ksl

    menu .$id.menubar.view -title "View" -tearoff $mged_default(tearoff_menus)
    .$id.menubar.view add command -label "Top (t)" -underline 5\
	-command "mged_apply $id \"press top\""
    hoc_register_menu_data "View" "Top" "Top View"\
	{ { summary "View of the top (i.e. azimuth = 270, elevation = 90)." }
	    { accelerator "t" }
	    { see_also "press, ae, view" } }
    .$id.menubar.view add command -label "Bottom (b)" -underline 8\
	-command "mged_apply $id \"press bottom\""
    hoc_register_menu_data "View" "Bottom" "Bottom View"\
	{ { summary "View of the bottom (i.e. azimuth = 270 , elevation = -90)." }
	    { accelerator "b" }
	    { see_also "press, ae, view" } }
    .$id.menubar.view add command -label "Right (r)" -underline 7\
	-command "mged_apply $id \"press right\""
    hoc_register_menu_data "View" "Right" "Right View"\
	{ { summary "View of the right side (i.e. azimuth = 270, elevation = 0)." }
	    { accelerator "r" }
	    { see_also "press, ae, view" } }
    .$id.menubar.view add command -label "Left (l)" -underline 6\
	-command "mged_apply $id \"press left\""
    hoc_register_menu_data "View" "Left" "Left View"\
	{ { summary "View of the left side (i.e. azimuth = 90, elevation = 0)." }
	    { accelerator "l" }
	    { see_also "press, ae, view" } }
    .$id.menubar.view add command -label "Front (f)" -underline 7\
	-command "mged_apply $id \"press front\""
    hoc_register_menu_data "View" "Front" "Front View"\
	{ { summary "View of the front (i.e. azimuth = 0, elevation = 0)." }
	    { accelerator "f" }
	    { see_also "press, ae, view" } }
    .$id.menubar.view add command -label "Rear (R)" -underline 6\
	-command "mged_apply $id \"press rear\""
    hoc_register_menu_data "View" "Rear" "Rear View"\
	{ { summary "View of the rear (i.e. azimuth = 180, elevation = 0)." }
	    { accelerator "R" }
	    { see_also "press, ae, view" } }
    .$id.menubar.view add command -label "az35,el25" -underline 2\
	-command "mged_apply $id \"press 35,25\""
    hoc_register_menu_data "View" "az35,el25" "View - 35,25"\
	{ { summary "View with an azimuth of 35 and an elevation of 25." }
	    { accelerator "3" }
	    { see_also "press, ae, view" } }
    .$id.menubar.view add command -label "az45,el45" -underline 2\
	-command "mged_apply $id \"press 45,45\""
    hoc_register_menu_data "View" "az45,el45" "View - 45,45"\
	{ { summary "View with an azimuth of 45 and an elevation of 45." }
	    { accelerator "4" }
	    { see_also "press, ae, view" } }
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
	    { see_also "press, ae, view" } }
    .$id.menubar.view add command -label "Multipane Defaults"\
	-underline 0 -command "set_default_views $id"
    hoc_register_menu_data "View" "Multipane Defaults" "Multipane Default Views"\
	{ { summary "Sets the view of all four panes to their defaults.\n
\tupper left\taz = 90, el = 0
\tupper right\taz = 35, el = 25
\tlower left\taz = 0, el = 0
\tlower right\taz = 90, el = 0" }
	    { see_also "press, ae, view" } }
    .$id.menubar.view add command -label "Zero" -underline 0\
	-command "mged_apply $id \"knob zero\""
    hoc_register_menu_data "View" "Zero" "Zero Knobs"\
	{ { summary "Stop all rate transformations." }
	    { accelerator "0" }
	    { see_also "knob, press" } }

    menu .$id.menubar.viewring -title "ViewRing" -tearoff $mged_default(tearoff_menus)
    .$id.menubar.viewring add command -label "Add View" -underline 0 -command "view_ring_add $id"
    hoc_register_menu_data "ViewRing" "Add View" "Add View"\
	{ { synopsis "Add a view to the view ring." }
	    { description "A view ring is a mechanism for managing multiple
	views within a single pane or display manager. Each pane
	has its own view ring where any number of views can be stored.
	The stored views can be removed or traversed." } }
    .$id.menubar.viewring add cascade -label "Select View" -underline 0 -menu .$id.menubar.viewring.select
    .$id.menubar.viewring add cascade -label "Delete View" -underline 0 -menu .$id.menubar.viewring.delete
    .$id.menubar.viewring add command -label "Next View" -underline 0 -command "view_ring_next $id"
    hoc_register_menu_data "ViewRing" "Next View" "Next View"\
	{ { synopsis "Go to the next view on the view ring." }
	    { accelerator "Control-n" } }
    .$id.menubar.viewring add command -label "Prev View" -underline 0 -command "view_ring_prev $id"
    hoc_register_menu_data "ViewRing" "Prev View" "Previous View"\
	{ { synopsis "Go to the previous view on the view ring." }
	    { accelerator "Control-p" } }
    .$id.menubar.viewring add command -label "Last View" -underline 0 -command "view_ring_toggle $id"
    hoc_register_menu_data "ViewRing" "Last View" "Last View"\
	{ { synopsis "Go to the last view. This can be used to toggle
	between two views." }
	    { accelerator "Control-t" } }

    #menu .$id.menubar.viewring.select -title "Select View" -tearoff $mged_default(tearoff_menus)\
	-postcommand "update_view_ring_labels $id"
    menu .$id.menubar.viewring.select -title "Select View" -tearoff $mged_default(tearoff_menus) \
	-postcommand "view_ring_save_curr $id"

    update_view_ring_entries $id s

    set mged_gui($id,views) ""
    set view_ring($id) 0
    set view_ring($id,curr) 0
    set view_ring($id,prev) 0
    #menu .$id.menubar.viewring.delete -title "Delete View" -tearoff $mged_default(tearoff_menus)\
	-postcommand "update_view_ring_labels $id"
    menu .$id.menubar.viewring.delete -title "Delete View" -tearoff $mged_default(tearoff_menus)

    update_view_ring_entries $id d

    menu .$id.menubar.settings -title "Settings" -tearoff $mged_default(tearoff_menus)
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
    .$id.menubar.settings add cascade -label "View Axes Position" -underline 0\
	-menu .$id.menubar.settings.vap

    menu .$id.menubar.settings.applyTo -title "Apply To" -tearoff $mged_default(tearoff_menus)
    .$id.menubar.settings.applyTo add radiobutton -value 0 -variable mged_gui($id,apply_to)\
	-label "Active Pane" -underline 0
    hoc_register_menu_data "Apply To" "Active Pane" "Active Pane"\
	{ { summary "Set the \"Apply To\" mode such that the user's
	interaction with the GUI is applied to the active pane." } }
    .$id.menubar.settings.applyTo add radiobutton -value 1 -variable mged_gui($id,apply_to)\
	-label "Local Panes" -underline 0
    hoc_register_menu_data "Apply To" "Local Panes" "Local Panes"\
	{ { summary "Set the \"Apply To\" mode such that the user's
	interaction with the GUI is applied to all panes
	local to this instance of the GUI." } }
    .$id.menubar.settings.applyTo add radiobutton -value 2 -variable mged_gui($id,apply_to)\
	-label "Listed Panes" -underline 1
    hoc_register_menu_data "Apply To" "Listed Panes" "Listed Panes"\
	{ { summary "Set the \"Apply To\" mode such that the user's
	interaction with the GUI is applied to all panes
	listed in the Tcl variable mged_gui(id,apply_list)
	(Note - id refers to the GUI's id)." } }
    .$id.menubar.settings.applyTo add radiobutton -value 3 -variable mged_gui($id,apply_to)\
	-label "All Panes" -underline 4
    hoc_register_menu_data "Apply To" "All Panes" "All Panes"\
	{ { summary "Set the \"Apply To\" mode such that the user's
	interaction with the GUI is applied to all panes." } }

    menu .$id.menubar.settings.mouse_behavior -title "Mouse Behavior" -tearoff $mged_default(tearoff_menus)
    .$id.menubar.settings.mouse_behavior add radiobutton -value d -variable mged_gui($id,mouse_behavior)\
	-label "Default" -underline 0\
	-command "set_mouse_behavior $id"
    hoc_register_menu_data "Mouse Behavior" "Default" "Default Mouse Behavior"\
	{ { synopsis "Enter the default MGED mouse behavior mode." }
	    { description "In this mode, the user gets mouse behavior that is
	the same as MGED 4.5 and earlier. See the table below.\n\n
	\tMouse Button\t\t\tBehavior
	\t\t1\t\tZoom out by a factor of 2
	\t\t2\t\tCenter View
	\t\t3\t\tZoom in by a factor of 2" }
	    { see_also "rset, vars" } }
    .$id.menubar.settings.mouse_behavior add separator
    .$id.menubar.settings.mouse_behavior add radiobutton -value s -variable mged_gui($id,mouse_behavior)\
	-label "Pick Edit-Primitive" -underline 10\
	-command "set_mouse_behavior $id"
    hoc_register_menu_data "Mouse Behavior" "Pick Edit-Primitive" "Pick Edit-Primitive"\
	{ { synopsis "Enter pick edit-solid mode." }
	    { description "In this mode, the mouse is used to fire rays for selecting
	a solid to edit. If more than one solid is hit, a listbox of the hit
	solids is presented. The user then selects a solid to edit from
	this listbox. If a single solid is hit, it is selected for editing.
	If no solids were hit, a dialog is popped up saying that nothing
	was hit. The user must then fire another ray to continue selecting
	a solid. When a solid is finally selected, solid edit mode is
	entered. When this happens, the mouse behavior mode is set to
	default mode. Note - When selecting items from a listbox, a left
	buttonpress highlights the solid in question until the button is
	released. To select a solid, double click with the left mouse button.\n
	\tMouse Button\t\t\tBehavior
	\t\t1\t\tZoom out by a factor of 2
	\t\t2\t\tFire edit-solid ray
	\t\t3\t\tZoom in by a factor of 2" }
	    { see_also "nirt, qray, rset, sed, vars" } }
    .$id.menubar.settings.mouse_behavior add radiobutton -value m -variable mged_gui($id,mouse_behavior)\
	-label "Pick Edit-Matrix" -underline 10\
	-command "set_mouse_behavior $id"
    hoc_register_menu_data "Mouse Behavior" "Pick Edit-Matrix" "Pick Edit-Matrix"\
	{ { synopsis "Enter pick edit-matrix mode." }
	    { description "In this mode, the mouse is used to fire rays for selecting
	a matrix to edit. If more than one solid is hit, a listbox of the
	hit solids is presented. The user then selects a solid
	from this listbox. If a single solid is hit, that solid is selected.
	If no solids were hit, a dialog is popped up saying that nothing was hit.
	The user must then fire another ray to continue selecting a matrix to edit.
	When a solid is finally selected, the user is presented with a listbox consisting
	of the path components of the selected solid. From this lisbox, the
	user selects a path component. This component determines which
	matrix will be edited. After selecting the path component, object/matrix
	edit mode is entered. When this happens, the mouse behavior mode
	is set to default mode. Note - When selecting items
	from a listbox, a left buttonpress highlights the solid/matrix in question
	until the button is released. To select a solid/matrix, double click with
	the left mouse button.\n
	\tMouse Button\t\t\tBehavior
	\t\t1\t\tZoom out by a factor of 2
	\t\t2\t\tFire edit-matrix ray
	\t\t3\t\tZoom in by a factor of 2" }
	    { see_also "rset, vars" } }
    .$id.menubar.settings.mouse_behavior add radiobutton -value c -variable mged_gui($id,mouse_behavior)\
	-label "Pick Edit-Combination" -underline 10\
	-command "set_mouse_behavior $id"
    hoc_register_menu_data "Mouse Behavior" "Pick Edit-Combination" "Pick Edit-Combination"\
	{ { synopsis "Enter pick edit-combination mode." }
	    { description "In this mode, the mouse is used to fire rays for selecting
	a combination to edit. If more than one combination is hit, a listbox of the
	hit combinations is presented. The user then selects a combination
	from this menu. If a single combination is hit, that combination is selected.
	If no combinations were hit, a dialog is popped up saying that nothing was hit.
	The user must then fire another ray to continue selecting a combination to edit.
	When a combination is finally selected, the combination edit tool is presented
	and initialized with the values of the selected combination. When this happens,
	the mouse behavior mode is set to default mode. Note - When selecting items
	from a menu, a left buttonpress highlights the combination in question
	until the button is released. To select a combination, double click with
	the left mouse button.\n
	\tMouse Button\t\t\tBehavior
	\t\t1\t\tZoom out by a factor of 2
	\t\t2\t\tFire edit-combination ray
	\t\t3\t\tZoom in by a factor of 2" }
	    { see_also "rset, vars" } }
    .$id.menubar.settings.mouse_behavior add separator
    .$id.menubar.settings.mouse_behavior add radiobutton -value r -variable mged_gui($id,mouse_behavior)\
	-label "Sweep Raytrace-Rectangle" -underline 6\
	-command "set_mouse_behavior $id"
    hoc_register_menu_data "Mouse Behavior" "Sweep Raytrace-Rectangle" "Sweep Raytrace-Rectangle"\
	{ { synopsis "Enter sweep raytrace-rectangle mode." }
	    { description "If the framebuffer is active, the rectangular area as
	specified by the user is raytraced. The rectangular area is
	also painted with the current contents of the framebuffer. Otherwise,
	only the rectangle is drawn.\n
	\tMouse Button\t\t\tBehavior
	\t\t1\t\tZoom out by a factor of 2
	\t\t2\t\tDraw raytrace-rectangle
	\t\t3\t\tZoom in by a factor of 2" }
	    { see_also "rset, vars" } }
    .$id.menubar.settings.mouse_behavior add radiobutton -value o -variable mged_gui($id,mouse_behavior)\
	-label "Pick Raytrace-Object(s)" -underline 14\
	-command "set_mouse_behavior $id"
    hoc_register_menu_data "Mouse Behavior" "Pick Raytrace-Object(s)" "Pick Raytrace-Object(s)"\
	{ { synopsis "Enter pick raytrace-object mode." }
	    { description "Pick an object for raytracing or for adding to the
	list of objects to be raytraced." }
	    { see_also "rset, vars" } }
    .$id.menubar.settings.mouse_behavior add separator
    .$id.menubar.settings.mouse_behavior add radiobutton -value q -variable mged_gui($id,mouse_behavior)\
	-label "Query Ray" -underline 0\
	-command "set_mouse_behavior $id"
    hoc_register_menu_data "Mouse Behavior" "Query Ray" "Query Ray"\
	{ { synopsis "Enter query ray mode." }
	    { description "In this mode, the mouse is used to fire rays. The data
	from the fired rays can be viewed textually, graphically
	or both.\n
	\tMouse Button\t\t\tBehavior
	\t\t1\t\tZoom out by a factor of 2
	\t\t2\t\tFire query ray
	\t\t3\t\tZoom in by a factor of 2" }
	    { see_also "nirt, qray, rset, vars" } }
    .$id.menubar.settings.mouse_behavior add radiobutton -value p -variable mged_gui($id,mouse_behavior)\
	-label "Sweep Paint-Rectangle" -underline 6\
	-command "set_mouse_behavior $id"
    hoc_register_menu_data "Mouse Behavior" "Sweep Paint-Rectangle" "Sweep Paint-Rectangle"\
	{ { synopsis "Enter sweep paint-rectangle mode." }
	    { description "If the framebuffer is active, the rectangular area
	as specified by the user is painted with the current contents of the
	framebuffer. Otherwise, only the rectangle is drawn.\n
	\tMouse Button\t\t\tBehavior
	\t\t1\t\tZoom out by a factor of 2
	\t\t2\t\tDraw paint rectangle
	\t\t3\t\tZoom in by a factor of 2" }
	    { see_also "rset, vars" } }
    .$id.menubar.settings.mouse_behavior add radiobutton -value z -variable mged_gui($id,mouse_behavior)\
	-label "Sweep Zoom-Rectangle" -underline 6\
	-command "set_mouse_behavior $id"
    hoc_register_menu_data "Mouse Behavior" "Sweep Zoom-Rectangle" "Sweep Zoom-Rectangle"\
	{ { synopsis "Enter sweep zoom-rectangle mode." }
	    { description "The rectangular area as specified by the user is used
	to zoom the view. Note - as the user stretches out the zoom
	rectangle, the rectangle is constrained to be the same shape as the
	window. This insures that the user gets what he or she sees.\n
	\tMouse Button\t\t\tBehavior
	\t\t1\t\tZoom out by a factor of 2
	\t\t2\t\tDraw zoom rectangle
	\t\t3\t\tZoom in by a factor of 2" }
	    { see_also "rset, vars" } }

    menu .$id.menubar.settings.qray -title "Query Ray Effects" -tearoff $mged_default(tearoff_menus)
    .$id.menubar.settings.qray add radiobutton -value t -variable mged_gui($id,qray_effects)\
	-label "Text" -underline 0\
	-command "mged_apply $id \"qray effects \$mged_gui($id,qray_effects)\""
    hoc_register_menu_data "Query Ray Effects" "Text" "Query Ray Effects - Text"\
	{ { summary "Set qray effects mode to 'text'. In this mode,
	only textual output is used to represent the results
	of firing a query ray." }
	    { see_also "qray" } }
    .$id.menubar.settings.qray add radiobutton -value g -variable mged_gui($id,qray_effects)\
	-label "Graphics" -underline 0\
	-command "mged_apply $id \"qray effects \$mged_gui($id,qray_effects)\""
    hoc_register_menu_data "Query Ray Effects" "Graphics" "Query Ray Effects - Graphics"\
	{ { summary "Set qray effects mode to 'graphics'. In this mode,
	only graphical output is used to represent the results
	of firing a query ray." }
	    { see_also "qray" } }
    .$id.menubar.settings.qray add radiobutton -value b -variable mged_gui($id,qray_effects)\
	-label "Both" -underline 0\
	-command "mged_apply $id \"qray effects \$mged_gui($id,qray_effects)\""
    hoc_register_menu_data "Query Ray Effects" "Both" "Query Ray Effects - Both"\
	{ { summary "Set qray effects mode to 'both'. In this mode,
	both textual and graphical output is used to
	represent the results of firing a query ray." }
	    { see_also "qray" } }

    set tmp_summary "Set the active pane to be the upper left pane. Any interaction with the GUI that affects a pane
	(display manager) will be directed at the upper left pane.

	The following is a list of default key bindings for panes:

	$mged_default(dm_key_bindings)"
    set tmp_hoc [list [list summary $tmp_summary]]
    menu .$id.menubar.settings.mpane -title "Active Pane" -tearoff $mged_default(tearoff_menus)
    .$id.menubar.settings.mpane add radiobutton -value ul -variable mged_gui($id,dm_loc)\
	-label "Upper Left" -underline 6\
	-command "set_active_dm $id"
    hoc_register_menu_data "Active Pane" "Upper Left" "Active Pane - Upper Left" $tmp_hoc

    set tmp_summary "Set the active pane to be the upper right pane. Any interaction with the GUI that affects a pane
	(display manager) will be directed at the upper right pane.

	The following is a list of default key bindings for panes:

	$mged_default(dm_key_bindings)"
    set tmp_hoc [list [list summary $tmp_summary]]
    .$id.menubar.settings.mpane add radiobutton -value ur -variable mged_gui($id,dm_loc)\
	-label "Upper Right" -underline 6\
	-command "set_active_dm $id"
    hoc_register_menu_data "Active Pane" "Upper Right" "Active Pane - Upper Right" $tmp_hoc

    set tmp_summary "Set the active pane to be the lower left pane. Any interaction with the GUI that affects a pane
	(display manager) will be directed at the lower left pane.

	The following is a list of default key bindings for panes:

	$mged_default(dm_key_bindings)"
    set tmp_hoc [list [list summary $tmp_summary]]
    .$id.menubar.settings.mpane add radiobutton -value ll -variable mged_gui($id,dm_loc)\
	-label "Lower Left" -underline 2\
	-command "set_active_dm $id"
    hoc_register_menu_data "Active Pane" "Lower Left" "Active Pane - Lower Left" $tmp_hoc

    set tmp_summary "Set the active pane to be the lower right pane. Any interaction with the GUI that affects a pane
	(display manager) will be directed at the lower right pane.

	The following is a list of default key bindings for panes:

	$mged_default(dm_key_bindings)"
    set tmp_hoc [list [list summary $tmp_summary]]
    .$id.menubar.settings.mpane add radiobutton -value lr -variable mged_gui($id,dm_loc)\
	-label "Lower Right" -underline 3\
	-command "set_active_dm $id"
    hoc_register_menu_data "Active Pane" "Lower Right" "Active Pane - Lower Right" $tmp_hoc

    menu .$id.menubar.settings.fb -title "Framebuffer" -tearoff $mged_default(tearoff_menus)
    .$id.menubar.settings.fb add radiobutton -value 1 -variable mged_gui($id,fb_all)\
	-label "All" -underline 0\
	-command "mged_apply $id \"set fb_all \$mged_gui($id,fb_all)\"; rt_update_dest $id"
    hoc_register_menu_data "Framebuffer" "All" "Framebuffer - All"\
	{ { summary "Use the entire pane for the framebuffer." }
	    { see_also "rset, vars" } }
    .$id.menubar.settings.fb add radiobutton -value 0 -variable mged_gui($id,fb_all)\
	-label "Rectangle Area" -underline 0\
	-command "mged_apply $id \"set fb_all \$mged_gui($id,fb_all)\"; rt_update_dest $id"
    hoc_register_menu_data "Framebuffer" "Rectangle Area" "Framebuffer - Rectangle Area"\
	{ { summary "Use the rectangle area for the framebuffer." }
	    { see_also "rset, vars" } }
    .$id.menubar.settings.fb add separator
    .$id.menubar.settings.fb add radiobutton -value 2 -variable mged_gui($id,fb_overlay)\
	-label "Overlay" -underline 0\
	-command "mged_apply $id \"set fb_overlay \$mged_gui($id,fb_overlay)\"; rt_update_dest $id"
    hoc_register_menu_data "Framebuffer" "Overlay" "Framebuffer - Overlay"\
	{ { summary "Put the framebuffer in overlay mode. In this mode,
	the framebuffer data is placed above everything.
	(i.e. above the geometry and faceplate" }
	    { see_also "rset, vars" } }
    .$id.menubar.settings.fb add radiobutton -value 1 -variable mged_gui($id,fb_overlay)\
	-label "Interlay" -underline 0\
	-command "mged_apply $id \"set fb_overlay \$mged_gui($id,fb_overlay)\"; rt_update_dest $id"
    hoc_register_menu_data "Framebuffer" "Interlay" "Framebuffer - Interlay"\
	{ { summary "Put the framebuffer in interlay mode. In this mode,
	the framebuffer data is placed above the geometry
	and below the faceplate." }
	    { see_also "rset, vars" } }
    .$id.menubar.settings.fb add radiobutton -value 0 -variable mged_gui($id,fb_overlay)\
	-label "Underlay" -underline 0\
	-command "mged_apply $id \"set fb_overlay \$mged_gui($id,fb_overlay)\"; rt_update_dest $id"
    hoc_register_menu_data "Framebuffer" "Underlay" "Framebuffer - Underlay"\
	{ { summary "Put the framebuffer in underlay mode. In this mode,
	the framebuffer data is placed under everything.
	(i.e. faceplate and geometry is drawn on top of
	the framebuffer data)." }
	    { see_also "rset, vars" } }
    .$id.menubar.settings.fb add separator
    .$id.menubar.settings.fb add checkbutton -offvalue 0 -onvalue 1 -variable mged_gui($id,fb)\
	-label "Framebuffer Active" -underline 0\
	-command "set_fb $id"
    hoc_register_menu_data "Framebuffer" "Framebuffer Active" "Framebuffer Active"\
	{ { summary "This activates/deactivates the framebuffer." }
	    { see_also "rset, vars" } }
    .$id.menubar.settings.fb add checkbutton -offvalue 0 -onvalue 1 -variable mged_gui($id,listen)\
	-label "Listen For Clients" -underline 0\
	-command "set_listen $id"
    hoc_register_menu_data "Framebuffer" "Listen For Clients" "Listen For Clients"\
	{ { summary "This activates/deactivates listening for clients.
	If the framebuffer is listening for clients, new
	data can be passed into the framebuffer. Otherwise,
	the framebuffer is write protected. Actually, it is
	also read protected. The one exception is that MGED
	can still read the framebuffer data." }
	    { see_also "rset, vars" } }

    menu .$id.menubar.settings.vap -title "View Axes Position" -tearoff $mged_default(tearoff_menus)
    .$id.menubar.settings.vap add radiobutton -value 0 -variable mged_gui($id,view_pos)\
	-label "Center" -underline 0\
	-command "mged_apply $id \"rset ax view_pos 0 0\""
    hoc_register_menu_data "View Axes Position" "Center" "View Axes Position - Center"\
	{ { summary "Place the view axes in the center of the active pane." }
	    { see_also "rset" } }
    .$id.menubar.settings.vap add radiobutton -value 1 -variable mged_gui($id,view_pos)\
	-label "Lower Left" -underline 2\
	-command "mged_apply $id \"rset ax view_pos -1750 -1750\""
    hoc_register_menu_data "View Axes Position" "Lower Left" "View Axes Position - Lower Left"\
	{ { summary "Place the view axes in the lower left corner of the active pane." }
	    { see_also "rset" } }
    .$id.menubar.settings.vap add radiobutton -value 2 -variable mged_gui($id,view_pos)\
	-label "Upper Left" -underline 6\
	-command "mged_apply $id \"rset ax view_pos -1750 1750\""
    hoc_register_menu_data "View Axes Position" "Upper Left" "View Axes Position - Upper Left"\
	{ { summary "Place the view axes in the upper left corner of the active pane." }
	    { see_also "rset" } }
    .$id.menubar.settings.vap add radiobutton -value 3 -variable mged_gui($id,view_pos)\
	-label "Upper Right" -underline 6\
	-command "mged_apply $id \"rset ax view_pos 1750 1750\""
    hoc_register_menu_data "View Axes Position" "Upper Right" "View Axes Position - Upper Right"\
	{ { summary "Place the view axes in the upper right corner of the active pane." }
	    { see_also "rset" } }
    .$id.menubar.settings.vap add radiobutton -value 4 -variable mged_gui($id,view_pos)\
	-label "Lower Right" -underline 3\
	-command "mged_apply $id \"rset ax view_pos 1750 -1750\""
    hoc_register_menu_data "View Axes Position" "Lower Right" "View Axes Position - Lower Right"\
	{ { summary "Place the view axes in the lower right corner of the active pane." }
	    { see_also "rset" } }

    menu .$id.menubar.settings.grid -title "Grid" -tearoff $mged_default(tearoff_menus)
    .$id.menubar.settings.grid add command -label "Anchor" -underline 0\
	-command "do_grid_anchor $id"
    hoc_register_menu_data "Grid" "Anchor" "Grid Anchor"\
	{ { summary "Pops up the grid anchor entry dialog." }
	    { see_also "rset" } }
    .$id.menubar.settings.grid add cascade -label "Spacing" -underline 1\
	-menu .$id.menubar.settings.grid.spacing
    .$id.menubar.settings.grid add separator
    .$id.menubar.settings.grid add checkbutton -offvalue 0 -onvalue 1 -variable mged_gui($id,grid_draw)\
	-label "Draw Grid" -underline 0\
	-command "mged_draw_grid $id"
    hoc_register_menu_data "Grid" "Draw Grid" "Draw Grid"\
	{ { summary "Toggle drawing the grid. The grid is a lattice
	of points over the pane (geometry window). The
	regular spacing between the points gives the user
	accurate visual cues regarding dimension. This
	spacing can be set by the user." }
	    { see_also "rset" } }
    .$id.menubar.settings.grid add checkbutton -offvalue 0 -onvalue 1 -variable mged_gui($id,draw_snap)\
	-label "Snap To Grid" -underline 0\
	-command "mged_apply $id \"rset grid snap \$mged_gui($id,draw_snap)\""
    hoc_register_menu_data "Grid" "Snap To Grid" "Snap To Grid"\
	{ { summary "Toggle grid snapping. When snapping to grid,
	the internal routines that use the mouse pointer
	location, move/snap that location to the nearest
	grid point. This gives the user high accuracy with
	the mouse for transforming the view or editing
	solids/matrices." }
	    { see_also "rset" } }

    menu .$id.menubar.settings.grid.spacing -title "Grid Spacing" -tearoff $mged_default(tearoff_menus)
    .$id.menubar.settings.grid.spacing add command -label "Autosize" -underline 0\
	-command "grid_spacing_autosize $id; grid_spacing_apply $id b"
    hoc_register_menu_data "Grid Spacing" "Autosize" "Grid Spacing - Autosize"\
	{ { summary "Set the grid spacing according to the current view size.
	The number of ticks will be between 20 and 200 in user units.
	The major spacing will be set to 10 ticks per major." }
	    { see_also "rset" } }
    .$id.menubar.settings.grid.spacing add command -label "Arbitrary" -underline 1\
	-command "do_grid_spacing $id b"
    hoc_register_menu_data "Grid Spacing" "Arbitrary" "Grid Spacing - Arbitrary"\
	{ { summary "Pops up the grid spacing entry dialog. The user
	can use this to set both the horizontal and
	vertical tick spacing." }
	    { see_also "rset" } }
    .$id.menubar.settings.grid.spacing add separator
    .$id.menubar.settings.grid.spacing add command -label "Micrometer" -underline 4\
	-command "set_grid_spacing $id micrometer 1"
    hoc_register_menu_data "Grid Spacing" "Micrometer" "Grid Spacing - Micrometer"\
	{ { summary "Set the horizontal and vertical tick
	spacing to 1 micrometer." }
	    { see_also "rset" } }
    .$id.menubar.settings.grid.spacing add command -label "Millimeter" -underline 2\
	-command "set_grid_spacing $id millimeter 1"
    hoc_register_menu_data "Grid Spacing" "Millimeter" "Grid Spacing - Millimeter"\
	{ { summary "Set the horizontal and vertical tick
	spacing to 1 millimeter." }
	    { see_also "rset" } }
    .$id.menubar.settings.grid.spacing add command -label "Centimeter" -underline 0\
	-command "set_grid_spacing $id centimeter 1"
    hoc_register_menu_data "Grid Spacing" "Centimeter" "Grid Spacing - Centimeter"\
	{ { summary "Set the horizontal and vertical tick
	spacing to 1 centimeter." }
	    { see_also "rset" } }
    .$id.menubar.settings.grid.spacing add command -label "Decimeter" -underline 0\
	-command "set_grid_spacing $id decimeter 1"
    hoc_register_menu_data "Grid Spacing" "Decimeter" "Grid Spacing - Decimeter"\
	{ { summary "Set the horizontal and vertical tick
	spacing to 1 decimeter." }
	    { see_also "rset" } }
    .$id.menubar.settings.grid.spacing add command -label "Meter" -underline 0\
	-command "set_grid_spacing $id meter 1"
    hoc_register_menu_data "Grid Spacing" "Meter" "Grid Spacing - Meter"\
	{ { summary "Set the horizontal and vertical tick
	spacing to 1 meter." }
	    { see_also "rset" } }
    .$id.menubar.settings.grid.spacing add command -label "Kilometer" -underline 0\
	-command "set_grid_spacing $id kilometer 1"
    hoc_register_menu_data "Grid Spacing" "Kilometer" "Grid Spacing - Kilometer"\
	{ { summary "Set the horizontal and vertical tick
	spacing to 1 kilometer." }
	    { see_also "rset" } }
    .$id.menubar.settings.grid.spacing add separator
    .$id.menubar.settings.grid.spacing add command -label "1/10 Inch" -underline 0\
	-command "set_grid_spacing $id \"1/10 inch\" 1"
    hoc_register_menu_data "Grid Spacing" "1/10 Inch" "Grid Spacing - 1/10 Inch"\
	{ { summary "Set the horizontal and vertical tick
	spacing to 1/10 inches." }
	    { see_also "rset" } }
    .$id.menubar.settings.grid.spacing add command -label "1/4 Inch" -underline 2\
	-command "set_grid_spacing $id \"1/4 inch\" 1"
    hoc_register_menu_data "Grid Spacing" "1/4 Inch" "Grid Spacing - 1/4 Inch"\
	{ { summary "Set the horizontal and vertical tick
	spacing to 1/4 inches." }
	    { see_also "rset" } }
    .$id.menubar.settings.grid.spacing add command -label "1/2 Inch" -underline 2\
	-command "set_grid_spacing $id \"1/2 inch\" 1"
    hoc_register_menu_data "Grid Spacing" "1/2 Inch" "Grid Spacing - 1/2 Inch"\
	{ { summary "Set the horizontal and vertical tick
	spacing to 1/2 inches." }
	    { see_also "rset" } }
    .$id.menubar.settings.grid.spacing add command -label "Inch" -underline 0\
	-command "set_grid_spacing $id inch 1"
    hoc_register_menu_data "Grid Spacing" "Inch" "Grid Spacing - Inch"\
	{ { summary "Set the horizontal and vertical tick
	spacing to 1 inch." }
	    { see_also "rset" } }
    .$id.menubar.settings.grid.spacing add command -label "Foot" -underline 0\
	-command "set_grid_spacing $id foot 1"
    hoc_register_menu_data "Grid Spacing" "Foot" "Grid Spacing - Foot"\
	{ { summary "Set the horizontal and vertical tick
	spacing to 1 foot." }
	    { see_also "rset" } }
    .$id.menubar.settings.grid.spacing add command -label "Yard" -underline 0\
	-command "set_grid_spacing $id yard 1"
    hoc_register_menu_data "Grid Spacing" "Yard" "Grid Spacing - Yard"\
	{ { summary "Set the horizontal and vertical tick
	spacing to 1 yard." }
	    { see_also "rset" } }
    .$id.menubar.settings.grid.spacing add command -label "Mile" -underline 3\
	-command "set_grid_spacing $id mile 1"
    hoc_register_menu_data "Grid Spacing" "Mile" "Grid Spacing - Mile"\
	{ { summary "Set the horizontal and vertical tick
	spacing to 1 mile." }
	    { see_also "rset" } }

    #
    # Don't need to register menu data here. Already being done above.
    #
    menu .$id.menubar.settings.grid_spacing -title "Grid Spacing" -tearoff $mged_default(tearoff_menus)
    .$id.menubar.settings.grid_spacing add command -label "Autosize" -underline 0\
	-command "grid_spacing_autosize $id; grid_spacing_apply $id b"
    .$id.menubar.settings.grid_spacing add command -label "Arbitrary" -underline 1\
	-command "do_grid_spacing $id b"
    .$id.menubar.settings.grid_spacing add separator
    .$id.menubar.settings.grid_spacing add command -label "Micrometer" -underline 4\
	-command "set_grid_spacing $id micrometer 1"
    .$id.menubar.settings.grid_spacing add command -label "Millimeter" -underline 2\
	-command "set_grid_spacing $id millimeter 1"
    .$id.menubar.settings.grid_spacing add command -label "Centimeter" -underline 0\
	-command "set_grid_spacing $id centimeter 1"
    .$id.menubar.settings.grid_spacing add command -label "Decimeter" -underline 0\
	-command "set_grid_spacing $id decimeter 1"
    .$id.menubar.settings.grid_spacing add command -label "Meter" -underline 0\
	-command "set_grid_spacing $id meter 1"
    .$id.menubar.settings.grid_spacing add command -label "Kilometer" -underline 0\
	-command "set_grid_spacing $id kilometer 1"
    .$id.menubar.settings.grid_spacing add separator
    .$id.menubar.settings.grid_spacing add command -label "1/10 Inch" -underline 0\
	-command "set_grid_spacing $id \"1/10 inch\" 1"
    .$id.menubar.settings.grid_spacing add command -label "1/4 Inch" -underline 2\
	-command "set_grid_spacing $id \"1/4 inch\" 1"
    .$id.menubar.settings.grid_spacing add command -label "1/2 Inch" -underline 2\
	-command "set_grid_spacing $id \"1/2 inch\" 1"
    .$id.menubar.settings.grid_spacing add command -label "Inch" -underline 0\
	-command "set_grid_spacing $id inch 1"
    .$id.menubar.settings.grid_spacing add command -label "Foot" -underline 0\
	-command "set_grid_spacing $id foot 1"
    .$id.menubar.settings.grid_spacing add command -label "Yard" -underline 0\
	-command "set_grid_spacing $id yard 1"
    .$id.menubar.settings.grid_spacing add command -label "Mile" -underline 3\
	-command "set_grid_spacing $id mile 1"

    menu .$id.menubar.settings.coord -title "Constraint Coords" -tearoff $mged_default(tearoff_menus)
    .$id.menubar.settings.coord add radiobutton -value m -variable mged_gui($id,coords)\
	-label "Model" -underline 0\
	-command "mged_apply $id \"set coords \$mged_gui($id,coords)\""
    hoc_register_menu_data "Constraint Coords" "Model" "Constraint Coords - Model"\
	{ { summary "Constrained transformations will use model coordinates." }
	    { see_also "rset, vars" } }
    .$id.menubar.settings.coord add radiobutton -value v -variable mged_gui($id,coords)\
	-label "View" -underline 0\
	-command "mged_apply $id \"set coords \$mged_gui($id,coords)\""
    hoc_register_menu_data "Constraint Coords" "View" "Constraint Coords - View"\
	{ { summary "Constrained transformations will use view coordinates." }
	    { see_also "rset, vars" } }
    .$id.menubar.settings.coord add radiobutton -value o -variable mged_gui($id,coords)\
	-label "Object" -underline 0\
	-command "mged_apply $id \"set coords \$mged_gui($id,coords)\"" -state disabled
    hoc_register_menu_data "Constraint Coords" "Object" "Constraint Coords - Object"\
	{ { summary "Constrained transformations will use object coordinates." }
	    { see_also "rset, vars" } }

    menu .$id.menubar.settings.origin -title "Rotate About" -tearoff $mged_default(tearoff_menus)
    .$id.menubar.settings.origin add radiobutton -value v -variable mged_gui($id,rotate_about)\
	-label "View Center" -underline 0\
	-command "mged_apply $id \"set rotate_about \$mged_gui($id,rotate_about)\""
    hoc_register_menu_data "Rotate About" "View Center" "Rotate About - View Center"\
	{ { summary "Set the center of rotation to be about the view center." }
	    { see_also "rset, vars" } }
    .$id.menubar.settings.origin add radiobutton -value e -variable mged_gui($id,rotate_about)\
	-label "Eye" -underline 0\
	-command "mged_apply $id \"set rotate_about \$mged_gui($id,rotate_about)\""
    hoc_register_menu_data "Rotate About" "Eye" "Rotate About - Eye"\
	{ { summary "Set the center of rotation to be about the eye." }
	    { see_also "rset, vars" } }
    .$id.menubar.settings.origin add radiobutton -value m -variable mged_gui($id,rotate_about)\
	-label "Model Origin" -underline 0\
	-command "mged_apply $id \"set rotate_about \$mged_gui($id,rotate_about)\""
    hoc_register_menu_data "Rotate About" "Model Origin" "Rotate About - Model Origin"\
	{ { summary "Set the center of rotation to be about the model origin." }
	    { see_also "rset, vars" } }
    .$id.menubar.settings.origin add radiobutton -value k -variable mged_gui($id,rotate_about)\
	-label "Key Point" -underline 0\
	-command "mged_apply $id \"set rotate_about \$mged_gui($id,rotate_about)\"" -state disabled
    hoc_register_menu_data "Rotate About" "Key Point" "Rotate About - Key Point"\
	{ { summary "Set the center of rotation to be about the key point." }
	    { see_also "rset, vars" } }

    menu .$id.menubar.settings.transform -title "Transform" -tearoff $mged_default(tearoff_menus)
    .$id.menubar.settings.transform add radiobutton -value v -variable mged_gui($id,transform)\
	-label "View" -underline 0\
	-command "set_transform $id"
    hoc_register_menu_data "Transform" "View" "Transform - View"\
	{ { summary "Set the transform mode to VIEW. When in VIEW mode, the mouse
	can be used to transform the view. This is the default." }
	    { see_also "rset, vars" } }
    .$id.menubar.settings.transform add radiobutton -value a -variable mged_gui($id,transform)\
	-label "ADC" -underline 0\
	-command "set_transform $id"
    hoc_register_menu_data "Transform" "ADC" "Transform - ADC"\
	{ { summary "Set the transform mode to ADC. When in ADC mode, the mouse
	can be used to transform the angle distance cursor while the
	angle distance cursor is being displayed. If the angle distance
	cursor is not being displayed, the behavior is the same as VIEW
	mode." }
	    { see_also "rset, vars" } }
    .$id.menubar.settings.transform add radiobutton -value e -variable mged_gui($id,transform)\
	-label "Model Params" -underline 0\
	-command "set_transform $id" -state disabled
    hoc_register_menu_data "Transform" "Model Params" "Transform - Model Params"\
	{ { summary "Set the transform mode to Model Params. When in Model Params
	mode, the mouse can be used to transform the model parameters." }
	    { see_also "rset, vars" } }

    menu .$id.menubar.modes -title "Modes" -tearoff $mged_default(tearoff_menus)
    .$id.menubar.modes add checkbutton -offvalue 0 -onvalue 1 -variable mged_gui($id,grid_draw)\
	-label "Draw Grid" -underline 0\
	-command "mged_draw_grid $id"
    hoc_register_menu_data "Modes" "Draw Grid" "Draw Grid"\
	{ { summary "Toggle drawing the grid. The grid is a lattice
	of points over the pane (geometry window). The
	regular spacing between the points gives the user
	accurate visual cues regarding dimension. This
	spacing can be set by the user." }
	    { see_also "rset" } }
    .$id.menubar.modes add checkbutton -offvalue 0 -onvalue 1 -variable mged_gui($id,draw_snap)\
	-label "Snap To Grid" -underline 0\
	-command "mged_apply $id \"rset grid snap \$mged_gui($id,draw_snap)\""
    hoc_register_menu_data "Modes" "Snap To Grid" "Snap To Grid"\
	{ { summary "Toggle grid snapping. When snapping to grid,
	the internal routines that use the mouse pointer
	location, move/snap that location to the nearest
	grid point. This gives the user high accuracy with
	the mouse for transforming the view or editing
	solids/matrices." }
	    { see_also "rset" } }
    .$id.menubar.modes add separator
    .$id.menubar.modes add checkbutton -offvalue 0 -onvalue 1 -variable mged_gui($id,fb)\
	-label "Framebuffer Active" -underline 0 \
	-command "set_fb $id"
    hoc_register_menu_data "Modes" "Framebuffer Active" "Framebuffer Active"\
	{ { summary "This activates/deactivates the framebuffer." }
	    { see_also "rset, vars" } }
    .$id.menubar.modes add checkbutton -offvalue 0 -onvalue 1 -variable mged_gui($id,listen)\
	-label "Listen For Clients" -underline 0\
	-command "set_listen $id"
    hoc_register_menu_data "Modes" "Listen For Clients" "Listen For Clients"\
	{ { summary "
	This activates/deactivates listening for clients. If
	the framebuffer is listening for clients, new data can
	be passed into the framebuffer. Otherwise, the framebuffer
	is write protected. Actually, it is also read protected
	with one exception. MGED can still read the framebuffer
	data." }
	    { see_also "rset, vars" } }
    .$id.menubar.modes add separator
    .$id.menubar.modes add checkbutton -offvalue 0 -onvalue 1 -variable mged_gui($id,rubber_band)\
	-label "Persistent Sweep Rectangle" -underline 0\
	-command "mged_apply $id \"rset rb draw \$mged_gui($id,rubber_band)\""
    hoc_register_menu_data "Modes" "Persistent sweep rectangle" "Persistent Rubber Band"\
	{ { summary "Toggle drawing the rectangle while idle." }
	    { see_also "rset" } }
    .$id.menubar.modes add checkbutton -offvalue 0 -onvalue 1 -variable mged_gui($id,adc_draw)\
	-label "Angle/Dist Cursor" -underline 0 \
	-command "adc_CBHandler $id"
    hoc_register_menu_data "Modes" "Angle/Dist Cursor" "Angle/Dist Cursor"\
	{ { summary "Toggle drawing the angle distance cursor." }
	    { see_also "adc" } }
    .$id.menubar.modes add checkbutton -offvalue 0 -onvalue 1 -variable mged_gui($id,faceplate)\
	-label "Faceplate" -underline 7\
	-command "mged_apply $id \"set faceplate \$mged_gui($id,faceplate)\""
    hoc_register_menu_data "Modes" "Faceplate" "Faceplate"\
	{ { summary "Toggle drawing the MGED faceplate." }
	    { see_also "rset, vars" } }
    .$id.menubar.modes add cascade -label "Axes" -underline 1\
	-menu .$id.menubar.modes.axes
    .$id.menubar.modes add separator
    .$id.menubar.modes add cascade -label "Display Manager" -underline 1\
        -menu .$id.menubar.modes.dmtype
    .$id.menubar.modes add checkbutton -offvalue 0 -onvalue 1 -variable mged_gui($id,multi_pane)\
	-label "Multipane" -underline 0 -command "setmv $id"
    hoc_register_menu_data "Modes" "Multipane" "Multipane"\
	{ { summary "
	Toggle between multipane and single pane mode.
	In multipane mode there are four panes, each
	with its own state." } }
    .$id.menubar.modes add checkbutton -offvalue 0 -onvalue 1 -variable mged_gui($id,show_edit_info)\
	-label "Edit Info" -underline 0\
	-command "toggle_edit_info $id"
    hoc_register_menu_data "Modes" "Edit Info" "Edit Info"\
	{ { summary "
	Toggle display of edit information. If in
	solid edit state, the edit information is
	displayed in the internal solid editor. This
	editor, as its name implies, can be used to
	edit the solid as well as to view its contents.
	If in object edit state, the object information
	is displayed in a dialog box." } }
    .$id.menubar.modes add checkbutton -offvalue 0 -onvalue 1 -variable mged_gui($id,show_status)\
	-label "Status Bar" -underline 7\
	-command "toggle_status_bar $id"
    hoc_register_menu_data "Modes" "Status Bar" "Status Bar"\
	{ { summary "Toggle display of the command window's status bar." } }
    if {$comb} {
	.$id.menubar.modes add checkbutton -offvalue 0 -onvalue 1 -variable mged_gui($id,show_cmd)\
	    -label "Command Window" -underline 0\
	    -command "set_cmd_win $id"
	hoc_register_menu_data "Modes" "Command Window" "Command Window"\
	    { { summary "Toggle display of the command window." } }
	.$id.menubar.modes add checkbutton -offvalue 0 -onvalue 1 -variable mged_gui($id,show_dm)\
	    -label "Graphics Window" -underline 0\
	    -command "set_dm_win $id"
	hoc_register_menu_data "Modes" "Graphics Window" "Graphics Window"\
	    { { summary "Toggle display of the graphics window." } }
    }
    .$id.menubar.modes add separator
    .$id.menubar.modes add checkbutton -offvalue 0 -onvalue 1 -variable mged_gui($id,collaborate)\
	-label "Collaborate" -underline 0\
	-command "collab_doit $id"
    hoc_register_menu_data "Modes" "Collaborate" "Collaborate"\
	{ { summary "Toggle collaborate mode. When in collaborate
	mode, the upper right pane's view can be shared
	with other instances of MGED's new GUI that are
	also collaborating." } }
    .$id.menubar.modes add checkbutton -offvalue 0 -onvalue 1 -variable mged_gui($id,rateknobs)\
	-label "Rateknobs" -underline 0\
	-command "mged_apply $id \"set rateknobs \$mged_gui($id,rateknobs)\""
    hoc_register_menu_data "Modes" "Rateknobs" "Rate Knobs"\
	{ { summary "Toggle rate knob mode. When in rate knob mode,
	transformation with the mouse becomes rate based.
	For example, if the user rotates the view about
	the X axis, the view continues to rotate about the
	X axis until the rate rotation is stopped." }
	    { see_also "knob" } }
    .$id.menubar.modes add checkbutton -offvalue 0 -onvalue 1 -variable mged_gui($id,dlist)\
	-label "Display Lists" -underline 8\
	-command "mged_apply $id \"set dlist \$mged_gui($id,dlist)\""
    hoc_register_menu_data "Modes" "Display Lists" "Display Lists"\
	{ { summary "Toggle the use of display lists. This currently affects
	only Ogl display managers. When using display lists the
	screen update time is significantly faster. This is especially
	noticable when running MGED remotely. Use of display lists
	is encouraged unless the geometry being viewed is bigger
	than the Ogl server can handle (i.e. the server runs out
	of available memory for storing display lists). When this
	happens the machine will begin to swap (and little else).
	If huge pieces of geometry need to be viewed, consider
	toggling off display lists. Note that using display lists
	while viewing geometry of any significant size will incur
	noticable compute time up front to create the display lists."} }

    menu .$id.menubar.modes.axes -title "Axes" -tearoff $mged_default(tearoff_menus)
    .$id.menubar.modes.axes add checkbutton -offvalue 0 -onvalue 1\
	-variable mged_gui($id,view_draw) -label "View" -underline 0\
	-command "mged_apply $id \"rset ax view_draw \$mged_gui($id,view_draw)\""
    hoc_register_menu_data "Axes" "View" "View Axes"\
	{ { summary "Toggle display of the view axes. The view axes
	are used to give the user visual cues indicating
	the current view of model space. These axes are
	drawn the same as the model axes, except that the
	view axes' position is fixed in view space. This
	position as well as other characteristics can be
	set by the user." }
	    { see_also "rset" } }
    .$id.menubar.modes.axes add checkbutton -offvalue 0 -onvalue 1\
	-variable mged_gui($id,model_draw) -label "Model" -underline 0\
	-command "mged_apply $id \"rset ax model_draw \$mged_gui($id,model_draw)\""
    hoc_register_menu_data "Axes" "Model" "Model Axes"\
	{ { summary "Toggle display of the model axes. The model axes
	are used to give the user visual cues indicating
	the current view of model space. The model axes
	are by default located at the model origin and are
	fixed in model space. So, if the user transforms
	the view, the model axes will move with respect to
	the view. The model axes position as well as other
	characteristics can be set by the user." }
	    { see_also "rset" } }
    .$id.menubar.modes.axes add checkbutton -offvalue 0 -onvalue 1\
	-variable mged_gui($id,edit_draw) -label "Edit" -underline 0\
	-command "mged_apply $id \"rset ax edit_draw \$mged_gui($id,edit_draw)\""
    hoc_register_menu_data "Axes" "Edit" "Edit Axes"\
	{ { summary "Toggle display of the edit axes. The edit axes
	are used to give the user visual cues indicating
	how the edit is progressing. They consist of a
	pair of axes. One remains unmoved, while the other
	moves to indicate how things have changed." }
	    { see_also "rset" } }

    menu .$id.menubar.modes.dmtype -title "Display Manager" -tearoff $mged_default(tearoff_menus)
    set have_dm [dm valid ogl]
    if {$have_dm == "ogl"} {
    .$id.menubar.modes.dmtype add radiobutton -value s -variable mged_gui($id,dtype)\
        -label "OpenGL" -underline 0\
        -command "dmtype set ogl"
    }
    set have_dm [dm valid wgl]
    if {$have_dm == "wgl"} {
    .$id.menubar.modes.dmtype add radiobutton -value s -variable mged_gui($id,dtype)\
        -label "Windows OpenGL" -underline 0\
        -command "dmtype set wgl"
    }
    set have_dm [dm valid X]
    if {$have_dm == "X"} {
    .$id.menubar.modes.dmtype add radiobutton -value s -variable mged_gui($id,dtype)\
        -label "X Windows" -underline 0\
        -command "dmtype set X"
    }
    set have_dm [dm valid glx]
    if {$have_dm == "glx"} {
    .$id.menubar.modes.dmtype add radiobutton -value s -variable mged_gui($id,dtype)\
        -label "GLX" -underline 0\
        -command "dmtype set glx"
    }
    set have_dm [dm valid rtgl]
    if {$have_dm == "rtgl"} {
    .$id.menubar.modes.dmtype add radiobutton -value s -variable mged_gui($id,dtype)\
        -label "Ray Traced" -underline 0\
        -command "dmtype set rtgl"
    }
    set have_dm [dm valid tk]
    if {$have_dm == "tk"} {
    .$id.menubar.modes.dmtype add radiobutton -value s -variable mged_gui($id,dtype)\
        -label "Tk" -underline 0\
        -command "dmtype set tk"
    }
    hoc_register_menu_data "Modes" "Display Manager" "Display Manager"\
	{ { summary "Change the display manager being used to render wireframe and/or
        shaded displays of BRL-CAD models." }
	    { see_also "dmtype" } }
   

    menu .$id.menubar.misc -title "Misc" -tearoff $mged_default(tearoff_menus)
    .$id.menubar.misc add checkbutton -offvalue 0 -onvalue 1\
	-variable mged_gui($id,zclip) -label "Z Clipping" -underline 0\
	-command "mged_apply $id \"dm set zclip \$mged_gui($id,zclip)\""
    hoc_register_menu_data "Misc" "Z Clipping" "Z Clipping"\
	{ { summary "Toggle zclipping. When zclipping is active, the Z value
	of each point is checked against the min and max Z values
	of the viewing cube. If the Z value of the point is found
	to be outside this range, it is clipped (i.e. not drawn).
	Zclipping can be used to remove geometric detail that may
	be occluding geometry of greater interest." }
	    { see_also "dm" } }
    .$id.menubar.misc add checkbutton -offvalue 0 -onvalue 1\
	-variable mged_gui($id,perspective_mode) -label "Perspective" -underline 0\
	-command "mged_apply $id \"set perspective_mode \$mged_gui($id,perspective_mode)\""
    hoc_register_menu_data "Misc" "Perspective" "Perspective"\
	{ { summary "Toggle perspective_mode."}
	    { see_also "rset, vars" } }
    .$id.menubar.misc add checkbutton -offvalue 0 -onvalue 1\
	-variable mged_gui($id,faceplate) -label "Faceplate" -underline 0\
	-command "mged_apply $id \"set faceplate \$mged_gui($id,faceplate)\""
    hoc_register_menu_data "Misc" "Faceplate" "Faceplate"\
	{ { summary "Toggle drawing the MGED faceplate." }
	    { see_also "rset, vars" } }
    .$id.menubar.misc add checkbutton -offvalue 0 -onvalue 1\
	-variable mged_gui($id,orig_gui) -label "Faceplate GUI" -underline 10\
	-command "mged_apply $id \"set orig_gui \$mged_gui($id,orig_gui)\""
    hoc_register_menu_data "Misc" "Faceplate GUI" "Faceplate GUI"\
	{ { summary "Toggle drawing the MGED faceplate GUI. The faceplate GUI
	consists of the faceplate menu and sliders." }
	    { see_also "rset, vars" } }
    .$id.menubar.misc add checkbutton -offvalue 0 -onvalue 1\
	-variable mged_gui($id,forward_keys) -label "Keystroke Forwarding" -underline 0\
	-command "mged_apply $id \"set_forward_keys \\\[winset\\\] \$mged_gui($id,forward_keys)\""
    hoc_register_menu_data "Misc" "Keystroke Forwarding" "Keystroke Forwarding"\
	{ { summary "Toggle keystroke forwarding. When forwarding keystrokes, each
	key event within the drawing window is forwarded to the command
	window. Drawing window specific commands (i.e. commands that
	modify the state of the drawing window) will apply only to the
	drawing window wherein the user typed. This feature is provided
	to lessen the need to use the mouse." } }
    if {$mged_gui($id,dtype) == "ogl"} {
	.$id.menubar.misc add checkbutton -offvalue 0 -onvalue 1\
	    -variable mged_gui($id,depthcue) -label "Depth Cueing" -underline 0\
	    -command "mged_apply $id \"dm set depthcue \$mged_gui($id,depthcue)\""
	hoc_register_menu_data "Misc" "Depth Cueing" "Depth Cueing"\
	    { { summary "Toggle depth cueing. When depth cueing is active,
		lines that are farther away appear more faint." }
		{ see_also "dm" } }
	.$id.menubar.misc add checkbutton -offvalue 0 -onvalue 1\
	    -variable mged_gui($id,zbuffer) -label "Z Buffer" -underline 2\
	    -command "mged_apply $id \"dm set zbuffer \$mged_gui($id,zbuffer)\""
	hoc_register_menu_data "Misc" "Z Buffer" "Z Buffer"\
	    { { summary "Toggle Z buffer." }
		{ see_also "dm" } }
	.$id.menubar.misc add checkbutton -offvalue 0 -onvalue 1\
	    -variable mged_gui($id,lighting) -label "Lighting" -underline 0\
	    -command "mged_apply $id \"dm set lighting \$mged_gui($id,lighting)\""
	hoc_register_menu_data "Misc" "Lighting" "Lighting"\
	    { { summary "Toggle lighting." }
		{ see_also "dm" } }
    }

    # BEGIN TOOLS MENU

    # BEGIN CONTROL PANELS (control behavior and settings)

    menu .$id.menubar.tools -title "Tools" -tearoff $mged_default(tearoff_menus)
    .$id.menubar.tools add command -label "ADC Control Panel" -underline 0\
	-command "init_adc_control $id"
    hoc_register_menu_data "Tools" "ADC Control Panel" "ADC Control Panel"\
	{ { summary "Tool for controlling the angle distance cursor." }
	    { see_also "adc" } }

    .$id.menubar.tools add command -label "AnimMate Control Panel" -underline 1\
	-command "animmate $id .$id"
    hoc_register_menu_data "Tools" "AnimMate Control Panel" "AnimMate Control Panel"\
	{ { summary "Tool for building animation scripts." }
	    { see_also animmate } }

    .$id.menubar.tools add command -label "Grid Control Panel" -underline 0\
	-command "init_grid_control $id"
    hoc_register_menu_data "Tools" "Grid Control Panel" "Grid Control Panel"\
	{ { summary "Tool for setting grid parameters." }
	    { see_also "rset" } }

    .$id.menubar.tools add command -label "Query Ray Control Panel" -underline 0\
	-command "init_qray_control $id"
    hoc_register_menu_data "Tools" "Query Ray Control Panel" "Query Ray Control Panel"\
	{ { summary "Tool for setting query ray parameters." }
	    { see_also "qray" } }

    .$id.menubar.tools add command -label "Raytrace Control Panel" -underline 0\
	-command "init_Raytrace $id"
    hoc_register_menu_data "Tools" "Raytrace Control Panel" "Raytrace Control Panel"\
	{ { summary "Tool for raytracing." }
	    { see_also rt } }

    # BEGIN TOOLS (perform operation (subtle (bogus) difference))

    .$id.menubar.tools add separator

    .$id.menubar.tools add command -label "BoT Edit Tool" -underline 0\
	-command "bot_askforname .$id $screen"
    hoc_register_menu_data "Tools" "BoT Edit Tool" "BoT Edit Tool"\
	{ { summary "A tool for performing various editing operations on BoTs." } }

    .$id.menubar.tools add command -label "Build Pattern Tool" -underline 0\
	-command "pattern_control .\#auto"
    hoc_register_menu_data "Tools" "Build Pattern Tool" "Build Pattern Tool"\
	{ { summary "A tool for building a repetitive pattern from an existing object." } }

    .$id.menubar.tools add command -label "Color Selector" -underline 1\
	-command "cadColorWidget tool .$id colorEditTool\
			-title \"Color Selector\"\
			-initialcolor black"
    hoc_register_menu_data "Tools" "Color Selector" "Color Selector"\
	{ { summary "Tool for creating, displaying, and selecting colors." } }

    .$id.menubar.tools add command -label "Geometry Browser" -underline 0\
	-command "geometree"
    hoc_register_menu_data "Tools" "Geometry Browser" "Geometry Browser"\
	{ { summary "Tool for browsing the geometry in a database." } }

    .$id.menubar.tools add command -label "Overlap Tool" -underline 0\
	-command "overlap_tool $id"
    hoc_register_menu_data "Tools" "Overlap Tool" "Overlap Tool"\
	{ { summary "A tool for discovering and correcting overlapping regions." } }

    # BEGIN ACTIONS (perform some operation (subtle (bogus) difference))

    .$id.menubar.tools add separator

    .$id.menubar.tools add command -label "Upgrade Database..." -underline 1\
	-command "call_dbupgrade $id"
    hoc_register_menu_data "Tools" "Upgrade Database..." "Upgrade Database..."\
	{ { summary "Upgrade to the current database format." }
	    { see_also dbupgrade } }

    # BEGIN WINDOWS (display main windows)

    .$id.menubar.tools add separator

    .$id.menubar.tools add command -label "Command Window" -underline 6\
	-command "raise .$id"
    hoc_register_menu_data "Tools" "Command Window" "Command Window"\
	{ { summary "Raise the command window." } }
    .$id.menubar.tools add command -label "Graphics Window" -underline 7\
	-command "raise $mged_gui($id,top)"
    hoc_register_menu_data "Tools" "Graphics Window" "Graphics Window"\
	{ { summary "Raise the geometry window." } }

    # END TOOLS MENU

    menu .$id.menubar.help -title "Help" -tearoff $mged_default(tearoff_menus)
    .$id.menubar.help add command -label "Dedication" -underline 0\
	-command "mike_dedication $id"
    .$id.menubar.help add command -label "About MGED" -underline 0\
	-command "do_About_MGED $id"
    hoc_register_menu_data "Help" "About MGED" "About MGED"\
	{ { summary "Information about MGED" } }
   .$id.menubar.help add command -label "Command Manual Pages" -underline 0\
	-command "man"
    .$id.menubar.help add command -label "Shift Grips" -underline 0\
	-command "hoc_dialog .$id.menubar.help \"Help,Shift Grips\""
    hoc_register_menu_data "Help" "Shift Grips" "Shift Grips"\
	{ { summary "			MGED offers the user a unified mouse-based interface for \"grabbing\"
	things and manipulating them.	 Since it was built for compatibility on
	top of the older interface:

	----------------------------------------
	Mouse button	View operation
	----------------------------------------
	Button-1		Zoom out
	Button-2		Recenter view at
	the specified point
	Button-3		Zoom in
	----------------------------------------

	it uses the modifier keys:	Shift, Control, and Alt.	This use of modified
	mouse clicks to grab things is called the \"shift-grip\" interface.	 The Shift
	and Control keys are assigned in combinations to the three basic transformation
	operations as follows:

	-----------------------------------------
	Modifier key	Transformation operation
	-----------------------------------------
	Shift			Translate
	Ctrl			Rotate
	Shift & Ctrl		Scale
	-----------------------------------------

	and the Alt key is assigned the meaning \"constrained transformation,\" which
	is described below.	 Thus, in general, holding the Shift key and a mouse
	button down and moving the mouse drags things around on the screen.	 The
	Control key and a mouse button allow one to rotate things, and the combination
	of Shift, Control, and a mouse button allow one to expand and contract things.
	These general functionalities are consistent throughout MGED, providing a
	unified interface.	The precise meanings of \"drag things around,\" \"rotate
	things,\" and \"expand and contract things\" depends on the operating context.
	When one is merely viewing geometry the shift grips apply by default to
	the view itself.	Thus they amount to panning, rotating, and zooming the
	eye relative to the geometry being displayed.	 When one is in solid-edit
	or matrix-edit mode (what used to be called object-edit mode), the shift
	grips apply by default to the model parameters.	 In this case, they modify
	the location, orientation, or size of object features or entire objects in
	the database.
	The default behaviors in the viewing and editing modes may be overridden
	by the \"Transform\" item in the \"Settings\" menu.	 This allows the user to
	specify that the shift grips should transform the view, the model parameters
	(if one is currently editing a solid or matrix) or the angle-distance cursor
	(in which case the mouse may be used to position the ADC, to change its angles,
	and to expand and contract its distance ticks).	 The behavior of the shift
	grips may be further changed by the \"Rotate About\" item in the \"Settings\"
	menu, which allows the user to specify the point about which shift-grip
	rotations should be performed.	The choices include the view center, the eye,
	the model origin, and an object's key point.

	CONSTRAINED TRANSFORMATIONS

	When the Alt key is held down along with either of the Shift and Control keys
	the transformations are constrained to a particular axis.	 For such
	constrained transformations the mouse buttons have the following meanings:

	------------------------
	Mouse button	Axis
	------------------------
	Button-1		 x
	Button-2		 y
	Button-3		 z
	------------------------

	Thus, if the view is being transformed, Alt-Shift-Button-1 allows one to drag
	the objects being viewed left to right along the view-x axis.	 Similarly, if
	the model parameters are being transformed, Alt-Ctrl-Button-2 allows one to
	rotate the object about a line passing through the rotate-about point (as
	described above) and parallel to a y-axis.	The coordinate system to which
	these transformations are constrained may be specified by the \"Constraint
	Coords\" item in the \"Settings\" menu, which allows the selection of any one of
	the model, view, and object coordinate systems.
	" } }
    .$id.menubar.help add command -label "Apropos" -underline 1\
	-command "ia_apropos .$id $screen"
    hoc_register_menu_data "Help" "Apropos" "Apropos"\
	{ { summary "Tool for searching for information about
	MGED's commands." }
	    { see_also "apropos" } }

    if {$tcl_platform(os) == "Windows NT"} {
	set web_cmd "exec \$mged_browser \$mged_html_dir/index.html &"
    } elseif {$tcl_platform(os) == "Darwin"} {
	set web_cmd "exec \$mged_browser \$mged_html_dir/index.html"
    } else {
	set web_cmd "exec \$mged_browser -display $screen \$mged_html_dir/index.html 2> /dev/null &"
    }

    .$id.menubar.help add command -label "Manual" -underline 0 -command $web_cmd
    hoc_register_menu_data "Help" "Manual" "Manual"\
	{ { summary "Start a tool for browsing the online MGED manual.
	The web browser that gets started is dependent, first, on the
	WEB_BROWSER environment variable. If this variable exists and
	the browser identified by this variable exists, then that browser
	is used. Failing that the browser specified by the
	mged_default(web_browser) Tcl variable is tried. As a last
	resort the existence of /usr/bin/netscape, /usr/local/bin/netscape
	and /usr/X11/bin/netscape is checked. If a browser has still not
	been located, the built-in Tcl browser is used." } }

    #==============================================================================
    # PHASE 3: Bottom-row display
    #==============================================================================
    frame .$id.status
    frame .$id.status.dpy
    frame .$id.status.illum

    label .$id.status.cent -textvar mged_display($mged_gui($id,active_dm),center) -anchor w
    hoc_register_data .$id.status.cent "View Center"\
	{ { summary "These numbers indicate the view center in\nmodel coordinates (local units)." }
	    { see_also "center, view" } }
    label .$id.status.size -textvar mged_display($mged_gui($id,active_dm),size) -anchor w
    hoc_register_data .$id.status.size "View Size"\
	{ { summary "This number indicates the view size (local units)." }
	    { see_also size} }
    label .$id.status.units -textvar mged_display(units) -anchor w -padx 4
    hoc_register_data .$id.status.units "Units"\
	{ { summary "This indicates the local units.		 " }
	    { see_also units} }
    label .$id.status.aet -textvar mged_display($mged_gui($id,active_dm),aet) -anchor w
    hoc_register_data .$id.status.aet "View Orientation"\
	{ { summary "These numbers indicate the view orientation using azimuth,\nelevation and twist." }
	    { see_also "ae, view" } }
    label .$id.status.ang -textvar mged_display($mged_gui($id,active_dm),ang) -anchor w -padx 4
    hoc_register_data .$id.status.ang "Rateknobs"\
	{ { summary "These numbers give some indication of\nrate of rotation about the x,y,z axes." }
	    { see_also knob} }
    label .$id.status.illum.label -textvar mged_gui($id,illum_label)
    hoc_register_data .$id.status.illum.label "Status Area"\
	{ { summary "This area is for displaying either the frames per second,
	the illuminated path, the keypoint during an edit
	or the ADC attributes." } }

    #==============================================================================
    # PHASE 4: Text widget for interaction
    #==============================================================================
    frame .$id.tf
    if {$comb} {
	text .$id.t -height $mged_gui($id,num_lines) \
	    -relief sunken \
	    -bd 2 \
	    -yscrollcommand ".$id.s set" \
	    -undo 1 \
	    -autoseparators 0
    } else {
	text .$id.t -relief sunken \
	    -bd 2 \
	    -yscrollcommand ".$id.s set" \
	    -undo 1 \
	    -autoseparators 0
    }
    scrollbar .$id.s -relief flat -command ".$id.t yview"

    if { $::tcl_platform(platform) != "windows" && $::tcl_platform(os) != "Darwin" } {
	bind .$id.t <Enter> "focus .$id.t; break"
    } else {
	# some platforms should not be forced window activiation
	focus .$id.t
    }

    hoc_register_data .$id.t "Command Window"\
	{ { summary "This is MGED's default command window. Its main
	function is to allow the user to enter commands.
	The command window supports command line editing
	and command history. The two supported command line
	edit modes are emacs and vi. Look under File/
	Preferences/Command_Line_Edit to change the edit mode.

	There are also two command interpretation modes. One
	is where MGED performs object name matching (i.e. globbing
	against the database) before passing the command line to MGED's
	built-in Tcl interpreter. This is the same behavior seen
	in previous releases. The other command interpretation
	mode (Tcl Evaluation) passes the command line directly to
	the Tcl interpreter. Look under File/Preferences/
	Special_Characters to change the interpetation mode.

	The command window also supports cut and paste as well
	as text scrolling. The default bindings for these operations
	are similar to those found in typical X Window applications
	such as xterm. For example:

	ButtonPress-1						begin text selection
	ButtonRelease-1					end text selection
	Button1-Motion				 add to text selection
	Shift-Button1						 modify text selection
	Double-Button-1				 select word
	Triple-Button-1					select line
	ButtonPress-2						begin text operation
	ButtonRelease-2				 paste text
	Button2-Motion				 scroll text

	Note - If motion was detected while Button2 was
	being pressed, no text will be pasted. In this case,
	it is assumed that scrolling was the intended operation.
	The user can also scroll the window using the scrollbar." } }

    set mged_gui($id,edit_style) $mged_default(edit_style)
    set mged_gui(.$id.t,insert_char_flag) 0
    set_text_key_bindings $id
    set_text_button_bindings .$id.t

    set mged_gui($id,cmd_prefix) ""
    set mged_gui($id,more_default) ""
    mged_print_prompt .$id.t "mged> "
    .$id.t insert insert " "
    beginning_of_line .$id.t
    .$id.t  edit reset
    set mged_gui(.$id.t,moveView) 0
    set mged_gui(.$id.t,freshline) 1
    set mged_gui(.$id.t,scratchline) ""
    set vi_state(.$id.t,overwrite_flag) 0
    set vi_state(.$id.t,yank_flag) 0
    set vi_state(.$id.t,delete_flag) 0
    set vi_state(.$id.t,change_flag) 0
    set vi_state(.$id.t,search_flag) ""
    set vi_state(.$id.t,hsrch_flag) 0
    set vi_state(.$id.t,count_flag) 0
    set vi_state(.$id.t,warn_flag) 0
    set vi_state(.$id.t,dot_flag) 0
    set vi_state(.$id.t,cmd_count) 1
    set vi_state(.$id.t,pos_count) 1
    set vi_state(.$id.t,tmp_count) 1
    set vi_state(.$id.t,cmd_list) [list]
    set vi_state(.$id.t,dot_list) [list]
    set vi_state(.$id.t,cut_buf) ""
    set vi_state(.$id.t,reset_buf) ""
    set vi_state(.$id.t,insert_buf) ""
    set vi_state(.$id.t,hsrch_buf) ""
    set vi_state(.$id.t,hsrch_type) ""
    set vi_state(.$id.t,search_char) ""
    set vi_state(.$id.t,search_type) ""

    .$id.t tag configure sel -background #fefe8e
    .$id.t tag configure result -foreground blue3
    .$id.t tag configure oldcmd -foreground red3
    .$id.t tag configure prompt -foreground red1

    #==============================================================================
    # Pack windows
    #==============================================================================
    setupmv $id
    setmv $id

    if { $comb } {
	if { $mged_gui($id,show_dm) } {
	    grid $mged_gui($id,dmc) -sticky nsew -row 0 -column 0
	}
    }

    grid .$id.t .$id.s -in .$id.tf -sticky "nsew"
    grid columnconfigure .$id.tf 0 -weight 1
    grid rowconfigure .$id.tf 0 -weight 1

    if { !$comb || ($comb && $mged_gui($id,show_cmd)) } {
	grid .$id.tf -sticky "nsew" -row 1 -column 0
    }

    grid .$id.status.cent .$id.status.size .$id.status.units .$id.status.aet\
	.$id.status.ang x -in .$id.status.dpy -sticky "ew"
    grid columnconfigure .$id.status.dpy 5 -weight 1
    grid .$id.status.dpy -sticky "ew"
    grid .$id.status.illum.label x -sticky "ew"
    grid columnconfigure .$id.status.illum 1 -weight 1
    grid .$id.status.illum -sticky "w"
    grid columnconfigure .$id.status 0 -weight 1

    if { $mged_gui($id,show_status) } {
	grid .$id.status -sticky "ew" -row 2 -column 0
    }

    grid columnconfigure .$id 0 -weight 1
    if { $comb } {
	# let only the display manager window grow
	grid rowconfigure .$id 0 -weight 1
    } else {
	# let only the text window (i.e. enter commands here) grow
	grid rowconfigure .$id 1 -weight 1
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

    cmd_win open $id
    _mged_tie $id $mged_gui($id,active_dm)
    reconfig_gui_default $id

    # Force display manager windows to update their respective color schemes
    mged_apply_local $id "rset cs mode 0"
    rset cs mode 1

    if { $join_c } {
	collaborate join $id
    }

    trace variable mged_display($mged_gui($id,active_dm),fps) w "ia_changestate $id"
    update_mged_vars $id
    set mged_gui($id,qray_effects) [qray effects]

    # reset current_cmd_list so that its cur_hist gets updated
    cmd_win set $save_id

    # cause all 4 windows to share menu state
    share m $mged_gui($id,top).ul $mged_gui($id,top).ur
    share m $mged_gui($id,top).ul $mged_gui($id,top).ll
    share m $mged_gui($id,top).ul $mged_gui($id,top).lr

    do_rebind_keys $id

    # Throw away key events
    bind $mged_gui($id,top) <KeyPress> { break }
    bind $mged_gui($id,top) <Configure> "mged_handle_configure $id"

    set dbname [_mged_opendb]
    set_wm_title $id $dbname

    # set the size here in case the user didn't specify it in mged_default(ggeom)
    set height [expr [winfo screenheight $mged_gui($id,top)] - 70]
    set width $height
    wm geometry $mged_gui($id,top) $width\x$height

    # set geometry (i.e. size and position) according to mged_default(ggeom)
    wm geometry $mged_gui($id,top) $mged_default(ggeom)

    wm protocol $mged_gui($id,top) WM_DELETE_WINDOW "gui_destroy $id"

    if { $comb } {
	if { !$mged_gui($id,show_dm) } {
	    update
	    set_dm_win $id
	}
    } else {
	wm geometry .$id $mged_default(geom)
	update

	# Prevent command window from resizing itself as labels change
	set geometry [wm geometry .$id]
	wm geometry .$id $geometry
    }
}

proc gui_destroy args {
    global mged_gui
    global mged_players
    global mged_collaborators

    if { [llength $args] != 1 } {
	return [help gui_destroy]
    }

    set id [lindex $args 0]

    set i [lsearch -exact $mged_players $id]
    if { $i == -1 } {
	return "gui_destroy: bad id - $id"
    }
    set mged_players [lreplace $mged_players $i $i]

    if { [lsearch -exact $mged_collaborators $id] != -1 } {
	collaborate quit $id
    }

    set mged_gui($id,multi_pane) 0
    set mged_gui($id,show_edit_info) 0

    releasemv $id
    catch { cmd_win close $id }
    catch { destroy .mmenu$id }
    catch { destroy .sliders$id }
    catch { destroy $mged_gui($id,top) }
    catch { destroy .$id }
}

proc reconfig_gui_default { id } {
    global mged_display

    cmd_win set $id
    set dm_id [_mged_tie $id]
    if { [llength $dm_id] != 1 } {
	return
    }

    .$id.status.cent configure -textvar mged_display($dm_id,center)
    .$id.status.size configure -textvar mged_display($dm_id,size)
    .$id.status.units configure -textvar mged_display(units)

    .$id.status.aet configure -textvar mged_display($dm_id,aet)
    .$id.status.ang configure -textvar mged_display($dm_id,ang)

    #		 update_view_ring_entries $id s
    #		 update_view_ring_entries $id d
    #		 reconfig_mmenu $id
}

proc reconfig_all_gui_default {} {
    global mged_collaborators

    foreach id $mged_collaborators {
	reconfig_gui_default $id
    }
}

proc update_mged_vars { id } {
    global mged_gui
    global forwarding_key
    global rateknobs
    global use_air
    global listen
    global fb
    global fb_all
    global fb_overlay
    global dlist
    global mouse_behavior
    global coords
    global rotate_about
    global transform
    global faceplate
    global perspective_mode
    global orig_gui

    winset $mged_gui($id,active_dm)
    set mged_gui($id,rateknobs) $rateknobs
    if {![catch {adc draw} result]} {
	set mged_gui($id,adc_draw) $result
    }
    set mged_gui($id,model_draw) [rset ax model_draw]
    set mged_gui($id,view_draw) [rset ax view_draw]
    set mged_gui($id,edit_draw) [rset ax edit_draw]
    set mged($id,use_air) $use_air
    set mged_gui($id,dlist) $dlist
    set mged_gui($id,rubber_band) [rset rb draw]
    set mged_gui($id,mouse_behavior) $mouse_behavior
    set mged_gui($id,coords) $coords
    set mged_gui($id,rotate_about) $rotate_about
    set mged_gui($id,transform) $transform
    set mged_gui($id,grid_draw) [rset grid draw]
    set mged_gui($id,draw_snap) [rset grid snap]
    set mged_gui($id,faceplate) $faceplate
    set mged_gui($id,zclip) [dm set zclip]
    set mged_gui($id,perspective_mode) $perspective_mode
    set mged_gui($id,orig_gui) $orig_gui
    set mged_gui($id,forward_keys) $forwarding_key($mged_gui($id,active_dm))

    if {$mged_gui($id,dtype) == "ogl"} {
	set mged_gui($id,depthcue) [dm set depthcue]
	set mged_gui($id,zbuffer) [dm set zbuffer]
	set mged_gui($id,lighting) [dm set lighting]
    }

    set_mged_v_axes_pos $id

    set mged_gui($id,listen) $listen
    set mged_gui($id,fb) $fb
    set mged_gui($id,fb_all) $fb_all
    set mged_gui($id,fb_overlay) $fb_overlay
}

proc set_mged_v_axes_pos { id } {
    global mged_gui

    set view_pos [rset ax view_pos]
    set hpos [lindex $view_pos 0]
    set vpos [lindex $view_pos 1]

    if {$hpos == 0 && $vpos == 0} {
	set mged_gui($id,view_pos) 0
    } elseif {$hpos < 0 && $vpos < 0} {
	set mged_gui($id,view_pos) 1
    } elseif {$hpos < 0 && $vpos > 0} {
	set mged_gui($id,view_pos) 2
    } elseif {$hpos > 0 && $vpos > 0} {
	set mged_gui($id,view_pos) 3
    } else {
	set mged_gui($id,view_pos) 4
    }
}

proc toggle_button_menu { id } {
    global mged_gui

    if {[ winfo exists .mmenu$id ]} {
	destroy .mmenu$id
	set mged_gui($id,classic_buttons) 0
	return
    }

    mmenu_init $id
}

proc toggle_edit_info { id } {
    global mged_gui
    global mged_display

    if {$mged_gui($id,show_edit_info)} {
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
    global mged_gui

    if {[winfo exists .sei$id]} {
	raise .sei$id
	return
    }

    if {$mged_gui($id,show_edit_info)} {
	toplevel .sei$id -screen $mged_gui($id,screen)
	label .sei$id.l -bg black -fg yellow -textvar edit_info
	pack .sei$id.l -expand 1 -fill both

	wm title .sei$id "$id\'s Edit Info"
	wm protocol .sei$id WM_DELETE_WINDOW "destroy_edit_info $id"
	wm geometry .sei$id $mged_gui($id,edit_info_pos)
    }
}

proc destroy_edit_info { id } {
    global mged_gui

    if {![winfo exists .sei$id]} {
	return
    }

    regexp "\[-+\]\[0-9\]+\[-+\]\[0-9\]+" [wm geometry .sei$id] match
    set mged_gui($id,edit_info_pos) $match
    destroy .sei$id
}

# Print Mged Players
proc pmp {} {
    global mged_players

    return $mged_players
}

proc set_active_dm { id } {
    global mged_gui
    global mged_display
    global view_ring

    set new_dm $mged_gui($id,top).$mged_gui($id,dm_loc)

    # Nothing to do
    if { $new_dm == $mged_gui($id,active_dm) } {
	return
    }

    trace vdelete mged_display($mged_gui($id,active_dm),fps) w "ia_changestate $id"

    # make inactive
    winset $mged_gui($id,active_dm)
    rset cs mode 0

    set mged_gui($id,active_dm) $new_dm

    # make active
    winset $mged_gui($id,active_dm)
    rset cs mode 1

    trace variable mged_display($mged_gui($id,active_dm),fps) w "ia_changestate $id"

    update_mged_vars $id
    #		 set view_ring($id) [view_ring get]

    _mged_tie $id $mged_gui($id,active_dm)
    reconfig_gui_default $id

    if {!$mged_gui($id,multi_pane)} {
	setmv $id
    }

    # XXXX this is already done in the call to reconfig_gui_default above
    # update view_ring entries
    #		 update_view_ring_entries $id s
    #		 update_view_ring_entries $id d

    # update adc control panel
    adc_load $id

    # update grid control panel
    grid_control_reset $id

    # update query ray control panel
    qray_reset $id

    set dbname [_mged_opendb]
    set_wm_title $id $dbname
}

proc set_wm_title { id dbname } {
    global mged_gui
    global version

    set ver [lindex $version 2]

    if {$mged_gui($id,top) == $mged_gui($id,dmc)} {
	if {$mged_gui($id,dm_loc) == "ul"} {
	    wm title $mged_gui($id,top) "MGED $ver Graphics Window ($id) - $dbname - Upper Left"
	    wm title .$id "MGED $ver Command Window ($id) - $dbname - Upper Left"
	} elseif {$mged_gui($id,dm_loc) == "ur"} {
	    wm title $mged_gui($id,top) "MGED $ver Graphics Window ($id) - $dbname - Upper Right"
	    wm title .$id "MGED $ver Command Window ($id) - $dbname - Upper Right"
	} elseif {$mged_gui($id,dm_loc) == "ll"} {
	    wm title $mged_gui($id,top) "MGED $ver Graphics Window ($id) - $dbname - Lower Left"
	    wm title .$id "MGED $ver Command Window ($id) - $dbname - Lower Left"
	} elseif {$mged_gui($id,dm_loc) == "lr"} {
	    wm title $mged_gui($id,top) "MGED $ver Graphics Window ($id) - $dbname - Lower Right"
	    wm title .$id "MGED $ver Command Window ($id) - $dbname - Lower Right"
	}
    } else {
	if {$mged_gui($id,dm_loc) == "ul"} {
	    wm title $mged_gui($id,top) "MGED $ver Command Window ($id) - $dbname - Upper Left"
	} elseif {$mged_gui($id,dm_loc) == "ur"} {
	    wm title $mged_gui($id,top) "MGED $ver Command Window ($id) - $dbname - Upper Right"
	} elseif {$mged_gui($id,dm_loc) == "ll"} {
	    wm title $mged_gui($id,top) "MGED $ver Command Window ($id) - $dbname - Lower Left"
	} elseif {$mged_gui($id,dm_loc) == "lr"} {
	    wm title $mged_gui($id,top) "MGED $ver Command Window ($id) - $dbname - Lower Right"
	}
    }
}

proc set_cmd_win { id } {
    global mged_gui

    if { $mged_gui($id,show_cmd) } {
	if { $mged_gui($id,show_dm) } {
	    .$id.t configure -height $mged_gui($id,num_lines)
	    grid .$id.tf -sticky nsew -row 1 -column 0
	} else {
	    grid .$id.tf -sticky nsew -row 0 -column 0
	}
    } else {
	grid forget .$id.tf

	if { !$mged_gui($id,show_dm) } {
	    set mged_gui($id,show_dm) 1
	    grid $mged_gui($id,dmc) -sticky nsew -row 0 -column 0
	    update
	    setmv $id
	}
    }
}

proc set_dm_win { id } {
    global mged_gui

    if { $mged_gui($id,show_dm) } {
	if { $mged_gui($id,show_cmd) } {
	    grid forget .$id.tf
	    .$id.t configure -height $mged_gui($id,num_lines)
	    update
	}

	grid $mged_gui($id,dmc) -sticky nsew -row 0 -column 0

	if { $mged_gui($id,show_cmd) } {
	    grid .$id.tf -sticky nsew -row 1 -column 0
	    update
	    .$id.t see end
	}

	setmv $id
    } else {
	grid forget $mged_gui($id,dmc)

	set mged_gui($id,show_cmd) 1
	set_cmd_win $id
	set fh [get_font_height .$id.t]
	set h [winfo height $mged_gui($id,top)]

	if { $mged_gui($id,show_status) } {
	    set h [expr $h - [winfo height .$id.status]]
	}

	set nlines [expr $h / $fh]
	.$id.t configure -height $nlines
    }
}

proc view_ring_add {id} {
    global mged_gui
    global mged_default
    global view_ring
    global mged_collaborators

    winset $mged_gui($id,active_dm)

    # already have 10 views in the view ring, ignore add
    if {$mged_default(max_views) <= [llength $mged_gui($id,views)]} {
	return
    }

    # calculate a view id for the new view
    set vid [.$id.menubar.viewring.select entrycget end -value]
    if {$vid == ""} {
	set vid 0
    } else {
	incr vid
    }

    # get view parameters
    set aet [_mged_ae]
    set center [_mged_center]
    set size [_mged_size]

    # save view commands
    set vcmds "_mged_ae $aet; _mged_center $center; _mged_size $size"

    # format view parameters for display in menu
    set aet [format "az=%.2f el=%.2f tw=%.2f" \
		 [lindex $aet 0] [lindex $aet 1] [lindex $aet 2]]
    set center [format "cent=(%.3f %.3f %.3f)" \
		    [lindex $center 0] [lindex $center 1] [lindex $center 2]]
    set size [format "size=%.3f" $size]

    if {[lsearch -exact $mged_collaborators $id] != -1} {
	foreach cid $mged_collaborators {
	    # append view commands to view list
	    lappend mged_gui($cid,views) $vcmds

	    .$cid.menubar.viewring.select add radiobutton -value $vid -variable view_ring($cid) \
		-label "$center $size $aet" -command "view_ring_goto $cid $vid"
	    .$cid.menubar.viewring.delete add command -label "$center $size $aet" \
		-command "view_ring_delete $cid $vid"

	    # remember the last selected radiobutton
	    set view_ring($cid,prev) $view_ring($cid)

	    # update radio buttons
	    set view_ring($cid) $vid
	}
    } else {
	# append view commands to view list
	lappend mged_gui($id,views) $vcmds

	.$id.menubar.viewring.select add radiobutton -value $vid -variable view_ring($id) \
	    -label "$center $size $aet" -command "view_ring_goto $id $vid"
	.$id.menubar.viewring.delete add command -label "$center $size $aet" \
	    -command "view_ring_delete $id $vid"

	# remember the last selected radiobutton
	set view_ring($id,prev) $view_ring($id)

	# update radio buttons
	set view_ring($id) $vid
    }
}

proc find_view_index {vid vi_in m} {
    global mged_default
    upvar $vi_in vi

    # find view index of menu entry whose value is $vid
    for {set vi 0} {$vi < $mged_default(max_views)} {incr vi} {
	if {[$m entrycget $vi -value] == $vid} {
	    return 1
	}
    }

    return 0
}

#
# Note - the view index (vi) corresponds to both the menu entry
#				 and the view command list (mged_gui($id,views)) entry.
#				 The view id (vid) is a value that corresponds to one of
#				 the radiobutton entries.
#
proc view_ring_set_view {id vid vi} {
    global mged_gui
    global mged_collaborators
    global view_ring

    # we're collaborating, so update collaborators
    if {[lsearch -exact $mged_collaborators $id] != -1 && \
	    "$mged_gui($id,top).ur" == "$mged_gui($id,active_dm)"} {
	foreach cid $mged_collaborators {
	    if {"$mged_gui($cid,top).ur" == "$mged_gui($cid,active_dm)"} {
		set view_ring($cid,prev) $view_ring($cid)
		set view_ring($cid) $vid
		winset $mged_gui($cid,active_dm)
		eval [lindex $mged_gui($cid,views) $vi]
	    }
	}
    } else {
	set view_ring($id,prev) $view_ring($id)
	set view_ring($id) $vid
	winset $mged_gui($id,active_dm)
	eval [lindex $mged_gui($id,views) $vi]
    }
}

proc view_ring_delete {id vid} {
    global mged_gui
    global mged_default
    global view_ring
    global mged_collaborators

    #		 winset $mged_gui($id,active_dm)

    if {![find_view_index $vid vi .$id.menubar.viewring.select]} {
	return
    }

    # we're collaborating, so update collaborators
    if {[lsearch -exact $mged_collaborators $id] != -1} {
	foreach cid $mged_collaborators {
	    .$cid.menubar.viewring.select delete $vi
	    .$cid.menubar.viewring.delete delete $vi
	    set mged_gui($cid,views) [lreplace $mged_gui($cid,views) $vi $vi]
	    set view_ring($cid) 0
	    set view_ring($cid,prev) 0
	}
    } else {
	.$id.menubar.viewring.select delete $vi
	.$id.menubar.viewring.delete delete $vi
	set mged_gui($id,views) [lreplace $mged_gui($id,views) $vi $vi]
	set view_ring($id) 0
	set view_ring($id,prev) 0
    }
}

#
# This gets called when the .$id.menubar.viewring.select menu is posted
# to capture the current value of view_ring($id) before it gets
# modified by selecting one of the entries (i.e. view_ring($id) is tied
# to the radiobuttons).
#
proc view_ring_save_curr {id} {
    global view_ring

    set view_ring($id,curr) $view_ring($id)
}

proc view_ring_goto {id vid} {
    global mged_gui
    global mged_default
    global view_ring
    global mged_collaborators

    #		 winset $mged_gui($id,active_dm)

    if {![find_view_index $vid vi .$id.menubar.viewring.select]} {
	return
    }

    # Since view_ring(id) has been modified by the radiobutton, we'll put
    # it back the way it was for now. view_ring_set_view will restore it again.
    # This is done so that view_ring(id,prev) gets updated properly.
    set view_ring($id) $view_ring($id,curr)
    view_ring_set_view $id $vid $vi
}

proc view_ring_next {id} {
    global mged_gui
    global view_ring

    #		 winset $mged_gui($id,active_dm)

    # find view index of menu entry whose value is $view_ring($id)
    if {![find_view_index $view_ring($id) vi .$id.menubar.viewring.select]} {
	return
    }

    # advance view index to next
    incr vi

    # see if we have to wrap
    if {[.$id.menubar.viewring.select index end] < $vi} {
	set vi 0
    }

    set vid [.$id.menubar.viewring.select entrycget $vi -value]
    view_ring_set_view $id $vid $vi
}

proc view_ring_prev {id} {
    global mged_gui
    global view_ring

    #		 winset $mged_gui($id,active_dm)

    # find view index of menu entry whose value is $view_ring($id)
    if {![find_view_index $view_ring($id) vi .$id.menubar.viewring.select]} {
	return
    }

    # advance view index to next
    incr vi -1

    # see if we have to wrap
    if {$vi < 0} {
	set vi [.$id.menubar.viewring.select index end]
    }

    set vid [.$id.menubar.viewring.select entrycget $vi -value]
    view_ring_set_view $id $vid $vi
}

proc view_ring_toggle {id} {
    global mged_gui
    global view_ring

    # validate view_ring(id)
    if {![find_view_index $view_ring($id) vi .$id.menubar.viewring.select]} {
	return
    }

    # validate view_ring(id,prev) and find its corresponding menu view index
    if {![find_view_index $view_ring($id,prev) vi_prev .$id.menubar.viewring.select]} {
	return
    }

    view_ring_set_view $id $view_ring($id,prev) $vi_prev
}

proc view_ring_copy {from to} {
    global mged_gui
    global view_ring

    # first, delete all menu entries in select and delete menus
    .$to.menubar.viewring.select delete 0 end
    .$to.menubar.viewring.delete delete 0 end

    # update list of views
    set mged_gui($to,views) $mged_gui($from,views)

    # redo the select and delete menus
    set len [llength $mged_gui($to,views)]
    for {set i 0} {$i < $len} {incr i} {
	# get the label from the from_menu
	set label [.$from.menubar.viewring.select entrycget $i -label]

	# get the value/view_id from the from_menu
	set vid [.$from.menubar.viewring.select entrycget $i -value]

	# recreate the entries for the select and delete menus
	.$to.menubar.viewring.select add radiobutton -value $vid -variable view_ring($to) \
	    -label $label -command "view_ring_goto $to $vid"
	.$to.menubar.viewring.delete add command -label $label \
	    -command "view_ring_delete $to $vid"
    }
}

proc update_view_ring_entries { id m } {
    global view_ring

    if {0} {
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
	    puts "Usage: update_view_ring_entries w s|d"
	}
    }
}

proc update_view_ring_labels { id } {
    global mged_gui
    global view_ring

    if {0} {
	if {[_mged_opendb] == ""} {
	    error "No database has been opened!"
	}

	winset $mged_gui($id,active_dm)
	set view_ring($id) [view_ring get]
	set views [view_ring get -a]
	set llen [llength $views]

	# we need to also save the previous view so that
	# toggle will continue to work
	view_ring toggle
	set prev [view_ring get]

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

	# restore both previous and current views
	view_ring goto $prev
	view_ring goto $view_ring($id)
    }
}

proc toggle_status_bar { id } {
    global mged_gui

    if {$mged_gui($id,show_status)} {
	grid .$id.status -sticky ew -row 2 -column 0
    } else {
	grid forget .$id.status
    }
}

proc set_transform { id } {
    global mged_gui
    global transform

    winset $mged_gui($id,top).ul
    set transform $mged_gui($id,transform)
    default_mouse_bindings $mged_gui($id,top).ul

    winset $mged_gui($id,top).ur
    set transform $mged_gui($id,transform)
    default_mouse_bindings $mged_gui($id,top).ur

    winset $mged_gui($id,top).ll
    set transform $mged_gui($id,transform)
    default_mouse_bindings $mged_gui($id,top).ll

    winset $mged_gui($id,top).lr
    set transform $mged_gui($id,transform)
    default_mouse_bindings $mged_gui($id,top).lr

    winset $mged_gui($id,active_dm)
}

proc do_rebind_keys { id } {
    global mged_gui

    bind $mged_gui($id,top).ul <Control-n> "winset $mged_gui($id,top).ul; view_ring_next $id; break"
    bind $mged_gui($id,top).ur <Control-n> "winset $mged_gui($id,top).ur; view_ring_next $id; break"
    bind $mged_gui($id,top).ll <Control-n> "winset $mged_gui($id,top).ll; view_ring_next $id; break"
    bind $mged_gui($id,top).lr <Control-n> "winset $mged_gui($id,top).lr; view_ring_next $id; break"

    bind $mged_gui($id,top).ul <Control-p> "winset $mged_gui($id,top).ul; view_ring_prev $id; break"
    bind $mged_gui($id,top).ur <Control-p> "winset $mged_gui($id,top).ur; view_ring_prev $id; break"
    bind $mged_gui($id,top).ll <Control-p> "winset $mged_gui($id,top).ll; view_ring_prev $id; break"
    bind $mged_gui($id,top).lr <Control-p> "winset $mged_gui($id,top).lr; view_ring_prev $id; break"

    bind $mged_gui($id,top).ul <Control-t> "winset $mged_gui($id,top).ul; view_ring_toggle $id; break"
    bind $mged_gui($id,top).ur <Control-t> "winset $mged_gui($id,top).ur; view_ring_toggle $id; break"
    bind $mged_gui($id,top).ll <Control-t> "winset $mged_gui($id,top).ll; view_ring_toggle $id; break"
    bind $mged_gui($id,top).lr <Control-t> "winset $mged_gui($id,top).lr; view_ring_toggle $id; break"
}

proc adc { args } {
    global mged_gui
    global transform

    set result [eval _mged_adc $args]

    # toggling ADC on/off
    if { ![llength $args] } {
	set dm_id [winset]
	set tie_list [_mged_tie]
	set id mged

	# see if dm_id is tied to a command window
	foreach pair $tie_list {
	    if { [lindex $pair 1] == $dm_id } {
		set id [lindex $pair 0]
		break
	    }
	}

	if {[info exists mged_gui($id,adc_draw)]} {
	    set mged_gui($id,adc_draw) [_mged_adc draw]
	}

	default_mouse_bindings [winset]
    }

    return $result
}

proc set_listen { id } {
    global mged_gui
    global listen

    mged_apply $id "set listen \$mged_gui($id,listen)"

    # In case things didn't work.
    set mged_gui($id,listen) $listen
}

proc set_fb { id } {
    global mged_gui
    global fb
    global listen

    mged_apply $id "set fb \$mged_gui($id,fb)"
    if {$fb && !$listen} {
	mged_apply $id "set listen 1"
    }

    set mged_gui($id,listen) $listen

    # update raytrace control panel
    rt_update_dest $id
}

proc set_mouse_behavior { id } {
    global mged_gui

    mged_apply $id "set mouse_behavior $mged_gui($id,mouse_behavior); refresh"

    switch $mged_gui($id,mouse_behavior) {
	"o" {
	    # use the entire framebuffer
	    set mged_gui($id,fb_all) 1
	    mged_apply $id "set fb_all 1"
	}
	"p" -
	"r" {
	    # use only the area of the framebuffer specified by the rectangle
	    set mged_gui($id,fb_all) 0
	    mged_apply $id "set fb_all 0"

	    # activate the framebuffer
	    set mged_gui($id,fb) 1
	    set_fb $id
	}
    }

    # update raytrace control panel
    rt_update_src $id
}

proc get_font_height { w } {
    if { [winfo class $w] != "Text" } {
	return 0
    }

    set tmp_font [$w cget -font]

    return [font metrics $tmp_font -linespace]
}

proc get_cmd_win_height { id } {
    global mged_gui

    set fh [get_font_height .$id.t]

    return [expr $fh * $mged_gui($id,num_lines)]
}

proc mged_handle_configure { id } {
    if {[winfo exists .$id.rt]} {
	rt_handle_configure $id
    }
}

proc mged_draw_grid {id} {
    global mged_gui

    winset $mged_gui($id,active_dm)
    mged_apply $id "rset grid draw $mged_gui($id,grid_draw)"

    # Reconcile the Tcl grid_draw with the internal grid_draw.
    # Note - the internal grid_draw cannot be set to 1 if a database
    #				 is not currently open (i.e. dbip == DBI_NULL).
    set mged_gui($id,grid_draw) [rset grid draw]
}

proc call_dbupgrade {id} {
    catch {dbupgrade} msg

    if {$msg != ""} {
	# Calling with .$id instead of .$id.t to fool it into
	# adding the msg text
	distribute_text .$id dbupgrade $msg
    }
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
