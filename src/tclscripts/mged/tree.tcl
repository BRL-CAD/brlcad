#                        T R E E . T C L
# BRL-CAD
#
# Copyright (c) 1998-2011 United States Government as represented by
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
# Description -
#       This enhances MGED's tree command by adding a
#       few options.
#
# Usage:
#       tree [-c] [-i n] [-d n] [-o outfile] object(s)
#
proc tree {args} {
    set argc [llength $args]
    if {$argc == 0} {
	return [_mged_tree]
    }

    set indent 0
    set fid ""
    set cflag 0
    set displayDepth 0

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
	    -d 
	    -
	    -depth {
		incr i
		if {$i == $argc} {
		    if {$fid != ""} {
			close $fid
		    }
		    return [_mged_tree]
		}
		set displayDepth [lindex $args $i]
		if {[string is integer $displayDepth] == 0} {
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

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
