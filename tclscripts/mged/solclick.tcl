#
# Modifications -
#        (Bob Parker):
#             Generalized the code to accommodate multiple instances of the
#             user interface.
#
proc solclick { } {
    echo \nShoot a ray by clicking the middle mouse button.
    # Replace mouse event handler
    proc M { up x y } {
	if {$up == 0} {
	    return
	}

	# Reset mouse event handler
	proc M args {
	    eval [concat _mged_M $args]
	}	

#	set w .solclick$id
	set w .metasolclick.solclick

	catch { destroy $w }

	set solids [solids_on_ray $x $y]

#	toplevel $w -screen $player_screen($id)
	toplevel $w
	wm title $w "Solid edit"
	set i 0
	foreach solid $solids {
	    button $w.s$i -text [lindex [split $solid /] end] \
		    -command "destroy $w; sed $solid"
	    pack $w.s$i -side top -fill x -expand yes
	    incr i
	}
	button $w.s$i -text "CANCEL" -command "destroy $w"
	pack $w.s$i -side top -fill x -expand yes
    }
}

proc init_solclick { id } {
    global player_screen
    global solclick_user

    set w .metasolclick
    if [winfo exists $w] {
	mged_dialog .solclick_dialog $player_screen($id) "In Use" "The solid click tool is in use by $solclick_user" info 0 OK
	return
    }
    
#    catch { destroy $w }
    set solclick_user $id
    toplevel $w -screen $player_screen($id)
    wm title $w "My 1st menu"
    button $w.s0 -text "SOLID CLICK" -command "solclick"
    pack $w.s0 -side top -fill x -fill y -expand yes
}
