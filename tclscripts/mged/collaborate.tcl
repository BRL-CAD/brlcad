#			C O L L A B O R A T E . T C L
#
#	TCL macros for maintaining collaborative relationships.
#
#	Author -
#		Robert Parker
#

proc collaborate { cmd {id ""} } {
    global mged_collaborators

    if {$cmd == "join"} {
	if {$id == ""} {
	    return [helpdevel collaborate]
	}

	collab_join $id
	return
    }

    if {$cmd == "quit"} {
	if {$id == ""} {
	    return [helpdevel collaborate]
	}

	collab_quit $id
	return
    }

    if {$cmd == "show"} {
	return $mged_collaborators
    }
}

# Join or quit the Mged Collaborative Session
proc collab_doit { id } {
    global mged_gui

    if {$mged_gui($id,collaborate)} {
	collab_join $id
    } else {
	collab_quit $id
    }
}

# Join Mged Collaborative Session
proc collab_join { id } {
    global mged_gui
    global mged_collaborators
    global mged_players

    if { [lsearch -exact $mged_players $id] == -1 } {
	return "collab_join: $id is not listed as an mged_player"
    }

    if { [lsearch -exact $mged_collaborators $id] != -1 } {
	return "collab_join: $id is already in the collaborative session"
    }

    if [winfo exists $mged_gui($id,active_dm)] {
	set nw $mged_gui($id,top).ur
    } else {
	return "collab_join: unrecognized pathname - $mged_gui($id,active_dm)"
    }

    if [llength $mged_collaborators] {
	set cid [lindex $mged_collaborators 0]
	if [winfo exists $mged_gui($cid,top).ur] {
	    set ow $mged_gui($cid,top).ur
	} else {
	    return "collab_join: me thinks the session is corrupted"
	}

	catch { share view $ow $nw }
	reconfig_gui_default $id
	view_ring_copy $cid $id
    }

    set mged_gui($id,collaborate) 1
    lappend mged_collaborators $id
}

# Quit Mged Collaborative Session
proc collab_quit { id } {
    global mged_collaborators
    global mged_gui

    set i [lsearch -exact $mged_collaborators $id]
    if { $i == -1 } {
	return "collab_quit: bad id - $id"
    }

    if [winfo exists $mged_gui($id,active_dm)] {
	set w $mged_gui($id,active_dm)
    } else {
	return "collab_quit: unrecognized pathname - $mged_gui($id,active_dm)"
    }

    catch {share -u view $w}
    set mged_gui($id,collaborate) 0
    set mged_collaborators [lreplace $mged_collaborators $i $i]
}
