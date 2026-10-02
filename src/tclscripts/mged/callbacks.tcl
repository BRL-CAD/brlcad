#                   C A L L B A C K S . T C L
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
###
#
# Description -
#	This file contains the standard mged callback proc's.
#
#

namespace eval ::brlcad::mged::callbacks {
    variable in_begin_edit
    if {![info exists in_begin_edit]} {
	set in_begin_edit 0
    }
}

## - ::brlcad::mged::callbacks::open_database
#
# This is called upon opening a database.
#
proc ::brlcad::mged::callbacks::open_database {dbname} {
    global mged_players

    if ![info exists mged_players] {
	return
    }

    foreach id $mged_players {
	set_wm_title $id $dbname
	::brlcad::mged::raytrace::open_database_callback $id
    }
}

## - ::brlcad::mged::callbacks::begin_edit
#
# This is called at the start of an edit.
#
proc ::brlcad::mged::callbacks::begin_edit {path} {
    variable in_begin_edit
    global mged_gui
    global mged_display
    global mged_players

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

    set in_begin_edit 1

    if {$mged_display(state) == "SOL EDIT"} {
	catch {::brlcad::mged get_sed} esolint_info
	set esolint_type [lindex $esolint_info 1]

	if {$esolint_type == "sketch"} {
	    set in_begin_edit 0
	    Sketch_editor .#auto [lindex $esolint_info 0] $path

	    # jump out of solid edit state
	    ::brlcad::mged press reject
	    return
	}

	foreach id $mged_players {
	    if {$mged_gui($id,show_edit_info)} {
		init_edit_solid_int $id
	    }
	}

	# load solid edit menus
	set edit_menus [::brlcad::mged get_sed_menus]
	init_solid_edit_menus $esolint_type $edit_menus
    } elseif {$mged_display(state) == "OBJ EDIT"} {
	# load object edit menus
	init_object_edit_menus

	foreach id $mged_players {
	    build_edit_info $id
	}
    }

    set in_begin_edit 0
    return
}

## - ::brlcad::mged::callbacks::active_edit
#
# This is called during an active edit after MGED perceives a change
# to the solid/object being edited.
#
proc ::brlcad::mged::callbacks::active_edit {} {
    variable in_begin_edit
    global mged_display

    if ![info exists mged_display] {
	return
    }

    if {$in_begin_edit} {
	return
    }

    if {$mged_display(state) == "SOL EDIT"} {
	esolint_update
    }
}

## - ::brlcad::mged::callbacks::end_edit
#
# This is called at the end of an edit.
#
proc ::brlcad::mged::callbacks::end_edit {} {
    global mged_players

    if ![info exists mged_players] {
	return
    }

    undo_edit_menus
    foreach id $mged_players {
	esolint_destroy $id
    }
}

## - ::brlcad::mged::callbacks::output
#
# This is called when things need to be printed to the command window(s)
#
proc ::brlcad::mged::callbacks::output {str} {
    distribute_text {} {} $str
    update idletasks
}

## - ::brlcad::mged::callbacks::solid_list
#
# This is called whenever MGED's internal solid list changes.
#
proc ::brlcad::mged::callbacks::solid_list {} {
    global mged_players

    if ![info exists mged_players] {
	return
    }

    foreach id $mged_players {
	::brlcad::mged::raytrace::solid_list_callback $id
    }
}

# Compatibility entry points for scripts and interactive use.
proc ::opendb_callback {args} {
    tailcall ::brlcad::mged::callbacks::open_database {*}$args
}
proc ::begin_edit_callback {args} {
    tailcall ::brlcad::mged::callbacks::begin_edit {*}$args
}
proc ::active_edit_callback {args} {
    tailcall ::brlcad::mged::callbacks::active_edit {*}$args
}
proc ::end_edit_callback {args} {
    tailcall ::brlcad::mged::callbacks::end_edit {*}$args
}
proc ::output_callback {args} {
    tailcall ::brlcad::mged::callbacks::output {*}$args
}
proc ::solid_list_callback {args} {
    tailcall ::brlcad::mged::callbacks::solid_list {*}$args
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
