#                    C A L I P E R S . T C L
# BRL-CAD
#
# Copyright (c) 2004-2006 United States Government as represented by
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
# wm geometry . {+1-20}
# wm positionfrom . program

#
#	Ensure that all commands that this script uses without defining
#	are provided by the calling application
#
check_externs "_mged_view"

#	XXX	kludge for edit-test cycle


########################################################################
#
#	Global Variables
#
########################################################################

# How user specifies landmarks
set landmarkmode {View center}

# Draw caliper legs in geometry windows?
set show_cal_legs	0

# Location of each leg
set cal_leg_loc(1) "1 2 3"
set cal_leg_loc(2) "0 0 0"
set cal_dist ""

proc do_caliper_leg {leg} {
    global landmarkmode
    global cal_leg_loc

    if {$landmarkmode == {View center}} {
	set cal_leg_loc($leg) [_mged_view center]
    } elseif {$landmarkmode == {Model origin}} {
	set cal_leg_loc($leg) {0 0 0}
    } else {
	set cal_leg_loc($leg) {1 2 3}
    }
    calculate_distance
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


proc calipers {} {
    global landmarkmode
    global show_cal_legs

    toplevel .cal
    wm title .cal "MGED Calipers"


########################################################################
#
#	The menu bar
#
########################################################################
frame .cal.mbar -relief raised -bd 2
menubutton .cal.mbar.mode -text Mode -menu .cal.mbar.mode.menu
    menu .cal.mbar.mode.menu
    .cal.mbar.mode.menu add radiobutton -label "View center" \
	-variable landmarkmode
    .cal.mbar.mode.menu add radiobutton -label "Model origin" \
	-variable landmarkmode
    .cal.mbar.mode.menu add cascade -label "Primitive parameters" \
	-menu .cal.mbar.mode.menu.params
	menu .cal.mbar.mode.menu.params
	.cal.mbar.mode.menu.params add radiobutton -label V -variable landmarkmode
	.cal.mbar.mode.menu.params add radiobutton -label A -variable landmarkmode
	.cal.mbar.mode.menu.params add radiobutton -label B -variable landmarkmode
	.cal.mbar.mode.menu.params add radiobutton -label C -variable landmarkmode
	.cal.mbar.mode.menu.params add radiobutton -label D -variable landmarkmode
    .cal.mbar.mode.menu add radiobutton -label Keypoint -variable landmarkmode
    .cal.mbar.mode.menu add radiobutton -label History -variable landmarkmode
    .cal.mbar.mode.menu entryconfigure {Primitive parameters} -state disabled
    .cal.mbar.mode.menu entryconfigure Keypoint -state disabled
    .cal.mbar.mode.menu entryconfigure History -state disabled
menubutton .cal.mbar.options -text Options -menu .cal.mbar.options.menu
    menu .cal.mbar.options.menu
    .cal.mbar.options.menu add checkbutton -label "Display legs" \
				       -variable show_cal_legs
button .cal.mbar.dismiss -bd 0 -text Dismiss -command {destroy .cal}
#
pack .cal.mbar.mode .cal.mbar.options -side left -in .cal.mbar
pack .cal.mbar.dismiss -side right -in .cal.mbar
pack .cal.mbar -fill x -side top

########################################################################
#
#	The main window
#
########################################################################

frame .cal.leg1line
button .cal.leg1 -text "Place leg 1" -command {do_caliper_leg 1}
label .cal.leg1val -textvariable cal_leg_loc(1)
pack .cal.leg1 .cal.leg1val -side left -in .cal.leg1line

frame .cal.leg2line
button .cal.leg2 -text "Place leg 2" -command {do_caliper_leg 2}
label .cal.leg2val -textvariable cal_leg_loc(2)
pack .cal.leg2 .cal.leg2val -side left -in .cal.leg2line

frame .cal.distline
label .cal.distlabel -text "Distance:"
label .cal.distance -textvariable cal_dist
pack .cal.distlabel .cal.distance -side left -in .cal.distline


frame .cal.modeline
label .cal.modelabel -text "Leg placement mode:"
tk_optionMenu .cal.modemenu landmarkmode {View center} {Model origin} {Primitive parameters} Keypoint History
.cal.modemenu.menu entryconfigure {Primitive parameters} -state disabled
.cal.modemenu.menu entryconfigure Keypoint -state disabled
.cal.modemenu.menu entryconfigure History -state disabled
pack .cal.modelabel .cal.modemenu -side left -in .cal.modeline


pack .cal.mbar .cal.leg1line .cal.leg2line .cal.distline .cal.modeline -side top -in .cal
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
