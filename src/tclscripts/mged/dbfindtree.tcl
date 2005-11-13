#                  D B F I N D T R E E . T C L
# BRL-CAD
#
# Copyright (C) 2004-2005 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#
###
#
#	d b f i n d t r e e
#
#  Find a database object in the tree and print the path(s) to the root(s) of the
#  geometry tree from the object
#
#  example:
#
#	mged> in leaf sph 0 0 0 1
#	mged> g father leaf
#	mged> g mother leaf
#	mged> g gma father
#	mged> g gpa father
#	mged> g grandma mother
#	mged> g grandpa mother
#	mged> g maleAncestors gpa grandpa
#	mged> g femaleAncestors gma grandma
#	mged> g all.g femaleAncestors maleAncestors
#
#	mged> dbfindtree leaf
#	all.g/femaleAncestors/gma/father/leaf
#	all.g/maleAncestors/gpa/father/leaf
#	all.g/femaleAncestors/grandma/mother/leaf
#	all.g/maleAncestors/grandpa/mother/leaf
#	mged>
#
proc dbfindtree {args} {
    global glob_compat_mode
    set save $glob_compat_mode
    set glob_compat_mode 0

    set find_paths $args


    set do_more 1
    while {$do_more} {

	set do_more 0
	set new_paths {}

	foreach i $find_paths {
	    # get the first token from the path list
	    set token [lindex $i 0]

	    # find where it's used
	    set search_result [dbfind $token]

	    # if it is used, create path(s)
	    if { [llength $search_result] } {
		set do_more 1
		foreach n $search_result {
		    lappend new_paths [concat $n $i]
		}
	    } else {
		# not used, so carry the old path forward (if not a single token)
		if {[llength $i] > 1} {
		    lappend new_paths $i
		}
	    }
	}
	set find_paths $new_paths
    }
    foreach i $new_paths { puts [string map {\  /} $i] }
    set glob_compat_mode $save
    # prevent printing of value of $save
    set i {}
}
# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
