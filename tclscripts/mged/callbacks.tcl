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
# Distribution Notice -
#	Re-distribution of this software is restricted, as described in
#       your "Statement of Terms and Conditions for the Release of
#       The BRL-CAD Package" agreement.
#
# Description -
#	This file contains the standard mged callback proc's.
#	
#

## - new_db_callback
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

## - begin_edit_callback
#
# This is called at the start of an edit.
#
proc begin_edit_callback {} {
    global mged_gui
    global mged_display
    global mged_players

    if ![info exists mged_players] {
	return
    }

    if {$mged_display(state) == "SOL EDIT"} {
	foreach id $mged_players {
	    if {$mged_gui($id,show_edit_info)} {
		init_edit_solid_int $id
	    }
	}

	set esolint_info [get_edit_solid]
	set esolint_type [lindex $esolint_info 1]

	# load solid edit menus
	set edit_menus [get_edit_solid_menus]
	if {$esolint_type == "arb8"} {
	    eval do_arb_edit_menu $esolint_type $edit_menus
	} else {
	    eval do_edit_menu $esolint_type $edit_menus
	}
    } elseif {$mged_display(state) == "OBJ EDIT"} {
	# load object edit menus
	do_edit_menu {} {}

	foreach id $mged_players {
	    build_edit_info $id
	}
    }
}

## - active_edit_callback
#
# This is called during an active edit after MGED perceives a change
# to the solid/object being edited.
#
proc active_edit_callback {} {
    esolint_update
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

    undo_edit_menu
    foreach id $mged_players {
	esolint_destroy $id
    }
}

##- output_callback
#
# This is called when things need to be printed to the command window(s)
#
proc output_callback { str } {
    distribute_text {} {} $str
    mged_update 1
}

proc solid_list_callback {} {
    global mged_players

    if ![info exists mged_players] {
	return
    }

    foreach id $mged_players {
	rt_solid_list_callback $id
    }
}
