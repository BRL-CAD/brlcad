proc solclick { } {
    echo Shoot a ray by clicking the middle mouse button.
    # Replace mouse event handler
    proc M { up x y } {
	# Reset mouse event handler
	proc M args {
	    eval [concat _mged_M $args]
	}
	catch { destroy .solclick }

	set solids [solids_on_ray $x $y]
	set len [llength $solids]
	echo \"solids_on_ray $x $y\" sees $len solid(s)
	if { $len<=0 } then return
	echo Solid list: $solids
	
	toplevel .solclick
	wm title .solclick "Solid edit"
	set i 0
	foreach solid $solids {
	    button .solclick.s$i -text $solid \
		    -command "destroy .solclick; sed $solid"
	    pack .solclick.s$i -side top -fill x
	    incr i
	}
	button .solclick.s$i -text "CANCEL" \
	    -command "destroy .solclick"
	pack .solclick.s$i -side top -fill x
    }
}

toplevel .metasolclick
wm title .metasolclick "My 1st menu"
button .metasolclick.s0 -text "SOLID CLICK" \
	-command "solclick"
pack .metasolclick.s0 -side top -fill x
