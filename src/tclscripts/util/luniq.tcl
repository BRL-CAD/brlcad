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
