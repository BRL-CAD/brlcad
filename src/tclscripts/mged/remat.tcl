#                       R E M A T . T C L
# BRL-CAD
#
# Copyright (c) 2005 United States Government as represented by
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
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#
###
# Author: Christopher Sean Morrison
#
# assign material IDs to all regions under some assembly to some given
# material ID number.
#

# make sure the mged commands we need actually exist
set extern_commands "attr"
foreach cmd $extern_commands {
    if {[expr [string compare [info command $cmd] $cmd] != 0]} {
	puts stderr "[info script]: Application fails to provide command '$cmd'"
	return
    }
}


proc  remat { args } {
  if { [llength $args] != 2 } {
    puts "Usage: remat assembly GIFTmater"
    return
  }
  
  set name [lindex $args 0]
  set matid [lindex $args 1]
  
  set objData [db get $name]
  if { [lindex $objData 0] != "comb" } {
    return
  }
  
  set children [lt $name]
  while { $children != "" } {
    foreach node $children {
      set child [lindex $node 1]
      
      if { [lindex [db get $child] 2] == "yes" } {
	attr set $child material_id $matid
      } else {
	set children [concat $children [lt $child]]
      }

      set children [lrange $children 1 end]
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
