#=============================================================================
# When the sliders exist, pressing the "sliders" button makes them go away.
# When they don't exist, pressing the "sliders" button makes them appear.
# They are modeled after the 4D knobs (dials), right down to the -2048 to 2047
#   range established in dm-4d.c.
# Only the field-of-view slider has its value shown (0 to 120); it might be
#   confusing to see -2048 to 2047 on the others (besides, it would take up
#   more space.)
#=============================================================================

# size of dead spot on sliders
set sliders(NOISE) 128

proc sliders args {
    global sliders
    
    set result [eval _mged_sliders $args]

    if { [llength $args]>0 && [_mged_sliders] && ![winfo exists .sliders] } then {
	toplevel .sliders -class Dialog

	frame .sliders.f -borderwidth 3
	label .sliders.f.ratelabel -text "Rate Based Sliders" -anchor c
	scale .sliders.f.kX -label "X Translate" -showvalue no \
		-from -2048 -to 2047 -orient horizontal \
		-length 400 -command "sliders_change X"
	scale .sliders.f.kY -label "Y Translate" -showvalue no \
		-from -2048 -to 2047 -orient horizontal \
		-length 400 -command "sliders_change Y"
	scale .sliders.f.kZ -label "Z Translate" -showvalue no \
		-from -2048 -to 2047 -orient horizontal \
		-length 400 -command "sliders_change Z"
	scale .sliders.f.kS -label "Zoom" -showvalue no \
		-from -2048 -to 2047 -orient horizontal -length 400 \
		-command "sliders_change S"
	scale .sliders.f.kx -label "X Rotate" -showvalue no \
		-from -2048 -to 2047 -orient horizontal -length 400 \
		-command "sliders_change x"
	scale .sliders.f.ky -label "Y Rotate" -showvalue no \
		-from -2048 -to 2047 -orient horizontal -length 400 \
		-command "sliders_change y"
	scale .sliders.f.kz -label "Z Rotate" -showvalue no \
		-from -2048 -to 2047 -orient horizontal -length 400 \
		-command "sliders_change z"
		
	label .sliders.f.abslabel -text "Absolute Sliders" -anchor c
	scale .sliders.f.kaX -label "X Translate" -showvalue no \
		-from -2048 -to 2047 -orient horizontal \
		-length 400 -command "sliders_change aX"
	scale .sliders.f.kaY -label "Y Translate" -showvalue no \
		-from -2048 -to 2047 -orient horizontal \
		-length 400 -command "sliders_change aY"
	scale .sliders.f.kaZ -label "Z Translate" -showvalue no \
		-from -2048 -to 2047 -orient horizontal \
		-length 400 -command "sliders_change aZ"
	scale .sliders.f.kaS -label "Zoom" -showvalue no \
		-from -2048 -to 2047 -orient horizontal -length 400 \
		-command "sliders_change aS"
	scale .sliders.f.kax -label "X Rotate" -showvalue no \
		-from -2048 -to 2047 -orient horizontal -length 400 \
		-command "sliders_change ax"
	scale .sliders.f.kay -label "Y Rotate" -showvalue no \
		-from -2048 -to 2047 -orient horizontal -length 400 \
		-command "sliders_change ay"
	scale .sliders.f.kaz -label "Z Rotate" -showvalue no \
		-from -2048 -to 2047 -orient horizontal -length 400 \
		-command "sliders_change az"
	scale .sliders.f.fov -label "Field of view" -showvalue yes \
		-from -1 -to 120 -orient horizontal -length 400 \
		-variable perspective

	pack .sliders.f -padx 1m -pady 1m
	pack .sliders.f.ratelabel -pady 4
	pack .sliders.f.kX .sliders.f.kY .sliders.f.kZ .sliders.f.kS \
		.sliders.f.kx .sliders.f.ky .sliders.f.kz
	pack .sliders.f.abslabel -pady 4
	pack .sliders.f.kaX .sliders.f.kaY .sliders.f.kaZ \
		.sliders.f.kaS .sliders.f.kax .sliders.f.kay \
		.sliders.f.kaz .sliders.f.fov

	foreach knob { X Y Z S x y z aX aY aZ aS ax ay az } {
	    .sliders.f.k$knob set [expr round(2048.0*[getknob $knob])]
	}

	global perspective
	.sliders.f.fov set $perspective
    } elseif { [llength $args]>0 && ![_mged_sliders] && [winfo exists .sliders] } {
	destroy .sliders
    }

    return $result
}


## sliders_irlimit
##   Because the sliders may seem too sensitive, setting them exactly to zero
##   may be hard.  This function can be used to extend the location of "zero" 
##   into "the dead zone".

proc sliders_irlimit { val } {
    global sliders

    if { [expr $val > $sliders(NOISE)] } then {
	return [expr ($val-$sliders(NOISE))*2048/(2048-$sliders(NOISE))]
    }

    if { [expr $val < -$sliders(NOISE)] } then {
	return [expr ($val+$sliders(NOISE))*2048/(2048-$sliders(NOISE))]
    }
    
    return 0
}


## sliders_change
##   Generic slider-changing callback.

proc sliders_change { which val } {
    _mged_knob $which [expr [sliders_irlimit $val] / 2048.0]
}

## sliders_zero
##   Zeroes the sliders.

proc sliders_zero { } {
    global sliders

    if [winfo exists .sliders] then {
	foreach knob { X Y Z S x y z aX aY aZ aS ax ay az } {
	    _mged_knob $knob 0
	    .sliders.f.k$knob set 0
	}
    }
}

## knob
##   To replace the regular knob function.

proc knob args {
    global sliders
    
    eval _mged_knob $args
    if { [llength $args]==1 && [string compare [lindex $args 0] zero]==0 } {
	sliders_zero
    } elseif [winfo exists .sliders] {
	.sliders.f.k[lindex $args 0] set [expr 2048.0*[lindex $args 1]]
    }
}
