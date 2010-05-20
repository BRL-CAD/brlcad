#!/bin/sh
# the next line restarts using bwish \
exec bwish "$0" "$@"

package require Togl
package require isst

# create ::isst namespace
namespace eval ::isst {
}

proc ::isst::overwin {x y W} {
    set rootxmin [winfo rootx $W]
    set rootymin [winfo rooty $W]
    set rootxmax [expr $rootxmin + [winfo width $W]] 
    set rootymax [expr $rootymin + [winfo height $W]] 
    set currpointerx [winfo pointerx $W]
    set currpointery [winfo pointery $W]
    if { $currpointerx <= $rootxmax && $currpointerx >= $rootxmin &&
         $currpointery <= $rootymax && $currpointery >= $rootymin} {
	 return 1
       } else {
         return 0
       }
    return 0
}

proc ::isst::setup {} {
    global resolution oglwin fullscreenmode
    set resolution 20
    set fullscreenmode 0
    wm title . "ISST - Interactive Geometry Viewing"
    frame .f
    pack .f -side top
    button .f.b1 -text " Quit " -command exit 
    pack .f.b1 -side left -anchor w -padx 5

    bind . <Key-1> {if {[::isst::overwin %X %Y $oglwin]} {render_mode $oglwin phong}}
    bind . <Key-2> {if {[::isst::overwin %X %Y $oglwin]} {render_mode $oglwin normal} else {puts "Place mouse over geometry window."}}
    bind . <Key-3> {if {[::isst::overwin %X %Y $oglwin]} {render_mode $oglwin depth}}
    bind . <Key-4> {if {[::isst::overwin %X %Y $oglwin]} {render_mode $oglwin component}}
    bind . <Key-0> {if {[::isst::overwin %X %Y $oglwin]} {reset $oglwin}}
     
    bind . <Key-w> {if {[::isst::overwin %X %Y $oglwin]} {::isst::MoveForward $oglwin}}
    bind . <Key-s> {if {[::isst::overwin %X %Y $oglwin]} {::isst::MoveBackward $oglwin}}
    bind . <Key-a> {if {[::isst::overwin %X %Y $oglwin]} {::isst::MoveLeft $oglwin}}
    bind . <Key-d> {if {[::isst::overwin %X %Y $oglwin]} {::isst::MoveRight $oglwin}}

    bind . <Key-F5> {if {$fullscreenmode} {wm attributes . -fullscreen 0; set fullscreenmode 0} else {wm attributes . -fullscreen 1; set fullscreenmode 1}}

    bind . <Key-minus> {if {[::isst::overwin %X %Y $oglwin]} {::isst::Resolution $oglwin -1}}
    bind . <Key-equal> {if {[::isst::overwin %X %Y $oglwin]} {::isst::Resolution $oglwin 1}}
     
    bind . <ButtonPress-1> {if {[::isst::overwin %X %Y $oglwin]} {::isst::RotStart %x %y $oglwin}}
    bind . <ButtonPress-3> {if {[::isst::overwin %X %Y $oglwin]} {::isst::RotStart %x %y $oglwin}}
    bind . <B1-Motion> {if {[::isst::overwin %X %Y $oglwin]} {::isst::RotMove %x %y $oglwin}}
    bind . <B3-Motion> {if {[::isst::overwin %X %Y $oglwin]} {::isst::RotMove2 %x %y $oglwin}}
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
    if { $resolution < 20 && $n > 0 } {
      set_resolution $W [expr $resolution + $n]
      set resolution [expr $resolution + $n]
    }
    if { $resolution > 1 && $n < 0 } {
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
