#!/bin/sh
# the next line restarts using tclsh \
exec tclsh "$0" "$@"

# $Id$

# Togl - a Tk OpenGL widget
# Copyright (C) 1996  Brian Paul and Ben Bederson
# Copyright (C) 2006-2007  Greg Couch
# See the LICENSE file for copyright details.


# An Tk/OpenGL widget demo with two double-buffered OpenGL windows.
# The first shows the aliased object, the second show the results of
# rendering the same object in a higher resolution Pbuffer and using
# texture mapping to antialias it.

package provide pbuffer 1.0

# add parent directory to path to find Togl's pkgIndex in current directory
if { [file exists pkgIndex.tcl] } {
    set auto_path [linsert $auto_path 0 ..]
}
# following load also loads Tk and Togl packages
load [file dirname [info script]]/pbuffer[info sharedlibextension]

# create ::pbuffer namespace
namespace eval ::pbuffer {
}

proc pbuffer::setup {} {
    wm title . "Pbuffer test"

#debug
    image create photo test
    image create photo test2
#end debug

    # create first Togl widget
    togl .o1 -width 300 -height 300 -rgba true -double true -depth true -ident main -create create_cb -reshape reshape_cb -display display_cb
    label .l1 -text "RGB, Z, double"

    # create second Togl widget, share display lists with first widget, no depth
    togl .o2 -width 300 -height 300 -rgba true -double true -sharelist main -reshape reshape2_cb -display display2_cb -ident second
    setOutput .o2
    label .l2 -text "RGB from pbuffer texture"

    # create off-screen pbuffer, share display lists with other widgets
    # must power of 2 squared in size
    togl .pbuf -width 2048 -height 2048 -rgba true -depth true -sharelist main -pbuffer 1 -reshape reshape_cb -display pbuffer_cb -ident pbuffer

    scale .sx -label {X Axis} -from 0 -to 360 -command {::pbuffer::setAngle x} -orient horizontal
    scale .sy -label {Y Axis} -from 0 -to 360 -command {::pbuffer::setAngle y} -orient horizontal
    button .btn -text Quit -command exit

    bind .o1 <B1-Motion> {
	::pbuffer::motion_event [lindex [%W config -width] 4] \
		     [lindex [%W config -height] 4] \
		     %x %y
    }

    bind .o2 <B1-Motion> {
	::pbuffer::motion_event [lindex [%W config -width] 4] \
		     [lindex [%W config -height] 4] \
		     %x %y
    }

    grid rowconfigure . 0 -weight 1
    grid columnconfigure . 0 -weight 1 -uniform same
    grid columnconfigure . 1 -weight 1 -uniform same
    grid .o1 -row 0 -column 0 -sticky nesw -padx 3 -pady 3
    grid .o2 -row 0 -column 1 -sticky nesw -padx 3 -pady 3
    grid .l1 -row 1 -column 0 -sticky ew -padx 3 -pady 3
    grid .l2 -row 1 -column 1 -sticky ew -padx 3 -pady 3
    grid .sx -row 2 -column 0 -columnspan 2 -sticky ew
    grid .sy -row 3 -column 0 -columnspan 2 -sticky ew
    grid .btn -row 4 -column 0 -columnspan 2 -sticky ew
}


proc pbuffer::display { } {
	pbuffer_cb .pbuf
	.o2 postredisplay
}


# This is called when mouse button 1 is pressed and moved in either of
# the OpenGL windows.
proc pbuffer::motion_event { width height x y } {
    .sx set [setXrot [expr 360.0 * $y / $height]]
    .sy set [setYrot [expr 360.0 * ($width - $x) / $width]]

    .o1 postredisplay
    .pbuf postredisplay
}

# This is called when a slider is changed.
proc pbuffer::setAngle {axis value} {
    global xAngle yAngle zAngle

    switch -exact $axis {
	x {setXrot $value
	   setXrot $value}
	y {setYrot $value
	   setYrot $value}
    }

    .o1 postredisplay
    .pbuf postredisplay
}

# Execution starts here!
if { [info script] == $argv0 } {
	::pbuffer::setup
}
