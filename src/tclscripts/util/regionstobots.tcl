#                 R E G I O N S T O B O T S . T C L
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

namespace eval ::brlcad::mged {}

proc ::brlcad::mged::regions_to_bots {} {
    set old_globcompat $::glob_compat_mode
    set ::glob_compat_mode 0
    try {
	set regions [::brlcad::mged search -type region]
	foreach region $regions {
	    set bot ${region}.bot
	    puts "facetizing $region"
	    if {[catch {::brlcad::mged facetize $bot $region} message]} {
		puts "facetizing $region failed: $message"
		continue
	    }
	    if {[::brlcad::mged exists $bot]} {
		puts "facetized $region to $bot"
		::brlcad::mged kill $region
		::brlcad::mged g $region $bot
	    }
	}
    } finally {
	set ::glob_compat_mode $old_globcompat
    }
}

# Keep the historical command-line entry point.
proc ::regions_to_bots {} {
    tailcall ::brlcad::mged::regions_to_bots
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
