#!/bin/sh
# the next line restarts using bwish \
exec bwish "$0" "$@"

package require Togl
package require isst

# create ::isst namespace
namespace eval ::isst {
}

proc ::isst::setup {} {
    global resolution oglwin
    set resolution 0
    wm title . "ISST - Interactive Geometry Viewing"
    frame .f
    pack .f -side top
    button .f.b1 -text " Quit " -command exit 
    pack .f.b1 -side left -anchor w -padx 5
    bind . <Key-1> {render_mode $oglwin phong}
    bind . <Key-2> {render_mode $oglwin normal}
    bind . <Key-3> {render_mode $oglwin depth}
    bind . <Key-4> {render_mode $oglwin component}
    bind . <Key-0> {reset $oglwin}
     
    bind . <Key-w> {::isst::MoveForward $oglwin}
    bind . <Key-s> {::isst::MoveBackward $oglwin}
    bind . <Key-a> {::isst::MoveLeft $oglwin}
    bind . <Key-d> {::isst::MoveRight $oglwin}

    bind . <Key-minus> {::isst::Resolution $oglwin 1}
    bind . <Key-equal> {::isst::Resolution $oglwin -1}
     
    bind . <ButtonPress-1> {::isst::RotStart %x %y $oglwin}
    bind . <ButtonPress-3> {::isst::RotStart %x %y $oglwin}
    bind . <B1-Motion> {::isst::RotMove %x %y $oglwin}
    bind . <B3-Motion> {::isst::RotMove2 %x %y $oglwin}
    drawview .w0 10
}

proc ::isst::drawview {win {tick 100} } {
    global az el resolution oglwin
    togl $win -width 800 -height 600 -rgba true -double true -depth true -privatecmap false -time $tick -create isst_init -destroy isst_zap -display refresh_ogl -reshape reshape -timer idle
    set oglwin $win
    load_g $win /usr/brlcad/rel-7.16.2/share/brlcad/7.16.2/db/ktank.g tank
#    load_g $win /usr/brlcad/share/brlcad/7.16.9/db/ktank.g tank
    pack $win -expand true -fill both
}

proc ::isst::RotStart {x y W} {
    global startx starty
    set startx $x
    set starty $y
}

proc ::isst::MoveForward {W} {
    walk $W 1
}

proc ::isst::MoveBackward {W} {
    walk $W -1
}

proc ::isst::MoveLeft {W} {
    strafe $W 1
}

proc ::isst::MoveRight {W} {
    strafe $W -1
}

proc ::isst::Resolution {W n} {
    global resolution
    if { $resolution < 2 && $n > 0 } {
      set_resolution $W [expr $resolution + $n]
      set resolution [expr $resolution + $n]
    }
    if { $resolution > 0 && $n < 0 } {
      set_resolution $W [expr $resolution + $n]
      set resolution [expr $resolution + $n]
    }
}

proc ::isst::RotMove {x y W} {
    global startx starty
    set dx [expr ($x - $startx)*0.005]
    set dy [expr ($y - $starty)*0.005]
    aetolookat $W $dx $dy
    set startx $x
    set starty $y
}

proc ::isst::RotMove2 {x y W} {
    global startx starty
    set dx [expr ($x - $startx)*0.005]
    set dy [expr ($y - $starty)*0.005]
    aerotate $W $dx $dy
    set startx $x
    set starty $y
}

if { [info script] == $argv0 } {
    global az el oglwin
    ::isst::setup
}
