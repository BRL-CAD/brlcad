#!/bin/sh
#
# restart with wish \
exec wish "$0" "$@"

wm geometry . {+1-20}
# wm positionfrom . program

#
#	XXX	for now am hardwired to grab Lee's menu package
#
#

source ~butler/src/Tcl/menuPackage.tcl

########################################################################
##
##	Global Variables
##
##
##
########################################################################

# Location to use as landmark
set landmarkmode {View center}

# Draw caliper legs in geometry windows?
set show_cal_legs	0

set cal_leg_loc(1) "1 2 3"
set cal_leg_loc(2) "0 0 0"

# Database editing units
set units "millimeters"

# Location about which rotations occur
#	ViewCenter, Eye, Origin, Keypoint
set rotplace {View Center}

# Absolute/Rate based manipulations
set absrate Position

# What transformation are we modifying:  View, Solid Params, ADC values
set talon {View}

# Transformation in View vs Model coordinates.  
# XXX How does this relate to the "talon" variable.  Could this be
#	folded in to the talon variable's function?
set modelview "View"

proc null_proc {} {

}

########################################################################
##
##	Menu Creation
##
########################################################################
proc new_proc {} {
	puts "New ..."
}

proc open_proc {} {
	puts "Open ..."
}

proc insert_proc {} {
	puts "Insert ..."
}

proc extract_proc {} {
	puts "Extract ..."
}

proc ray_proc {} {
	puts "Raytrace ..."
}

proc save_as {type} {
puts "Save As $type"
}

MenuSetup

Menu		Mode	left	0
MenuRadio	Mode	{View center}		landmarkmode
MenuRadio	Mode	{Model origin}		landmarkmode
MenuCascade	Mode	{Solid parameter}
MenuRadio	{Solid parameter}	V	landmarkmode
MenuRadio	{Solid parameter}	A	landmarkmode
MenuRadio	{Solid parameter}	B	landmarkmode
MenuRadio	{Solid parameter}	C	landmarkmode
MenuRadio	{Solid parameter}	D	landmarkmode
MenuEntryConfigure Mode {Solid parameter} -state disabled
MenuRadio	Mode	Keypoint		landmarkmode
MenuEntryConfigure Mode Keypoint -state disabled
MenuRadio	Mode	History			landmarkmode
MenuEntryConfigure Mode History -state disabled



proc solid_select {} {

tk_dialog .d about {solid list...} {} 0 {Solid Click} OK Dismiss
# MenuCommand	Edit	{Solid Click}		solid_click	0
# MenuCommand	Edit	{Solid Illum}		solid_illum	0

}


proc matrix_select {} {
tk_dialog .d about {matrix list...} {} 0 {Matrix Click} OK Dismiss
# MenuCommand	Edit	{Matrix Illum}		matrix_illum	0
}

#MenuCommand	Edit	{Edit Solid}	solid_select	5
#MenuCommand	Edit	{Edit Matrix}	matrix_select	5
#MenuSeparator	Edit
#MenuCommand	Edit	Reject		{ puts "reject_edit" }	0
#MenuCommand	Edit	Accept		{ puts "accept_edit" }	0


Menu		Options	left	0
MenuCheck	Options {Display legs}	show_cal_legs

Menu		Help	right	0
MenuCommand	Help	About		{tk_dialog .d about {BRL-CAD MGED} {} 0 Dismiss}
MenuCommand	Help	{On Context}	{tk_dialog .d about {MGED Context Help} {} 0 Dismiss}	0
MenuCommand	Help	Commands...	{tk_dialog .d about {MGED Commands} {} 0 Dismiss}	0
MenuCommand	Help	Apropos...	{tk_dialog .d about {MGED Apropos Command} {} 0 Dismiss}	0
MenuCommand	Help	Manual...	{tk_dialog .d about {MGED Online Manual} {} 0 Dismiss}	0


proc new_proc {} {
	puts "new"
}

frame .mbar -relief raised -bd 2
menubutton .mbar.mode -text Mode -menu .mbar.mode.menu
    menu .mbar.mode.menu
    .mbar.mode.menu add radiobutton -label "View center" \
	-variable landmarkmode
    .mbar.mode.menu add radiobutton -label "Model origin" \
	-variable landmarkmode
    .mbar.mode.menu add cascade -label "Solid parameter" \
	-menu .mbar.mode.menu.params
	menu .mbar.mode.menu.params
	.mbar.mode.menu.params add radiobutton -label V -variable landmarkmode
	.mbar.mode.menu.params add radiobutton -label A -variable landmarkmode
	.mbar.mode.menu.params add radiobutton -label B -variable landmarkmode
	.mbar.mode.menu.params add radiobutton -label C -variable landmarkmode
	.mbar.mode.menu.params add radiobutton -label D -variable landmarkmode
    .mbar.mode.menu add radiobutton -label Keypoint -variable landmarkmode
    .mbar.mode.menu add radiobutton -label History -variable landmarkmode
    .mbar.mode.menu entryconfigure {Solid parameter} -state disabled
    .mbar.mode.menu entryconfigure Keypoint -state disabled
    .mbar.mode.menu entryconfigure History -state disabled
menubutton .mbar.options -text Options -menu .mbar.options.menu
    menu .mbar.options.menu
    .mbar.options.menu add checkbutton -label "Display legs" \
				       -variable show_cal_legs
button .mbar.dismiss -bd 0 -text Dismiss -command exit
#
pack .mbar.mode .mbar.options -side left -in .mbar
pack .mbar.dismiss -side right -in .mbar
pack .mbar -fill x -side top

########################################################################
##
##	Main Window
##
########################################################################

proc caliper_leg {leg} {
    global cal_leg_loc

    set cal_leg_loc($leg) {1 2 3}
}

frame .leg1line
button .leg1 -text "Place leg 1" -command {caliper_leg 1}
label .leg1val -textvariable cal_leg_loc(1)
pack .leg1 .leg1val -side left -in .leg1line

frame .leg2line
button .leg2 -text "Place leg 2" -command {caliper_leg 2}
label .leg2val -textvariable cal_leg_loc(2)
pack .leg2 .leg2val -side left -in .leg2line


frame .modeline
label .modelabel -text "Leg placement mode:"
tk_optionMenu .modemenu landmarkmode {View center} {Model origin} {Solid parameter} Keypoint History
.modemenu.menu entryconfigure {Solid parameter} -state disabled
.modemenu.menu entryconfigure Keypoint -state disabled
.modemenu.menu entryconfigure History -state disabled
pack .modelabel .modemenu -side left -in .modeline


#label .mode_id -textvariable landmarkmode


pack .mbar .leg1line .leg2line .modeline -side top

