#
#			H E L P C O M M . T C L
#
# Authors -
#	Lee Butler
#	Robert G. Parker
#
# Source -
#	The U. S. Army Research Laboratory
#	Aberdeen Proving Ground, Maryland  21005
#
# Distribution Notice -
#	Re-distribution of this software is restricted, as described in
#       your "Statement of Terms and Conditions for the Release of
#       The BRL-CAD Package" agreement.
#
# Description -
#	Routines common to the BRL-CAD help system.
#

proc help_comm {data args} {
	global $data

	if {[llength $args] > 0}	{
		set cmd [lindex $args 0]
		if [info exists [subst $data]($cmd)] {
		    return "Usage: $cmd [lindex [subst $[subst $data]($cmd)] 0]\n\t([lindex [subst $[subst $data]($cmd)] 1])"
		} else {
			return "No help found for $cmd"
		}
	} else {
		foreach cmd [lsort [array names [subst $data]]] {
			append info "$cmd [lindex [subst $[subst $data]($cmd)] 0]\n\t[lindex [subst $[subst $data]($cmd)] 1]\n"
		}

		return $info
	}
}


proc ?_comm {data min ncol} {
    global $data

    set i 1
    foreach cmd [lsort [array names [subst $data]]] {
	append info [format "%-[subst $min]s" $cmd]
	if { ![expr $i % [subst $ncol]] } {
	    append info "\n"
	}
	incr i
    }
    return $info
}


proc apropos_comm {data key} {
    global $data

    set info ""
    foreach cmd [lsort [array names [subst $data]]] {
	if {[string first $key $cmd] != -1} {
	    append info "$cmd "
	} elseif {[string first $key [lindex [subst $[subst $data]($cmd)] 0]] != -1} {
	    append info "$cmd "
	} elseif {[string first $key [lindex [subst $[subst $data]($cmd)] 1]] != -1} {
	    append info "$cmd "
	}
    }

    return $info
}