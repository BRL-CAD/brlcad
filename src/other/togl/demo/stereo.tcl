#!/bin/sh
# the next line restarts using tclsh \
exec tclsh "$0" "$@"

# $Id$

# Togl - a Tk OpenGL widget
# Copyright (C) 1996  Brian Paul and Ben Bederson
# Copyright (C) 2006-2009  Greg Couch
# See the LICENSE file for copyright details.

# add parent directory to path to find Togl's pkgIndex in current directory
if { [file exists pkgIndex.tcl] } {
    set auto_path [linsert $auto_path 0 ..]
}
# following load also loads Tk and Togl packages
load [file dirname [info script]]/stereo[info sharedlibextension]

# create ::stereo namespace
namespace eval ::stereo {
}

variable stereo::mode none
proc stereo::setup {} {
	grid rowconfigure . 0 -weight 1 -minsize 200p
	grid columnconfigure . 1 -weight 1 -minsize 200p
	labelframe .c -text "Stereo mode:"
	grid .c -padx 2 -pady 2 -ipadx 2 -ipady 1
	foreach {b} {none native sgioldstyle anaglyph cross-eye wall-eye DTI "row interleaved" "left eye" "right eye" } {
		set name [string map {- _ " " _} $b]
		radiobutton .c.b$name -text "$b" -command "::stereo::makeGraphics {$b}" -variable stereo::mode -value "$b"
		pack .c.b$name -padx 2 -pady 1 -anchor w
	}
	scale .sx -label {X Axis} -from 0 -to 360 -command {::stereo::setAngle x} -orient horizontal
	grid .sx -columnspan 2 -sticky ew
	scale .sy -label {Y Axis} -from 0 -to 360 -command {::stereo::setAngle y} -orient horizontal
	grid .sy -columnspan 2 -sticky ew
	if {[string first IRIX $::tcl_platform(os)] != -1} {
		label .irix -justify left -wraplength 250p -text "Use /usr/gfx/setmon or /usr/bin/X11/xsetmon to change video mode for native stereo (eg., 1024x768_120s) or sgioldstyle stereo (eg., str_bot) and back."
		grid .irix -sticky new -columnspan 2
	}
	button .quit -text Close -command exit
	grid .quit -sticky se -columnspan 2 -padx 2 -pady 2
	frame .f -relief groove -borderwidth 2 -bg black
	grid .f -row 0 -column 1 -sticky news
	bind . <Key-Escape> {exit}
	label .f.error -wraplength 100p -bg black -fg white
	::stereo::makeGraphics $stereo::mode
}

set stereo::count 0
set stereo::gwidget ""
proc stereo::makeGraphics {mode} {
	incr stereo::count
	set name .f.gr$stereo::count
	set width 200
	set height 200
	if { [catch { togl $name -width $width -height $height -rgba true -stereo "$mode" -double true -depth true -sharelist main -create create_cb -display display_cb -reshape reshape_cb -eyeseparation 0.05 -convergence 2.0 -stencil true } error] } {
		pack forget $stereo::gwidget
		.f.error configure -text "$error\n\nMake another choice from the stereo modes on the left."
		pack .f.error -expand 1 -fill both
	} else {
		pack forget .f.error
		$name configure -ident main
		if { "$stereo::gwidget" != "" } {
			destroy $stereo::gwidget
		}
		set stereo::gwidget $name
		pack $name -expand 1 -fill both
		bind $name <B1-Motion> {
			::stereo::motion_event %W \
				[lindex [%W config -width] 4] \
				[lindex [%W config -height] 4] %x %y
		}
	}
}

# This is called when mouse button 1 is pressed and moved
proc stereo::motion_event { widget width height x y } {
    setXrot $widget [expr 360.0 * $y / $height]
    setYrot $widget [expr 360.0 * ($width - $x) / $width]

#    .sx set [expr 360.0 * $y / $height]
#    .sy set [expr 360.0 * ($width - $x) / $width]

    .sx set [getXrot]
    .sy set [getYrot]
}

# This is called when a slider is changed.
proc stereo::setAngle {axis value} {
    # catch because .f.gr might be a label instead of a Togl widget
    catch {
	    switch -exact $axis {
		x {setXrot $stereo::gwidget $value}
		y {setYrot $stereo::gwidget $value}
	    }
    }
}

if { [info script] == $argv0 } {
	if { $argc == 1 } {
		set stereo::mode [lindex $argv 0]
	}
	::stereo::setup
}
