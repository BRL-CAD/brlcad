char sliders_gui_str[] = "\
\
set sliders(NOISE) 64;\
set sliders(rate) 1;\
set sliders(adc) 0;\
set sliders(cmd) \"knob\";\
\
proc sliders args {\
    global sliders;\
    global player_screen;\
\
    set result [eval _mged_sliders $args];\
    set id_list [cmd_get];\
\
    foreach id $id_list {\
        if { [string compare $id \"mged\"]==0 } {\
	    continue;\
	};\
\
        set w .sliders$id;\
	if { [llength $args]>0 && [_mged_sliders] } {\
	    if [winfo exists $w] {\
		destroy $w;\
	    };\
\
	    toplevel $w -class Dialog -screen $player_screen($id);\
	    frame $w.f -borderwidth 3;\
\
	    if { $sliders(rate) } {\
		label $w.f.ratelabel -text \"Rate Based Sliders\" -anchor c;\
		scale $w.f.kX -label \"X Translate\" -showvalue no \
			-from -2048 -to 2047 -orient horizontal \
			-length 400 -command \"sliders_change $id X\";\
		scale $w.f.kY -label \"Y Translate\" -showvalue no \
			-from -2048 -to 2047 -orient horizontal \
			-length 400 -command \"sliders_change $id Y\";\
		scale $w.f.kZ -label \"Z Translate\" -showvalue no \
			-from -2048 -to 2047 -orient horizontal \
			-length 400 -command \"sliders_change $id Z\";\
		scale $w.f.kS -label \"Zoom\" -showvalue no \
			-from -2048 -to 2047 -orient horizontal -length 400 \
			-command \"sliders_change $id S\";\
		scale $w.f.kx -label \"X Rotate\" -showvalue no \
			-from -2048 -to 2047 -orient horizontal -length 400 \
			-command \"sliders_change $id x\";\
		scale $w.f.ky -label \"Y Rotate\" -showvalue no \
			-from -2048 -to 2047 -orient horizontal -length 400 \
			-command \"sliders_change $id y\";\
		scale $w.f.kz -label \"Z Rotate\" -showvalue no \
			-from -2048 -to 2047 -orient horizontal -length 400 \
			-command \"sliders_change $id z\";\
\
		foreach knob { X Y Z S x y z } {\
		    $w.f.k$knob set [sliders_add_limit [expr [getknob $knob] * 2047.0]]\
		};\
\
		pack $w.f.ratelabel -pady 4;\
		pack $w.f.kX $w.f.kY $w.f.kZ $w.f.kS \
			$w.f.kx $w.f.ky $w.f.kz;\
	    } else {\
		label $w.f.abslabel -text \"Absolute Sliders\" -anchor c;\
		scale $w.f.kaX -label \"X Translate\" -showvalue no \
			-from -2048 -to 2047 -orient horizontal \
			-length 400 -command \"sliders_change $id aX\";\
		scale $w.f.kaY -label \"Y Translate\" -showvalue no \
			-from -2048 -to 2047 -orient horizontal \
			-length 400 -command \"sliders_change $id aY\";\
		scale $w.f.kaZ -label \"Z Translate\" -showvalue no \
			-from -2048 -to 2047 -orient horizontal \
			-length 400 -command \"sliders_change $id aZ\";\
		scale $w.f.kaS -label \"Zoom\" -showvalue no \
			-from -2048 -to 2047 -orient horizontal -length 400 \
			-command \"sliders_change $id aS\";\
		scale $w.f.kax -label \"X Rotate\" -showvalue no \
			-from -2048 -to 2047 -orient horizontal -length 400 \
			-command \"sliders_change $id ax\";\
		scale $w.f.kay -label \"Y Rotate\" -showvalue no \
			-from -2048 -to 2047 -orient horizontal -length 400 \
			-command \"sliders_change $id ay\";\
		scale $w.f.kaz -label \"Z Rotate\" -showvalue no \
			-from -2048 -to 2047 -orient horizontal -length 400 \
			-command \"sliders_change $id az\";\
\
		foreach knob { aX aY aZ aS ax ay az } {\
		    $w.f.k$knob set [sliders_add_limit [expr [getknob $knob] * 2047.0]];\
		};\
\
		pack $w.f.abslabel -pady 4;\
		pack $w.f.kaX $w.f.kaY $w.f.kaZ \
			$w.f.kaS $w.f.kax $w.f.kay \
			$w.f.kaz;\
	    };\
\
	    scale $w.f.fov -label \"Field of view\" -showvalue yes \
		    -from -1 -to 120 -orient horizontal -length 400 \
		    -variable perspective;\
\
	    global perspective;\
	    $w.f.fov set $perspective;\
	    pack $w.f.fov;\
\
	    if { $sliders(adc) } {\
		label $w.f.adclabel -text \"Adc Sliders\" -anchor c;\
		scale $w.f.kxadc -label \"X adc\" -showvalue no \
			-from -2048 -to 2047 -orient horizontal \
			-length 400 -command \"sliders_change $id xadc\";\
		scale $w.f.kyadc -label \"Y adc\" -showvalue no \
			-from -2048 -to 2047 -orient horizontal \
			-length 400 -command \"sliders_change $id yadc\";\
		scale $w.f.kang1 -label \"Angle 1\" -showvalue no \
			-from -2048 -to 2047 -orient horizontal -length 400 \
			-command \"sliders_change $id ang1\";\
		scale $w.f.kang2 -label \"Angle 2\" -showvalue no \
			-from -2048 -to 2047 -orient horizontal -length 400 \
			-command \"sliders_change $id ang2\";\
		scale $w.f.kdistadc -label \"Adc distance\" -showvalue no \
			-from -2048 -to 2047 -orient horizontal -length 400 \
			-command \"sliders_change $id distadc\";\
\
		foreach knob { xadc yadc ang1 ang2 distadc } {\
		    $w.f.k$knob set [sliders_add_limit [getknob $knob]];\
		};\
\
		pack $w.f.adclabel -pady 4;\
		pack $w.f.kxadc $w.f.kyadc $w.f.kang1 \
			$w.f.kang2 $w.f.kdistadc;\
	    };\
\
	    pack $w.f -padx 1m -pady 1m;\
	} elseif { [llength $args]>0 && ![_mged_sliders] && [winfo exists $w] } {\
	    destroy $w;\
	};\
    };\
\
    return $result;\
};\
\
proc sliders_irlimit { val } {\
    global sliders;\
\
    if { [expr $val > $sliders(NOISE)] } then {\
	return [expr $val - $sliders(NOISE)];\
    };\
\
    if { [expr $val < -$sliders(NOISE)] } then {\
	return [expr $val + $sliders(NOISE)];\
    };\
\
    return 0;\
};\
\
proc sliders_add_limit { val } {\
    global sliders;\
\
    if { [expr $val > 0] } then {\
	return [expr $val + $sliders(NOISE)];\
    };\
\
    if { [expr $val < 0] } then {\
	return [expr $val - $sliders(NOISE)];\
    }\
\
    return 0;\
};\
\
proc sliders_change { id knob val } {\
    global sliders;\
\
    cmd_set $id;\
    set id_list [cmd_get];\
\
    if { [string length $knob] <= 2 } {\
	_mged_knob $knob [expr [sliders_irlimit $val] / 2047.0];\
    } else {\
	_mged_knob $knob [sliders_irlimit $val];\
    };\
\
    foreach s_id $id_list {\
	if [winfo exists .sliders$s_id] {\
	    .sliders$s_id.f.k$knob set $val;\
	};\
    };\
};\
\
proc sliders_zero { w } {\
    global sliders;\
\
    _mged_knob zero;\
\
    if { $sliders(rate) } {\
	foreach knob { X Y Z S x y z } {\
	    $w.f.k$knob set 0;\
	};\
    } else {\
	foreach knob { aX aY aZ aS ax ay az } {\
	    $w.f.k$knob set 0;\
	};\
    };\
\
    if { $sliders(adc) } {\
	foreach knob { xadc yadc ang1 ang2 distadc } {\
	    $w.f.k$knob set 0;\
	};\
    };\
};\
\
proc knob {args} {\
    set do_mged_knob 1;\
    set nob [lindex $args 0];\
    set id_list [cmd_get];\
    set id [lindex $id_list 0];\
\
    if { [llength $args]==1 } {\
	if { [string compare $nob zero]==0 } {\
	    if [winfo exists .sliders$id] {\
		set do_mged_knob 0;\
			sliders_zero .sliders$id;\
	    };\
	};\
    } else {\
	if [winfo exists .sliders$id.f.k$nob] {\
	    set do_mged_knob 0;\
\
	    if { [string length $nob] > 2 } {\
		.sliders$id.f.k$nob set [lindex $args 1];\
	    } else {\
		.sliders$id.f.k$nob set [expr 2047.0*[lindex $args 1]];\
	    };\
	};\
    };\
\
    if {$do_mged_knob} {\
	if { [llength $args]==2 } {\
	    if { [string length $nob] > 2 } {\
		set new_args [lreplace $args 1 1 [sliders_irlimit [lindex $args 1]]];\
	    } else {\
		set new_args [lreplace $args 1 1\
			[expr [sliders_irlimit [expr 2047.0*[lindex $args 1]]] / 2047.0]];\
	    };\
\
	    eval _mged_knob $new_args;\
	} else {\
	    eval _mged_knob $args;\
	};\
    };\
};\
\
proc iknob {args} {\
    set do_mged_knob 1;\
    set nob [lindex $args 0];\
    set id_list [cmd_get];\
    set id [lindex $id_list 0];\
\
    if { [llength $args]==1 } {\
	if { [string compare $nob zero]==0 } {\
	    if [winfo exists .sliders$id] {\
		set do_mged_knob 0;\
		sliders_zero .sliders$id;\
	    };\
	};\
    } else {\
	if [winfo exists .sliders$id.f.k$nob] {\
	    set do_mged_knob 0;\
\
	    if { [string length $nob] > 2 } {\
		.sliders$id.f.k$nob set \
			[expr [.sliders$id.f.k$nob get] + \
			[lindex $args 1]];\
	    } else {\
		set val [expr [.sliders$id.f.k$nob get] + [lindex $args 1]*2047.0];\
\
		if [string match a\[xyz\] $nob] {\
		    if {$val > 2047.0} {\
			set val [expr $val - 4095];\
		    } elseif {$val < -2048.0} {\
			set val [expr $val + 4095];\
		    };\
		};\
		.sliders$id.f.k$nob set $val;\
	    };\
	};\
    };\
\
    if {$do_mged_knob} {\
	eval _mged_iknob $args;\
    };\
};\
";
