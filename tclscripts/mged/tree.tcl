## -                            T R E E . T C L
# Author -
#	Bob Parker
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
# Copyright Notice -
#       This software is Copyright (C) 1998 by the United States Army
#       in all countries except the USA.  All rights reserved.
#
# Description -
#       This enhances MGED's tree command by adding a
#       few options.
#
# Usage:
#       tree [-c] [-i n] [-o outfile] object(s)
#
proc tree {args} {
    set argc [llength $args]
    if {$argc == 0} {
	return [_mged_tree]
    }

    set indent 0
    set fid ""
    set cflag 0

    # process options
    for {set i 0} {$i < $argc} {incr i} {
	set arg [lindex $args $i]
	switch -- $arg {
	    -c
	        -
	    -comb {
		set cflag 1
	    }
	    -i
	        -
	    -indent {
		incr i
		if {$i == $argc} {
		    if {$fid != ""} {
			close $fid
		    }
		    return [_mged_tree]
		}
		set indent [lindex $args $i]
		if {[string is integer $indent] == 0} {
		    if {$fid != ""} {
			close $fid
		    }
		    return [_mged_tree]
		}
	    }
	    -o
	        -
	    -outfile {
		incr i
		if {$i == $argc} {
		    if {$fid != ""} {
			close $fid
		    }
		    return [_mged_tree]
		}
		set fid [open [lindex $args $i] a+]
	    }
	    default {
		if {[string index $arg 0] == "-"} {
		    if {$fid != ""} {
			close $fid
		    }

		    # bad option
		    return [_mged_tree]
		}
		break
	    }
	}
    }

    set args [lrange $args $i end]

    if {$cflag} {
	set args "-c $args"
    }

    if {[catch {eval _mged_tree $args} result] == 0} {
	if {$indent > 0} {
	    set indent_string [string repeat " " $indent]
	    regsub -all \t $result $indent_string result
	}

	if {$fid != ""} {
	    puts $fid $result
	    close $fid
	    return
	} else {
	    return $result
	}
    } else {
	if {$fid != ""} {
	    close $fid
	}
	return $result
    }
}
