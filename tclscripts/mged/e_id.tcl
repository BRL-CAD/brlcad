#
#				E - I D
#
#	A TCL macro for MGED(1) to edit all objects with specified ident numbers
#
#	Usage -  'e-id ident[-ident] ...'
#	Author - Paul Tanenbaum
#
#	Grab the output of MGED's 'whichid' command, and use that to construct
#	an invocation of its 'e' command to display exactly those objects with
#	the specified ident.

#
#    Preliminary...
#    If any application besides MGED tries to run this script,
#    print a warning message
#
if {[info exists mged_display] == 0} {
    set f_name [info script]
    puts "Warning: script '$f_name' contains MGED(1)-specific macros"
}

#
#    The actual macro
#
proc e-id {args} {
    #
    #	Ensure that at least one argument was given
    #
    if {$args == ""} {
	puts "Usage: e-id <idents and/or ident_ranges>\n       \
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
