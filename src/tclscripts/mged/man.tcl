#                   M A N . T C L
# BRL-CAD
#
# Copyright (c) 2004-2026 United States Government as represented by
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


encoding system utf-8

proc man {args} {
    global mged_console_mode
    global tcl_platform
    if {![info exists mged_console_mode]} {
	error "Unable to determine MGED console mode."
    }
    if {$mged_console_mode == "gui"} {
	package require ManBrowser 1.0
	if {![winfo exists .mgedMan]} {
	    ManBrowser .mgedMan -useToC 1 -defaultDir n -parentName MGED
	}

	set search_mode ""
	set query {}
	set page_args {}
	for {set i 0} {$i < [llength $args]} {incr i} {
	    set arg [lindex $args $i]
	    switch -- $arg {
		-k {
		    set search_mode short
		    if {$i + 1 < [llength $args]} {
			incr i
			lappend query [lindex $args $i]
		    }
		}
		-K {
		    set search_mode full
		    if {$i + 1 < [llength $args]} {
			incr i
			lappend query [lindex $args $i]
		    }
		}
		default {
		    if {$search_mode != ""} {
			lappend query $arg
		    } else {
			lappend page_args $arg
		    }
		}
	    }
	}

	if {$search_mode != ""} {
	    if {[llength $query] == 0} {
		error "Usage: man ?-k|-K? keyword"
	    }
	    .mgedMan search [join $query " "] $search_mode
	} elseif {[llength $page_args] == 1 && ![.mgedMan select [lindex $page_args 0]]} {
	    error "couldn't find manual page \"[lindex $page_args 0]\""
	} elseif {[llength $page_args] > 1} {
	    error "Usage: man ?command?"
	}
	.mgedMan activate
    }
    if {$mged_console_mode == "classic" || $mged_console_mode == "batch"} {
	if {[llength $args] != 0} {
	    set exe_ext ""
	    if {$::tcl_platform(platform) == "windows"} {
		set exe_ext ".exe"
	    }
	    set cmd [file join [bu_dir bin] brlman$exe_ext]
	    exec $cmd {*}$args
	}
    }
}

proc brlman {args} {
    # simple (intentionally undocumented) pass-through alias
    man {*}$args
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
