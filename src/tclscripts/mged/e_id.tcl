#                        E _ I D . T C L
# BRL-CAD
#
# Copyright (c) 2004-2007 United States Government as represented by
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
#				E _ I D
#
#	TCL macro for MGED(1) to edit all objects w/ specified ident numbers
#
#	Usage -  'e_id ident[-ident] ...'
#	Author - Paul Tanenbaum
#
#	Grab the output of MGED's 'whichid' command, and use it to construct
#	an invocation of its 'e' command to display exactly the objects with
#	the specified ident.

#
#    Preliminary...
#    Ensure that all commands used here but not defined herein
#    are provided by the application
#

set extern_commands "whichid e"
foreach cmd $extern_commands {
    if {[expr [string compare [info command $cmd] $cmd] != 0]} {
	puts stderr "[info script]: Application fails to provide command '$cmd'"
	return
    }
}

#
#    The actual macro
#
proc e_id {args} {
    #
    #	Ensure that at least one argument was given
    #
    if {$args == ""} {
	puts "Usage: e_id <idents and/or ident_ranges>\n       \
		(edit object(s) with the specified idents)"
	return
    }
    #
    #	Construct a list of all the specified identifiers
    #
    set ident_list {}
    foreach arg $args {
	if {[regexp {([0-9]+)-([0-9]+)} $arg range lo hi] == 1} {
	    if {$lo > $hi} {
		puts "Illegal ident range: '$arg'"
	    }
	    for {set i $lo} {$i <= $hi} {incr i} {
		lappend ident_list $i
	    }
	} elseif {[regexp {([0-9]+)} $arg lo] == 1} {
	    lappend ident_list $lo
	} else {
	    puts "Invalid ident specification: '$arg'"
	}
    }
    #
    #	Sort the specified identifiers, throwing out any duplicates
    #
    set ident_list [lsort $ident_list]
    set uniq_ident_list {}
    set prev_ident ""
    foreach ident $ident_list {
	if {$ident != $prev_ident} {
	    lappend uniq_ident_list $ident
	}
	set prev_ident $ident
    }
    #
    #	Construct a list of the names of the appropriate regions
    #
    set cmd "whichid $uniq_ident_list"
    set raw_list [split [eval $cmd] \n]
    set result {}
    foreach element $raw_list {
	set non_null [expr {$element != {}}]
	set header [regexp {Region\[s\] with ident [0-9]+:} $element]
	if {[expr ($non_null && !$header)]} {
	    lappend result [string trim $element]
	}
    }
    #
    #	If any regions are appropriate, go ahead and 'e' them
    #
    if {$result == {}} {
	puts "There are no objects with idents '$args'"
    } else {
	puts "objects found: $result"
	eval [concat [list e] $result]
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
