#!/bin/sh
# the next line restarts using tclsh \
exec tclsh "$0" "$@"

# Togl - a Tk OpenGL widget
# Copyright (C) 1996-1997  Brian Paul and Ben Bederson
# Copyright (C) 2006-2007  Greg Couch
# See the LICENSE file for copyright details.


#
# Test Togl using GL Gears Demo
#
# Copyright (C) 1997 Philip Quaife
#

package require Togl

package provide gears 1.0

# following load also loads Tk and Togl packages
load [file dirname [info script]]/gears[info sharedlibextension]

# create ::gears namespace
namespace eval ::gears {
}

proc ::gears::setup {} {
    global startx starty xangle0 yangle0 xangle yangle RotCnt
    global vTime
    set RotCnt 1
    set xangle 0.0
    set yangle 0.0
    set vTime 100
    wm title . "Rotating Gear Widget Test"

    label .t -text "Click and drag to rotate image"
    pack .t -side top -padx 2 -pady 10
    frame .f
    pack .f -side top
    button .f.n1 -text "  Add " -command ::gears::AutoRot
    button .f.r1 -text "Remove" -command ::gears::DelRot
    button .f.b1 -text " Quit " -command exit 
    entry .f.t -width 4 -textvariable vTime
    pack .f.n1 .f.t .f.r1 .f.b1 -side left -anchor w -padx 5
    newRot .w0 10

}
proc ::gears::AutoRot {} {
    global RotCnt vTime
    newRot .w$RotCnt $vTime
    set RotCnt [expr $RotCnt + 1]
}

proc ::gears::DelRot {} {
    global RotCnt vTime
    if { $RotCnt != 0 } {
      set RotCnt [expr $RotCnt - 1]
      destroy .w$RotCnt
    }
}

proc ::gears::newRot {win {tick 100} } {
    togl $win -width 200 -height 200 -rgba true -double true -depth true -privatecmap false -time $tick -create init -destroy zap -display draw -reshape reshape -timer idle
    bind $win <ButtonPress-1> {::gears::RotStart %x %y %W}
    bind $win <B1-Motion> {::gears::RotMove %x %y %W}
    pack $win -expand true -fill both
}

proc ::gears::RotStart {x y W} {
    global startx starty xangle0 yangle0 xangle yangle
    set startx $x
    set starty $y
    set vPos [position $W]
    set xangle0 [lindex $vPos 0]
    set yangle0 [lindex $vPos 1]
}

proc ::gears::RotMove {x y W} {
    global startx starty xangle0 yangle0 xangle yangle
    set xangle [expr $xangle0 + ($x - $startx)]
    set yangle [expr $yangle0 + ($y - $starty)]
    rotate $W $xangle $yangle
}

if { [info script] == $argv0 } {
	::gears::setup
}
