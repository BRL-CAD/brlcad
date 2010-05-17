#!/bin/sh
# the next line restarts using bwish \
exec bwish "$0" "$@"

package require Togl
package require isst

# create ::isst namespace
namespace eval ::isst {
}

proc ::isst::setup {} {
    global az el
    wm title . "ISST - Interactive Geometry Viewing"

    frame .f
    pack .f -side top
    button .f.b1 -text " Quit " -command exit 
    pack .f.b1 -side left -anchor w -padx 5
    drawview .w0 10
}

proc ::isst::drawview {win {tick 100} } {
    global az el
    togl $win -width 800 -height 600 -rgba true -double true -depth true -privatecmap false -time $tick -create isst_init -destroy isst_zap -display refresh_ogl -reshape reshape -timer idle
     focus $win
     bind $win <Key-1> {focus %W; render_mode %W phong}
     bind $win <Key-2> {focus %W; render_mode %W normal}
     bind $win <Key-3> {focus %W; render_mode %W depth}
     bind $win <Key-4> {focus %W; render_mode %W component}
     
    bind $win <ButtonPress-1> {::isst::RotStart %x %y %W}
    bind $win <B1-Motion> {::isst::RotMove %x %y %W}
    load_g $win /usr/brlcad/rel-7.16.2/share/brlcad/7.16.2/db/ktank.g tank
    pack $win -expand true -fill both
}

proc ::isst::RotStart {x y W} {
    global startx starty
    set startx $x
    set starty $y
}

proc ::isst::RotMove {x y W} {
    global az el startx starty
    set az [expr $az + ($x - $startx)*0.0001]
    set el [expr $el + ($y - $starty)*0.0001]
    aetolookat $W $az $el
}

if { [info script] == $argv0 } {
    global az el
	::isst::setup
        puts "$az $el"
}
