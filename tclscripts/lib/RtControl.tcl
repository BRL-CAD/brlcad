##                 R T C O N T R O L . T C L
#
# Author -
#	Bob Parker
#
# Source -
#	The U. S. Army Research Laboratory
#	Aberdeen Proving Ground, Maryland  21005
#
# Distribution Notice -
#	Re-distribution of this software is restricted, as described in
#       your "Statement of Terms and Conditions for the Release of
#       The BRL-CAD Package" agreement.
#
# Copyright Notice -
#       This software is Copyright (C) 1998 by the United States Army
#       in all countries except the USA.  All rights reserved.
#
# Description -
#	The raytrace control panel is designed to be used with an
#	Mged object. It probably should be an inner class of the Mged
#	class.
#

itk::usual RtControl {
    keep -tearoff
}

#XXX This should work.
option add *RtControl*tearoff 0 widgetDefault

class RtControl {
    inherit itk::Toplevel

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

    private method build_adv {}
    private method set_src {pane}
    private method set_dest {pane}
    private method set_size {size}
    private method set_jitter {j}
    private method set_lmodel {lm}
    private method cook_dest {dest}
    private method fb_mode {}
    private method ok {}
    private method raytrace {}
    private method abort {}
    private method clear {}
    private method get_color {}
    private method get_size {}
    private method get_cooked_dest {}
    private method update_control_panel {}
    private method menuStatusCB {w}
    private method leaveCB {}
    private method enterRaytraceCB {}

    private variable fb_mode 0
    private variable raw_src ""
    private variable win_geom ""
    private variable win_geom_adv ""

    constructor {args} {}
}

body RtControl::constructor {args} {
    # revive a few ignored options
    #itk_option add hull.screen

    itk_component add gridF1 {
	::frame $itk_interior.gridF1
    } {
	usual
    }

    itk_component add gridF2 {
	::frame $itk_interior.gridF2 -relief groove -bd 2
    } {
	usual
    }

    itk_component add gridF3 {
	::frame $itk_interior.gridF3
    } {
	usual
    }

    itk_component add menubar {
	::menu $itk_interior.menubar
    } {
	usual
    }
# Why doesn't this work???
#    $itk_component(hull) configure -menu $itk_component(menubar)
    $this component hull configure -menu $itk_component(menubar)

    itk_component add fbM {
	::menu $itk_component(menubar).fb -title "Framebuffer"
    } {
	usual
    }

    itk_component add objM {
	::menu $itk_component(menubar).obj -title "Objects"
    } {
	usual
    }

    $itk_component(menubar) add cascade -label "Framebuffer" -underline 0 -menu $itk_component(fbM)
    if {0} {
	$itk_component(menubar) add cascade -label "Objects" -underline 0 -menu $itk_component(objM)
    }

    $itk_component(fbM) add radiobutton -value 0 -variable [scope fb_mode] \
	    -label "Inactive" -underline 0 \
	    -command [code $this fb_mode]
    $itk_component(fbM) add radiobutton -value 1 -variable [scope fb_mode] \
	    -label "Underlay" -underline 0 \
	    -command [code $this fb_mode]
    $itk_component(fbM) add radiobutton -value 2 -variable [scope fb_mode] \
	    -label "Overlay" -underline 0 \
	    -command [code $this fb_mode]

    itk_component add srcL {
	::label $itk_interior.srcL -text "Source" -anchor e
    } {
	usual
    }

    itk_component add srcCB {
	cadwidgets::ComboBox $itk_interior.srcCB
    } {
	usual
	rename -state -sourceState sourceState SourceState
    }

    # populate source's combobox menu
    $itk_component(srcCB) add command -label "Active Pane" \
	    -command [code $this set_src active]
    $itk_component(srcCB) add separator
    $itk_component(srcCB) add command -label "Upper Left" \
	    -command [code $this set_src ul]
    $itk_component(srcCB) add command -label "Upper Right" \
	    -command [code $this set_src ur]
    $itk_component(srcCB) add command -label "Lower Left" \
	    -command [code $this set_src ll]
    $itk_component(srcCB) add command -label "Lower Right" \
	    -command [code $this set_src lr]

    itk_component add destL {
	::label $itk_interior.destL -text "Destination" -anchor e
    } {
	usual
    }

    itk_component add destCB {
	cadwidgets::ComboBox $itk_interior.destCB
    } {
	usual
    }

    # populate destination's combobox menu
    $itk_component(destCB) add command -label "Active Pane" \
	    -command [code $this set_dest active]
    $itk_component(destCB) add separator
    $itk_component(destCB) add command -label "Upper Left" \
	    -command [code $this set_dest ul]
    $itk_component(destCB) add command -label "Upper Right" \
	    -command [code $this set_dest ur]
    $itk_component(destCB) add command -label "Lower Left" \
	    -command [code $this set_dest ll]
    $itk_component(destCB) add command -label "Lower Right" \
	    -command [code $this set_dest lr]

#    bind [$itk_component(destCB) component entry] <KeyRelease> [code $this cook_dest]

    itk_component add sizeL {
	::label $itk_interior.sizeL -text "Size" -anchor e
    } {
	usual
    }

    itk_component add sizeCB {
	cadwidgets::ComboBox $itk_interior.sizeCB
    } {
	usual
    }

    # populate size's combobox
    $itk_component(sizeCB) add command -label "Size of Pane" \
	    -command [code $this set_size "Size of Pane"]
    $itk_component(sizeCB) add command -label "128" \
	    -command [code $this set_size 128]
    $itk_component(sizeCB) add command -label "256" \
	    -command [code $this set_size 256]
    $itk_component(sizeCB) add command -label "512" \
	    -command [code $this set_size 512]
    $itk_component(sizeCB) add command -label "640x480" \
	    -command [code $this set_size "640x480"]
    $itk_component(sizeCB) add command -label "720x486" \
	    -command [code $this set_size "720x486"]
    $itk_component(sizeCB) add command -label "1024" \
	    -command [code $this set_size 1024]

    itk_component add bgcolorL {
	::label $itk_interior.bgcolorL -text "Background Color" -anchor e
    } {
	usual
    }

    itk_component add bgcolorCB {
	cadwidgets::ColorEntry $itk_interior.bgcolorCB
    } {
	usual
    }

    itk_component add advB {
	::button $itk_interior.advB -relief raised -text "Advanced Settings..." \
		-command [code $this activate_adv]
    } {
	usual
    }

    itk_component add okB {
	::button $itk_interior.okB  -relief raised -text "Ok" \
		-command [code $this ok]
    } {
	usual
    }

    itk_component add raytraceB {
	::button $itk_interior.raytraceB  -relief raised -text "Raytrace" \
		-command [code $this raytrace]
    } {
	usual
    }
    bind $itk_component(raytraceB) <Enter> [code $this enterRaytraceCB]
    bind $itk_component(raytraceB) <Leave> [code $this leaveCB]

    itk_component add abortB {
	::button $itk_interior.abortB  -relief raised -text "Abort" \
		-command [code $this abort]
    } {
	usual
    }

    itk_component add clearB {
	::button $itk_interior.clearB  -relief raised -text "Clear" \
		-command [code $this clear]
    } {
	usual
    }

    itk_component add dismissB {
	::button $itk_interior.dismissB  -relief raised -text "Dismiss" \
		-command [code $this deactivate]
    } {
	usual
    }

    itk_component add statusL {
	::label $itk_interior.statusL -anchor w -relief sunken -bd 2
    } {
	usual
    }

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

    # process options
    eval itk_initialize $args

    # Disable the source, jitter and lmodel CombBox's entry widgets
    configure -sourceState disabled -jitterState disabled -lmodelState disabled

    # Link the ComboBox's entry widgets to this class' variables
    $itk_component(sizeCB) configure -entryvariable [scope itk_option(-size)]
    $itk_component(bgcolorCB) configure -entryvariable [scope itk_option(-color)]
    $itk_component(destCB) configure -entryvariable [scope itk_option(-dest)]

    wm withdraw $itk_component(hull)
}

itcl::body RtControl::build_adv {} {
    itk_component add adv {
	toplevel $itk_interior.adv
    }

    itk_component add adv_gridF1 {
	::frame $itk_component(adv).gridF1
    } {
	usual
    }

    itk_component add adv_gridF2 {
	::frame $itk_component(adv).gridF2 -relief groove -bd 2
    } {
	usual
    }

    itk_component add adv_nprocL {
	::label $itk_component(adv).nprocL -text "# of Processors" -anchor e
    } {
	usual
    }

    itk_component add adv_nprocE {
	::entry $itk_component(adv).nprocE -relief sunken -bd 2 -width 2 \
		-textvar [scope itk_option(-nproc)]
    } {
	usual
    }

    itk_component add adv_hsampleL {
	::label $itk_component(adv).hsampleL -text "Hypersample" -anchor e
    } {
	usual
    }

    itk_component add adv_hsampleE {
	::entry $itk_component(adv).hsampleE -relief sunken -bd 2 -width 2 \
		-textvar [scope itk_option(-hsample)]
    } {
	usual
    }

    itk_component add adv_jitterL {
	::label $itk_component(adv).jitterL -text "Jitter" -anchor e
    } {
	usual
    }

    itk_component add adv_jitterCB {
	cadwidgets::ComboBox $itk_component(adv).jitterCB
    } {
	usual
	rename -state -jitterState jitterState JitterState
    }

    # populate jitter's combobox menu
    $itk_component(adv_jitterCB) add command -label "None" \
	    -command [code $this set_jitter 0]
    $itk_component(adv_jitterCB) add command -label "Cell" \
	    -command [code $this set_jitter 1]
    $itk_component(adv_jitterCB) add command -label "Frame" \
	    -command [code $this set_jitter 2]
    $itk_component(adv_jitterCB) add command -label "Both" \
	    -command [code $this set_jitter 3]

    itk_component add adv_lmodelL {
	::label $itk_component(adv).lightL -text "Light Model" -anchor e
    } {
	usual
    }

    itk_component add adv_lmodelCB {
	cadwidgets::ComboBox $itk_component(adv).lightCB
    } {
	usual
	rename -state -lmodelState lmodelState LmodelState
    }

    # populate lmodel's combobox menu
    $itk_component(adv_lmodelCB) add command -label "Full" \
	    -command [code $this set_lmodel 0]
    $itk_component(adv_lmodelCB) add command -label "Diffuse" \
	    -command [code $this set_lmodel 1]
    $itk_component(adv_lmodelCB) add command -label "Surface Normals" \
	    -command [code $this set_lmodel 2]
    $itk_component(adv_lmodelCB) add command -label "Diffuse - 3 light" \
	    -command [code $this set_lmodel 3]
    $itk_component(adv_lmodelCB) add command -label "Curvature - inverse radius" \
	    -command [code $this set_lmodel 4]
    $itk_component(adv_lmodelCB) add command -label "Curvature - direction vector" \
	    -command [code $this set_lmodel 5]

    itk_component add adv_otherL {
	::label $itk_component(adv).otherL -text "Other Options" -anchor e
    } {
	usual
    }

    itk_component add adv_otherE {
	::entry $itk_component(adv).otherE -relief sunken -bd 2 -width 2 \
		-textvar [scope itk_option(-other)]
    } {
	usual
    }

    itk_component add adv_dismissB {
	::button $itk_component(adv).buttonB -relief raised -text "Dismiss" \
		-command [code $this deactivate_adv]
    } {
	usual
    }

    # remove labels from the ComboBox
    grid forget [$itk_component(adv_jitterCB) component label]
    grid forget [$itk_component(adv_lmodelCB) component label]

    grid $itk_component(adv_nprocL) $itk_component(adv_nprocE) \
	    -sticky nsew -pady 1 -in $itk_component(adv_gridF1)
    grid $itk_component(adv_hsampleL) $itk_component(adv_hsampleE) \
	    -sticky nsew -pady 1 -in $itk_component(adv_gridF1)
    grid $itk_component(adv_jitterL) $itk_component(adv_jitterCB) \
	    -sticky nsew -pady 1 -in $itk_component(adv_gridF1)
    grid $itk_component(adv_lmodelL) $itk_component(adv_lmodelCB) \
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
    grid columnconfigure $itk_component(adv) 0 -weight 1
    grid rowconfigure $itk_component(adv) 0 -weight 1

    wm withdraw $itk_component(adv)
    wm title $itk_component(adv) "Advanced Settings ($itk_component(adv))"
}

configbody RtControl::nproc {
    if {![regexp "^\[0-9\]+$" $itk_option(-nproc)]} {
	error "Bad value - $itk_option(-nproc)"
    }
}

configbody RtControl::hsample {
    if {![regexp "^\[0-9\]+$" $itk_option(-hsample)]} {
	error "Bad value - $itk_option(-hsample)"
    }
}

configbody RtControl::jitter {
    set_jitter $itk_option(-jitter)
}

configbody RtControl::lmodel {
    set_lmodel $itk_option(-lmodel)
}

configbody RtControl::size {
    set_size $itk_option(-size)
}

configbody RtControl::color {
    eval $itk_component(bgcolorCB) setColor $itk_option(-color)
}

configbody RtControl::mged {
    if {$itk_option(-mged) == ""} {
	return
    }

    update_control_panel
}

itcl::body RtControl::activate {} {
    raise $itk_component(hull)

    # center on screen
    if {$win_geom == ""} {
	center $itk_component(hull) win_geom
    }
    wm geometry $itk_component(hull) $win_geom
    wm deiconify $itk_component(hull)
}

itcl::body RtControl::activate_adv {} {
    raise $itk_component(adv)

    # center over control panel
    if {$win_geom_adv == ""} {
	center $itk_component(adv) win_geom_adv $itk_component(hull)
    }
    wm geometry $itk_component(adv) $win_geom_adv
    wm deiconify $itk_component(adv)
}

itcl::body RtControl::deactivate {} {
    set win_geom [wm geometry $itk_component(hull)]
    wm withdraw $itk_component(hull)
    deactivate_adv
}

itcl::body RtControl::deactivate_adv {} {
    set win_geom_adv [wm geometry $itk_component(adv)]
    wm withdraw $itk_component(adv)
}

itcl::body RtControl::center {w gs {cw ""}} {
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

itcl::body RtControl::set_src {pane} {
    if {[catch {$itk_option(-mged) isa Mged} result]} {
	error "Raytrace Control Panel($this) is not associated with an Mged object"
    }

    if {$pane == "active"} {
	set pane [$itk_option(-mged) pane]
    }

    $itk_component(srcCB) setText $pane
}

itcl::body RtControl::set_dest {pane} {
    if {[catch {$itk_option(-mged) isa Mged} result]} {
	error "Raytrace Control Panel($this) is not associated with an Mged object"
    }

    if {$pane == "active"} {
	set pane [$itk_option(-mged) pane]
    }

    # update the destCB entry
    set itk_option(-dest) $pane

    # update the Inactive/Underlay/Overlay radiobutton
    set fb_mode [$itk_option(-mged) component $itk_option(-dest) fb_active]
}

itcl::body RtControl::set_size {size} {
    if {[catch {$itk_option(-mged) isa Mged} result]} {
	error "Raytrace Control Panel($this) is not associated with an Mged object"
    }

    if {$size == "Size of Pane"} {
	# Try using the destination for obtaining the size.
	set pane $itk_option(-dest)
	switch -- $pane {
	    ul -
	    ur -
	    ll -
	    lr {
		set itk_option(-size) [$itk_option(-mged) component $pane cget -dmsize]
		return
	    }
	}

	# The destination could be a file or a framebuffer.
	# We don't know what its size is so try to use the
	# source pane for obtaining the size.
	set pane [$itk_component(srcCB) getText]
	switch -- $pane {
	    ul -
	    ur -
	    ll -
	    lr {
		set itk_option(-size) [$itk_option(-mged) component $pane cget -dmsize]
		return
	    }
	}

	# We failed to get the size using the destination and source panes.
	# So, we use the active pane for obtaining the size.
	set pane [$itk_option(-mged) pane]
	set itk_option(-size) [$itk_option(-mged) component $pane cget -dmsize]
	return
    }

    if {![regexp "^(\[ \]*\[0-9\]+)((\[ \]*\[xX\]?\[ \]*)|(\[ \]+))(\[0-9\]*\[ \]*)$" $size]} {
	error "Bad size - $size"
    }
    set itk_option(-size) $size
}

itcl::body RtControl::set_jitter {j} {
    switch -- $j {
	0 {
	    set itk_option(-jitter) 0
	    $itk_component(adv_jitterCB) setText "None"
	}
	1 {
	    set itk_option(-jitter) 1
	    $itk_component(adv_jitterCB) setText "Cell"
	}
	2 {
	    set itk_option(-jitter) 2
	    $itk_component(adv_jitterCB) setText "Frame"
	}
	3 {
	    set itk_option(-jitter) 3
	    $itk_component(adv_jitterCB) setText "Both"
	}
	default {
	    error "RtControl::set_jitter: bad value - $j"
	}
    }
}

itcl::body RtControl::set_lmodel {lm} {
    switch -- $lm {
	0 {
	    set itk_option(-lmodel) 0
	    $itk_component(adv_lmodelCB) setText "Full"
	}
	1 {
	    set itk_option(-lmodel) 1
	    $itk_component(adv_lmodelCB) setText "Diffuse"
	}
	2 {
	    set itk_option(-lmodel) 2
	    $itk_component(adv_lmodelCB) setText "Surface Normals"
	}
	3 {
	    set itk_option(-lmodel) 3
	    $itk_component(adv_lmodelCB) setText "Diffuse - 3 light"
	}
	4 {
	    set itk_option(-lmodel) 4
	    $itk_component(adv_lmodelCB) setText "Curvature - inverse radius"
	}
	5 {
	    set itk_option(-lmodel) 5
	    $itk_component(adv_lmodelCB) setText "Curvature - directioin vector"
	}
	default {
	    error "RtControl::set_lmodel: bad value - $lm"
	}
    }
}

## - cook_dest
#
# Return a port number if possible. Otherwise, return
# whatever the user specified (i.e. it's already cooked)
#
itcl::body RtControl::cook_dest {dest} {
    switch -- $dest {
	ul -
	ur -
	ll -
	lr {
	    if {![$itk_option(-mged) component $dest fb_active]} {
		$itk_option(-mged) component $dest fb_active 1

		# update the Inactive/Underlay/Overlay radiobutton
		set fb_mode 1
	    }
	    return [$itk_option(-mged) component $dest listen]
	}
	default {
	    # Already cooked.
	    return $dest
	}
    }
}

itcl::body RtControl::fb_mode {} {
    if {[catch {$itk_option(-mged) isa Mged} result]} {
	error "Raytrace Control Panel($this) is not associated with an Mged object"
    }

    switch -- $itk_option(-dest) {
	ul -
	ur -
	ll -
	lr {
	    $itk_option(-mged) component $itk_option(-dest) fb_active $fb_mode
	}
	default {
	    # Use the active pane
	    $itk_option(-mged) fb_active $fb_mode
	}
    }
}

itcl::body RtControl::ok {} {
    if {[catch {$itk_option(-mged) isa Mged} result]} {
	error "Raytrace Control Panel($this) is not associated with an Mged object"
    }

    raytrace
    deactivate
}

itcl::body RtControl::raytrace {} {
    if {[catch {$itk_option(-mged) isa Mged} result]} {
	error "Raytrace Control Panel($this) is not associated with an Mged object"
    }

    set rt_cmd "$itk_option(-mged) component [$itk_component(srcCB) getText] \
	    rt -F [get_cooked_dest]"

    if {$itk_option(-size) != ""} {
	set result [regexp "^(\[ \]*\[0-9\]+)((\[ \]*\[xX\]?\[ \]*)|(\[ \]+))(\[0-9\]*\[ \]*)$"\
		$itk_option(-size) smatch width junkA junkB junkC height]
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
	    error "Bad size - $itk_option(-size)"
	}
    }

    set color [get_color]
    if {$color != ""} {
	append rt_cmd " -C[lindex $color 0]/[lindex $color 1]/[lindex $color 2]"
    }

    if {$itk_option(-nproc) != ""} {
	append rt_cmd " -P$itk_option(-nproc)"
    }

    if {$itk_option(-hsample) != ""} {
	append rt_cmd " -H$itk_option(-hsample)"
    }

    append rt_cmd " -J$itk_option(-jitter)"

    if {$itk_option(-lmodel) != ""} {
	append rt_cmd " -l$itk_option(-lmodel)"
    }

    if {$itk_option(-other) != ""} {
	append rt_cmd " $itk_option(-other)"
    }

    set result [catch {eval $rt_cmd} rt_error]
    if {$result} {
	error $rt_error
    }
}

itcl::body RtControl::abort {} {
    if {[catch {$itk_option(-mged) isa Mged} result]} {
	error "Raytrace Control Panel($this) is not associated with an Mged object"
    }

    $itk_option(-mged) component [$itk_component(srcCB) getText] rtabort
}

itcl::body RtControl::clear {} {
    if {[catch {$itk_option(-mged) isa Mged} result]} {
	error "Raytrace Control Panel($this) is not associated with an Mged object"
    }

    set cooked_dest [get_cooked_dest]
    set color [get_color]

    if {$color == ""} {
	set result [catch {exec fbclear -F $cooked_dest 0 0 0 &} rt_error]
    } else {
	set result [catch {eval exec fbclear -F $cooked_dest $color &} rt_error]
    }

    if {$result} {
	error $rt_error
    }
}

itcl::body RtControl::get_color {} {
    if {$itk_option(-color) != ""} {
	set result [regexp "^\[0-9\]+\[ \]+\[0-9\]+\[ \]+\[0-9\]+$" $itk_option(-color)]
	if {!$result} {
	    error "Improper color specification: $itk_option(-color)"
	}
    }

    return $itk_option(-color)
}

itcl::body RtControl::get_size {} {
    return $size
}

## - get_cooked_dest
#
# Returns a port number or something specified by
# the user (i.e. hostname:port, or filename). No error
# checking is performed on user specified strings.
#
itcl::body RtControl::get_cooked_dest {} {
    set dest $itk_option(-dest)
    if {$dest == ""} {
	# use the active pane
	set dest [$itk_option(-mged) pane]
    }
    set cooked_dest [cook_dest $dest]
    if {$cooked_dest == -1} {
	switch -- $dest {
	    ul -
	    ur -
	    ll -
	    lr {
		# Cause the framebuffer to listen for clients on port 0.
		# If port 0 isn't available, the next available port will
		# be returned.
		set cooked_dest [$itk_option(-mged) component $dest listen 0]
	    }
	    default {
		# We should only get here if $dest is -1,
		# which had to be specified by the user.
		error "Invalid destination - $dest"
	    }
	}
    }

    return $cooked_dest
}

itcl::body RtControl::update_control_panel {} {
    if {[catch {$itk_option(-mged) isa Mged} result]} {
	error "Raytrace Control Panel($this) is not associated with an Mged object"
    }

    if {![$itk_option(-mged) fb_active]} {
	# Framebuffer is not active, so activate it
	# by putting it in "Underlay" mode.
	$itk_option(-mged) fb_active 1
	set fb_mode 1
    }

    set pane [$itk_option(-mged) pane]
    $itk_component(srcCB) setText $pane
#    $itk_component(destCB) setText $pane
    set itk_option(-dest) $pane
    set_size "Size of Pane"

    # Calling setColor so that the menubutton color gets set.
    eval $itk_component(bgcolorCB) setColor [$itk_option(-mged) bg]
}

itcl::body RtControl::menuStatusCB {w} {
    set op [$w entrycget active -label]
    switch -- $op {
	default {
	    set msg ""
	}
    }
}

itcl::body RtControl::leaveCB {} {
    $itk_component(statusL) configure -text ""
}

itcl::body RtControl::enterOkCB {} {
    if {[catch {$itk_option(-mged) isa Mged} result]} {
	$itk_component(statusL) configure -text "Not associated with an Mged object"
    } else {
	$itk_component(statusL) configure -text "Raytrace [$itk_component(srcCB) getText]'s view into $itk_option(-dest)"
    }
}

itcl::body RtControl::enterRaytraceCB {} {
    if {[catch {$itk_option(-mged) isa Mged} result]} {
	$itk_component(statusL) configure -text "Not associated with an Mged object"
    } else {
	$itk_component(statusL) configure -text "Raytrace [$itk_component(srcCB) getText]'s view into $itk_option(-dest)"
    }
}
