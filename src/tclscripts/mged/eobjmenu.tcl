#                    E O B J M E N U . T C L
# BRL-CAD
#
# Copyright (c) 2004-2011 United States Government as represented by
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
#			E O B J M E N U . T C L
#
#	TCL macros for MGED(1) to specify an object for editing
#	from among those currently being displayed
#
#	Author - Paul Tanenbaum
#
#	Grab the output of MGED's 'x' command, and use it to build a menu

#
#	Ensure that all commands that this script uses without defining
#	are provided by the calling application
#
check_externs "_mged_x _mged_press _mged_aip _mged_M"

proc eobjmenu {} {

    if {![llength [_mged_x -1]]} {
	puts "No objects are currently being displayed"
	return
    }

    #
    #	Define a local proc: illum
    #
    proc illum {flag} {

	#	flag = 0	==>	Go only as far as solid illuminate
	#	flag = 1	==>	Go all the way to solid edit

	set pointery [winfo pointery .om.meat.objects]
	set rooty [winfo rooty .om.meat.objects]
	set rely [expr $pointery - $rooty]
	set index [.om.meat.objects nearest $rely]
	.om.meat.objects selection clear 0 end
	.om.meat.objects selection set $index

	_mged_press oill
	for {set i 0} {$i < $index} {incr i} {
	    _mged_aip f
	}
	if {$flag != 0} {
	    _mged_M 1 0 0
	}
    }

    toplevel .om
    wm title .om "Object-edit selection menu"

    frame .om.meat
    listbox .om.meat.objects -yscrollcommand {.om.meat.slider set}
    set i 0
    foreach word [_mged_x -1] {
	.om.meat.objects insert end $word
	incr i
    }

    bind .om <Leave> {
	if {[info vars omready] == "omready"} {
	    unset omready
	    destroy .om
	    delete_procs illum
	}
	break
    }
    bind .om.meat.objects <ButtonPress-1> {illum 1; break}
    bind .om.meat.objects <ButtonRelease-1> {set omready 1; break}
    bind .om.meat.objects <ButtonPress-2> {illum 0; break}
    bind .om.meat.objects <ButtonRelease-2> {
	.om.meat.objects selection clear 0 end
	_mged_press reject
	break
    }
    .om.meat.objects configure -width 0
    scrollbar .om.meat.slider -command ".om.meat.objects yview"

    pack .om.meat.objects -in .om.meat -side left
    pack .om.meat.slider -in .om.meat -side right -fill y
    pack .om.meat -in .om -side top

    button .om.abort -text "Abort object-edit selection" \
	-command {destroy .om; delete_procs illum}
    pack .om.abort -in .om -side bottom
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
