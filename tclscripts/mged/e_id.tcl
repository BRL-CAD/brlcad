#
#				E - I D
#
#	A TCL macro for MGED(1) to edit all objects with a specified ident number
#
#	Usage -  'e-id ident'
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
if {[winfo name .] != "MGED"} {
    set f_name [info script]
    puts "Warning: script '$f_name' contains MGED(1)-specific macros"
}

#
#    The actual macro
#
proc e-id {{ident ""}} {
    #
    #	Handle command-line syntax
    #
    if {$ident == ""} {
	puts "Usage: e-id <ident>\n       \
		(edit object(s) with the specified ident)"
	return
    }
    #
    #	Construct a list of the names of the appropriate regions
    #
    set raw_result [whichid $ident]
    set index [string first "\n" $raw_result]
    set reg_list [split [string trim [string range $raw_result $index end]]]
    set result {}
    foreach element $reg_list {
	if {$element != {}} {
	    set result [lappend result $element]
	}
    }
    #
    #	If any regions are appropriate, go ahead and 'e' them
    #
    if {$reg_list == {}} {
	puts "There are no objects with ident '$ident'"
    } else {
	puts "objects found: $result"
	eval [concat [list e] $result]
    }
}
