#                        I S S T . T C L
# BRL-CAD
#
# Copyright (c) 2016-2026 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# version 2.1 as published by the Free Software Foundation.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#
###

namespace eval ::isst {
    variable fullscreenmode 0
    variable oglwin .w0
    variable resolution_min 1
    variable resolution_max 20
    variable resolution $resolution_max
    variable startx 0
    variable starty 0
}

# open_dm expects this legacy callback, but ISST no longer loads libdm.tcl.
proc ::isst::initDm {func w} {}
proc ::_init_dm {func w} {
    ::isst::initDm $func $w
}

proc ::isst::overwin {x y W} {
    set rootxmin [winfo rootx $W]
    set rootymin [winfo rooty $W]
    set rootxmax [expr {$rootxmin + [winfo width $W]}]
    set rootymax [expr {$rootymin + [winfo height $W]}]
    return [expr {$x <= $rootxmax && $x >= $rootxmin &&
	$y <= $rootymax && $y >= $rootymin}]
}

proc ::isst::invokeOver {x y W command {outsideCommand {}}} {
    if {[::isst::overwin $x $y $W]} {
	{*}$command
    } elseif {[llength $outsideCommand]} {
	{*}$outsideCommand
    }
}

proc ::isst::buildlist {listwidget toplevelwidget filename} {
    variable oglwin
    set selectedobjs {}
    set selectednums [$listwidget curselection]
    foreach listitem $selectednums {
	lappend selectedobjs [$listwidget get $listitem]
    }
    if {[llength $selectedobjs]} {
        ::brlcad::isst load_g $oglwin $filename $selectedobjs
    }
    destroy $toplevelwidget
}

proc ::isst::geomlist {filename} {
    variable oglwin
    toplevel .geomlist
    wm title .geomlist "Select Geometry to be Viewed"
    frame .geomlist.listing
    scrollbar .geomlist.listing.s -command [list .geomlist.listing.l yview]
    listbox .geomlist.listing.l -selectmode multiple -bd 2 \
	-yscroll [list .geomlist.listing.s set] -width 50 -height 50
    grid .geomlist.listing.l  .geomlist.listing.s -sticky nsew -in .geomlist.listing
    grid columnconfigure .geomlist.listing 0 -weight 0
    grid rowconfigure .geomlist.listing 0 -weight 1
    set objects {}
    ::brlcad::isst list_g $filename objects
    set objects [lsort $objects]
    foreach object $objects {
        .geomlist.listing.l insert end $object
    }
    pack .geomlist.listing -side top -expand yes -fill both

    frame .geomlist.gl
    pack .geomlist.gl -side bottom
    button .geomlist.gl.b1 -text " OK " \
	-command [list ::isst::buildlist .geomlist.listing.l .geomlist $filename]
    pack .geomlist.gl.b1 -side left -anchor w -padx 5
}

proc ::isst::loaddatabase {} {
    set filetypes {
        {{BRL-CAD .g Files}	{.g} 	}
        {{All Files}		*	}
    }
    set filename [tk_getOpenFile -filetypes $filetypes]

    if {$filename ne ""} {
       ::isst::geomlist $filename
    }
}

proc ::isst::setup {} {
    variable fullscreenmode
    variable oglwin
    variable resolution
    variable resolution_max
    set resolution $resolution_max
    set fullscreenmode 0
    wm title . "ISST - Interactive Geometry Viewing"

    menu .mb
    . configure -menu .mb
    .mb add cascade -label "File" -menu .mb.file -underline 0
    menu .mb.file -tearoff 0
    .mb.file add command -label "Open Database" -underline 0 \
	-command [list ::isst::loaddatabase]
    .mb.file add command -label "Exit" -underline 0 -command [list exit]

    .mb add cascade -label "Mode" -menu .mb.mode -underline 0
    menu .mb.mode -tearoff 0
    .mb.mode add command -label "Phong" -underline 0 \
	-command [list ::brlcad::isst render_mode $oglwin phong]
    .mb.mode add command -label "Normal" -underline 0 \
	-command [list ::brlcad::isst render_mode $oglwin normal]
    .mb.mode add command -label "Depth" -underline 0 \
	-command [list ::brlcad::isst render_mode $oglwin depth]

    bind . <Key-1> [list ::isst::invokeOver %X %Y $oglwin \
	[list ::brlcad::isst render_mode $oglwin phong]]
    bind . <Key-2> [list ::isst::invokeOver %X %Y $oglwin \
	[list ::brlcad::isst render_mode $oglwin normal] \
	[list puts "Place mouse over geometry window."]]
    bind . <Key-3> [list ::isst::invokeOver %X %Y $oglwin \
	[list ::brlcad::isst render_mode $oglwin depth]]
    bind . <Key-0> [list ::isst::invokeOver %X %Y $oglwin \
	[list ::brlcad::isst reset $oglwin]]

    bind . <Key-w> [list ::isst::invokeOver %X %Y $oglwin \
	[list ::isst::MoveForward $oglwin]]
    bind . <Key-s> [list ::isst::invokeOver %X %Y $oglwin \
	[list ::isst::MoveBackward $oglwin]]
    bind . <Key-a> [list ::isst::invokeOver %X %Y $oglwin \
	[list ::isst::MoveLeft $oglwin]]
    bind . <Key-d> [list ::isst::invokeOver %X %Y $oglwin \
	[list ::isst::MoveRight $oglwin]]

    bind . <Key-F5> [list ::isst::ToggleFullscreen]

    bind . <Key-minus> [list ::isst::invokeOver %X %Y $oglwin \
	[list ::isst::Resolution $oglwin -1]]
    bind . <Key-equal> [list ::isst::invokeOver %X %Y $oglwin \
	[list ::isst::Resolution $oglwin 1]]
    bind . <Control-Key-minus> [list ::isst::invokeOver %X %Y $oglwin \
	[list ::isst::Resolution $oglwin [expr {-$resolution_max}]]]
    bind . <Control-Key-equal> [list ::isst::invokeOver %X %Y $oglwin \
	[list ::isst::Resolution $oglwin $resolution_max]]

    bind . <ButtonPress-1> [list ::isst::invokeOver %X %Y $oglwin \
	[list ::isst::RotStart %x %y $oglwin]]
    bind . <ButtonPress-3> [list ::isst::invokeOver %X %Y $oglwin \
	[list ::isst::RotStart %x %y $oglwin]]
    bind . <B1-Motion> [list ::isst::invokeOver %X %Y $oglwin \
	[list ::isst::RotMove %x %y $oglwin]]
    bind . <B3-Motion> [list ::isst::invokeOver %X %Y $oglwin \
	[list ::isst::RotMove2 %x %y $oglwin]]

    bind . <Key-Escape> [list exit]

    ::brlcad::isst init
    ::brlcad::isst open_dm
    bind $oglwin <Configure> [list ::brlcad::isst reshape %w %h]
    pack $oglwin -expand true -fill both
}

proc ::isst::ToggleFullscreen {} {
    variable fullscreenmode
    set fullscreenmode [expr {!$fullscreenmode}]
    wm attributes . -fullscreen $fullscreenmode
}

proc ::isst::RotStart {x y W} {
    variable startx
    variable starty
    set startx $x
    set starty $y
}

proc ::isst::MoveForward {W} {
    ::brlcad::isst walk $W 1
}

proc ::isst::MoveBackward {W} {
    ::brlcad::isst walk $W -1
}

proc ::isst::MoveLeft {W} {
    ::brlcad::isst strafe $W 1
}

proc ::isst::MoveRight {W} {
    ::brlcad::isst strafe $W -1
}

proc ::isst::Resolution {W n} {
    variable resolution
    variable resolution_min
    variable resolution_max
    set requested [expr {$resolution + $n}]
    set requested [expr {max($resolution_min,
	min($resolution_max, $requested))}]
    if {$requested == $resolution} {
	return
    }
    set resolution $requested
    ::brlcad::isst set_resolution $resolution
}

proc ::isst::RotMove {x y W} {
    variable startx
    variable starty
    set dx [expr {($x - $startx) * 0.005}]
    set dy [expr {($y - $starty) * 0.005}]
    ::brlcad::isst aetolookat $W $dx $dy
    set startx $x
    set starty $y
}

proc ::isst::RotMove2 {x y W} {
    variable startx
    variable starty
    set dx [expr {($x - $startx) * 0.005}]
    set dy [expr {($y - $starty) * 0.005}]
    ::brlcad::isst aerotate $W $dx $dy
    set startx $x
    set starty $y
}

proc ::isst::main {} {
    global argc argv
    variable oglwin

    ::isst::setup
    if {$argc == 1} {
	::isst::geomlist [lindex $argv 0]
    }
    if {$argc == 2} {
	::brlcad::isst load_g $oglwin [lindex $argv 0] \
	    [list [lindex $argv 1]]
    }

    # Exercise the initialized view before entering the application loop.
    update
    ::brlcad::isst walk $oglwin 1
    ::brlcad::isst walk $oglwin -1
    update
    while {1} {
	update
	::brlcad::isst refresh_ogl $oglwin
    }
}

# Local Variables:
# tab-width: 8
# mode: Tcl
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
