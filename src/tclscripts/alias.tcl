#                         A L I A S . T C L
# BRL-CAD
#
# Copyright (c) 2026 United States Government as represented by
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
# Lightweight command aliases usable with or without Tk.

proc alias {{newcmd {}} args} {
    if {$newcmd eq {}} {
	set result {}
	foreach name [interp aliases] {
	    lappend result [list $name -> [interp alias {} $name]]
	}
	return [join $result \n]
    }

    if {![llength $args]} {
	return [interp alias {} $newcmd]
    }

    return [interp alias {} $newcmd {} {*}$args]
}

proc unalias {cmd} {
    interp alias {} $cmd {}
}

# Local Variables:
# tab-width: 8
# mode: Tcl
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8 cino=N-s
