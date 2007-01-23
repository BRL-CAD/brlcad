#                   C A L L B A C K S . T C L
# BRL-CAD
#
# Copyright (c) 2004-2007 United States Government as represented by
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
##
#				C A L L B A C K S . T C L
#
# Authors -
#	Bob Parker
#
# Source -
#	The U. S. Army Research Laboratory
#	Aberdeen Proving Ground, Maryland  21005
#
#
# Description -
#	This file contains the standard mged callback proc's.
#
#

## - opendb_callback
#
# This is called upon opening a database.
#
proc opendb_callback { dbname } {
    global mged_players

    if ![info exists mged_players] {
	return
    }

    foreach id $mged_players {
	set_wm_title $id $dbname
	rt_opendb_callback $id
    }
}

if ![info exists in_begin_edit_callback] {
    set in_begin_edit_callback 0
}

## - begin_edit_callback
#
# This is called at the start of an edit.
#
proc begin_edit_callback {path} {
    global mged_gui
    global mged_display
    global mged_players
    global in_begin_edit_callback

    if ![info exists mged_players] {
	return
    }

    if ![info exists mged_display] {
	return
    }

    # remove leading /
    if {[string index $path 0] == "/"} {
	set path [string range $path 1 end]
    }

    set in_begin_edit_callback 1

    if {$mged_display(state) == "SOL EDIT"} {
	catch {get_sed} esolint_info
	set esolint_type [lindex $esolint_info 1]

	if {$esolint_type == "sketch"} {
	    set in_begin_edit_callback 0
	    Sketch_editor .#auto [lindex $esolint_info 0] $path

	    # jump out of solid edit state
	    press reject
	    return
	}

	foreach id $mged_players {
	    if {$mged_gui($id,show_edit_info)} {
		init_edit_solid_int $id
	    }
	}

	# load solid edit menus
	set edit_menus [get_sed_menus]
	init_solid_edit_menus $esolint_type $edit_menus
    } elseif {$mged_display(state) == "OBJ EDIT"} {
	# load object edit menus
	init_object_edit_menus

	foreach id $mged_players {
	    build_edit_info $id
	}
    }

    set in_begin_edit_callback 0
    return
}

## - active_edit_callback
#
# This is called during an active edit after MGED perceives a change
# to the solid/object being edited.
#
proc active_edit_callback {} {
    global mged_display
    global in_begin_edit_callback

    if ![info exists mged_display] {
	return
    }

    if {$in_begin_edit_callback} {
	return
    }

    if {$mged_display(state) == "SOL EDIT"} {
	esolint_update
    }
}

## - end_edit_callback
#
# This is called at the end of an edit.
#
proc end_edit_callback {} {
    global mged_players

    if ![info exists mged_players] {
	return
    }

    undo_edit_menus
    foreach id $mged_players {
	esolint_destroy $id
    }
}

## - output_callback
#
# This is called when things need to be printed to the command window(s)
#
proc output_callback { str } {
    distribute_text {} {} $str
    update idletasks
}

## - solid_list_callback
#
# This is called whenever MGED's internal solid list changes.
#
proc solid_list_callback {} {
    global mged_players

    if ![info exists mged_players] {
	return
    }

    foreach id $mged_players {
	rt_solid_list_callback $id
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
