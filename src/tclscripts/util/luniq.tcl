#                       L U N I Q . T C L
# BRL-CAD
#
# Copyright (c) 2004-2007 United States Government as represented by
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
#				L U N I Q
#
#	Accepts a list and replaces multiple consecutive occurences
#	of a value with a single copy
#
proc luniq {list_in} {
    set length_in [llength $list_in]
    set result {}
    for {set i 0} {$i < $length_in} {set i $j} {
	lappend result [lindex $list_in $i]
	for {set j [expr $i + 1]} {$j < $length_in} {incr j} {
	    if {[string compare [lindex $list_in $i] [lindex $list_in $j]]} {
		break
	    }
	}
    }
    return $result
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
