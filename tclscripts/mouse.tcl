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
#	This software is Copyright (C) 1995 by the United States Army
#	in all countries except the USA.  All rights reserved.
#
# Description -
#       Mouse routines.

proc place_near_mouse { top } {
    set pxy [winfo pointerxy $top]
    set x [lindex $pxy 0]
    set y [lindex $pxy 1]

    set x [expr {int($x - [winfo reqwidth $top] * 0.5)}]
    set y [expr {int($y - [winfo reqheight $top] * 0.5)}]

    catch { wm geometry $top +$x+$y }
}
