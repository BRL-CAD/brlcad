#!/bin/sh
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

    return $mged_answer
}

proc x_coord {string} {
    lindex $string 0
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
	    #.options.menu entryconfigure "Link to MGED..." -state disabled
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
	    label .mdl_lbl -text "Model Coordinates"
	    #
	    frame .xline
	    label .xlbl -text "x"
	    entry .xentry -textvariable x_value
	    pack .xlbl .xentry -side left -in .xline
	    #
	    frame .yline
	    label .ylbl -text "y"
	    entry .yentry -textvariable y_value
	    pack .ylbl .yentry -side left -in .yline
	    #
	    frame .zline
	    label .zlbl -text "z"
	    entry .zentry -textvariable z_value
	    pack .zlbl .zentry -side left -in .zline
	    #
	    pack .mdl_lbl .xline .yline .zline -side top -in .origin_mdl
	frame .origin_view
	    label .view_lbl -text "View Coordinates"
	    #
	    frame .hline
	    label .hlbl -text "h"
	    entry .hentry -textvariable h_value
	    pack .hlbl .hentry -side left -in .hline
	    #
	    frame .vline
	    label .vlbl -text "v"
	    entry .ventry -textvariable v_value
	    pack .vlbl .ventry -side left -in .vline
	    #
	    frame .dline
	    label .dlbl -text "d"
	    entry .dentry -textvariable d_value
	    pack .dlbl .dentry -side left -in .dline
	    #
	    pack .view_lbl .hline .vline .dline -side top -in .origin_view
	pack .origin_lbl -side top -in .origin
	pack .origin_mdl .origin_view -side left -in .origin

    frame .direc -relief raised -borderwidth 4
	label .direc_lbl -text "Ray Direction"
	frame .direc_mdl
	    label .dir_lbl -text "Model Coordinates"
	    #
	    frame .dxline
	    label .dxlbl -text "dx"
	    entry .dxentry -textvariable x_value
	    pack .dxlbl .dxentry -side left -in .dxline
	    #
	    frame .dyline
	    label .dylbl -text "dy"
	    entry .dyentry -textvariable y_value
	    pack .dylbl .dyentry -side left -in .dyline
	    #
	    frame .dzline
	    label .dzlbl -text "dz"
	    entry .dzentry -textvariable z_value
	    pack .dzlbl .dzentry -side left -in .dzline
	    #
	    pack .dir_lbl .dxline .dyline .dzline -side top -in .direc_mdl
	frame .az_el
	    label .az_el_lbl -text "Azimuth & Elevation"
	    #
	    frame .az_line
	    label .az_lbl -text "az"
	    entry .az_entry -textvariable h_value
	    pack .az_lbl .az_entry -side left -in .az_line
	    #
	    frame .el_line
	    label .el_lbl -text "el"
	    entry .el_entry -textvariable v_value
	    pack .el_lbl .el_entry -side left -in .el_line
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

