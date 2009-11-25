#!/bin/sh
# the next line restarts using tclsh \
exec tclsh "$0" "$@"

# $Id$

# Togl - a Tk OpenGL widget
# Copyright (C) 1996  Brian Paul and Ben Bederson
# Copyright (C) 2006-2007  Greg Couch
# See the LICENSE file for copyright details.


# A Tk/OpenGL widget demo using an overlay.

# add parent directory to path to find Togl's pkgIndex in current directory
if { [file exists pkgIndex.tcl] } {
    set auto_path [linsert $auto_path 0 ..]
}
# following load also loads Tk and Togl packages
load [file dirname [info script]]/overlay[info sharedlibextension]

proc setup {} {
    wm title . "Overlay demo"

    togl .win -width 200 -height 200 -rgba true -double false -overlay true -create create_cb -reshape reshape_cb -display display_cb -overlaydisplay overlay_display_cb
    button .btn -text Quit -command exit

    pack .win -expand true -fill both
    pack .btn -expand true -fill both
}


# Execution starts here!
# Execution starts here!
if { [info script] == $argv0 } {
	setup
}
