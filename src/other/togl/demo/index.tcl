#!/bin/sh
# the next line restarts using tclsh \
exec tclsh "$0" "$@"

# $Id$

# Togl - a Tk OpenGL widget
# Copyright (C) 1996  Brian Paul and Ben Bederson
# Copyright (C) 2006-2007  Greg Couch
# See the LICENSE file for copyright details.


# A Tk/OpenGL widget demo using color-index mode.

package provide index 1.0

# add parent directory to path to find Togl's pkgIndex in current directory
if { [file exists pkgIndex.tcl] } {
    set auto_path [linsert $auto_path 0 ..]
}
# following load also loads Tk and Togl packages
load [file dirname [info script]]/index[info sharedlibextension]

# create ::index namespace
namespace eval ::index {
}

proc ::index::setup {} {
    wm title . "Color index demo"

    togl .win -width 200 -height 200 -rgba false -double true -privatecmap false -time 10 -timer ::index::timer_cb -create ::index::create_cb -reshape ::index::reshape_cb -display ::index::display_cb
    button .photo -text "Take Photo" -command ::index::take_photo
    button .btn -text Quit -command exit

    pack .win -expand true -fill both
    pack .photo -expand true -fill both
    pack .btn -expand true -fill both
}

proc ::index::take_photo {} {
	image create photo img
	.win takephoto img
	img write image.ppm -format ppm
}


# Execution starts here!
if { [info script] == $argv0 } {
	::index::setup
}
