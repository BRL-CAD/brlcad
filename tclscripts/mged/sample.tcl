#                              S A M P L E . T C L
#
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