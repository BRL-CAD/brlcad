#                     R A Y P I C K . T C L
# BRL-CAD
#
# Copyright (C) 2004-2005 United States Government as represented by
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
#    Preliminary...
#    Ensure that all commands used here but not defined herein
#    are provided by the application
#

set extern_commands "M _mged_M"
foreach cmd $extern_commands {
    if {[expr [string compare [info command $cmd] $cmd] != 0]} {
	puts stderr "Application fails to provide command '$cmd'"
	return
    }
}


proc raypick { } {
    echo Shoot a ray by clicking the middle mouse button.
    # Replace mouse event handler
    proc M { up x y } {
	# Reset mouse event handler
	proc M args {
	    eval [concat _mged_M $args]
	}
	catch { destroy .raypick }

	set solids [solids_on_ray $x $y]
	set len [llength $solids]
	echo \"solids_on_ray $x $y\" sees $len solid(s)
	if { $len<=0 } then return
	echo Primitive list: $solids

	toplevel .raypick
	wm title .raypick "Primitive edit"
	set i 0
	foreach solid $solids {
	    button .raypick.s$i -text $solid \
		    -command "destroy .raypick; sed $solid"
	    pack .raypick.s$i -side top -fill x
	    incr i
	}
    }
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
