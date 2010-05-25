#!/bin/sh
# the next line restarts using tclsh \
exec tclsh "$0" "$@"

# $Id$

# Togl - a Tk OpenGL widget
# Copyright (C) 1996  Brian Paul and Ben Bederson
# Copyright (C) 2006-2007  Greg Couch
# See the LICENSE file for copyright details.


# An Tk/OpenGL widget demo with two windows, one single buffered and the
# other double buffered.

package provide double 1.0

# add parent directory to path to find Togl's pkgIndex in current directory
if { [file exists pkgIndex.tcl] } {
    set auto_path [linsert $auto_path 0 ..]
}
# following load also loads Tk and Togl packages
load [file dirname [info script]]/double[info sharedlibextension]

# create ::double namespace
namespace eval ::double {
}

proc double::setup {} {
    wm title . "Single vs Double Buffering"

    # create first Togl widget
    togl .o1 -width 200 -height 200 -rgba true -double false -depth true -ident "Single Buffered" -create double::create_cb -display double::display_cb -reshape double::reshape_cb

    # create second Togl widget, share display lists with first widget
    togl .o2 -width 200 -height 200 -rgba true -double true -depth true -ident "Double Buffered" -sharelist "Single Buffered" -create double::create_cb -display double::display_cb -reshape double::reshape_cb

    scale .sx -label {X Axis} -from 0 -to 360 -command {::double::setAngle x} -orient horizontal
    scale .sy -label {Y Axis} -from 0 -to 360 -command {::double::setAngle y} -orient horizontal
    button .btn -text Quit -command exit

    bind .o1 <B1-Motion> {
	::double::motion_event [lindex [%W config -width] 4] \
		     [lindex [%W config -height] 4] \
		     %x %y
    }

    bind .o2 <B1-Motion> {
	::double::motion_event [lindex [%W config -width] 4] \
		     [lindex [%W config -height] 4] \
		     %x %y
    }

    grid rowconfigure . 0 -weight 1
    grid columnconfigure . 0 -weight 1 -uniform same
    grid columnconfigure . 1 -weight 1 -uniform same
    grid .o1 -row 0 -column 0 -sticky nesw -padx 3 -pady 3
    grid .o2 -row 0 -column 1 -sticky nesw -padx 3 -pady 3
    #grid .l1 -row 1 -column 0 -sticky ew -padx 3 -pady 3
    #grid .l2 -row 1 -column 1 -sticky ew -padx 3 -pady 3
    grid .sx -row 2 -column 0 -columnspan 2 -sticky ew
    grid .sy -row 3 -column 0 -columnspan 2 -sticky ew
    grid .btn -row 4 -column 0 -columnspan 2 -sticky ew
}



# This is called when mouse button 1 is pressed and moved in either of
# the OpenGL windows.
proc double::motion_event { width height x y } {
    .sx set [double::setXrot [expr 360.0 * $y / $height]]
    .sy set [double::setYrot [expr 360.0 * ($width - $x) / $width]]

    .o1 postredisplay
    .o2 postredisplay
}

# This is called when a slider is changed.
proc double::setAngle {axis value} {
    global xAngle yAngle zAngle

    switch -exact $axis {
	x {double::setXrot $value
	   double::setXrot $value}
	y {double::setYrot $value
	   double::setYrot $value}
    }

    .o1 postredisplay
    .o2 postredisplay
}

# Execution starts here!
if { [info script] == $argv0 } {
	::double::setup
}
