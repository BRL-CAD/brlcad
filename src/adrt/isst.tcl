#                        I S S T . T C L
# BRL-CAD
#
# Copyright (c) 2016-2020 United States Government as represented by
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

# open_dm uses this callback, but we don't load the tclscripts/libdm.tcl stuff any more
proc _init_dm { func w } { }

# create ::isst namespace
namespace eval ::isst {
}

set oglwin .w0

proc ::isst::overwin {x y W} {
    global selectedobjs
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

proc ::isst::buildlist {listwidget toplevelwidget filename} {
    global oglwin selectedobjs
    set selectedobjs [list ]
    set selectednums [$listwidget curselection]
    foreach listitem $selectednums {
	set selectedobjs [concat $selectedobjs [$listwidget get $listitem]]
    }
    puts $selectedobjs
    if {$selectedobjs != ""} {
        load_g $oglwin $filename [split $selectedobjs]
    }
    destroy $toplevelwidget
}

proc ::isst::geomlist {filename} {
    global oglwin selectedobjs
    toplevel .geomlist
    wm title .geomlist "Select Geometry to be Viewed"
    frame .geomlist.listing
    scrollbar .geomlist.listing.s -command ".geomlist.listing.l yview"
    listbox .geomlist.listing.l -selectmode multiple -bd 2 -yscroll ".geomlist.listing.s set" -width 50 -height 50
    grid .geomlist.listing.l  .geomlist.listing.s -sticky nsew -in .geomlist.listing
    grid columnconfigure .geomlist.listing 0 -weight 0
    grid rowconfigure .geomlist.listing 0 -weight 1
    set objects [list ]
    list_g $filename objects
    set objects [lsort $objects]
    foreach object $objects {
        .geomlist.listing.l insert end $object
    }
    pack .geomlist.listing -side top -expand yes -fill both

    frame .geomlist.gl
    pack .geomlist.gl -side bottom
    button .geomlist.gl.b1 -text " OK " -command "::isst::buildlist .geomlist.listing.l .geomlist $filename"
    pack .geomlist.gl.b1 -side left -anchor w -padx 5
}

proc ::isst::loaddatabase {} {
    global oglwin selectedobjs
    set selectedobjs [list ]
    set filetypes {
        {{BRL-CAD .g Files}	{.g} 	}
        {{All Files}		*	}
    }
    set filename [tk_getOpenFile -filetypes $filetypes]

    if {$filename != ""} {
       ::isst::geomlist $filename
    }
}

proc ::isst::cut {} {
    global focus_x focus_y focus_z pos_x pos_y pos_z oglwin
    set dir_x [expr $focus_x - $pos_x]
    set dir_y [expr $focus_y - $pos_y]
    set dir_z [expr $focus_z - $pos_z]
    render_mode $oglwin cut "#($pos_x $pos_y $pos_z) #($dir_x $dir_y $dir_z)"
    ::isst::MoveRight $oglwin
}

proc ::isst::setup {} {
    global resolution oglwin fullscreenmode
    set resolution 20
    set fullscreenmode 0
    wm title . "ISST - Interactive Geometry Viewing"

    menu .mb
    . configure -menu .mb
    .mb add cascade -label "File" -menu .mb.file -underline 0
    menu .mb.file -tearoff 0
    .mb.file add command -label "Open Database" -underline 0 -command ::isst::loaddatabase
    .mb.file add command -label "Exit" -underline 0 -command exit

    .mb add cascade -label "Mode" -menu .mb.mode -underline 0
    menu .mb.mode -tearoff 0
    .mb.mode add command -label "Phong" -underline 0 -command {render_mode $oglwin phong}
    .mb.mode add command -label "Normal" -underline 0 -command {render_mode $oglwin normal}
    .mb.mode add command -label "Depth" -underline 0 -command {render_mode $oglwin depth}
#    .mb.mode add command -label "Cut" -underline 0 -command { ::isst::cut }

    bind . <Key-1> {if {[::isst::overwin %X %Y $oglwin]} {render_mode $oglwin phong}}
    bind . <Key-2> {if {[::isst::overwin %X %Y $oglwin]} {render_mode $oglwin normal} else {puts "Place mouse over geometry window."}}
    bind . <Key-3> {if {[::isst::overwin %X %Y $oglwin]} {render_mode $oglwin depth}}
#    bind . <Key-4> {if {[::isst::overwin %X %Y $oglwin]} {render_mode $oglwin component}}
#    bind . <Key-5> {if {[::isst::overwin %X %Y $oglwin]} {::isst::cut}}
    bind . <Key-0> {if {[::isst::overwin %X %Y $oglwin]} {reset $oglwin}}

    bind . <Key-w> {if {[::isst::overwin %X %Y $oglwin]} {::isst::MoveForward $oglwin}}
    bind . <Key-s> {if {[::isst::overwin %X %Y $oglwin]} {::isst::MoveBackward $oglwin}}
    bind . <Key-a> {if {[::isst::overwin %X %Y $oglwin]} {::isst::MoveLeft $oglwin}}
    bind . <Key-d> {if {[::isst::overwin %X %Y $oglwin]} {::isst::MoveRight $oglwin}}

    bind . <Key-F5> {if {$fullscreenmode} {wm attributes . -fullscreen 0; set fullscreenmode 0} else {wm attributes . -fullscreen 1; set fullscreenmode 1}}

    bind . <Key-minus> {if {[::isst::overwin %X %Y $oglwin]} {::isst::Resolution $oglwin -1}}
    bind . <Key-equal> {if {[::isst::overwin %X %Y $oglwin]} {::isst::Resolution $oglwin 1}}
    bind . <Control-Key-minus> {if {[::isst::overwin %X %Y $oglwin]} {::isst::Resolution $oglwin [expr 1 - $resolution] }}
    bind . <Control-Key-equal> {if {[::isst::overwin %X %Y $oglwin]} {::isst::Resolution $oglwin [expr 20 - $resolution] }}

    bind . <ButtonPress-1> {if {[::isst::overwin %X %Y $oglwin]} {::isst::RotStart %x %y $oglwin}}
    bind . <ButtonPress-3> {if {[::isst::overwin %X %Y $oglwin]} {::isst::RotStart %x %y $oglwin}}
    bind . <B1-Motion> {if {[::isst::overwin %X %Y $oglwin]} {::isst::RotMove %x %y $oglwin}}
    bind . <B3-Motion> {if {[::isst::overwin %X %Y $oglwin]} {::isst::RotMove2 %x %y $oglwin}}

    bind . <Key-Escape> { exit }

    isst_init
    open_dm
    bind $oglwin <Configure> { reshape %w %h }
    pack $oglwin -expand true -fill both
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
      set_resolution [expr $resolution + $n]
      set resolution [expr $resolution + $n]
    }
    if { $resolution > 1 && $n < 0 } {
      set_resolution [expr $resolution + $n]
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

global az el oglwin
::isst::setup
if {$argc == 1} {
   ::isst::geomlist [lindex $argv 0]
}
if {$argc == 2} {
   load_g $oglwin [lindex $argv 0] [lindex $argv 1]
}
# Do a couple operations to make sure
# the view is properly initialized on OSX
update
walk $oglwin 1
walk $oglwin -1
update
while 1 {
    update
    refresh_ogl $oglwin
}

# Local Variables:
# tab-width: 8
# mode: Tcl
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
