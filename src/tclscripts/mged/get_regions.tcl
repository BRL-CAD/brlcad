#                 G E T _ R E G I O N S . T C L
# BRL-CAD
#
# Copyright (c) 2005-2011 United States Government as represented by
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
#
# list all regions at or under a given hierarchy node
#

set extern_commands [list db]
foreach cmd $extern_commands {
    catch {auto_load $cmd} val
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
	# ignore primitive
	return ""
    }
    if { [lindex $objectData 2] == "yes" } {
	# found region, go no further
	return $object
    }

    # list of all regions underneath this node (including duplicates)
    set regions [list]

    # process children
    set children [lt $object]
    if { $children != "" } {
	foreach node $children {
	    set child [lindex $node 1]

	    if { [lindex [get $child] 0] != "comb" } {
		# ignore primitive
		continue
	    }

	    if { [lindex [get $child] 2] == "yes" } {
		# found a region, add to the list and stop recursion
		lappend regions $child
	    } else {
		# found a combination, recurse
		set regions [concat $regions [get_regions $child]]
	    }
	}
    }

    set unique [list]
    # if we haven't already encountered this region.
    foreach region $regions {
	if { [lsearch $unique $region] == -1 } {
	    lappend unique $region
	}
    }

    return $unique
}


# Local Variables:
# tab-width: 8
# mode: Tcl
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
