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

#XXX This should work.
option add *RtControl*tearoff 0 widgetDefault

class RtControl {
    inherit itk::Toplevel

    itk_option define -olist olist Olist {}
    itk_option define -omode omode Omode {}
    itk_option define -nproc nproc Nproc 1
    itk_option define -hsample hsample Hsample 0
    itk_option define -jitter jitter Jitter 0
    itk_option define -jitterTitle jitterTitle JitterTitle "None"
    itk_option define -lmodel lmodel Lmodel 0
    itk_option define -lmodelTitle lmodelTitle LmodelTitle "Full"
    itk_option define -other other Other {}
    itk_option define -size size Size 512
    itk_option define -color color Color {0 0 0}
    itk_option define -dest dest Dest ""
    itk_option define -mged mged Mged ""

    public method activate {}
    public method deactivate {}
    public method center {}

    private method set_src {pane}
    private method set_dest {pane}
    private method set_size {size}
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

    private variable fb_mode 0
    private variable raw_src ""
    private variable win_geom ""

    constructor {args} {}
}

body RtControl::constructor {args} {
    # revive a few ignored options
    #itk_option add hull.screen

    itk_component add gridF1 {
	frame $itk_interior.gridF1
    } {
	usual
    }

    itk_component add gridF2 {
	frame $itk_interior.gridF2 -relief groove -bd 2
    } {
	usual
    }

    itk_component add gridF3 {
	frame $itk_interior.gridF3
    } {
	usual
    }

    itk_component add menubar {
	menu $itk_interior.menubar
    } {
	usual
    }
# Why doesn't this work???
#    $itk_component(hull) configure -menu $itk_component(menubar)
    $this component hull configure -menu $itk_component(menubar)

    itk_component add fbM {
	menu $itk_component(menubar).fb -title "Framebuffer"
    } {
	usual
    }

    itk_component add objM {
	menu $itk_component(menubar).obj -title "Objects"
    } {
	usual
    }

    $itk_component(menubar) add cascade -label "Framebuffer" -underline 0 -menu $itk_component(fbM)
    $itk_component(menubar) add cascade -label "Objects" -underline 0 -menu $itk_component(objM)

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
	label $itk_interior.srcL -text "Source" -anchor e
    } {}

    itk_component add srcCB {
	cadwidgets::ComboBox $itk_interior.srcCB -state disabled
    } {
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
	label $itk_interior.destL -text "Destination" -anchor e
    } {}

    itk_component add destCB {
	cadwidgets::ComboBox $itk_interior.destCB
    } {
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
	label $itk_interior.sizeL -text "Size" -anchor e
    } {}

    itk_component add sizeCB {
	cadwidgets::ComboBox $itk_interior.sizeCB
    } {
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
	label $itk_interior.bgcolorL -text "Background Color" -anchor e
    } {}

    itk_component add bgcolorCB {
	cadwidgets::ColorEntry $itk_interior.bgcolorCB
    } {
    }

    itk_component add advB {
	button $itk_interior.advB -relief raised -text "Advanced Settings..." \
		-command {puts "do advanced settings"}
    } {
	usual
    }

    itk_component add okB {
	button $itk_interior.okB  -relief raised -text "Ok" \
		-command [code $this ok]
    } {
	usual
    }

    itk_component add raytraceB {
	button $itk_interior.raytraceB  -relief raised -text "Raytrace" \
		-command [code $this raytrace]
    } {
	usual
    }

    itk_component add abortB {
	button $itk_interior.abortB  -relief raised -text "Abort" \
		-command [code $this abort]
    } {
	usual
    }

    itk_component add clearB {
	button $itk_interior.clearB  -relief raised -text "Clear" \
		-command [code $this clear]
    } {
	usual
    }

    itk_component add dismissB {
	button $itk_interior.dismissB  -relief raised -text "Dismiss" \
		-command [code $this deactivate]
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
    grid columnconfigure $itk_component(hull) 0 -weight 1
    grid rowconfigure $itk_component(hull) 0 -weight 1

    # process options
    eval itk_initialize $args

    # Link the ComboBox's entry widgets to this class' variables
    $itk_component(sizeCB) configure -entryvariable [scope itk_option(-size)]
    $itk_component(bgcolorCB) configure -entryvariable [scope itk_option(-color)]
    $itk_component(destCB) configure -entryvariable [scope itk_option(-dest)]

    wm withdraw $itk_component(hull)
    center
}

configbody RtControl::mged {
    if {$itk_option(-mged) == ""} {
	return
    }

    update_control_panel
}

itcl::body RtControl::activate {} {
    puts "activate: win_geom - $win_geom"
    wm geometry $itk_component(hull) $win_geom
    wm deiconify $itk_component(hull)
}

itcl::body RtControl::deactivate {} {
    set win_geom [wm geometry $itk_component(hull)]
    wm withdraw $itk_component(hull)
}

itcl::body RtControl::center {} {
    update idletasks
    set width [winfo reqwidth $itk_component(hull)]
    set height [winfo reqheight $itk_component(hull)]
    set screenwidth [winfo screenwidth $itk_component(hull)]
    set screenheight [winfo screenheight $itk_component(hull)]

    set x [expr {int($screenwidth * 0.5 - $width * 0.5)}]
    set y [expr {int($screenheight * 0.5 - $height * 0.5)}]
    wm geometry $itk_component(hull) +$x+$y
    set win_geom +$x+$y
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

#    $itk_component(destCB) setText $pane
    set itk_option(-dest) $pane
}

itcl::body RtControl::set_size {size} {
    if {[catch {$itk_option(-mged) isa Mged} result]} {
	error "Raytrace Control Panel($this) is not associated with an Mged object"
    }

    if {$size == "Size of Pane"} {
	# Try using the destination for obtaining the size.
#	set pane [$itk_component(destCB) getText]
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

    set itk_option(-size) $size
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

    if {$itk_option(-jitter) != ""} {
	append rt_cmd " -J$itk_option(-jitter)"
    }

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
    puts "RtControl::abort - fix me!"
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
#    set dest [$itk_component(destCB) getText]
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
