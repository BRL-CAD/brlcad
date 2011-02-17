#                   R T C O N T R O L . T C L
# BRL-CAD
#
# Copyright (c) 1998-2011 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# version 2.1 as published by the Free Software Foundation.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#
###
#
# Description -
#	The raytrace control panel is designed to be used with an
#	Mged object. It probably should be an inner class of the Mged
#	class.
#

::itk::usual RtControl {
}

::itcl::class RtControl {
    inherit ::itk::Toplevel

    itk_option define -olist olist Olist {}
    itk_option define -omode omode Omode {}
    itk_option define -nproc nproc Nproc 16
    itk_option define -hsample hsample Hsample 0
    itk_option define -jitter jitter Jitter 0
    itk_option define -lmodel lmodel Lmodel 0
    itk_option define -other other Other "-A 0.9"
    itk_option define -size size Size 512
    itk_option define -color color Color {0 0 0}
    itk_option define -mged mged Mged ""
    itk_option define -fb_active_pane_callback fb_active_pane_callback FB_Active_Pane_Callback ""
    itk_option define -fb_enabled fb_enabled FB_Enabled 0
    itk_option define -fb_enabled_callback fb_enabled_callback FB_Enabled_Callback ""
    itk_option define -fb_mode_callback fb_mode_callback FB_Mode_Callback ""

    constructor {args} {}

    public {
	common FB_MODES {Underlay Interlay Overlay}

	method activate {}
	method activate_adv {}
	method deactivate {}
	method deactivate_adv {}
	method center {w gs {cw ""}}

	method abort {}
	method clear {}
	method raytrace {}
	method raytracePlus {}
	method setActivePane {{_pane ""}}
	method setFbMode {args}
	method toggleFbMode {}
	method toggleFB {}
	method updateControlPanel {}
    }    

    protected {
	variable pmGlobalPhotonsEntry 16384
	variable pmGlobalPhotonsScale 14
	variable pmCausticsPercentScale 0
	variable pmIrradianceRaysEntry 100
	variable pmIrradianceRaysScale 10
	variable pmAngularTolerance 60.0
	variable pmRandomSeedEntry 0
	variable pmRandomSeedScale 0
	variable pmImportanceMapping 0
	variable pmIrradianceHypersamplingCache 0
	variable pmVisualizeIrradiance 0
	variable pmScaleIndirectEntry 1.0
	variable pmScaleIndirectScale 1.0
	variable pmCacheFileEntry ""

	variable saveVisibilityBinding {}
	variable saveFocusOutBinding {}

	variable fb_mode 1
	variable fb_mode_ur 1
	variable fb_mode_ul 1
	variable fb_mode_ll 1
	variable fb_mode_lr 1
	variable fb_mode_str [lindex $FB_MODES 0]
	variable rtActivePane ""
	variable rtActivePaneStr ""
	variable rtColor "0 0 50"
	variable rtPrevColor "0 0 0"
	variable rtSize ""
	variable rtLightModel 0
	variable rtLightModelSpec "Full"
	variable rtJitter 0
	variable rtJitterSpec "None"
	variable win_geom ""
	variable win_geom_adv ""
	variable msg ""
	variable msg_adv ""
	variable colorM
	variable colorE
	variable jitterM
	variable lmodelM
	variable isaMged 0
	variable isaGed 0

	method build_adv {}
	method build_photon_map {_parent}
	method build_pm_scale {_parent _name _text _cmd _from _to _vname1 _vname2 _orient}
	method pm_nonLinearEvent {_entry _sval}
	method pm_linearEvent {_entry _sval}
	method pm_linearEventRound {_entry _sval}
	method pm_raysEvent {_entry _sval}
	method colorChooser {_color}
	method set_color {}
	method set_fb_mode {}
	method set_fb_mode_str {}
	method set_size {}
	method set_jitter {}
	method set_lmodel {}
	method cook_dest {dest}
	method fb_mode {}
	method ok {}
	method get_cooked_dest {}
	method menuStatusCB {w}
	method leaveCB {}
	method enterAdvCB {}
	method enterEnablefbCB {}
	method enterOkCB {}
	method enterRaytraceCB {}
	method enterAbortCB {}
	method enterClearCB {}
	method enterDismissCB {}
	method enterDestCB {}
	method enterSizeCB {}
	method enterColorCB {}
	method getPaneStr {}
	method getSize {}

	method enableFB {}
	method menuStatusAdvCB {w}
	method leaveAdvCB {}
	method enterDismissAdvCB {}
	method enterNProcCB {}
	method enterHSampleCB {}
	method enterOtherCB {}

	method update_control_panel {}
    }
}

::itcl::body RtControl::constructor {args} {
    # revive a few ignored options
    #itk_option add hull.screen

    itk_component add gridF1 {
	::ttk::frame $itk_interior.gridF1
    } {}

    itk_component add gridF2 {
	::ttk::frame $itk_interior.gridF2 -relief groove -borderwidth 2
    } {}

    itk_component add gridF3 {
	::ttk::frame $itk_interior.gridF3
    } {}

    itk_component add gridF4 {
	::ttk::frame $itk_interior.gridF4
    } {}

    itk_component add apaneL {
	::ttk::label $itk_interior.apaneL \
	    -text "Active Pane" \
	    -anchor e
    } {}

    itk_component add apaneCB {
	::ttk::combobox $itk_interior.apaneCB \
	    -state readonly \
	    -textvariable [::itcl::scope rtActivePaneStr] \
	    -values {{Upper Left} {Upper Right} {Lower Left} {Lower Right}}
    } {}

    bind $itk_component(apaneCB) <<ComboboxSelected>> [::itcl::code $this setActivePane]

    itk_component add fbmodeL {
	::ttk::label $itk_interior.fbmodeL \
	    -text "Framebuffer Mode" \
	    -anchor e
    } {}

    itk_component add fbmodeCB {
	::ttk::combobox $itk_interior.fbmodeCB \
	    -state readonly \
	    -textvariable [::itcl::scope fb_mode_str] \
	    -values $FB_MODES
    } {}

    bind $itk_component(fbmodeCB) <<ComboboxSelected>> [::itcl::code $this set_fb_mode]

    itk_component add bgcolorF {
	::ttk::frame $itk_interior.bgcolorF
    } {}

    set bg [eval ::cadwidgets::Ged::rgb_to_tk $rtColor]
    itk_component add bgcolorpatchL {
	::ttk::label $itk_component(bgcolorF).bgcolorpatchL \
	    -text " " \
	    -width 2 \
	    -background $bg \
	    -anchor w
    } {}

    itk_component add bgcolorL {
	::ttk::label $itk_component(bgcolorF).bgcolorL \
	    -text "Background Color" \
	    -anchor e
    } {}

    itk_component add bgcolorCB {
	::ttk::combobox $itk_interior.bgcolorCB \
	    -state readonly \
	    -textvariable [::itcl::scope rtColor] \
	    -values {Navy Black White Red Green Blue Yellow Cyan Magenta {Color Tool...}}
    } {}

    bind $itk_component(bgcolorCB) <<ComboboxSelected>> [::itcl::code $this set_color]

    itk_component add advB {
	::ttk::button $itk_interior.advB \
	    -text "Advanced Settings..." \
	    -width 16 \
	    -command [::itcl::code $this activate_adv]
    } {}
    bind $itk_component(advB) <Enter> [::itcl::code $this enterAdvCB]
    bind $itk_component(advB) <Leave> [::itcl::code $this leaveCB]

    itk_component add enablefbB {
	::ttk::checkbutton $itk_interior.enablefbB \
	    -text "Enable Framebuffer" \
	    -width 16 \
	    -variable [::itcl::scope itk_option(-fb_enabled)] \
	    -command [::itcl::code $this enableFB]
    } {}
    bind $itk_component(enablefbB) <Enter> [::itcl::code $this enterEnablefbCB]
    bind $itk_component(enablefbB) <Leave> [::itcl::code $this leaveCB]

    itk_component add okB {
	::ttk::button $itk_interior.okB \
	    -text "Ok" \
	    -width 7 \
	    -command [::itcl::code $this ok]
    } {}
    bind $itk_component(okB) <Enter> [::itcl::code $this enterOkCB]
    bind $itk_component(okB) <Leave> [::itcl::code $this leaveCB]

    itk_component add raytraceB {
	::ttk::button $itk_interior.raytraceB \
	    -text "Raytrace" \
	    -width 7 \
	    -command [::itcl::code $this raytrace]
    } {}
    bind $itk_component(raytraceB) <Enter> [::itcl::code $this enterRaytraceCB]
    bind $itk_component(raytraceB) <Leave> [::itcl::code $this leaveCB]

    itk_component add abortB {
	::ttk::button $itk_interior.abortB \
	    -text "Abort" \
	    -width 7 \
	    -command [::itcl::code $this abort]
    } {}
    bind $itk_component(abortB) <Enter> [::itcl::code $this enterAbortCB]
    bind $itk_component(abortB) <Leave> [::itcl::code $this leaveCB]

    itk_component add clearB {
	::ttk::button $itk_interior.clearB \
	    -text "Clear" \
	    -width 7 \
	    -command [::itcl::code $this clear]
    } {}
    bind $itk_component(clearB) <Enter> [::itcl::code $this enterClearCB]
    bind $itk_component(clearB) <Leave> [::itcl::code $this leaveCB]

    itk_component add dismissB {
	::ttk::button $itk_interior.dismissB \
	    -text "Dismiss" \
	    -width 7 \
	    -command [::itcl::code $this deactivate]
    } {}
    bind $itk_component(dismissB) <Enter> [::itcl::code $this enterDismissCB]
    bind $itk_component(dismissB) <Leave> [::itcl::code $this leaveCB]

    itk_component add statusL {
	::ttk::label $itk_interior.statusL \
	    -anchor w \
	    -relief sunken \
	    -textvariable [::itcl::scope msg]
    } {}

    grid $itk_component(bgcolorpatchL) $itk_component(bgcolorL) -sticky nsew
    grid columnconfigure $itk_component(bgcolorF) 0 -weight 1

    grid x $itk_component(advB) x $itk_component(enablefbB) x -sticky nsew -in $itk_component(gridF3)
    grid columnconfigure $itk_component(gridF3) 0 -weight 1
    grid columnconfigure $itk_component(gridF3) 2 -weight 1
    grid columnconfigure $itk_component(gridF3) 4 -weight 1

    grid $itk_component(apaneL) $itk_component(apaneCB) -pady 1 -sticky nsew -in $itk_component(gridF1)
    grid $itk_component(fbmodeL) $itk_component(fbmodeCB) -pady 1 -sticky nsew -in $itk_component(gridF1)
    grid $itk_component(bgcolorF) $itk_component(bgcolorCB) -pady 1 -sticky nsew -in $itk_component(gridF1)

    grid columnconfigure $itk_component(gridF1) 1 -weight 1
    grid rowconfigure $itk_component(gridF1) 0 -weight 1
    grid rowconfigure $itk_component(gridF1) 1 -weight 1
    grid rowconfigure $itk_component(gridF1) 2 -weight 1
    grid rowconfigure $itk_component(gridF1) 3 -weight 1

    grid $itk_component(gridF1) -padx 4 -pady 4 -sticky nsew -in $itk_component(gridF2)
    grid columnconfigure $itk_component(gridF2) 0 -weight 1
    grid rowconfigure $itk_component(gridF2) 0 -weight 1

    grid $itk_component(okB) $itk_component(raytraceB) \
	$itk_component(abortB) x $itk_component(clearB) x \
	$itk_component(dismissB) -sticky "nsew" -in $itk_component(gridF4)
    grid columnconfigure $itk_component(gridF4) 3 -weight 1
    grid columnconfigure $itk_component(gridF4) 5 -weight 1

    grid $itk_component(gridF2) -padx 4 -pady 2 -sticky nsew
    grid $itk_component(gridF3) -padx 4 -pady 2 -sticky nsew
    grid $itk_component(gridF4) -padx 4 -pady 2 -sticky nsew
    grid $itk_component(statusL) -padx 2 -pady 2 -sticky nsew
    grid columnconfigure $itk_component(hull) 0 -weight 1
    grid rowconfigure $itk_component(hull) 0 -weight 1

    # build the advanced settings panel
    build_adv

    $this configure -background [::ttk::style lookup label -background]

    # process options
    eval itk_initialize $args

    wm withdraw $itk_component(hull)
    wm title $itk_component(hull) "Raytrace Control Panel"
}


############################### Configuration Options ###############################

::itcl::configbody RtControl::color {\
    set rtColor $itk_option(-color)
    set bg [eval ::cadwidgets::Ged::rgb_to_tk $rtColor]
    $itk_component(bgcolorpatchL) configure -background $bg
}

::itcl::configbody RtControl::nproc {
    if {![regexp "^\[0-9\]+$" $itk_option(-nproc)]} {
	error "Bad value - $itk_option(-nproc)"
    }
}

::itcl::configbody RtControl::hsample {
    if {![regexp "^\[0-9\]+$" $itk_option(-hsample)]} {
	error "Bad value - $itk_option(-hsample)"
    }
}

::itcl::configbody RtControl::mged {
    if {$itk_option(-mged) == ""} {
	return
    }

    updateControlPanel
}

::itcl::configbody RtControl::fb_enabled {
    if {![string is digit $itk_option(-fb_enabled)]} {
	error "Bad value - $itk_option(-fb_enabled)"
    }

    enableFB
}


################################# Public Methods ################################


::itcl::body RtControl::activate {} {
    raise $itk_component(hull)

    # center on screen
    if {$win_geom == ""} {
	center $itk_component(hull) win_geom
    }
    wm geometry $itk_component(hull) $win_geom
    wm deiconify $itk_component(hull)

    set_size 
}

::itcl::body RtControl::activate_adv {} {
    set saveVisibilityBinding [bind $itk_component(hull) <Visibility>]
    set saveFocusOutBinding [bind $itk_component(hull) <FocusOut>]
    bind $itk_component(hull) <Visibility> {}
    bind $itk_component(hull) <FocusOut> {}

    raise $itk_component(adv)

    # center over control panel
    if {$win_geom_adv == ""} {
	center $itk_component(adv) win_geom_adv $itk_component(hull)
    }
    wm geometry $itk_component(adv) $win_geom_adv
    wm deiconify $itk_component(adv)
}

::itcl::body RtControl::deactivate {} {
    set win_geom [wm geometry $itk_component(hull)]
    wm withdraw $itk_component(hull)
    deactivate_adv
}

::itcl::body RtControl::deactivate_adv {} {
    set win_geom_adv [wm geometry $itk_component(adv)]
    wm withdraw $itk_component(adv)

    bind $itk_component(hull) <Visibility> $saveVisibilityBinding 
    bind $itk_component(hull) <FocusOut> $saveFocusOutBinding 
    raise $itk_component(hull)
}

::itcl::body RtControl::center {w gs {cw ""}} {
    upvar $gs geom

    update idletasks

    if {$cw != "" && [winfo exists $cw]} {
	set x [expr {int([winfo reqwidth $cw] * 0.5 + [winfo x $cw] - [winfo reqwidth $w] * 0.5)}]
	set y [expr {int([winfo reqheight $cw] * 0.5 + [winfo y $cw] - [winfo reqheight $w] * 0.5)}]
    } else {
	set x [expr {int([winfo screenwidth $w] * 0.5 - [winfo reqwidth $w] * 0.5)}]
	set y [expr {int([winfo screenheight $w] * 0.5 - [winfo reqheight $w] * 0.5)}]
    }

    wm geometry $w +$x+$y
    set geom +$x+$y
}

::itcl::body RtControl::abort {} {
    if {!$isaMged && !$isaGed} {
	error "Raytrace Control Panel($this) is not associated with an Mged object"
    }

    if {$isaMged} {
	$itk_option(-mged) component $rtActivePane rtabort
    } else {
	$itk_option(-mged) rtabort
    }
}

::itcl::body RtControl::clear {} {
    global tcl_platform

    if {!$isaMged && !$isaGed} {
	error "Raytrace Control Panel($this) is not associated with an Mged object"
    }

    set cooked_dest [get_cooked_dest]

    set fbclear [file join [bu_brlcad_root "bin"] fbclear]
    set result [catch {eval exec "\"$fbclear\"" -F $cooked_dest $rtColor &} rt_error]

    if {$result} {
	error $rt_error
    }
}

::itcl::body RtControl::raytrace {} {
    if {!$isaMged && !$isaGed} {
	error "Raytrace Control Panel($this) is not associated with an Mged object"
    }

    if {$isaMged} {
	set rt_cmd "$itk_option(-mged) component $rtActivePane rt -F [get_cooked_dest]"
    } else {
	# isaGed must be true
	set rt_cmd "$itk_option(-mged) pane_rt $rtActivePane -F [get_cooked_dest]"

	if {[$itk_option(-mged) rect draw]} {
	    set pos [$itk_option(-mged) rect pos]
	    set dim [$itk_option(-mged) rect dim]

	    set xmin [lindex $pos 0]
	    set ymin [lindex $pos 1]
	    set width [lindex $dim 0]
	    set height [lindex $dim 1]

	    if {$width != 0 && $height != 0} {
		if {$width > 0} {
		    set xmax [expr $xmin + $width]
		} else {
		    set xmax $xmin
		    set xmin [expr $xmax + $width]
		}

		if {$height > 0} {
		    set ymax [expr $ymin + $height]
		} else {
		    set ymax $ymin
		    set ymin [expr $ymax + $height]
		}

		append rt_cmd " -j $xmin,$ymin,$xmax,$ymax"
	    }
	}
    }

    if {$rtSize != ""} {
	set result [regexp "^(\[ \]*\[0-9\]+)((\[ \]*\[xX\]?\[ \]*)|(\[ \]+))(\[0-9\]*\[ \]*)$"\
			$rtSize smatch width junkA junkB junkC height]
	if {$result} {
	    if {$height != ""} {
		append rt_cmd " -w $width -n $height"
		set width $width.0
		set height $height.0
		set aspect [expr {$width / $height}]
		append rt_cmd " -V $aspect"
	    } else {
		append rt_cmd " -s $width"
	    }
	} else {
	    error "Bad size - $rtSize"
	}
    }

    append rt_cmd " -C[lindex $rtColor 0]/[lindex $rtColor 1]/[lindex $rtColor 2]"

    if {$itk_option(-nproc) != ""} {
	append rt_cmd " -P$itk_option(-nproc)"
    }

    if {$itk_option(-hsample) != ""} {
	append rt_cmd " -H$itk_option(-hsample)"
    }

    append rt_cmd " -J$rtJitter"

    if {$rtLightModel != ""} {
	append rt_cmd " -l$rtLightModel"

	if {$rtLightModel == 7} {
	    append rt_cmd ",$pmGlobalPhotonsEntry,$pmCausticsPercentScale,$pmIrradianceRaysScale,$pmAngularTolerance,$pmRandomSeedEntry,$pmImportanceMapping,$pmIrradianceHypersamplingCache,$pmVisualizeIrradiance,$pmScaleIndirectScale,$pmCacheFileEntry -A0"
	}
    }

    if {$itk_option(-other) != ""} {
	append rt_cmd " $itk_option(-other)"
    }

    set result [catch {eval $rt_cmd} rt_error]
    if {$result} {
	error $rt_error
    }
}

::itcl::body RtControl::raytracePlus {} {
    if {!$itk_option(-fb_enabled)} {
	toggleFB
    }

    raytrace
}

::itcl::body RtControl::setActivePane {{_pane ""}} {
    if {!$isaMged && !$isaGed} {
	error "Raytrace Control Panel($this) is not associated with an Mged object"
    }

    if {$_pane == ""} {
	switch -- $rtActivePaneStr {
	    "Upper Left" {
		set rtActivePane ul
	    }
	    "Upper Right" {
		set rtActivePane ur
	    }
	    "Lower Left" {
		set rtActivePane ll
	    }
	    "Lower Right" {
		set rtActivePane lr
	    }
	}
    } else {
	set rtActivePane $_pane
	switch -- $rtActivePane {
	    "ul" {
		set rtActivePaneStr "Upper Left"
	    }
	    "ur" {
		set rtActivePaneStr "Upper Right"
	    }
	    "ll" {
		set rtActivePaneStr "Lower Left"
	    }
	    "lr" {
		set rtActivePaneStr "Lower Right"
	    }
	}
    }

    if {$itk_option(-fb_active_pane_callback) != ""} {
	catch {$itk_option(-fb_active_pane_callback) $rtActivePane}
    }

    update_control_panel

    if {$itk_option(-fb_enabled_callback) != ""} {
	catch {$itk_option(-fb_enabled_callback) $itk_option(-fb_enabled)}
    }

    if {$itk_option(-fb_mode_callback) != ""} {
	catch {$itk_option(-fb_mode_callback) $fb_mode}
    }
}

::itcl::body RtControl::setFbMode {args} {
    if {$args == ""} {
	return $fb_mode
    }

    set arg [lindex $args 0]
    if {![string is digit $arg]} {
	return
    }

    switch -- $arg {
	0 -
	1 -
	2 -
	3 {
	    set fb_mode $arg
	    enableFB
	    set_fb_mode_str
	}
	default {
	    return
	}
    }
}

::itcl::body RtControl::toggleFbMode {} {
    switch -- $fb_mode {
	1 -
	2 {
	    incr fb_mode
	}
	3 {
	    set fb_mode 1
	}
	default {
	    return
	}
    }

    enableFB
    set_fb_mode_str
}

::itcl::body RtControl::toggleFB {} {
    if {$itk_option(-fb_enabled)} {
	set itk_option(-fb_enabled) 0
    } else {
	set_size 
	set itk_option(-fb_enabled) 1
    }

    enableFB
}

::itcl::body RtControl::updateControlPanel {} {
    if {[catch {$itk_option(-mged) isa Mged} isaMged] ||
	[catch {$itk_option(-mged) isa cadwidgets::Ged} isaGed] ||
	(!$isaMged && !$isaGed)} {
	error "Raytrace Control Panel($this) is not associated with an Mged object, itk_option(-mged) - $itk_option(-mged)"
    }

    set rtActivePane [$itk_option(-mged) pane]

    # Doing it this way eliminates the obnoxious window flash
    after idle [::itcl::code $this update_control_panel]
}


############################### Protected Methods ###############################

::itcl::body RtControl::build_adv {} {
    itk_component add adv {
	::toplevel $itk_interior.adv
    }
    bind $itk_component(adv) <Visibility> "raise $itk_component(adv); break"
    bind $itk_component(adv) <FocusOut> "raise $itk_component(adv); break"

    itk_component add adv_gridF1 {
	::ttk::frame $itk_component(adv).gridF1
    } {}

    itk_component add adv_gridF2 {
	::ttk::frame $itk_component(adv).gridF2 \
	    -relief groove \
	    -borderwidth 2
    } {}

    itk_component add adv_gridF3 {
	::ttk::labelframe $itk_component(adv_gridF2).gridF3 \
	    -borderwidth 2 \
	    -labelanchor n \
	    -relief groove \
	    -text "Photon Mapping Controls"
    } {}

    itk_component add adv_nprocL {
	::ttk::label $itk_component(adv).nprocL \
	    -text "# of Processors" \
	    -anchor e
    } {}

    itk_component add adv_nprocE {
	::ttk::entry $itk_component(adv).nprocE \
	    -width 2 \
	    -textvariable [::itcl::scope itk_option(-nproc)]
    } {}
    bind $itk_component(adv_nprocE) <Enter> [::itcl::code $this enterNProcCB]
    bind $itk_component(adv_nprocE) <Leave> [::itcl::code $this leaveAdvCB]

    itk_component add adv_hsampleL {
	::ttk::label $itk_component(adv).hsampleL \
	    -text "Hypersample" \
	    -anchor e
    } {}

    itk_component add adv_hsampleE {
	::ttk::entry $itk_component(adv).hsampleE \
	    -width 2 \
	    -textvariable [::itcl::scope itk_option(-hsample)]
    } {}
    bind $itk_component(adv_hsampleE) <Enter> [::itcl::code $this enterHSampleCB]
    bind $itk_component(adv_hsampleE) <Leave> [::itcl::code $this leaveAdvCB]

    itk_component add adv_jitterL {
	::ttk::label $itk_component(adv).jitterL \
	    -text "Jitter" \
	    -anchor e
    } {}

    itk_component add adv_jitterCB {
	::ttk::combobox $itk_component(adv).jitterCB \
	    -state readonly \
	    -textvariable [::itcl::scope rtJitterSpec] \
	    -values {None Cell Frame Both}
    } {}
    bind $itk_component(adv_jitterCB) <<ComboboxSelected>> [::itcl::code $this set_jitter]

    itk_component add adv_lmodelL {
	::ttk::label $itk_component(adv).lightL \
	    -text "Light Model" \
	    -anchor e
    } {}

    itk_component add adv_lmodelCB {
	::ttk::combobox $itk_component(adv).lightCB \
	    -state readonly \
	    -textvariable [::itcl::scope rtLightModelSpec] \
	    -values {Full Diffuse {Surface Normals} {Curvature - inverse radius} {Curvature - direction vector} {Photon Mapping}}
    } {}
    bind $itk_component(adv_lmodelCB) <<ComboboxSelected>> [::itcl::code $this set_lmodel]

    itk_component add adv_otherL {
	::ttk::label $itk_component(adv).otherL \
	    -text "Other Options" \
	    -anchor e
    } {}

    itk_component add adv_otherE {
	::ttk::entry $itk_component(adv).otherE \
	    -width 2 \
	    -textvariable [::itcl::scope itk_option(-other)]
    } {}
    bind $itk_component(adv_otherE) <Enter> [::itcl::code $this enterOtherCB]
    bind $itk_component(adv_otherE) <Leave> [::itcl::code $this leaveAdvCB]

    itk_component add adv_dismissB {
	::ttk::button $itk_component(adv).buttonB \
	    -text "Dismiss" \
	    -width 7 \
	    -command [::itcl::code $this deactivate_adv]
    } {}
    bind $itk_component(adv_dismissB) <Enter> [::itcl::code $this enterDismissAdvCB]
    bind $itk_component(adv_dismissB) <Leave> [::itcl::code $this leaveAdvCB]

    itk_component add adv_statusL {
	::ttk::label $itk_component(adv).statusL \
	    -anchor w \
	    -relief sunken \
	    -textvariable [::itcl::scope msg_adv]
    } {}

    grid $itk_component(adv_lmodelL) $itk_component(adv_lmodelCB) \
	-sticky nsew -pady 1 -in $itk_component(adv_gridF1)
    grid $itk_component(adv_jitterL) $itk_component(adv_jitterCB) \
	-sticky nsew -pady 1 -in $itk_component(adv_gridF1)
    grid $itk_component(adv_nprocL) $itk_component(adv_nprocE) \
	-sticky nsew -pady 1 -in $itk_component(adv_gridF1)
    grid $itk_component(adv_hsampleL) $itk_component(adv_hsampleE) \
	-sticky nsew -pady 1 -in $itk_component(adv_gridF1)
    grid $itk_component(adv_otherL) $itk_component(adv_otherE) \
	-sticky nsew -pady 1 -in $itk_component(adv_gridF1)
    grid columnconfigure $itk_component(adv_gridF1) 1 -weight 1
    grid rowconfigure $itk_component(adv_gridF1) 0 -weight 1
    grid rowconfigure $itk_component(adv_gridF1) 1 -weight 1
    grid rowconfigure $itk_component(adv_gridF1) 2 -weight 1
    grid rowconfigure $itk_component(adv_gridF1) 3 -weight 1
    grid rowconfigure $itk_component(adv_gridF1) 4 -weight 1

    grid $itk_component(adv_gridF1) -sticky nsew -padx 8 -pady 8 -in $itk_component(adv_gridF2)
    grid columnconfigure $itk_component(adv_gridF2) 0 -weight 1
    grid rowconfigure $itk_component(adv_gridF2) 0 -weight 1

    grid $itk_component(adv_gridF2) -row 0 -sticky nsew -padx 2 -pady 2
#    grid $itk_component(adv_gridF3) -sticky nsew -padx 2 -pady 2
    grid $itk_component(adv_dismissB) -row 2 -sticky s -padx 2 -pady 2
    grid $itk_component(adv_statusL) -row 3 -padx 2 -pady 2 -sticky nsew
    grid columnconfigure $itk_component(adv) 0 -weight 1
    grid rowconfigure $itk_component(adv) 0 -weight 1

    build_photon_map $itk_component(adv_gridF3)

    wm withdraw $itk_component(adv)
    wm title $itk_component(adv) "Advanced Settings"
}

::itcl::body RtControl::build_photon_map {_parent} {
    build_pm_scale $_parent pm_global "Global Photons" \
	pm_nonLinearEvent 10 24 pmGlobalPhotonsEntry pmGlobalPhotonsScale horizontal
    build_pm_scale $_parent pm_caustic "Caustic Percent" \
	pm_linearEventRound 0 100 pmCausticsPercentScale pmCausticsPercentScale horizontal
    build_pm_scale $_parent pm_irrad "Irradiance Rays" \
	pm_raysEvent 4 32 pmIrradianceRaysEntry pmIrradianceRaysScale horizontal
    build_pm_scale $_parent pm_angular "Angular Tolerance" \
	pm_linearEventRound 0 180 pmAngularTolerance pmAngularTolerance horizontal
    build_pm_scale $_parent pm_rseed "Random Seed" \
	pm_linearEventRound 0 9 pmRandomSeedEntry pmRandomSeedScale horizontal
    build_pm_scale $_parent pm_scalei "Scale Indirect" \
	pm_linearEvent 0.1 10 pmScaleIndirectEntry pmScaleIndirectScale horizontal

    itk_component add pm_cachefileL {
	::ttk::label $_parent.pm_cachefileL \
	    -text "Load/Save File"
    } {}

    itk_component add pm_cachefileE {
	::ttk::entry $_parent.pm_cachefileE \
	    -textvariable [::itcl::scope pmCacheFileEntry]
    } {}

    itk_component add pm_importanceCB {
	::ttk::checkbutton $_parent.pm_importanceCB \
	    -text "Importance Mapping" \
	    -variable [::itcl::scope pmImportanceMapping]
    } {}

    itk_component add pm_irradcacheCB {
	::ttk::checkbutton $_parent.pm_irradcacheCB \
	    -text "Irradiance Hypersampling Cache" \
	    -variable [::itcl::scope pmIrradianceHypersamplingCache]
    } {}

    itk_component add pm_visualirradCB {
	::ttk::checkbutton $_parent.pm_visualirradCB \
	    -text "Visualize Irradiance Cache" \
	    -variable [::itcl::scope pmVisualizeIrradiance]
    } {}

    grid $itk_component(pm_cachefileL) $itk_component(pm_cachefileE) - -sticky nsew
    grid x $itk_component(pm_importanceCB) - -sticky nsew
    grid x $itk_component(pm_irradcacheCB) - -sticky nsew
    grid x $itk_component(pm_visualirradCB) - -sticky nsew
}

::itcl::body RtControl::build_pm_scale {_parent _name _text _cmd _from _to _vname1 _vname2 _orient} {
    itk_component add $_name\L {
	::ttk::label $_parent.$_name\L \
	    -text $_text
    } {}

    itk_component add $_name\E {
	::ttk::entry $_parent.$_name\E \
	    -textvariable  [::itcl::scope $_vname1]
    } {}

    itk_component add $_name\SC {
	::ttk::scale $_parent.$_name\SC \
	    -command [::itcl::code $this $_cmd $itk_component($_name\E)] \
	    -from $_from \
	    -orient $_orient \
	    -to $_to \
	    -variable [::itcl::scope $_vname2]
    } {}

    grid $itk_component($_name\L) $itk_component($_name\E) $itk_component($_name\SC) -sticky nsew
}

::itcl::body RtControl::pm_nonLinearEvent {_entry _sval} {
    $_entry delete 0 end
    $_entry insert 0 [expr {int(pow(2,$_sval))}]
}

::itcl::body RtControl::pm_linearEvent {_entry _sval} {
    $_entry delete 0 end
    $_entry insert 0 [expr {double(round($_sval * 100.0)) / 100.0}]
#    $_entry insert 0 $_sval
}

::itcl::body RtControl::pm_linearEventRound {_entry _sval} {
    $_entry delete 0 end
    $_entry insert 0 [expr {round($_sval)}]
}

::itcl::body RtControl::pm_raysEvent {_entry _sval} {
    $_entry delete 0 end
    $_entry insert 0 [expr {int(pow($_sval,2))}]
}

::itcl::body RtControl::colorChooser {_color} {
    set ic [format "#%02x%02x%02x" [lindex $_color 0] [lindex $_color 1] [lindex $_color 2]]
    set c [tk_chooseColor -initialcolor $ic -title "Color Choose"]

    if {$c == ""} {
	return $_color
    }

    regexp {\#([0-9a-fA-F][0-9a-fA-F])([0-9a-fA-F][0-9a-fA-F])([0-9a-fA-F][0-9a-fA-F])} \
	$c rgb r g b

    return [format "%i %i %i" 0x$r 0x$g 0x$b]
}

::itcl::body RtControl::set_color {} {
    if {!$isaMged && !$isaGed} {
	error "Raytrace Control Panel($this) is not associated with an Mged object"
    }

    switch -- $rtColor {
	Navy {
	    set rtColor "0 0 50"
	}
	Black {
	    set rtColor "0 0 0"
	}
	White {
	    set rtColor "255 255 255"
	}
	Red {
	    set rtColor "255 0 0"
	}
	Green {
	    set rtColor "0 255 0"
	}
	Blue {
	    set rtColor "0 0 255"
	}
	Yellow {
	    set rtColor "255 255 0"
	}
	Cyan {
	    set rtColor "0 255 255"
	}
	Magenta {
	    set rtColor "255 0 255"
	}
	"Color Tool..." {
	    set rtColor [colorChooser $rtPrevColor]
	}
    }

    set rtPrevColor $rtColor
    
    set bg [eval ::cadwidgets::Ged::rgb_to_tk $rtColor]
    $itk_component(bgcolorpatchL) configure -background $bg
}

::itcl::body RtControl::set_fb_mode {} {
    if {!$isaMged && !$isaGed} {
	error "Raytrace Control Panel($this) is not associated with an Mged object"
    }

    switch -- $fb_mode_str {
	Underlay {
	    set fb_mode 1
	}
	Interlay {
	    set fb_mode 2
	}
	Overlay {
	    set fb_mode 3
	}
	default {
	    error "Raytrace Control Panel($this): unrecognize fb mode string - $fb_mode_str"
	}
    }

    fb_mode
}

::itcl::body RtControl::set_fb_mode_str {} {
    if {!$isaMged && !$isaGed} {
	error "Raytrace Control Panel($this) is not associated with an Mged object"
    }

    switch -- $fb_mode {
	1 {
	    set fb_mode_str "Underlay"
	}
	2 {
	    set fb_mode_str "Interlay"
	}
	3 {
	    set fb_mode_str "Overlay"
	}
	default {
	    error "Raytrace Control Panel($this): unrecognize fb mode - $fb_mode"
	}
    }
}

::itcl::body RtControl::set_size {} {
    if {!$isaMged && !$isaGed} {
	error "Raytrace Control Panel($this) is not associated with an Mged object"
    }

    set rtSize [getSize]
}

::itcl::body RtControl::set_jitter {} {
    switch -- $rtJitterSpec {
	None {
	    set rtJitter 0
	}
	Cell {
	    set rtJitter 1
	}
	Frame {
	    set rtJitter 2
	}
	Both {
	    set rtJitter 3
	}
	default {
	    error "RtControl::set_jitter: bad value - $jitterSpec"
	}
    }
}

::itcl::body RtControl::set_lmodel {} {
    switch -- $rtLightModelSpec {
	Full {
	    set rtLightModel 0
	}
	Diffuse {
	    set rtLightModel 1
	}
	"Surface Normals" {
	    set rtLightModel 2
	}
	"Curvature - inverse radius" {
	    set rtLightModel 4
	}
	"Curvature - direction vector" {
	    set rtLightModel 5
	}
	"Photon Mapping" {
	    set rtLightModel 7

	    if {![winfo ismapped $itk_component(adv_gridF3)]} {
		grid $itk_component(adv_gridF3) -row 1 -sticky nsew -padx 2 -pady 2
	    }
	}
	default {
	    error "RtControl::set_lmodel: bad value - $rtLightModelSpec"
	}
    }

    if {$rtLightModel != 7} {
	if {[winfo ismapped $itk_component(adv_gridF3)]} {
	    grid forget $itk_component(adv_gridF3)
	}
    } else {
	if {![winfo ismapped $itk_component(adv_gridF3)]} {
	    grid $itk_component(adv_gridF3) -row 1 -sticky nsew -padx 2 -pady 2
	}
    }
}

## - cook_dest
#
# Return a port number if possible. Otherwise, return
# whatever the user specified (i.e. it's already cooked)
#
::itcl::body RtControl::cook_dest {dest} {
    switch -- $dest {
	ul -
	ur -
	ll -
	lr {
	    if {$isaMged} {
		if {![$itk_option(-mged) component $dest fb_active]} {
		    $itk_option(-mged) component $dest fb_active 1

		    # update the Inactive/Underlay/Overlay radiobutton
		    set fb_mode [subst $[subst fb_mode_$dest]]
		    set_fb_mode_str

		    if {$itk_option(-fb_mode_callback) != ""} {
			catch {$itk_option(-fb_mode_callback) $fb_mode}
		    }
		}
		return [$itk_option(-mged) component $dest listen]
	    } else {
		if {![$itk_option(-mged) pane_set_fb_mode $dest]} {
		    $itk_option(-mged) component $dest fb_active 1

		    # update the Inactive/Underlay/Overlay radiobutton
		    set fb_mode [subst $[subst fb_mode_$dest]]
		    set_fb_mode_str

		    if {$itk_option(-fb_mode_callback) != ""} {
			catch {$itk_option(-fb_mode_callback) $fb_mode}
		    }
		}
		return [$itk_option(-mged) pane_listen $dest]
	    }
	}
	default {
	    # Already cooked.
	    return $dest
	}
    }
}

::itcl::body RtControl::fb_mode {} {
    if {$isaMged} {
	if {$itk_option(-fb_enabled)} {
	    $itk_option(-mged) component $rtActivePane fb_active $fb_mode
	} else {
	    $itk_option(-mged) component $rtActivePane fb_active 0
	}
    } else {
	if {$itk_option(-fb_enabled)} {
	    $itk_option(-mged) pane_set_fb_mode $rtActivePane $fb_mode
	} else {
	    $itk_option(-mged) pane_set_fb_mode $rtActivePane 0
	}
    }

    set fb_mode_$rtActivePane $fb_mode

    if {$itk_option(-fb_mode_callback) != ""} {
	catch {$itk_option(-fb_mode_callback) $fb_mode}
    }
}

::itcl::body RtControl::ok {} {
    if {!$isaMged && !$isaGed} {
	error "Raytrace Control Panel($this) is not associated with an Mged object"
    }

    raytrace
    deactivate
}

## - get_cooked_dest
#
# Returns a port number or something specified by
# the user (i.e. hostname:port, or filename). No error
# checking is performed on user specified strings.
#
::itcl::body RtControl::get_cooked_dest {} {
    if {$rtActivePane == ""} {
	# use the active pane
	set rtActivePane [$itk_option(-mged) pane]
    }

    set cooked_dest [cook_dest $rtActivePane]
    if {$cooked_dest == -1} {
	switch -- $rtActivePane {
	    ul -
	    ur -
	    ll -
	    lr {
		# Cause the framebuffer to listen for clients on port 0.
		# If port 0 isn't available, the next available port will
		# be returned.
		if {$isaMged} {
		    set cooked_dest [$itk_option(-mged) component $rtActivePane listen 0]
		} else {
		    set cooked_dest [$itk_option(-mged) pane_listen $rtActivePane 0]
		}
	    }
	    default {
		# We should only get here if $dest is -1,
		# which had to be specified by the user.
		error "Invalid destination - $rtDest"
	    }
	}
    }

    return $cooked_dest
}

::itcl::body RtControl::menuStatusCB {w} {
    if {[catch {$w entrycget active -label} op]} {
	# probably a tearoff entry
	set op ""
    }

    switch -- $op {
	"Inactive" {
	    if {[catch {getPaneStr} result]} {
		set msg $result
	    } else {
		set msg "Make the $result framebuffer inactive"
	    }
	}
	"Underlay" {
	    if {[catch {getPaneStr} result]} {
		set msg $result
	    } else {
		set msg "Put the $result framebuffer in underlay mode"
	    }
	}
	"Overlay" {
	    if {[catch {getPaneStr} result]} {
		set msg $result
	    } else {
		set msg "Put the $result framebuffer in overlay mode"
	    }
	}
	"Upper Left" {
	    set msg "Make the upper left pane the active pane"
	}
	"Upper Right" {
	    set msg "Make the upper right pane the active pane"
	}
	"Lower Left" {
	    set msg "Make the lower left pane the active pane"
	}
	"Lower Right" {
	    set msg "Make the lower right pane the active pane"
	}
	"Size of Pane" {
	    if {[catch {getSize} result]} {
		set msg $result
	    } else {
		set msg "Make the image size $result"
	    }
	}
	"128" -
	"256" -
	"512" -
	"1024" {
	    set msg "Make the image size ${op}x${op}"
	}
	"640x480" -
	"720x486" {
	    set msg "Make the image size $op"
	}
	"black" {
	    set msg "Make the background color black"
	}
	"white" {
	    set msg "Make the background color white"
	}
	"red" {
	    set msg "Make the background color red"
	}
	"green" {
	    set msg "Make the background color green"
	}
	"blue" {
	    set msg "Make the background color blue"
	}
	"yellow" {
	    set msg "Make the background color yellow"
	}
	"cyan" {
	    set msg "Make the background color cyan"
	}
	"magenta" {
	    set msg "Make the background color magenta"
	}
	"Color Tool..." {
	    set msg "Activate the color tool"
	}
	default {
	    set msg ""
	}
    }
}

::itcl::body RtControl::leaveCB {} {
    set msg ""
}

::itcl::body RtControl::enterAdvCB {} {
    set msg "Activate the advanced settings dialog"
}

::itcl::body RtControl::enterEnablefbCB {} {
    set msg "Toggle framebuffer on/off"
}

::itcl::body RtControl::enterOkCB {} {
    if {!$isaMged && !$isaGed} {
	set msg "Not associated with an Mged object"
    } else {
	set msg "Raytrace $rtActivePane's view and dismiss"
    }
}

::itcl::body RtControl::enterRaytraceCB {} {
    if {!$isaMged && !$isaGed} {
	set msg "Not associated with an Mged object"
    } else {
	set msg "Raytrace $rtActivePane's view"
    }
}

::itcl::body RtControl::enterAbortCB {} {
    if {!$isaMged && !$isaGed} {
	set msg "Not associated with an Mged object"
    } else {
	set msg "Abort all raytraces started from $rtActivePane"
    }
}

::itcl::body RtControl::enterClearCB {} {
    if {!$isaMged && !$isaGed} {
	set msg "Not associated with an Mged object"
    } else {
	set msg "Clear $rtActivePane with the following color - $rtColor"
    }
}

::itcl::body RtControl::enterDismissCB {} {
    set msg "Dismiss the raytrace control panel"
}

::itcl::body RtControl::enterDestCB {} {
    set msg "Specify a destination (i.e. framebuffer or file)"
}

::itcl::body RtControl::enterSizeCB {} {
    set msg "Specify an image size"
}

::itcl::body RtControl::enterColorCB {} {
    set msg "Specify an RGB color"
}

::itcl::body RtControl::getPaneStr {} {
    switch -- $rtActivePane {
	ul {
	    return "upper left"
	}
	ur {
	    return "upper right"
	}
	ll {
	    return "lower left"
	}
	lr {
	    return "lower right"
	}
    }
}

## - getSize
#
#
#
::itcl::body RtControl::getSize {} {
    if {!$isaMged && !$isaGed} {
	error "Not associated with an Mged object"
    }

    if {$isaMged} {
	set size [$itk_option(-mged) component $rtActivePane cget -dmsize]
    } else {
	set size [$itk_option(-mged) pane_win_size $rtActivePane]
    }
    return "[lindex $size 0]x[lindex $size 1]"
}

::itcl::body RtControl::enableFB {} {
    fb_mode

    if {$itk_option(-fb_enabled_callback) != ""} {
	catch {$itk_option(-fb_enabled_callback) $itk_option(-fb_enabled)}
    }
}

::itcl::body RtControl::menuStatusAdvCB {w} {
    if {[catch {$w entrycget active -label} op]} {
	# probably a tearoff entry
	set op ""
    }

    switch -- $op {
	"Full" {
	    set msg_adv "Full lighting model"
	}
	"Diffuse" {
	    set msg_adv "Diffuse lighting model (debugging)"
	}
	"Surface Normals" {
	    set msg_adv "Display surface normals"
	}
	"Diffuse - 3 light" {
	    set msg_adv "3-light diffuse (debugging)"
	}
	"Curvature - inverse radius" {
	    set msg_adv "Curvature debugging - inverse radius"
	}
	"Curvature - direction vector" {
	    set msg_adv "Curvature debugging - direction vector"
	}
	"None" {
	    set msg_adv "Fire rays from center of cell"
	}
	"Cell" {
	    set msg_adv "Jitter each cell by +/- one half pixel"
	}
	"Frame" {
	    set msg_adv "Jitter the frame by +/- one half pixel"
	}
	"Both" {
	    set msg_adv "Jitter the cells and the frame"
	}
	default {
	    set msg_adv ""
	}
    }
}

::itcl::body RtControl::leaveAdvCB {} {
    set msg_adv ""
}

::itcl::body RtControl::enterDismissAdvCB {} {
    set msg_adv "Dismiss the advanced settings dialog"
}

::itcl::body RtControl::enterNProcCB {} {
    set msg_adv "Requested number of processors"
}

::itcl::body RtControl::enterHSampleCB {} {
    set msg_adv "Extra rays to fire per pixel"
}

::itcl::body RtControl::enterOtherCB {} {
    set msg_adv "Other rt options"
}

::itcl::body RtControl::update_control_panel {} {
    ::update
    set_size

    # update the Inactive/Underlay/Overlay radiobutton
    if {$isaMged} {
	set mode [$itk_option(-mged) component $rtActivePane fb_active]
    } else {
	set mode [$itk_option(-mged) pane_set_fb_mode $rtActivePane]
    }

    if {$mode < 1} {
	set itk_option(-fb_enabled) 0
	set fb_mode [subst $[subst fb_mode_$rtActivePane]]
    } else {
	set itk_option(-fb_enabled) 1
	set fb_mode $mode
    }

    set_fb_mode_str
}


# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
