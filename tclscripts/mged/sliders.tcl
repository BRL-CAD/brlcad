#
# Modifications -
#        (Bob Parker):
#             Generalized to accommodate multiple instances of the
#             user interface.
#
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
set sliders(NOISE) 64
set sliders(scale) 2048.0;
set sliders(max) [expr $sliders(scale) + $sliders(NOISE)];
set sliders(min) [expr $sliders(max) * -1];
set sliders(range) [expr $sliders(max) * 2];

proc sliders args {
    global sliders
    global player_screen
    global rateknobs
    global adcflag
    
    set result [eval _mged_sliders $args]

    # get a list of the id's associated with the current command window
    set id_list [cmd_get]

    foreach id $id_list {
        if { [string compare $id "mged"]==0 } {
	    continue
	}

        set w .sliders$id
	if { [llength $args]>0 && [_mged_sliders] } then {

	    if [winfo exists $w] {
		destroy $w
	    }

	    toplevel $w -class Dialog -screen $player_screen($id)
	    
	    frame $w.f -borderwidth 3

	    if { $rateknobs } {
		label $w.f.ratelabel -text "Rate Based Sliders" -anchor c
		scale $w.f.kX -label "X Translate" -showvalue no \
			-from $sliders(min) -to $sliders(max) -orient horizontal \
			-length 400 -command "sliders_change $id X" -variable sliders($id,X)
		scale $w.f.kY -label "Y Translate" -showvalue no \
			-from $sliders(min) -to $sliders(max) -orient horizontal \
			-length 400 -command "sliders_change $id Y" -variable sliders($id,Y)
		scale $w.f.kZ -label "Z Translate" -showvalue no \
			-from $sliders(min) -to $sliders(max) -orient horizontal \
			-length 400 -command "sliders_change $id Z" -variable sliders($id,Z)
		scale $w.f.kS -label "Zoom" -showvalue no \
			-from $sliders(min) -to $sliders(max) -orient horizontal -length 400 \
			-command "sliders_change $id S" -variable sliders($id,S)
		scale $w.f.kx -label "X Rotate" -showvalue no \
			-from $sliders(min) -to $sliders(max) -orient horizontal -length 400 \
			-command "sliders_change $id x" -variable sliders($id,x)
		scale $w.f.ky -label "Y Rotate" -showvalue no \
			-from $sliders(min) -to $sliders(max) -orient horizontal -length 400 \
			-command "sliders_change $id y" -variable sliders($id,y)
		scale $w.f.kz -label "Z Rotate" -showvalue no \
			-from $sliders(min) -to $sliders(max) -orient horizontal -length 400 \
			-command "sliders_change $id z" -variable sliders($id,z)

		foreach knob { X Y Z S x y z } {
		    set sliders($id,$knob) [sliders_add_tol [expr [getknob $knob] * $sliders(scale)]]
		}

		pack $w.f.ratelabel -pady 4
		pack $w.f.kX $w.f.kY $w.f.kZ $w.f.kS \
			$w.f.kx $w.f.ky $w.f.kz

	    } else {
		label $w.f.abslabel -text "Absolute Sliders" -anchor c
		scale $w.f.kaX -label "X Translate" -showvalue no \
			-from $sliders(min) -to $sliders(max) -orient horizontal \
			-length 400 -command "sliders_change $id aX" -variable sliders($id,aX)
		scale $w.f.kaY -label "Y Translate" -showvalue no \
			-from $sliders(min) -to $sliders(max) -orient horizontal \
			-length 400 -command "sliders_change $id aY" -variable sliders($id,aY)
		scale $w.f.kaZ -label "Z Translate" -showvalue no \
			-from $sliders(min) -to $sliders(max) -orient horizontal \
			-length 400 -command "sliders_change $id aZ" -variable sliders($id,aZ)
		scale $w.f.kaS -label "Zoom" -showvalue no \
			-from $sliders(min) -to $sliders(max) -orient horizontal -length 400 \
			-command "sliders_change $id aS" -variable sliders($id,aS)
		scale $w.f.kax -label "X Rotate" -showvalue no \
			-from $sliders(min) -to $sliders(max) -orient horizontal -length 400 \
			-command "sliders_change $id ax" -variable sliders($id,ax)
		scale $w.f.kay -label "Y Rotate" -showvalue no \
			-from $sliders(min) -to $sliders(max) -orient horizontal -length 400 \
			-command "sliders_change $id ay" -variable sliders($id,ay)
		scale $w.f.kaz -label "Z Rotate" -showvalue no \
			-from $sliders(min) -to $sliders(max) -orient horizontal -length 400 \
			-command "sliders_change $id az" -variable sliders($id,az)

		foreach knob { aX aY aZ aS ax ay az } {
		    set sliders($id,$knob) [sliders_add_tol [expr [getknob $knob] * $sliders(scale)]]
		}

		pack $w.f.abslabel -pady 4
		pack $w.f.kaX $w.f.kaY $w.f.kaZ \
			$w.f.kaS $w.f.kax $w.f.kay \
			$w.f.kaz
	    }

	    scale $w.f.fov -label "Field of view" -showvalue yes \
		    -from -1 -to 120 -orient horizontal -length 400 \
		    -variable perspective

	    global perspective
	    $w.f.fov set $perspective
	    pack $w.f.fov

	    if { $adcflag } {
		label $w.f.adclabel -text "Adc Sliders" -anchor c
		scale $w.f.kxadc -label "X adc" -showvalue no \
			-from $sliders(min) -to $sliders(max) -orient horizontal \
			-length 400 -command "sliders_change $id xadc" -variable sliders($id,xadc)
		scale $w.f.kyadc -label "Y adc" -showvalue no \
			-from $sliders(min) -to $sliders(max) -orient horizontal \
			-length 400 -command "sliders_change $id yadc" -variable sliders($id,yadc)
		scale $w.f.kang1 -label "Angle 1" -showvalue no \
			-from $sliders(min) -to $sliders(max) -orient horizontal -length 400 \
			-command "sliders_change $id ang1" -variable sliders($id,ang1)
		scale $w.f.kang2 -label "Angle 2" -showvalue no \
			-from $sliders(min) -to $sliders(max) -orient horizontal -length 400 \
			-command "sliders_change $id ang2" -variable sliders($id,ang2)
		scale $w.f.kdistadc -label "Adc distance" -showvalue no \
			-from $sliders(min) -to $sliders(max) -orient horizontal -length 400 \
			-command "sliders_change $id distadc" -variable sliders($id,distadc)

		foreach knob { xadc yadc ang1 ang2 distadc } {
		    set sliders($id,$knob) [sliders_add_tol [getknob $knob]]
		}
		
		pack $w.f.adclabel -pady 4
		pack $w.f.kxadc $w.f.kyadc $w.f.kang1 \
			$w.f.kang2 $w.f.kdistadc
	    }

	    pack $w.f -padx 1m -pady 1m

	} elseif { [llength $args]>0 && ![_mged_sliders] && [winfo exists $w] } {
	    destroy $w
	}
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
	return [expr $val - $sliders(NOISE)]
    }

    if { [expr $val < -$sliders(NOISE)] } then {
	return [expr $val + $sliders(NOISE)]
    }
    
    return 0
}


## sliders_add_tol
##   This is used when creating the sliders to properly set their values
##   by adding in a tolerance.

proc sliders_add_tol { val } {
    global sliders

    if { [expr $val > 0] } then {
	return [expr $val + $sliders(NOISE)]
    }

    if { [expr $val < 0] } then {
	return [expr $val - $sliders(NOISE)]
    }
    
    return 0
}

## sliders_change $id
##   Generic slider-changing callback.

proc sliders_change { id knob val } {
    global sliders

    cmd_set $id
    set id_list [cmd_get]

    if { [string length $knob] <= 2 } {
	_mged_knob $knob [expr [sliders_irlimit $val] / $sliders(scale)]
    } else {
	_mged_knob $knob [sliders_irlimit $val]
    }

    foreach s_id $id_list {
	if [winfo exists .sliders$s_id] {
	    set sliders($s_id,$knob) $val

#This stops the result of the above set from being printed to the screen
	    set junk ""
	}
    }
}

## sliders_zero
##   Zeroes the sliders.

proc sliders_zero { id w } {
    global sliders
    global rateknobs
    global adcflag

    _mged_knob zero

    if { $rateknobs } {
	foreach knob { X Y Z S x y z } {
	    set sliders($id,$knob) 0
	}
    } else {
	foreach knob { aX aY aZ aS ax ay az } {
	    set sliders($id,$knob) 0
	}
    }

    if { $adcflag } {
	foreach knob { xadc yadc ang1 ang2 distadc } {
	    set sliders($id,$knob) 0
	}
    }
}

## knob
##   To replace the regular knob function.

proc knob args {
    global sliders
    global rateknobs
    global adcflag

    eval _mged_knob $args
    set id_list [cmd_get]

    if { $rateknobs } {
	foreach id $id_list {
	    foreach knob { X Y Z S x y z } {
		set sliders($id,$knob) [sliders_add_tol \
			[expr [getknob $knob] * $sliders(scale)]]
	    }

	    if { $adcflag } {
		foreach knob { xadc yadc ang1 ang2 distadc } {
		    set sliders($id,$knob) [sliders_add_tol [getknob $knob]]
		}
	    }
	}
    } else {
	foreach id $id_list {
	    foreach knob { aX aY aZ aS ax ay az } {
		set sliders($id,$knob) [sliders_add_tol \
			[expr [getknob $knob] * $sliders(scale)]]
	    }

	    if { $adcflag } {
		foreach knob { xadc yadc ang1 ang2 distadc } {
		    set sliders($id,$knob) [sliders_add_tol [getknob $knob]]
		}
	    }
	}
    }

#    return
}


## iknob
##   To replace the regular iknob function.

proc iknob args {
    global sliders
    global rateknobs
    global adcflag

    eval _mged_iknob $args
    set id_list [cmd_get]

    if { $rateknobs } {
	foreach id $id_list {
	    foreach knob { X Y Z S x y z } {
		set sliders($id,$knob) [sliders_add_tol \
			[expr [getknob $knob] * $sliders(scale)]]
	    }

	    if { $adcflag } {
		foreach knob { xadc yadc ang1 ang2 distadc } {
		    set sliders($id,$knob) [sliders_add_tol [getknob $knob]]
		}
	    }
	}
    } else {
	foreach id $id_list {
	    foreach knob { aX aY aZ aS ax ay az } {
		set sliders($id,$knob) [sliders_add_tol \
			[expr [getknob $knob] * $sliders(scale)]]
	    }

	    if { $adcflag } {
		foreach knob { xadc yadc ang1 ang2 distadc } {
		    set sliders($id,$knob) [sliders_add_tol [getknob $knob]]
		}
	    }
	}
    }

#    return
}



