#!/bin/sh
#                    P R O T O T Y P E . S H
# BRL-CAD
#
# Copyright (c) 2004-2007 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#
###
#
# restart with wish \
exec wish "$0" "$@"

wm title . "NIRT"

# wm geometry . {+1-20}
# wm positionfrom . program


########################################################################
#
#	Global Variables
#
########################################################################

set units	"millimeters"
set use_air	0

set x_value	0.0
set y_value	0.0
set z_value	0.0
set h_value	0.0
set v_value	0.0
set d_value	0.0
set az_value	0.0
set el_value	0.0

# Draw caliper legs in geometry windows?
set show_cal_legs	0

# Location of each leg
set cal_leg_loc(1) "1 2 3"
set cal_leg_loc(2) "0 0 0"
set cal_dist ""

########################################################################
#
#	Useful procs
#
########################################################################

proc Tcl_command {} {

    global do_it
    global Tcl_string

    set do_it 0
    toplevel .tcwin
    wm title .tcwin "Tcl command panel"
    frame .tcwin.eline
    frame .tcwin.buttons
    label .tcwin.elabel -text "Tcl string to interpret:"
    entry .tcwin.eentry -width 40 -textvariable Tcl_string
    bind .tcwin.eentry <Return> {set do_it 1; destroy .tcwin}
    button .tcwin.ok -text "OK" -command {set do_it 1; destroy .tcwin}
    button .tcwin.cancel -text "Cancel" -command {destroy .tcwin}
    button .tcwin.clear -text "Clear" -command {.tcwin.eentry delete 0 end}
    pack .tcwin.elabel .tcwin.eentry -in .tcwin.eline -side left
    pack .tcwin.cancel .tcwin.clear .tcwin.ok -in .tcwin.buttons -side left
    pack .tcwin.eline .tcwin.buttons -in .tcwin -side top
    bind .tcwin.elabel <Button-3> {context_help "tc command"}
    bind .tcwin.eentry <Button-3> {context_help "tc command"}
    bind .tcwin.cancel <Button-3> {context_help "tc cancel"}
    bind .tcwin.clear <Button-3> {context_help "tc clear"}
    bind .tcwin.ok <Button-3> {context_help "tc ok"}

    tkwait window .tcwin

    if {$do_it == 1} {
        if {$Tcl_string != ""} {
            printf [eval $Tcl_string]
        }
    }
}

proc printf {string} {
    .rslt_txt insert end $string
}

proc clearf {} {
    .rslt_txt delete 1.0 end
}

proc shoot_a_ray {} {
    set time [exec "date"]

    clearf
    printf "Shooting a ray at $time...\n"

    exec nirt ~pjt/mged/scene.g scene

}

proc select_an_mged {mgeds} {

    global mged_answer

    set mged_answer ""
    toplevel .mgeds
    wm title .mgeds "NIRT MGED selector"
    #
    listbox .mgeds.list
    foreach app $mgeds { .mgeds.list insert end $app }
    #
    button .mgeds.cancel -text "Cancel" -command {destroy .mgeds}
    pack .mgeds.list .mgeds.cancel -in .mgeds
    bind .mgeds.list <Double-Button-1> {
	set mged_answer [selection get]
	destroy .mgeds
    }
}

proc link_to_mged {} {

    global mged_answer

    set this_nirt [winfo name .]
    printf "We are $this_nirt\n"

    #
    #	See how many MGEDs there are
    #
    set mgeds []
    foreach app [winfo interps] {
	if {[regexp {mged.*} $app]} {
	    lappend mgeds $app
	}
    }
    if {[llength $mgeds] == 0} {
	printf "There are no mgeds\n"
	return ""
    } elseif {[llength $mgeds] == 1} {
	set mged_answer [lindex $mgeds 0]
    } else {
	select_an_mged $mgeds
	tkwait window .mgeds
	if {$mged_answer == ""} {
	    return ""
	}
    }

    send $mged_answer {puts "Hello from NIRT!\n"}
    send $mged_answer "send $this_nirt \{puts \"Hello from us\\n\"\}"

    set result [send $mged_answer {ae 35 25}]
    printf "The result of the transaction was: <$result>\n"

    return $mged_answer
}

proc set_xyz {} {

    global x_value
    global y_value
    global z_value

    toplevel .xyz
    wm title .xyz "NIRT Set xyz"

    set x_tmp $x_value
    set y_tmp $y_value
    set z_tmp $z_value

    frame .xyz.xline
	label .xyz.xlbl -text "x"
	entry .xyz.xentry -textvariable x_tmp
	pack .xyz.xlbl .xyz.xentry -side left -in .xyz.xline
    frame .xyz.yline
	label .xyz.ylbl -text "y"
	entry .xyz.yentry -textvariable y_tmp
	pack .xyz.ylbl .xyz.yentry -side left -in .xyz.yline
    frame .xyz.zline
	label .xyz.zlbl -text "z"
	entry .xyz.zentry -textvariable z_tmp
	pack .xyz.zlbl .xyz.zentry -side left -in .xyz.zline
    pack .xyz.xline .xyz.yline .xyz.zline

    frame .xyz.bbar
	button .xyz.ok -text "OK" \
	    -command {
		set x_value $x_tmp
		set y_value $y_tmp
		set z_value $z_tmp
		destroy .xyz
	    }
	button .xyz.cancel -text "Cancel" -command {destroy .xyz}
	pack .xyz.ok .xyz.cancel -side left -in .xyz.bbar
    pack .xyz.bbar
}

proc y_coord {string} {
    lindex $string 1
}

proc z_coord {string} {
    lindex $string 2
}

proc calculate_distance {} {
    global cal_leg_loc

    set x [expr [x_coord $cal_leg_loc(1)] - [x_coord $cal_leg_loc(2)]]
    set y [expr [y_coord $cal_leg_loc(1)] - [y_coord $cal_leg_loc(2)]]
    set z [expr [z_coord $cal_leg_loc(1)] - [z_coord $cal_leg_loc(2)]]
    puts "The delta is ($x $y $z)"
    set cal_dist [expr sqrt([expr $x*$x + $y*$y + $z*$z])]
    puts "OK, cal_dist is now $cal_dist\n"
}


    #toplevel .zal
    #wm title .zal "MGED Calipers"


########################################################################
#
#	The menu bar
#
########################################################################
frame .mbar -relief raised -bd 2
    menubutton .file -text "File" -menu .file.menu
	menu .file.menu
	    .file.menu add cascade -label "Output to..." \
		-menu .file.menu.dest
		menu .file.menu.dest
		    .file.menu.dest add command -label "Display panel" \
			-command {printf "me\n"}
		    .file.menu.dest add command -label "File..." \
			-command {printf "file\n"}
		    .file.menu.dest add command -label "Pipe..." \
			-command {printf "pipe\n"}
	    .file.menu add command -label "State file"
	    .file.menu add command -label "Dump state"
	    .file.menu add command -label "Load state"
	    .file.menu add command -label "Exit" -command {exit}
    menubutton .options -text Options -menu .options.menu
	menu .options.menu
	    .options.menu add cascade -label "Units..." \
		-menu .options.menu.units
		menu .options.menu.units
		    .options.menu.units add radiobutton -label "micrometers" \
			-variable units
		    .options.menu.units add radiobutton -label "millimeters" \
			-variable units
		    .options.menu.units add radiobutton -label "centimeters" \
			-variable units
		    .options.menu.units add radiobutton -label "meters" \
			-variable units
		    .options.menu.units add radiobutton -label "kilometers" \
			-variable units
		    .options.menu.units add separator
		    .options.menu.units add radiobutton -label "inches" \
			-variable units
		    .options.menu.units add radiobutton -label "feet" \
			-variable units
		    .options.menu.units add radiobutton -label "yards" \
			-variable units
		    .options.menu.units add radiobutton -label "miles" \
			-variable units
	    .options.menu add checkbutton -label "Use air" -variable "use_air"
	    .options.menu add command -label "Link to MGED..." \
		-command link_to_mged
	    .options.menu add command -label "Debug flags..." \
		-command {printf "twiddle bits\n"}
    menubutton .formats -text Formats -menu .formats.menu
	menu .formats.menu
	    .formats.menu add command -label "Set output formats"
	    .formats.menu entryconfigure "Set output formats" -state disabled
    button .tcl -bd 0 -text Tcl -command {Tcl_command}
    button .exit -bd 0 -text Exit -command {exit}
    pack .file .options .formats .tcl -side left -in .mbar
    pack .exit -side right -in .mbar
    pack .mbar -fill x -side top

########################################################################
#
#	The main window
#
########################################################################

frame .rayspec
    frame .origin -relief raised -borderwidth 4
	label .origin_lbl -text "Ray Origin"
	frame .origin_mdl
	    button .mdl_lbl -text "Model Coordinates" \
		-command set_xyz
	    #
	    frame .xline
	    label .xlbl -text "x ="
	    label .x_val -textvariable x_value
	    pack .xlbl .x_val -side left -in .xline
	    #
	    frame .yline
	    label .ylbl -text "y ="
	    label .y_val -textvariable y_value
	    pack .ylbl .y_val -side left -in .yline
	    #
	    frame .zline
	    label .zlbl -text "z ="
	    label .z_val -textvariable z_value
	    pack .zlbl .z_val -side left -in .zline
	    #
	    pack .mdl_lbl .xline .yline .zline -side top -in .origin_mdl
	frame .origin_view
	    button .view_lbl -text "View Coordinates"
	    #
	    frame .hline
	    label .hlbl -text "h ="
	    label .h_val -textvariable h_value
	    pack .hlbl .h_val -side left -in .hline
	    #
	    frame .vline
	    label .vlbl -text "v ="
	    label .v_val -textvariable v_value
	    pack .vlbl .v_val -side left -in .vline
	    #
	    frame .dline
	    label .dlbl -text "d ="
	    label .d_val -textvariable d_value
	    pack .dlbl .d_val -side left -in .dline
	    #
	    pack .view_lbl .hline .vline .dline -side top -in .origin_view
	pack .origin_lbl -side top -in .origin
	pack .origin_mdl .origin_view -side left -in .origin

    frame .direc -relief raised -borderwidth 4
	label .direc_lbl -text "Ray Direction"
	frame .direc_mdl
	    button .dir_lbl -text "Model Coordinates"
	    #
	    frame .dxline
	    label .dxlbl -text "dx ="
	    label .dx_val -textvariable x_value
	    pack .dxlbl .dx_val -side left -in .dxline
	    #
	    frame .dyline
	    label .dylbl -text "dy ="
	    label .dy_val -textvariable y_value
	    pack .dylbl .dy_val -side left -in .dyline
	    #
	    frame .dzline
	    label .dzlbl -text "dz ="
	    label .dz_val -textvariable z_value
	    pack .dzlbl .dz_val -side left -in .dzline
	    #
	    pack .dir_lbl .dxline .dyline .dzline -side top -in .direc_mdl
	frame .az_el
	    button .az_el_lbl -text "Azimuth & Elevation"
	    #
	    frame .az_line
	    label .az_lbl -text "az ="
	    label .az_val -textvariable h_value
	    pack .az_lbl .az_val -side left -in .az_line
	    #
	    frame .el_line
	    label .el_lbl -text "el ="
	    label .el_val -textvariable v_value
	    pack .el_lbl .el_val -side left -in .el_line
	    #
	    pack .az_el_lbl .az_line .el_line -side top -in .az_el
	pack .direc_lbl -side top -in .direc
	pack .direc_mdl .az_el -side left -in .direc

    pack .origin .direc -ipadx 1m -ipady 1m -side left -in .rayspec
pack .rayspec -side top

frame .btnbar
    button .backout -text "Back out" -command {printf "Backing out...\n"}
    button .shoot -text "Shoot" -command {shoot_a_ray}
    pack .backout -side left -in .btnbar
    pack .shoot -side right -in .btnbar
pack .btnbar -fill x -side top

frame .result -relief raised -bd 2
    text .rslt_txt -yscrollcommand {.rslt_y set}
#	.rslt_txt insert end "Here are 100 lines\n"
#	for {set i 0} {$i < 100} {incr i} {
#	    .rslt_txt insert end " Line number $i\n"
#	}
    pack .rslt_txt -side left -in .result
    scrollbar .rslt_y -command {.rslt_txt yview}
    pack .rslt_y -side right -fill y -in .result
pack .result -side top


# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
