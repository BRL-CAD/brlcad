# Author - Bob Parker

#
#    Preliminary...
#    Ensure that all commands used here but not defined herein
#    are provided by the application
#

set extern_commands "_mged_sliders _mged_knob"
foreach cmd $extern_commands {
    if {[string compare [info command $cmd] $cmd] != 0} {
	puts stderr "Application fails to provide command '$cmd'"
	return
    }
}

# Customization Variables
set sliders(orient) horizontal
set sliders(length) 350;
set sliders(width) 10;
set sliders(high_res) 0.01
set sliders(low_res) 1
set sliders(show_value) 0;
set sliders(NOISE) 0.03;
set sliders(scale) 1.0;
set sliders(ar_scale) 11.372222;  # 2047.0/180.0
set sliders(rr_scale) 341.16667;  # 2047.0/6.0
set sliders(adc_scale) 1.0;
set sliders(adc_ang_scale) 45.488889;  # 2047.0/45.0
set sliders(max) [expr $sliders(scale) + $sliders(NOISE)];
set sliders(min) [expr $sliders(max) * -1];
set sliders(range) [expr $sliders(max) * 2];
set sliders(from) -1
set sliders(to) 1
set sliders(rate_rot_from) -6
set sliders(rate_rot_to) 6
set sliders(abs_rot_from) -180
set sliders(abs_rot_to) 180
set sliders(adc_from) -2048
set sliders(adc_to) 2047
set sliders(adc_ang_from) 90
set sliders(adc_ang_to) 0

set EDIT_CLASS_TRAN 1
set EDIT_CLASS_ROTATE 2
set EDIT_CLASS_SCALE 3


proc sliders args {
    global sliders
    global player_screen
    global rateknobs
    global adcflag
    global scroll_edit
    global EDIT_CLASS_TRAN
    global EDIT_CLASS_ROTATE
    global EDIT_CLASS_SCALE
    global sliders_on

    set result [eval _mged_sliders $args]
    # get a list of the id's associated with the current command window
    set cmd_list [cmd_get]
    set id [lindex $cmd_list 0]
    set sliders_on($id) [_mged_sliders]
    set dm_id [lindex $cmd_list 1]
    set id_list [lindex $cmd_list 2]

    if { [string compare $id "mged"]==0 } {
	continue
    }

    set w .sliders$id
    set w_exists [winfo exists $w]
    if { [llength $args]>0 && $sliders_on($id) } then {

	if { $w_exists } {
	    reconfig_sliders $w.f $id $dm_id
	    return
	}

	toplevel $w \
		-class Dialog \
		-screen $player_screen($id)
	wm title $w "$id\'s Sliders"
	    
	frame $w.f -borderwidth 3
	frame $w.f.buttons -borderwidth 3
	button $w.f.rate -text "Rate/Abs" -command "cmd_set $id;\
		set rateknobs !;"
	button $w.f.adc -text "Adc" -command "cmd_set $id; adc"
	button $w.f.zero -text Zero -command "cmd_set $id; knob zero"
	button $w.f.close -text Close -command "cmd_set $id; sliders off"
	if { $rateknobs } {
	    label $w.f.label \
		    -text "Rate Based Sliders" \
		    -anchor c

	    if { $scroll_edit($id) == $EDIT_CLASS_TRAN } {
		build_tran_sliders $w.f $id " - EDIT" edit_rate_tran "" ""
	    } else {
		build_tran_sliders $w.f $id "" rate_tran "$dm_id\," ""
	    }

	    if { $scroll_edit($id) == $EDIT_CLASS_SCALE } {
		build_scale_sliders $w.f $id " - EDIT" edit_rate_scale "" ""
	    } else {
		build_scale_sliders $w.f $id "" rate_scale "\($dm_id\)" ""
	    }

	    if { $scroll_edit($id) == $EDIT_CLASS_ROTATE } {
		build_rotate_sliders $w.f $id " - EDIT" $sliders(rate_rot_from) \
			$sliders(rate_rot_to) edit_rate_rotate "" ""
	    } else {
		build_rotate_sliders $w.f $id "" $sliders(rate_rot_from) \
			$sliders(rate_rot_to) rate_rotate "$dm_id\," ""
	    }
	} else {
	    label $w.f.label \
		    -text "Absolute Sliders" \
		    -anchor c

	    if { $scroll_edit($id) == $EDIT_CLASS_TRAN } {
		build_tran_sliders $w.f $id " - EDIT" edit_abs_tran "" a
	    } else {
		build_tran_sliders $w.f $id "" abs_tran "$dm_id\," a
	    }

	    if { $scroll_edit($id) == $EDIT_CLASS_SCALE } {
		build_scale_sliders $w.f $id " - EDIT" edit_abs_scale "" a
	    } else {
		build_scale_sliders $w.f $id "" abs_scale "\($dm_id\)" a
	    }

	    if { $scroll_edit($id) == $EDIT_CLASS_ROTATE } {
		build_rotate_sliders $w.f $id " - EDIT" $sliders(abs_rot_from) \
			$sliders(abs_rot_to) edit_abs_rotate "" a
	    } else {
		build_rotate_sliders $w.f $id "" $sliders(abs_rot_from) \
			$sliders(abs_rot_to) abs_rotate "$dm_id\," a
	    }
	}

	pack $w.f.label
	pack $w.f.kX $w.f.kY $w.f.kZ $w.f.kS \
		$w.f.kx $w.f.ky $w.f.kz

	scale $w.f.fov \
		-label "Field of view" \
		-orient $sliders(orient) \
		-length $sliders(length) \
		-width $sliders(width) \
		-from -1 \
		-to 120 \
		-variable perspective \
		-showvalue $sliders(show_value)

	global perspective
	$w.f.fov set $perspective
	pack $w.f.fov

	if { $adcflag } {
	    build_adc_sliders $w.f $id $dm_id
		
	    pack $w.f.adclabel -pady 4
	    pack $w.f.kxadc $w.f.kyadc $w.f.kang1 \
		    $w.f.kang2 $w.f.kdistadc
	}

	pack $w.f.rate $w.f.adc $w.f.zero -in $w.f.buttons -side left
	pack $w.f.close -in $w.f.buttons -side right
	pack $w.f.buttons -expand 1 -fill both

	pack $w.f -padx 1m -pady 1m
	wm protocol .sliders$id WM_DELETE_WINDOW "toggle_sliders $id"
	wm resizable $w 0 0
    } elseif { [llength $args]>0 && !$sliders_on($id) && $w_exists } {
	destroy $w
    }

    return $result
}

##XXXXXXXX This really belongs inside the scale widget.
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


## sliders_change $id
##   Generic slider-changing callback.
proc sliders_change { id knob val } {
    global sliders

    cmd_set $id
    _mged_knob $knob $val
}

proc build_tran_sliders { w id lt vt vid kp } {
    global sliders

    scale $w.kX \
	    -label "X Translate $lt" \
	    -orient $sliders(orient) \
	    -length $sliders(length) \
	    -width $sliders(width) \
	    -from $sliders(from) \
	    -to $sliders(to) \
	    -resolution $sliders(high_res) \
	    -command "sliders_change $id $kp\X" \
	    -variable "$vt\($vid\X\)" \
	    -showvalue $sliders(show_value)
    scale $w.kY \
	    -label "Y Translate $lt" \
	    -orient $sliders(orient) \
	    -length $sliders(length) \
	    -width $sliders(width) \
	    -from $sliders(from) \
	    -to $sliders(to) \
	    -resolution $sliders(high_res) \
	    -command "sliders_change $id $kp\Y" \
	    -variable "$vt\($vid\Y\)" \
	    -showvalue $sliders(show_value)
    scale $w.kZ \
	    -label "Z Translate $lt" \
	    -orient $sliders(orient) \
	    -length $sliders(length) \
	    -width $sliders(width) \
	    -from $sliders(from) \
	    -to $sliders(to) \
	    -resolution $sliders(high_res) \
	    -command "sliders_change $id $kp\Z" \
	    -variable "$vt\($vid\Z\)" \
	    -showvalue $sliders(show_value)
}

proc build_rotate_sliders { w id lt from to vt vid kp } {
    global sliders

    scale $w.kx \
	    -label "X Rotate $lt" \
	    -orient $sliders(orient) \
	    -length $sliders(length) \
	    -width $sliders(width) \
	    -from $from \
	    -to $to \
	    -resolution $sliders(high_res) \
	    -command "sliders_change $id $kp\x" \
	    -variable "$vt\($vid\X\)" \
	    -showvalue $sliders(show_value)
    scale $w.ky \
	    -label "Y Rotate $lt" \
	    -orient $sliders(orient) \
	    -length $sliders(length) \
	    -width $sliders(width) \
	    -from $from \
	    -to $to \
	    -resolution $sliders(high_res) \
	    -command "sliders_change $id $kp\y" \
	    -variable "$vt\($vid\Y\)" \
	    -showvalue $sliders(show_value)
    scale $w.kz \
	    -label "Z Rotate $lt" \
	    -orient $sliders(orient) \
	    -length $sliders(length) \
	    -width $sliders(width) \
	    -from $from \
	    -to $to \
	    -resolution $sliders(high_res) \
	    -command "sliders_change $id $kp\z" \
	    -variable "$vt\($vid\Z\)" \
	    -showvalue $sliders(show_value)
}


proc build_scale_sliders { w id lt vt vid kp } {
    global sliders

    scale $w.kS \
	    -label "Scale $lt" \
	    -orient $sliders(orient) \
	    -length $sliders(length) \
	    -width $sliders(width) \
	    -from $sliders(from) \
	    -to $sliders(to) \
	    -resolution $sliders(high_res) \
	    -command "sliders_change $id $kp\S" \
	    -variable "$vt$vid" \
	    -showvalue $sliders(show_value)
}


proc build_adc_sliders { w id dm_id } {
    global sliders

    label $w.adclabel \
	    -text "Adc Sliders" \
	    -anchor c
    scale $w.kxadc \
	    -label "X adc" \
	    -orient $sliders(orient) \
	    -length $sliders(length) \
	    -width $sliders(width) \
	    -from $sliders(adc_from) \
	    -to $sliders(adc_to) \
	    -resolution $sliders(low_res) \
	    -command "sliders_change $id xadc" \
	    -variable xadc($dm_id) \
	    -showvalue $sliders(show_value)
    scale $w.kyadc \
	    -label "Y adc" \
	    -orient $sliders(orient) \
	    -width $sliders(width) \
	    -length $sliders(length) \
	    -from $sliders(adc_from) \
	    -to $sliders(adc_to) \
	    -resolution $sliders(low_res) \
	    -command "sliders_change $id yadc" \
	    -variable yadc($dm_id) \
	    -showvalue $sliders(show_value)
    scale $w.kang1 \
	    -label "Angle 1" \
	    -orient $sliders(orient) \
	    -length $sliders(length) \
	    -width $sliders(width) \
	    -from $sliders(adc_ang_from) \
	    -to $sliders(adc_ang_to) \
	    -resolution $sliders(high_res) \
	    -command "sliders_change $id ang1" \
	    -variable ang1($dm_id) \
	    -showvalue $sliders(show_value)
    scale $w.kang2 \
	    -label "Angle 2" \
	    -orient $sliders(orient) \
	    -length $sliders(length) \
	    -width $sliders(width) \
	    -from $sliders(adc_ang_from) \
	    -to $sliders(adc_ang_to) \
	    -resolution $sliders(high_res) \
	    -command "sliders_change $id ang2" \
	    -variable ang2($dm_id) \
	    -showvalue $sliders(show_value)
    scale $w.kdistadc \
	    -label "Adc distance" \
	    -orient $sliders(orient) \
	    -length $sliders(length) \
	    -width $sliders(width) \
	    -from $sliders(adc_from) \
	    -to $sliders(adc_to) \
	    -resolution $sliders(low_res) \
	    -command "sliders_change $id distadc" \
	    -variable distadc($dm_id) \
	    -showvalue 0
}


proc reconfig_sliders { w id dm_id } {
    global sliders
    global rateknobs
    global adcflag
    global scroll_edit
    global EDIT_CLASS_TRAN
    global EDIT_CLASS_ROTATE
    global EDIT_CLASS_SCALE
    global mged_variable

    if { $rateknobs } {
	$w.label configure -text "Rate Based Sliders"

	if { $scroll_edit($id) == $EDIT_CLASS_TRAN } {
	    reconfig_tran_sliders $w $id " - EDIT" edit_rate_tran "" ""
	} else {
	    reconfig_tran_sliders $w $id "" rate_tran "$dm_id\," ""
	}

	if { $scroll_edit($id) == $EDIT_CLASS_SCALE } {
	    reconfig_scale_sliders $w $id " - EDIT" edit_rate_scale "" ""
	} else {
	    reconfig_scale_sliders $w $id "" rate_scale "\($dm_id\)" ""
	}

	if { $scroll_edit($id) == $EDIT_CLASS_ROTATE } {
	    reconfig_rotate_sliders $w $id " - EDIT" $sliders(rate_rot_from) \
		    $sliders(rate_rot_to) edit_rate_rotate "" ""
	} else {
	    reconfig_rotate_sliders $w $id "" $sliders(rate_rot_from) \
		    $sliders(rate_rot_to) rate_rotate "$dm_id\," ""
	}
    } else {
	$w.label configure -text "Absolute Sliders"

	if { $scroll_edit($id) == $EDIT_CLASS_TRAN } {
	    reconfig_tran_sliders $w $id " - EDIT" edit_abs_tran "" a
	} else {
	    reconfig_tran_sliders $w $id "" abs_tran "$dm_id\," a
	}

	if { $scroll_edit($id) == $EDIT_CLASS_SCALE } {
	    reconfig_scale_sliders $w $id " - EDIT" edit_abs_scale "" a
	} else {
	    reconfig_scale_sliders $w $id "" abs_scale "\($dm_id\)" a
	}

	if { $scroll_edit($id) == $EDIT_CLASS_ROTATE } {
	    reconfig_rotate_sliders $w $id " - EDIT" $sliders(abs_rot_from) \
		    $sliders(abs_rot_to) edit_abs_rotate "" a
	} else {
	    reconfig_rotate_sliders $w $id "" $sliders(abs_rot_from) \
		    $sliders(abs_rot_to) abs_rotate "$dm_id\," a
	}
    }

    if { $adcflag } {
	if ![winfo exists $w.adclabel] {
	    build_adc_sliders $w $id $dm_id
	}

	pack $w.adclabel -pady 4 -before $w.buttons
	pack $w.kxadc $w.kyadc $w.kang1 \
		$w.kang2 $w.kdistadc -after $w.adclabel
    } else {
	if [winfo exists $w.adclabel] {
	    pack forget $w.adclabel -pady 4
	    pack forget $w.kxadc $w.kyadc $w.kang1 \
		$w.kang2 $w.kdistadc
	}
    }
}


proc reconfig_tran_sliders { w id lt vt vid kp } {
    $w.kX configure \
	    -label "X Translate $lt" \
	    -command "sliders_change $id $kp\X" \
	    -variable "$vt\($vid\X\)"
    $w.kY configure \
	    -label "Y Translate $lt" \
	    -command "sliders_change $id $kp\Y" \
	    -variable "$vt\($vid\Y\)"
    $w.kZ configure \
	    -label "Z Translate $lt" \
	    -command "sliders_change $id $kp\Z" \
	    -variable "$vt\($vid\Z\)"
}


proc reconfig_rotate_sliders { w id lt from to vt vid kp } {
    $w.kx configure \
	    -label "X Rotate $lt" \
	    -from $from \
	    -to $to \
	    -command "sliders_change $id $kp\x" \
	    -variable "$vt\($vid\X\)"
    $w.ky configure \
	    -label "Y Rotate $lt" \
	    -from $from \
	    -to $to \
	    -command "sliders_change $id $kp\y" \
	    -variable "$vt\($vid\Y\)"
    $w.kz configure \
	    -label "Z Rotate $lt" \
	    -from $from \
	    -to $to \
	    -command "sliders_change $id $kp\z" \
	    -variable "$vt\($vid\Z\)"
}

proc reconfig_scale_sliders { w id lt vt vid kp } {
    $w.kS configure \
	    -label "Scale $lt" \
	    -command "sliders_change $id $kp\S" \
	    -variable "$vt$vid"
}
