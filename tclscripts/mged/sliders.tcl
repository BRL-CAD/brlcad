#
# Modifications -
#        (Bob Parker):
#             Generalized to accommodate multiple slider instances.
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

#
#    Preliminary...
#    Ensure that all commands used here but not defined herein
#    are provided by the application
#

set extern_commands "_mged_sliders _mged_knob"
foreach cmd $extern_commands {
    if {[expr [string compare [info command $cmd] $cmd] != 0]} {
	puts stderr "Application fails to provide command '$cmd'"
	return
    }
}

# size of dead spot on sliders
set sliders(NOISE) 64;
set sliders(scale) 2047.0;
set sliders(ar_scale) 11.372222;  # 2047.0/180.0
set sliders(rr_scale) 341.16667;  # 2047.0/6.0
set sliders(adc_scale) 1.0;
set sliders(adc_ang_scale) 45.488889;  # 2047.0/45.0
set sliders(max) [expr $sliders(scale) + $sliders(NOISE)];
set sliders(min) [expr $sliders(max) * -1];
set sliders(range) [expr $sliders(max) * 2];
set sliders(width) 10;
set sliders(show_value) 0;

proc sliders args {
    global sliders
    global player_screen
    global rateknobs
    global adcflag
    
    set result [eval _mged_sliders $args]
    set sliders_on [_mged_sliders]

    # get a list of the id's associated with the current command window
    set id_list [cmd_get]

    foreach id $id_list {
        if { [string compare $id "mged"]==0 } {
	    continue
	}

        set w .sliders$id
	if { [llength $args]>0 && $sliders_on } then {

	    if [winfo exists $w] {
		destroy $w
	    }

	    toplevel $w -class Dialog -screen $player_screen($id)
	    
	    frame $w.f -borderwidth 3
	    if { $rateknobs } {
		label $w.f.ratelabel -text "Rate Based Sliders" -anchor c
		scale $w.f.kX -label "X Translate" -showvalue no \
			-from $sliders(min) -to $sliders(max) -orient horizontal \
			-length 400 -command "sliders_change $id $sliders(scale) X" -variable sliders($id,X) \
			-width $sliders(width) -showvalue $sliders(show_value)
		scale $w.f.kY -label "Y Translate" -showvalue no \
			-from $sliders(min) -to $sliders(max) -orient horizontal \
			-length 400 -command "sliders_change $id $sliders(scale) Y" -variable sliders($id,Y) \
			-width $sliders(width) -showvalue $sliders(show_value)
		scale $w.f.kZ -label "Z Translate" -showvalue no \
			-from $sliders(min) -to $sliders(max) -orient horizontal \
			-length 400 -command "sliders_change $id $sliders(scale) Z" -variable sliders($id,Z) \
			-width $sliders(width) -showvalue $sliders(show_value)
		scale $w.f.kS -label "Scale" -showvalue no \
			-from $sliders(min) -to $sliders(max) -orient horizontal -length 400 \
			-command "sliders_change $id $sliders(scale) S" -variable sliders($id,S) \
			-width $sliders(width) -showvalue $sliders(show_value)
		scale $w.f.kx -label "X Rotate" -showvalue no \
			-from $sliders(min) -to $sliders(max) -orient horizontal -length 400 \
			-command "sliders_change $id $sliders(rr_scale) x" -variable sliders($id,x) \
			-width $sliders(width) -showvalue $sliders(show_value)
		scale $w.f.ky -label "Y Rotate" -showvalue no \
			-from $sliders(min) -to $sliders(max) -orient horizontal -length 400 \
			-command "sliders_change $id $sliders(rr_scale) y" -variable sliders($id,y) \
			-width $sliders(width) -showvalue $sliders(show_value)
		scale $w.f.kz -label "Z Rotate" -showvalue no \
			-from $sliders(min) -to $sliders(max) -orient horizontal -length 400 \
			-command "sliders_change $id $sliders(rr_scale) z" -variable sliders($id,z) \
			-width $sliders(width) -showvalue $sliders(show_value)

		foreach knob { X Y Z S } {
		    set sliders($id,$knob) [sliders_add_tol [expr [getknob $knob] * $sliders(scale)]]
		}

		foreach knob { x y z } {
		    set sliders($id,$knob) [sliders_add_tol [expr [getknob $knob] * $sliders(rr_scale)]]
		}

		pack $w.f.ratelabel -pady 4
		pack $w.f.kX $w.f.kY $w.f.kZ $w.f.kS \
			$w.f.kx $w.f.ky $w.f.kz

	    } else {
		label $w.f.abslabel -text "Absolute Sliders" -anchor c
		scale $w.f.kaX -label "X Translate" -showvalue no \
			-from $sliders(min) -to $sliders(max) -orient horizontal \
			-length 400 -command "sliders_change $id $sliders(scale) aX" \
			-variable sliders($id,aX) -width $sliders(width) -showvalue $sliders(show_value)
		scale $w.f.kaY -label "Y Translate" -showvalue no \
			-from $sliders(min) -to $sliders(max) -orient horizontal \
			-length 400 -command "sliders_change $id $sliders(scale) aY" \
			-variable sliders($id,aY) -width $sliders(width) -showvalue $sliders(show_value)
		scale $w.f.kaZ -label "Z Translate" -showvalue no \
			-from $sliders(min) -to $sliders(max) -orient horizontal \
			-length 400 -command "sliders_change $id $sliders(scale) aZ" \
			-variable sliders($id,aZ) -width $sliders(width) -showvalue $sliders(show_value)
		scale $w.f.kaS -label "Scale" -showvalue no \
			-from $sliders(min) -to $sliders(max) -orient horizontal -length 400 \
			-command "sliders_change $id $sliders(scale) aS" -variable sliders($id,aS) \
			-width $sliders(width) -showvalue $sliders(show_value)
		scale $w.f.kax -label "X Rotate" -showvalue no \
			-from $sliders(min) -to $sliders(max) -orient horizontal -length 400 \
			-command "sliders_change $id $sliders(ar_scale) ax" -variable sliders($id,ax) \
			-width $sliders(width) -showvalue $sliders(show_value)
		scale $w.f.kay -label "Y Rotate" -showvalue no \
			-from $sliders(min) -to $sliders(max) -orient horizontal -length 400 \
			-command "sliders_change $id $sliders(ar_scale) ay" -variable sliders($id,ay) \
			-width $sliders(width) -showvalue $sliders(show_value)
		scale $w.f.kaz -label "Z Rotate" -showvalue no \
			-from $sliders(min) -to $sliders(max) -orient horizontal -length 400 \
			-command "sliders_change $id $sliders(ar_scale) az" -variable sliders($id,az) \
			-width $sliders(width) -showvalue $sliders(show_value)

		foreach knob { aX aY aZ aS } {
		    set sliders($id,$knob) [sliders_add_tol [expr [getknob $knob] * $sliders(scale)]]
		}
		foreach knob { ax ay az } {
		    set sliders($id,$knob) [sliders_add_tol [expr [getknob $knob] * $sliders(ar_scale)]]
		}

		pack $w.f.abslabel -pady 4
		pack $w.f.kaX $w.f.kaY $w.f.kaZ \
			$w.f.kaS $w.f.kax $w.f.kay \
			$w.f.kaz
	    }

	    scale $w.f.fov -label "Field of view" -showvalue yes \
		    -from -1 -to 120 -orient horizontal -length 400 \
		    -variable perspective -width $sliders(width) -showvalue $sliders(show_value)

	    global perspective
	    $w.f.fov set $perspective
	    pack $w.f.fov

	    if { $adcflag } {
		label $w.f.adclabel -text "Adc Sliders" -anchor c
		scale $w.f.kxadc -label "X adc" -showvalue no \
			-from $sliders(min) -to $sliders(max) -orient horizontal \
			-length 400 -command "sliders_change $id $sliders(adc_scale) xadc" \
			-variable sliders($id,xadc) -width $sliders(width) -showvalue $sliders(show_value)
		scale $w.f.kyadc -label "Y adc" -showvalue no \
			-from $sliders(min) -to $sliders(max) -orient horizontal \
			-length 400 -command "sliders_change $id $sliders(adc_scale) yadc" \
			-variable sliders($id,yadc) -width $sliders(width) -showvalue $sliders(show_value)
		scale $w.f.kang1 -label "Angle 1" -showvalue no \
			-from $sliders(min) -to $sliders(max) -orient horizontal -length 400 \
			-command "sliders_adc_ang_change $id $sliders(adc_ang_scale) ang1" -variable sliders($id,ang1) \
			-width $sliders(width) -showvalue $sliders(show_value)
		scale $w.f.kang2 -label "Angle 2" -showvalue no \
			-from $sliders(min) -to $sliders(max) -orient horizontal -length 400 \
			-command "sliders_adc_ang_change $id $sliders(adc_ang_scale) ang2" -variable sliders($id,ang2) \
			-width $sliders(width) -showvalue $sliders(show_value)
		scale $w.f.kdistadc -label "Adc distance" -showvalue no \
			-from $sliders(min) -to $sliders(max) -orient horizontal -length 400 \
			-command "sliders_change $id $sliders(adc_scale) distadc" -variable sliders($id,distadc) \
			-width $sliders(width) -showvalue $sliders(show_value)

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

proc sliders_change { id scale knob val } {
    global sliders

    cmd_set $id
    set id_list [cmd_get]

    _mged_knob $knob [expr [sliders_irlimit $val] / $scale]

    foreach s_id $id_list {
	if [winfo exists .sliders$s_id] {
	    set sliders($s_id,$knob) $val

#This stops the result of the above set from being printed to the screen
	    set junk ""
	}
    }
}

proc sliders_adc_ang_change { id scale knob val } {
    global sliders

    cmd_set $id
    set id_list [cmd_get]

    _mged_knob $knob [expr 45.0 - [sliders_irlimit $val] / $scale]

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

# set_sliders
proc set_sliders {} {
    global sliders
    global rateknobs
    global adcflag

    set id_list [cmd_get]
    if { [string compare [lindex $id_list 0] "mged"] == 0 } {
	return
    }

    if { $rateknobs } {
	foreach id $id_list {
	    foreach knob { X Y Z S } {
		set sliders($id,$knob) [sliders_add_tol \
			[expr [getknob $knob] * $sliders(scale)]]
	    }

	    foreach knob { x y z } {
		set sliders($id,$knob) [sliders_add_tol \
			[expr [getknob $knob] * $sliders(rr_scale)]]
	    }

	    if { $adcflag } {
		foreach knob { xadc yadc ang1 ang2 distadc } {
		    set sliders($id,$knob) [sliders_add_tol [getknob $knob]]
		}
	    }
	}
    } else {
	foreach id $id_list {
	    foreach knob { aX aY aZ aS } {
		set sliders($id,$knob) [sliders_add_tol \
			[expr [getknob $knob] * $sliders(scale)]]
	    }

	    foreach knob { ax ay az } {
		set sliders($id,$knob) [sliders_add_tol \
			[expr [getknob $knob] * $sliders(ar_scale)]]
	    }

	    if { $adcflag } {
		foreach knob { xadc yadc ang1 ang2 distadc } {
		    set sliders($id,$knob) [sliders_add_tol [getknob $knob]]
		}
	    }
	}
    }
}


## knob
##   To replace the regular knob function.
proc knob args {
    eval _mged_knob $args
    set_sliders
}
