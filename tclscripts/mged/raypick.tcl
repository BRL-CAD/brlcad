proc raypick { } {
    echo Shoot a ray by clicking the middle mouse button.
    # Replace mouse event handler
    proc M { up x y } {
	# Reset mouse event handler
	proc M args {
	    eval [concat _mged_M $args]
	}
	catch { destroy .raypick }

	set solids [solids_on_ray $x $y]
	set len [llength $solids]
	echo \"solids_on_ray $x $y\" sees $len solid(s)
	if { $len<=0 } then return
	echo Solid list: $solids
	
	toplevel .raypick
	wm title .raypick "Solid edit"
	set i 0
	foreach solid $solids {
	    button .raypick.s$i -text $solid \
		    -command "destroy .raypick; sed $solid"
	    pack .raypick.s$i -side top -fill x
	    incr i
	}
    }
}
