proc solclick { } {
    echo \nShoot a ray by clicking the middle mouse button.
    # Replace mouse event handler
    proc M { up x y } {
	# Reset mouse event handler
	proc M args {
	    eval [concat _mged_M $args]
	}
	catch { destroy .solclick }

	set solids [solids_on_ray $x $y]
	
	toplevel .solclick
	wm title .solclick "Solid edit"
	set i 0
	foreach solid $solids {
	    button .solclick.s$i -text [lindex [split $solid /] end] \
		    -command "destroy .solclick; sed $solid"
	    pack .solclick.s$i -side top -fill x -expand yes
	    incr i
	}
	button .solclick.s$i -text "CANCEL" -command "destroy .solclick"
	pack .solclick.s$i -side top -fill x -expand yes
    }
}

catch { destroy .metasolclick }

toplevel .metasolclick
wm title .metasolclick "My 1st menu"
button .metasolclick.s0 -text "SOLID CLICK" -command "solclick"
pack .metasolclick.s0 -side top -fill x -fill y -expand yes
