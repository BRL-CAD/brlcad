#!/bin/sh
# the next line restarts using bwish \
exec bwish "$0" "$@"

package require Togl
package require isst

# create ::isst namespace
namespace eval ::isst {
}

proc ::isst::setup {} {
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
    button .f.b1 -text " Quit " -command exit 
    pack .f.b1 -side left -anchor w -padx 5
    newRot .w0 10

}

proc ::isst::newRot {win {tick 100} } {
    togl $win -width 200 -height 200 -rgba true -double true -depth true -privatecmap false -time $tick -create gears_init -destroy gears_zap -display draw -reshape reshape -timer idle
    bind $win <ButtonPress-1> {::isst::RotStart %x %y %W}
    bind $win <B1-Motion> {::isst::RotMove %x %y %W}
    pack $win -expand true -fill both
}

proc ::isst::RotStart {x y W} {
    exit
}

proc ::isst::RotMove {x y W} {
    global startx starty xangle0 yangle0 xangle yangle
    set xangle [expr $xangle0 + ($x - $startx)]
    set yangle [expr $yangle0 + ($y - $starty)]
    rotate $W $xangle $yangle
}

if { [info script] == $argv0 } {
	::isst::setup
}
