#			M O U S E . T C L
#
# Author -
#	Robert Parker
#
# Source -
#	The U. S. Army Ballistic Research Laboratory
#	Aberdeen Proving Ground, Maryland  21005
#  
# Distribution Notice -
#	Re-distribution of this software is restricted, as described in
#	your "Statement of Terms and Conditions for the Release of
#	The BRL-CAD Package" agreement.
#
# Copyright Notice -
#	This software is Copyright (C) 1995-2004 by the United States Army
#	in all countries except the USA.  All rights reserved.
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
