#                      S A M P L E . T C L
# BRL-CAD
#
# Copyright (c) 1998-2007 United States Government as represented by
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
# Author -
#	Bob Parker
#
# Source -
#	The U. S. Army Research Laboratory
#	Aberdeen Proving Ground, Maryland  21005
#
#
#
# Description -
#	Routines that demonstrate the oddities of Tcl programming in MGED.
#

#
# Demonstrates how to prompt for input within MGED's command window.
#
proc feed_me {args} {
    set len [llength $args]

    switch $len {
	0 {
	    set_more_default tollhouse
	    error "more arguments needed::feed me a cookie \[tollhouse\]:"
	}
	1 {
	    set_more_default taffy
	    error "more arguments needed::feed me candy \[taffy\]:"
	}
	2 {
	    set_more_default apple
	    error "more arguments needed::feed me fruit \[apple\]:"
	}
	default -
	3 {
	    set cookie [lindex $args 0]
	    set candy [lindex $args 1]
	    set fruit [lindex $args 2]
	    return "I dined on a $cookie cookie,\nsome $candy candy and $fruit fruit.\nThat was good. Thank you!"
	}
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
