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

    frame .f1

    # create first Togl widget
    togl .f1.o1 -width 200 -height 200 -rgba true -double false -depth true -ident Single -create create_cb -display display_cb -reshape reshape_cb

    # create second Togl widget, share display lists with first widget
    togl .f1.o2 -width 200 -height 200 -rgba true -double true -depth true -ident Double -sharelist Single -create create_cb -display display_cb -reshape reshape_cb

    scale .sx -label {X Axis} -from 0 -to 360 -command {::double::setAngle x} -orient horizontal
    scale .sy -label {Y Axis} -from 0 -to 360 -command {::double::setAngle y} -orient horizontal
    button .btn -text Quit -command exit

    bind .f1.o1 <B1-Motion> {
	::double::motion_event [lindex [%W config -width] 4] \
		     [lindex [%W config -height] 4] \
		     %x %y
    }

    bind .f1.o2 <B1-Motion> {
	::double::motion_event [lindex [%W config -width] 4] \
		     [lindex [%W config -height] 4] \
		     %x %y
    }

    pack .f1.o1 .f1.o2 -side left -padx 3 -pady 3 -fill both -expand t
    pack .f1 -fill both -expand t
    pack .sx -fill x
    pack .sy -fill x
    pack .btn -fill x
}



# This is called when mouse button 1 is pressed and moved in either of
# the OpenGL windows.
proc double::motion_event { width height x y } {
    setXrot .f1.o1 [expr 360.0 * $y / $height]
    setXrot .f1.o2 [expr 360.0 * $y / $height]
    setYrot .f1.o1 [expr 360.0 * ($width - $x) / $width]
    setYrot .f1.o2 [expr 360.0 * ($width - $x) / $width]

#    .sx set [expr 360.0 * $y / $height]
#    .sy set [expr 360.0 * ($width - $x) / $width]

    .sx set [getXrot]
    .sy set [getYrot]
}

# This is called when a slider is changed.
proc double::setAngle {axis value} {
    global xAngle yAngle zAngle

    switch -exact $axis {
	x {setXrot .f1.o1 $value
	   setXrot .f1.o2 $value}
	y {setYrot .f1.o1 $value
	   setYrot .f1.o2 $value}
    }
}

# Execution starts here!
if { [info script] == $argv0 } {
	::double::setup
}
