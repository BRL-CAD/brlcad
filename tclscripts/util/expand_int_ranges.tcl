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
