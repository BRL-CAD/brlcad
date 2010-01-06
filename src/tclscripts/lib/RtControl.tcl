#                   R T C O N T R O L . T C L
# BRL-CAD
#
# Copyright (c) 1998-2010 United States Government as represented by
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
    keep -tearoff
}

#XXX This should work.
option add *RtControl*tearoff 0 widgetDefault

::itcl::class RtControl {
    inherit ::itk::Toplevel

    itk_option define -olist olist Olist {}
    itk_option define -omode omode Omode {}
    itk_option define -nproc nproc Nproc 1
    itk_option define -hsample hsample Hsample 0
    itk_option define -jitter jitter Jitter 0
    itk_option define -lmodel lmodel Lmodel 0
    itk_option define -other other Other "-A 0.9"
    itk_option define -size size Size 512
    itk_option define -color color Color {0 0 0}
    itk_option define -dest dest Dest ""
    itk_option define -mged mged Mged ""

    public method activate {}
    public method activate_adv {}
    public method deactivate {}
    public method deactivate_adv {}
    public method center {w gs {cw ""}}
    public method update_fb_mode {}

    private method build_adv {}
    private method set_src {}
    private method set_dest {}
    private method colorChooser {_color}
    private method set_color {}
    private method set_size {}
    private method set_jitter {}
    private method set_lmodel {}
    private method cook_dest {dest}
    private method fb_mode {}
    private method ok {}
    private method raytrace {}
    private method abort {}
    private method clear {}
    private method get_cooked_dest {}
    private method update_control_panel {}
    private method menuStatusCB {w}
    private method leaveCB {}
    private method enterAdvCB {}
    private method enterOkCB {}
    private method enterRaytraceCB {}
    private method enterAbortCB {}
    private method enterClearCB {}
    private method enterDismissCB {}
    private method enterDestCB {}
    private method enterSizeCB {}
    private method enterColorCB {}
    private method getPane {}
    private method getPaneStr {}
    private method getSize {}

    private method menuStatusAdvCB {w}
    private method leaveAdvCB {}
    private method enterDismissAdvCB {}
    private method enterNProcCB {}
    private method enterHSampleCB {}
    private method enterOtherCB {}

    private variable fb_mode 0
    private variable rtSrc ""
    private variable rtDest ""
    private variable rtColor "0 0 0"
    private variable rtPrevColor "0 0 0"
    private variable rtSize ""
    private variable rtLightModel 0
    private variable rtLightModelSpec "Full"
    private variable rtJitter 0
    private variable rtJitterSpec "None"
    private variable win_geom ""
    private variable win_geom_adv ""
    private variable msg ""
    private variable msg_adv ""
    private variable srcM
    private variable destM
    private variable destE
    private variable sizeM
    private variable sizeE
    private variable colorM
    private variable colorE
    private variable jitterM
    private variable lmodelM
    private variable isaMged 0
    private variable isaGed 0

    protected variable saveVisibilityBinding {}
    protected variable saveFocusOutBinding {}

    constructor {args} {}
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

    itk_component add menubar {
	::menu $itk_interior.menubar
    } {
	usual
    }
    #    $this component hull configure -menu $itk_component(menubar)
    $this configure -menu $itk_component(menubar)

    itk_component add fbM {
	::menu $itk_component(menubar).fb -title "Framebuffer"
    } {
	usual
    }
    bind $itk_component(fbM) <<MenuSelect>> [::itcl::code $this menuStatusCB %W]

    $itk_component(menubar) add cascade -label "Framebuffer" -underline 0 -menu $itk_component(fbM)

    if {0} {
	itk_component add objM {
	    ::menu $itk_component(menubar).obj -title "Objects"
	} {
	    usual
	}

	$itk_component(menubar) add cascade -label "Objects" -underline 0 -menu $itk_component(objM)
    }

    $itk_component(fbM) add radiobutton -value 2 -variable [::itcl::scope fb_mode] \
	-label "Overlay" -underline 0 \
	-command [::itcl::code $this fb_mode]
    $itk_component(fbM) add radiobutton -value 1 -variable [::itcl::scope fb_mode] \
	-label "Underlay" -underline 0 \
	-command [::itcl::code $this fb_mode]
    $itk_component(fbM) add radiobutton -value 0 -variable [::itcl::scope fb_mode] \
	-label "Inactive" -underline 0 \
	-command [::itcl::code $this fb_mode]

    itk_component add srcL {
	::ttk::label $itk_interior.srcL \
	    -text "Source" \
	    -anchor e
    } {}

    itk_component add srcCB {
	::ttk::combobox $itk_interior.srcCB \
	    -state readonly \
	    -textvariable [::itcl::scope rtSrc] \
	    -values {{Active Pane} {Upper Left} {Upper Right} {Lower Left} {Lower Right}}
    } {}

    bind $itk_component(srcCB) <<ComboboxSelected>> [::itcl::code $this set_src]

    itk_component add destL {
	::ttk::label $itk_interior.destL \
	    -text "Destination" \
	    -anchor e
    } {}

    itk_component add destCB {
	::ttk::combobox $itk_interior.destCB \
	    -state readonly \
	    -textvariable [::itcl::scope rtDest] \
	    -values {{Active Pane} {Upper Left} {Upper Right} {Lower Left} {Lower Right}}
    } {}

    bind $itk_component(destCB) <<ComboboxSelected>> [::itcl::code $this set_dest]

    itk_component add sizeL {
	::ttk::label $itk_interior.sizeL \
	    -text "Size" \
	    -anchor e
    } {}

    itk_component add sizeCB {
	::ttk::combobox $itk_interior.sizeCB \
	    -state readonly \
	    -textvariable [::itcl::scope rtSize] \
	    -values {{Size of Pane} 128 256 512 640x480 720x486 1024}
    } {}

    bind $itk_component(sizeCB) <<ComboboxSelected>> [::itcl::code $this set_size]

    itk_component add bgcolorL {
	::ttk::label $itk_interior.bgcolorL \
	    -text "Background Color" \
	    -anchor e
    } {}

    itk_component add bgcolorCB {
	::ttk::combobox $itk_interior.bgcolorCB \
	    -state readonly \
	    -textvariable [::itcl::scope rtColor] \
	    -values {Black White Red Green Blue Yellow Cyan Magenta {Color Tool...}}
    } {}

    bind $itk_component(bgcolorCB) <<ComboboxSelected>> [::itcl::code $this set_color]

    if {0} {
    set colorM [$itk_component(bgcolorCB) component menu]
    set colorE [$itk_component(bgcolorCB) component entry]
    bind $colorM <<MenuSelect>> [::itcl::code $this menuStatusCB %W]
    bind $colorE <Enter> [::itcl::code $this enterColorCB]
    bind $colorE <Leave> [::itcl::code $this leaveCB]
    }

    itk_component add advB {
	::ttk::button $itk_interior.advB \
	    -text "Advanced Settings..." \
	    -width 16 \
	    -command [::itcl::code $this activate_adv]
    } {}
    bind $itk_component(advB) <Enter> [::itcl::code $this enterAdvCB]
    bind $itk_component(advB) <Leave> [::itcl::code $this leaveCB]

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

    grid $itk_component(srcL) $itk_component(srcCB) -pady 1 -sticky nsew -in $itk_component(gridF1)
    grid $itk_component(destL) $itk_component(destCB) -pady 1 -sticky nsew -in $itk_component(gridF1)
    grid $itk_component(sizeL) $itk_component(sizeCB) -pady 1 -sticky nsew -in $itk_component(gridF1)
    grid $itk_component(bgcolorL) $itk_component(bgcolorCB) -pady 1 -sticky nsew -in $itk_component(gridF1)
    grid $itk_component(advB) - -pady 1 -sticky ns -in $itk_component(gridF1)

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
	$itk_component(dismissB) -sticky "nsew" -in $itk_component(gridF3)
    grid columnconfigure $itk_component(gridF3) 3 -weight 1
    grid columnconfigure $itk_component(gridF3) 5 -weight 1

    grid $itk_component(gridF2) -padx 4 -pady 4 -sticky nsew
    grid $itk_component(gridF3) -padx 4 -pady 4 -sticky nsew
    grid $itk_component(statusL) -padx 2 -pady 2 -sticky nsew
    grid columnconfigure $itk_component(hull) 0 -weight 1
    grid rowconfigure $itk_component(hull) 0 -weight 1

    # build the advanced settings panel
    build_adv

    $this configure -background [::ttk::style lookup label -background]

    # process options
    eval itk_initialize $args

    # Disable the source, jitter and lmodel CombBox's entry widgets
#    configure -sourceState disabled -jitterState disabled -lmodelState disabled

    # Link the ComboBox's entry widgets to this class' variables
#    $itk_component(bgcolorCB) configure -entryvariable [::itcl::scope itk_option(-color)]

    wm withdraw $itk_component(hull)
    wm title $itk_component(hull) "Raytrace Control Panel"
}

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
	::ttk::frame $itk_component(adv).gridF2 -relief groove -borderwidth 2
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
	    -values {Full Diffuse {Surface Normals} {Curvature - inverse radius} {Curvature - direction vector}}
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

    grid $itk_component(adv_gridF2) -sticky nsew -padx 2 -pady 2
    grid $itk_component(adv_dismissB) -sticky s -padx 2 -pady 2
    grid $itk_component(adv_statusL) -padx 2 -pady 2 -sticky nsew
    grid columnconfigure $itk_component(adv) 0 -weight 1
    grid rowconfigure $itk_component(adv) 0 -weight 1

    wm withdraw $itk_component(adv)
    wm title $itk_component(adv) "Advanced Settings"
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

    update_control_panel
}

::itcl::body RtControl::activate {} {
    raise $itk_component(hull)

    # center on screen
    if {$win_geom == ""} {
	center $itk_component(hull) win_geom
    }
    wm geometry $itk_component(hull) $win_geom
    wm deiconify $itk_component(hull)

    set rtSize "Size of Pane"
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

::itcl::body RtControl::update_fb_mode {} {
    # update the Inactive/Underlay/Overlay radiobutton
    if {$isaMged} {
	set fb_mode [$itk_option(-mged) component $rtDest fb_active]
    } else {
	set fb_mode [$itk_option(-mged) pane_set_fb_mode $rtDest]
    }
}

::itcl::body RtControl::set_src {} {
    if {!$isaMged && !$isaGed} {
	error "Raytrace Control Panel($this) is not associated with an Mged object"
    }

    switch -- $rtSrc {
	"Active Pane" {
	    set rtSrc [$itk_option(-mged) pane]
	}
	"Upper Left" {
	    set rtSrc ul
	}
	"Upper Right" {
	    set rtSrc ur
	}
	"Lower Left" {
	    set rtSrc ll
	}
	"Lower Right" {
	    set rtSrc lr
	}
    }
}

::itcl::body RtControl::set_dest {} {
    if {!$isaMged && !$isaGed} {
	error "Raytrace Control Panel($this) is not associated with an Mged object"
    }

    switch -- $rtDest {
	"Active Pane" {
	    set rtDest [$itk_option(-mged) pane]
	}
	"Upper Left" {
	    set rtDest ul
	}
	"Upper Right" {
	    set rtDest ur
	}
	"Lower Left" {
	    set rtDest ll
	}
	"Lower Right" {
	    set rtDest lr
	}
    }

    # update the Inactive/Underlay/Overlay radiobutton
    if {$isaMged} {
	set fb_mode [$itk_option(-mged) component $rtDest fb_active]
    } else {
	set fb_mode [$itk_option(-mged) pane_set_fb_mode $rtDest]
    }
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
}

::itcl::body RtControl::set_size {} {
    if {!$isaMged && !$isaGed} {
	error "Raytrace Control Panel($this) is not associated with an Mged object"
    }

    if {$rtSize == "Size of Pane"} {
	    set rtSize [getSize]
    }

    if {![regexp "^(\[ \]*\[0-9\]+)((\[ \]*\[xX\]?\[ \]*)|(\[ \]+))(\[0-9\]*\[ \]*)$" $rtSize]} {
	error "Bad size - $rtSize"
    }
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
	default {
	    error "RtControl::set_lmodel: bad value - $rtLightModelSpec"
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
		    set fb_mode 1
		}
		return [$itk_option(-mged) component $dest listen]
	    } else {
		if {![$itk_option(-mged) pane_set_fb_mode $dest]} {
		    $itk_option(-mged) component $dest fb_active 1

		    # update the Inactive/Underlay/Overlay radiobutton
		    set fb_mode 1
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
    set pane [getPane]

    if {$isaMged} {
	$itk_option(-mged) component $pane fb_active $fb_mode
    } else {
	$itk_option(-mged) pane_set_fb_mode $pane $fb_mode
    }
}

::itcl::body RtControl::ok {} {
    if {!$isaMged && !$isaGed} {
	error "Raytrace Control Panel($this) is not associated with an Mged object"
    }

    raytrace
    deactivate
}

::itcl::body RtControl::raytrace {} {
    if {!$isaMged && !$isaGed} {
	error "Raytrace Control Panel($this) is not associated with an Mged object"
    }

    if {$isaMged} {
	set rt_cmd "$itk_option(-mged) component $rtSrc rt -F [get_cooked_dest]"
    } else {
	set rt_cmd "$itk_option(-mged) pane_rt $rtSrc -F [get_cooked_dest]"
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
    }

    if {$itk_option(-other) != ""} {
	append rt_cmd " $itk_option(-other)"
    }

    set result [catch {eval $rt_cmd} rt_error]
    if {$result} {
	error $rt_error
    }
}

::itcl::body RtControl::abort {} {
    if {!$isaMged && !$isaGed} {
	error "Raytrace Control Panel($this) is not associated with an Mged object"
    }

    if {$isaMged} {
	$itk_option(-mged) component $rtSrc rtabort
    } else {
	$itk_option(-mged) rtabort
    }
}

::itcl::body RtControl::clear {} {
    if {!$isaMged && !$isaGed} {
	error "Raytrace Control Panel($this) is not associated with an Mged object"
    }

    set cooked_dest [get_cooked_dest]

    set fbclear [bu_brlcad_root "bin/fbclear"]
    set result [catch {eval exec $fbclear -F $cooked_dest $rtColor &} rt_error]

    if {$result} {
	error $rt_error
    }
}

## - get_cooked_dest
#
# Returns a port number or something specified by
# the user (i.e. hostname:port, or filename). No error
# checking is performed on user specified strings.
#
::itcl::body RtControl::get_cooked_dest {} {
    if {$rtDest == ""} {
	# use the active pane
	set rtDest [$itk_option(-mged) pane]
    }

    set cooked_dest [cook_dest $rtDest]
    if {$cooked_dest == -1} {
	switch -- $rtDest {
	    ul -
	    ur -
	    ll -
	    lr {
		# Cause the framebuffer to listen for clients on port 0.
		# If port 0 isn't available, the next available port will
		# be returned.
		if {$isaMged} {
		    set cooked_dest [$itk_option(-mged) component $rtDest listen 0]
		} else {
		    set cooked_dest [$itk_option(-mged) pane_listen $rtDest 0]
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

::itcl::body RtControl::update_control_panel {} {
    if {[catch {$itk_option(-mged) isa Mged} isaMged] ||
	[catch {$itk_option(-mged) isa cadwidgets::Ged} isaGed] ||
	(!$isaMged && !$isaGed)} {
	error "Raytrace Control Panel($this) is not associated with an Mged object, itk_option(-mged) - $itk_option(-mged)"
    }

    #    if {![$itk_option(-mged) fb_active]} {
    # Framebuffer is not active, so activate it
    # by putting it in "Underlay" mode.
    #	$itk_option(-mged) fb_active 1
    #	set fb_mode 1
    #    }

#    set pane [$itk_option(-mged) pane]
#    $itk_component(srcCB) setText $pane
    #    $itk_component(destCB) setText $pane
    set rtSrc [$itk_option(-mged) pane]
    set rtDest $rtSrc
    set rtSize "Size of Pane"
    set_size

    # Calling setColor so that the menubutton color gets set.
#    eval $itk_component(bgcolorCB) setColor [$itk_option(-mged) bg]
#    set rtColor [$itk_option(-mged) bg]
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
	"Active Pane" {
	    if {[catch {getPaneStr} result]} {
		set msg $result
	    } else {
		if {$srcM == $w} {
		    set msg "Make the $result pane the source"
		} else {
		    set msg "Make the $result pane the destination"
		}
	    }
	}
	"Upper Left" {
	    if {$srcM == $w} {
		set msg "Make the upper left pane the source"
	    } else {
		set msg "Make the upper left pane the destination"
	    }
	}
	"Upper Right" {
	    if {$srcM == $w} {
		set msg "Make the upper right pane the source"
	    } else {
		set msg "Make the upper right pane the destination"
	    }
	}
	"Lower Left" {
	    if {$srcM == $w} {
		set msg "Make the lower left pane the source"
	    } else {
		set msg "Make the lower left pane the destination"
	    }
	}
	"Lower Right" {
	    if {$srcM == $w} {
		set msg "Make the lower right pane the source"
	    } else {
		set msg "Make the lower right pane the destination"
	    }
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

::itcl::body RtControl::enterOkCB {} {
    if {!$isaMged && !$isaGed} {
	set msg "Not associated with an Mged object"
    } else {
	set msg "Raytrace $rtSrc's view into $rtDest and dismiss"
    }
}

::itcl::body RtControl::enterRaytraceCB {} {
    if {!$isaMged && !$isaGed} {
	set msg "Not associated with an Mged object"
    } else {
	set msg "Raytrace $rtSrc's view into $rtDest"
    }
}

::itcl::body RtControl::enterAbortCB {} {
    if {!$isaMged && !$isaGed} {
	set msg "Not associated with an Mged object"
    } else {
	set msg "Abort all raytraces started from $rtSrc"
    }
}

::itcl::body RtControl::enterClearCB {} {
    if {!$isaMged && !$isaGed} {
	set msg "Not associated with an Mged object"
    } else {
	set msg "Clear $rtDest with the following color - $rtColor"
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

## - getPane
#
# Return a pane associated with $itk_option(-mged)
# according to $rtDest.
#
# Note - if $rtDest is not a pane (i.e. it's a
#        file or some other framebuffer) then the active
#        pane is returned.
#
::itcl::body RtControl::getPane {} {
    if {!$isaMged && !$isaGed} {
	error "Not associated with an Mged object"
    }

    switch -- $rtDest {
	ul -
	ur -
	ll -
	lr {
	    return $rtDest
	}
	default {
	    # Use the active pane
	    return [$itk_option(-mged) pane]
	}
    }
}

::itcl::body RtControl::getPaneStr {} {
    if {[catch {getPane} result]} {
	return $result
    }

    switch -- $result {
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

    # Try using the destination for obtaining the size.
    switch -- $rtDest {
	ul -
	ur -
	ll -
	lr {
	    if {$isaMged} {
		set size [$itk_option(-mged) component $rtDest cget -dmsize]
	    } else {
		set size [$itk_option(-mged) pane_win_size $rtDest]
	    }
	    return "[lindex $size 0]x[lindex $size 1]"
	}
    }

    # The destination could be a file or a framebuffer.
    # We don't know what its size is so try to use the
    # source pane for obtaining the size.
    switch -- $rtSrc {
	ul -
	ur -
	ll -
	lr {
	    if {$isaMged} {
		set size [$itk_option(-mged) component $rtSrc cget -dmsize]
	    } else {
		set size [$itk_option(-mged) pane_win_size $rtSrc]
	    }
	    return "[lindex $size 0]x[lindex $size 1]"
	}
    }

    # We failed to get the size using the destination and source panes.
    # So, we use the active pane for obtaining the size.
    if {$isaMged} {
	set size [$itk_option(-mged) component [$itk_option(-mged) pane] cget -dmsize]
    } else {
	set size [$itk_option(-mged) pane_win_size [$itk_option(-mged) pane]]
    }
    return "[lindex $size 0]x[lindex $size 1]"
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

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
