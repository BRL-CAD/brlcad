#                        R E I D . T C L
# BRL-CAD
#
# Copyright (c) 2005-2007 United States Government as represented by
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
# Author: Christopher Sean Morrison
#
# assign region IDs to all regions under some assembly starting at
# some given region ID number.
#

set extern_commands "attr"
foreach cmd $extern_commands {
    if {[expr [string compare [info command $cmd] $cmd] != 0]} {
	puts stderr "[info script]: Application fails to provide command '$cmd'"
	return
    }
}


proc get_regions { args } {
    if { [llength $args] != 1 } {
	puts "Usage: get_regions object"
	return ""
    }

    set object [lindex $args 0]
    set objectData [db get $object]

    if { [lindex $objectData 0] != "comb" } {
	# found primitive
	return ""
    }

    set regions ""

    set children [lt $object]
    if { $children != "" } {
	foreach node $children {
	    set child [lindex $node 1]

	    if { [lindex [db get $child] 0] != "comb" } {
		# ignore primitive
		continue
	    }
	    
	    if { [lindex [db get $child] 2] == "yes" } {
		# found a region, add to the list and stop recursion
		set regions [concat $regions $child]
	    } else {
		# found a combination, recurse
		set regions [concat $regions [get_regions $child]]
	    }
	}
    }
    return "$regions"
}


proc  reid { args } {
    if { [llength $args] != 2 } {
	puts "Usage: reid assembly regionID"
	return
    }
    
    set name [lindex $args 0]
    set regionid [lindex $args 1]
    
    set objData [db get $name]
    if { [lindex $objData 0] != "comb" } {
	return
    }

    set regions [get_regions $name]
    foreach region $regions {
	attr set $region region_id $regionid
	incr regionid
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
