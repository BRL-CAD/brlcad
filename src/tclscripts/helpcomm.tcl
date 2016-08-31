#                    H E L P C O M M . T C L
# BRL-CAD
#
# Copyright (c) 2004-2016 United States Government as represented by
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
#	Routines common to the BRL-CAD help system.
#

proc help_comm {data args} {
    global $data

    if {[llength $args] > 0} {
	foreach cmd [join $args] {
	    if [info exists [subst $data]($cmd)] {
		append info "Usage: $cmd [lindex [subst $[subst $data]($cmd)] 0]\n\nDescription: [lindex [subst $[subst $data]($cmd)] 1]\n"
	    } else {
		append info "No help found for $cmd\n"
	    }
	}
    } else {
	foreach cmd [lsort [array names [subst $data]]] {
	    append info "$cmd [lindex [subst $[subst $data]($cmd)] 0]\n\t[lindex [subst $[subst $data]($cmd)] 1]\n"
	}
    }
    return $info
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
# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
