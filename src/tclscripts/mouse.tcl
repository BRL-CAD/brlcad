#                       M O U S E . T C L
# BRL-CAD
#
# Copyright (c) 1995-2006 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#
###
#
# Author -
#	Robert Parker
#
# Source -
#	The U. S. Army Ballistic Research Laboratory
#	Aberdeen Proving Ground, Maryland  21005
#
#
#
# Description -
#       Mouse routines.

## - place_near_mouse
#
# Places window near the mouse while keeping the
# window under the mouse and completely (mostly)
# on the screen.
#
proc place_near_mouse {top} {
    if {[winfo toplevel $top] != $top} {
	error "place_near_mouse: $top is not a toplevel window"
    }

    set pxy [winfo pointerxy $top]
    set px [lindex $pxy 0]
    set py [lindex $pxy 1]

    wm withdraw $top
    update
    set width [winfo reqwidth $top]
    set height [winfo reqheight $top]
    set screenwidth [winfo screenwidth $top]
    set screenheight [winfo screenheight $top]

    set x [expr {int($px - $width * 0.5)}]
    set y [expr {int($py - $height * 0.5)}]

    if {$x < 0} {
	set x 0
    } elseif {[expr {$x + $width}] > $screenwidth} {
	set x [expr {$screenwidth - $width}]
    }

    if {$y < 0} {
	set y 0
    } elseif {[expr {$y + $height}] > $screenheight} {
	set y [expr {$screenheight - $height}]
    }

    wm geometry $top +$x+$y
    wm deiconify $top
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
