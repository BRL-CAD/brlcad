#           E X P A N D _ I N T _ R A N G E S . T C L
# BRL-CAD
#
# Copyright (c) 2004-2011 United States Government as represented by
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
#		E X P A N D _ I N T _ R A N G E S
#
#	Accepts a string specifying zero or more ranges of integers
#	and returns a list of all the integers so specified
#
#	Example:
#		expand_int_ranges 2,10-12,6
#	returns
#		2 10 11 12 6
#
proc expand_int_ranges {spec} {
    set previ 0
    set result ""
    while {[string length $spec]} {
	#
	#	Pull off the next block,
	#	i.e., comma-separated chunk of the specification string
	#
	if {[set i [string first "," $spec]] == -1} {
	    set block [string range $spec 0 end]
	    set spec ""
	} else {
	    set block [string range $spec 0 [expr $i - 1]]
	    set spec [string range $spec [expr $i + 1] end]
	}
	set previ $i
	#
	#	Process the current block.
	#	A valid block is either a nonnegative integer
	#	or a dash-separated pair of nonnegative integers.
	#
	if {[regexp {^([0-9]+)-([0-9]+)$} $block match from to]} {
	    if {$from > $to} {
		error "Illegal block: $block:  $from > $to"
	    }
	    for {set n $from} {$n <= $to} {incr n} {
		lappend result $n
	    }
	} elseif {[regexp {^([0-9]+)$} $block match n]} {
	    lappend result $n
	} else {
	    error "Invalid intger range: $block"
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
