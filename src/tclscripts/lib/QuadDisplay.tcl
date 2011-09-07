#                 Q U A D D I S P L A Y . T C L
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

#####################################################################
# DEPRECATED: This widget is deprecated and should no longer be used.
# Use the Ged widget instead.
#####################################################################

#
# Description -
#	The QuadDisplay class is comprised of four Display objects. This class
#       keeps track of the current Display object and provides the means to toggle
#       between showing all four Display objects or just the current one.
#

option add *Pane*margin 0 widgetDefault
option add *QuadDisplay.width 400 widgetDefault
option add *QuadDisplay.height 400 widgetDefault

::itcl::class QuadDisplay {
    inherit iwidgets::Panedwindow

    itk_option define -pane pane Pane ur
    itk_option define -multi_pane multi_pane Multi_pane 0
    itk_option define -paneCallback paneCallback PaneCallback ""

    constructor {args} {}
    destructor {}

    public method getDrawLabelsHook {args}
    public method setDrawLabelsHook {args}
    public method setDrawLabelsHookAll {args}

    public method pane {args}
    public method multi_pane {args}
    public method refresh {}
    public method refreshAll {}

    # methods for controlling the view object
    public method ae {args}
    public method ae2dir {args}
    public method arot {args}
    public method base2local {}
    public method center {args}
    public method coord {args}
    public method viewDir {args}
    public method eye {args}
    public method eye_pos {args}
    public method invSize {args}
    public method keypoint {args}
    public method local2base {}
    public method lookat {args}
    public method m2vPoint {args}
    public method model2view {args}
    public method mrot {args}
    public method mrotPoint {args}
    public method orientation {args}
    public method pmat {args}
    public method pmodel2view {args}
    public method pov {args}
    public method rmat {args}
    public method rot {args}
    public method rotate_about {args}
    public method sca {args}
    public method setview {args}
    public method size {args}
    public method slew {args}
    public method tra {args}
    public method units {args}
    public method v2mPoint {args}
    public method view2model {args}
    public method vrot {args}
    public method vtra {args}
    public method zoom {args}

    public method autoview {{gindex 0}}
    public method autoviewAll {{gindex 0}}

    public method add {glist}
    public method addAll {glist}
    public method remove {glist}
    public method removeAll {glist}
    public method contents {}

    public method bg {args}
    public method bgAll {args}
    public method bounds {args}
    public method boundsAll {args}
    public method depthMask {args}
    public method light {args}
    public method linestyle {args}
    public method linewidth {args}
    public method listen {args}
    public method perspective {args}
    public method perspective_angle {args}
    public method sync {}
    public method png {args}
    public method mouse_nirt {x y}
    public method nirt {args}
    public method vnirt {vx vy}
    public method qray {args}
    public method rt {args}
    public method rtabort {{gi 0}}
    public method rtarea {args}
    public method rtcheck {args}
    public method rtedge {args}
    public method rtweight {args}
    public method transparency {args}
    public method zbuffer {args}
    public method zclip {args}

    if {$tcl_platform(os) != "Windows NT"} {
    }
    public method fb_active {args}
    public method fb_observe {args}

    public method toggle_centerDotEnable {args}
    public method toggle_centerDotEnableAll {}
    public method toggle_viewAxesEnable {args}
    public method toggle_viewAxesEnableAll {}
    public method toggle_modelAxesEnable {args}
    public method toggle_modelAxesEnableAll {}
    public method toggle_modelAxesTickEnable {args}
    public method toggle_modelAxesTickEnableAll {}
    public method toggle_scaleEnable {args}
    public method toggle_scaleEnableAll {}

    public method resetAll {}
    public method default_views {}
    public method attach_view {}
    public method attach_viewAll {}
    public method attach_drawable {dg}
    public method attach_drawableAll {dg}
    public method detach_view {}
    public method detach_viewAll {}
    public method detach_drawable {dg}
    public method detach_drawableAll {dg}

    public method lightAll {args}
    public method transparencyAll {args}
    public method zbufferAll {args}
    public method zclipAll {args}

    public method setCenterDotEnable {args}
    public method setViewAxesEnable {args}
    public method setModelAxesEnable {args}
    public method setModelAxesTickEnable {args}
    public method setViewAxesPosition {args}
    public method setModelAxesPosition {args}
    public method setModelAxesTickInterval {args}
    public method setModelAxesTicksPerMajor {args}
    public method setScaleEnable {args}

    public method idle_mode {}
    public method rotate_mode {x y}
    public method translate_mode {x y}
    public method scale_mode {x y}
    public method orotate_mode {x y func obj kx ky kz}
    public method oscale_mode {x y func obj kx ky kz}
    public method otranslate_mode {x y func obj}
    public method screen2model {x y}
    public method screen2view {x y}

    public method resetBindings {}
    public method resetBindingsAll {}

    public method ? {}
    public method apropos {key}
    public method getUserCmds {}
    public method help {args}

    protected method toggle_multi_pane {}

    private variable priv_pane ur
    private variable priv_multi_pane 1
}

::itcl::body QuadDisplay::constructor {args} {
    iwidgets::Panedwindow::add upper
    iwidgets::Panedwindow::add lower

    puts "DEPRECATION WARNING: The QuadDisplay widget should no longer be used.  Use the Ged widget instead."

    # create two more panedwindows
    itk_component add upw {
	::iwidgets::Panedwindow [childsite upper].pw -orient vertical
    } {
	usual
	keep -sashwidth -sashheight
	keep -sashborderwidth -sashindent
	keep -thickness -showhandle
	rename -sashcursor -hsashcursor hsashcursor HSashCursor
    }

    itk_component add lpw {
	::iwidgets::Panedwindow [childsite lower].pw -orient vertical
    } {
	usual
	keep -sashwidth -sashheight
	keep -sashborderwidth -sashindent
	keep -thickness -showhandle
	rename -sashcursor -hsashcursor hsashcursor HSashCursor
    }

    $itk_component(upw) add ulp
    $itk_component(upw) add urp
    $itk_component(lpw) add llp
    $itk_component(lpw) add lrp

    # create four instances of Display
    itk_component add ul {
	Display [$itk_component(upw) childsite ulp].display
    } {
	usual
    }

    itk_component add ur {
	Display [$itk_component(upw) childsite urp].display
    } {
	usual
    }

    itk_component add ll {
	Display [$itk_component(lpw) childsite llp].display
    } {
	usual
    }

    itk_component add lr {
	Display [$itk_component(lpw) childsite lrp].display
    } {
	usual
    }

    # initialize the views
    default_views

    pack $itk_component(ul) -fill both -expand yes
    pack $itk_component(ur) -fill both -expand yes
    pack $itk_component(ll) -fill both -expand yes
    pack $itk_component(lr) -fill both -expand yes

    pack $itk_component(upw) -fill both -expand yes
    pack $itk_component(lpw) -fill both -expand yes

    catch {eval itk_initialize $args}
}

::itcl::configbody QuadDisplay::pane {
    pane $itk_option(-pane)
}

::itcl::configbody QuadDisplay::multi_pane {
    multi_pane $itk_option(-multi_pane)
}

::itcl::body QuadDisplay::getDrawLabelsHook {args} {
    eval $itk_component($itk_option(-pane)) getDrawLabelsHook $args
}

::itcl::body QuadDisplay::setDrawLabelsHook {args} {
    eval $itk_component($itk_option(-pane)) setDrawLabelsHook $args
}

::itcl::body QuadDisplay::setDrawLabelsHookAll {args} {
    eval $itk_component(ul) setDrawLabelsHook $args
    eval $itk_component(ur) setDrawLabelsHook $args
    eval $itk_component(ll) setDrawLabelsHook $args
    eval $itk_component(lr) setDrawLabelsHook $args
}

::itcl::body QuadDisplay::pane {args} {
    # get the active pane
    if {$args == ""} {
	return $itk_option(-pane)
    }

    # set the active pane
    switch $args {
	ul -
	ur -
	ll -
	lr {
	    set itk_option(-pane) $args
	}
	default {
	    return -code error "pane: bad value - $args"
	}
    }

    if {!$itk_option(-multi_pane)} {
	# nothing to do
	if {$priv_pane == $itk_option(-pane)} {
	    return
	}

	switch $priv_pane {
	    ul {
		switch $itk_option(-pane) {
		    ur {
			$itk_component(upw) hide ulp
			$itk_component(upw) show urp
		    }
		    ll {
			iwidgets::Panedwindow::hide upper
			$itk_component(upw) show urp
			iwidgets::Panedwindow::show lower
			$itk_component(lpw) show llp
			$itk_component(lpw) hide lrp
		    }
		    lr {
			iwidgets::Panedwindow::hide upper
			$itk_component(upw) show urp
			iwidgets::Panedwindow::show lower
			$itk_component(lpw) hide llp
			$itk_component(lpw) show lrp
		    }
		}
	    }
	    ur {
		switch $itk_option(-pane) {
		    ul {
			$itk_component(upw) hide urp
			$itk_component(upw) show ulp
		    }
		    ll {
			iwidgets::Panedwindow::hide upper
			$itk_component(upw) show ulp
			iwidgets::Panedwindow::show lower
			$itk_component(lpw) show llp
			$itk_component(lpw) hide lrp
		    }
		    lr {
			iwidgets::Panedwindow::hide upper
			$itk_component(upw) show ulp
			iwidgets::Panedwindow::show lower
			$itk_component(lpw) hide llp
			$itk_component(lpw) show lrp
		    }
		}
	    }
	    ll {
		switch $itk_option(-pane) {
		    ul {
			iwidgets::Panedwindow::hide lower
			$itk_component(lpw) show lrp
			iwidgets::Panedwindow::show upper
			$itk_component(upw) hide urp
			$itk_component(upw) show ulp
		    }
		    ur {
			iwidgets::Panedwindow::hide lower
			$itk_component(lpw) show lrp
			iwidgets::Panedwindow::show upper
			$itk_component(upw) hide ulp
			$itk_component(upw) show urp
		    }
		    lr {
			$itk_component(lpw) hide llp
			$itk_component(lpw) show lrp
		    }
		}
	    }
	    lr {
		switch $itk_option(-pane) {
		    ul {
			iwidgets::Panedwindow::hide lower
			$itk_component(lpw) show llp
			iwidgets::Panedwindow::show upper
			$itk_component(upw) hide urp
			$itk_component(upw) show ulp
		    }
		    ur {
			iwidgets::Panedwindow::hide lower
			$itk_component(lpw) show llp
			iwidgets::Panedwindow::show upper
			$itk_component(upw) hide ulp
			$itk_component(upw) show urp
		    }
		    ll {
			$itk_component(lpw) hide lrp
			$itk_component(lpw) show llp
		    }
		}
	    }
	}
    }

    set priv_pane $itk_option(-pane)

    if {$itk_option(-paneCallback) != ""} {
	catch {eval $itk_option(-paneCallback) $args}
    }
}

::itcl::body QuadDisplay::multi_pane {args} {
    # get multi_pane
    if {$args == ""} {
	return $itk_option(-multi_pane)
    }

    # nothing to do
    if {$args == $priv_multi_pane} {
	return
    }

    switch $args {
	0 -
	1 {
	    toggle_multi_pane
	}
	default {
	    return -code error "mult_pane: bad value - $args"
	}
    }
}

::itcl::body QuadDisplay::refresh {} {
    $itk_component($itk_option(-pane)) refresh
}

::itcl::body QuadDisplay::refreshAll {} {
    $itk_component(ul) refresh
    $itk_component(ur) refresh
    $itk_component(ll) refresh
    $itk_component(lr) refresh
}

::itcl::body QuadDisplay::ae {args} {
    eval $itk_component($itk_option(-pane)) ae $args
}

::itcl::body QuadDisplay::ae2dir {args} {
    eval $itk_component($itk_option(-pane)) ae2dir $args
}

::itcl::body QuadDisplay::arot {args} {
    eval $itk_component($itk_option(-pane)) arot $args
}

::itcl::body QuadDisplay::base2local {} {
    $itk_component($itk_option(-pane)) base2local
}

::itcl::body QuadDisplay::center {args} {
    eval $itk_component($itk_option(-pane)) center $args
}

::itcl::body QuadDisplay::coord {args} {
    eval $itk_component($itk_option(-pane)) coord $args
}

::itcl::body QuadDisplay::viewDir {args} {
    eval $itk_component($itk_option(-pane)) viewDir $args
}

::itcl::body QuadDisplay::eye {args} {
    eval $itk_component($itk_option(-pane)) eye $args
}

::itcl::body QuadDisplay::eye_pos {args} {
    eval $itk_component($itk_option(-pane)) eye_pos $args
}

::itcl::body QuadDisplay::invSize {args} {
    eval $itk_component($itk_option(-pane)) invSize $args
}

::itcl::body QuadDisplay::keypoint {args} {
    eval $itk_component($itk_option(-pane)) keypoint $args
}

::itcl::body QuadDisplay::local2base {} {
    $itk_component($itk_option(-pane)) local2base
}

::itcl::body QuadDisplay::lookat {args} {
    eval $itk_component($itk_option(-pane)) lookat $args
}

::itcl::body QuadDisplay::m2vPoint {args} {
    eval $itk_component($itk_option(-pane)) m2vPoint $args
}

::itcl::body QuadDisplay::model2view {args} {
    eval $itk_component($itk_option(-pane)) model2view $args
}

::itcl::body QuadDisplay::mrot {args} {
    eval $itk_component($itk_option(-pane)) mrot $args
}

::itcl::body QuadDisplay::mrotPoint {args} {
    eval $itk_component($itk_option(-pane)) mrotPoint $args
}

::itcl::body QuadDisplay::orientation {args} {
    eval $itk_component($itk_option(-pane)) orientation $args
}

::itcl::body QuadDisplay::pmat {args} {
    eval $itk_component($itk_option(-pane)) pmat $args
}

::itcl::body QuadDisplay::pmodel2view {args} {
    eval $itk_component($itk_option(-pane)) pmodel2view $args
}

::itcl::body QuadDisplay::pov {args} {
    eval $itk_component($itk_option(-pane)) pov $args
}

::itcl::body QuadDisplay::rmat {args} {
    eval $itk_component($itk_option(-pane)) rmat $args
}

::itcl::body QuadDisplay::rot {args} {
    eval $itk_component($itk_option(-pane)) rot $args
}

::itcl::body QuadDisplay::rotate_about {args} {
    eval $itk_component($itk_option(-pane)) rotate_about $args
}

::itcl::body QuadDisplay::slew {args} {
    eval $itk_component($itk_option(-pane)) slew $args
}

::itcl::body QuadDisplay::sca {args} {
    eval $itk_component($itk_option(-pane)) sca $args
}

::itcl::body QuadDisplay::setview {args} {
    eval $itk_component($itk_option(-pane)) setview $args
}

::itcl::body QuadDisplay::size {args} {
    eval $itk_component($itk_option(-pane)) size $args
}

::itcl::body QuadDisplay::tra {args} {
    eval $itk_component($itk_option(-pane)) tra $args
}

::itcl::body QuadDisplay::units {args} {
    if {$args == ""} {
	return [$itk_component(ul) units]
    }

    eval $itk_component(ul) units $args
    eval $itk_component(ur) units $args
    eval $itk_component(ll) units $args
    eval $itk_component(lr) units $args
}

::itcl::body QuadDisplay::v2mPoint {args} {
    eval $itk_component($itk_option(-pane)) v2mPoint $args
}

::itcl::body QuadDisplay::view2model {args} {
    eval $itk_component($itk_option(-pane)) view2model $args
}

::itcl::body QuadDisplay::vrot {args} {
    eval $itk_component($itk_option(-pane)) vrot $args
}

::itcl::body QuadDisplay::vtra {args} {
    eval $itk_component($itk_option(-pane)) vtra $args
}

::itcl::body QuadDisplay::zoom {args} {
    eval $itk_component($itk_option(-pane)) zoom $args
}

::itcl::body QuadDisplay::autoview {{gindex 0}} {
    $itk_component($itk_option(-pane)) autoview $gindex
}

::itcl::body QuadDisplay::autoviewAll {{gindex 0}} {
    $itk_component(ul) autoview $gindex
    $itk_component(ur) autoview $gindex
    $itk_component(ll) autoview $gindex
    $itk_component(lr) autoview $gindex
}

::itcl::body QuadDisplay::add {glist} {
    $itk_component($itk_option(-pane)) add $glist
}

::itcl::body QuadDisplay::addAll {glist} {
    $itk_component(ul) add $glist
    $itk_component(ur) add $glist
    $itk_component(ll) add $glist
    $itk_component(lr) add $glist
}

::itcl::body QuadDisplay::remove {glist} {
    $itk_component($itk_option(-pane)) remove $glist
}

::itcl::body QuadDisplay::removeAll {glist} {
    $itk_component(ul) remove $glist
    $itk_component(ur) remove $glist
    $itk_component(ll) remove $glist
    $itk_component(lr) remove $glist
}

::itcl::body QuadDisplay::contents {} {
    $itk_component($itk_option(-pane)) contents
}

::itcl::body QuadDisplay::listen {args} {
    eval $itk_component($itk_option(-pane)) listen $args
}

::itcl::body QuadDisplay::linewidth {args} {
    set result [eval $itk_component($itk_option(-pane)) linewidth $args]
    if {$args != ""} {
	refresh
	return
    }
    return $result
}

::itcl::body QuadDisplay::linestyle {args} {
    eval $itk_component($itk_option(-pane)) linestyle $args
}

::itcl::body QuadDisplay::zclip {args} {
    eval $itk_component($itk_option(-pane)) zclip $args
}

::itcl::body QuadDisplay::toggle_modelAxesEnable {args} {
    switch -- $args {
	ul -
	ur -
	ll -
	lr {
	    eval $itk_component($args) toggle_modelAxesEnable
	}
	default {
	    eval $itk_component($itk_option(-pane)) toggle_modelAxesEnable
	}
    }
}

::itcl::body QuadDisplay::toggle_modelAxesEnableAll {} {
    eval $itk_component(ul) toggle_modelAxesEnable
    eval $itk_component(ur) toggle_modelAxesEnable
    eval $itk_component(ll) toggle_modelAxesEnable
    eval $itk_component(lr) toggle_modelAxesEnable
}

::itcl::body QuadDisplay::toggle_modelAxesTickEnable {args} {
    switch -- $args {
	ul -
	ur -
	ll -
	lr {
	    eval $itk_component($args) toggle_modelAxesTickEnable
	}
	default {
	    eval $itk_component($itk_option(-pane)) toggle_modelAxesTickEnable
	}
    }
}

::itcl::body QuadDisplay::toggle_modelAxesTickEnableAll {} {
    eval $itk_component(ul) toggle_modelAxesTickEnable
    eval $itk_component(ur) toggle_modelAxesTickEnable
    eval $itk_component(ll) toggle_modelAxesTickEnable
    eval $itk_component(lr) toggle_modelAxesTickEnable
}

::itcl::body QuadDisplay::toggle_viewAxesEnable {args} {
    switch -- $args {
	ul -
	ur -
	ll -
	lr {
	    eval $itk_component($args) toggle_viewAxesEnable
	}
	default {
	    eval $itk_component($itk_option(-pane)) toggle_viewAxesEnable
	}
    }
}

::itcl::body QuadDisplay::toggle_viewAxesEnableAll {} {
    eval $itk_component(ul) toggle_viewAxesEnable
    eval $itk_component(ur) toggle_viewAxesEnable
    eval $itk_component(ll) toggle_viewAxesEnable
    eval $itk_component(lr) toggle_viewAxesEnable
}

::itcl::body QuadDisplay::toggle_centerDotEnable {args} {
    switch -- $args {
	ul -
	ur -
	ll -
	lr {
	    eval $itk_component($args) toggle_centerDotEnable
	}
	default {
	    eval $itk_component($itk_option(-pane)) toggle_centerDotEnable
	}
    }
}

::itcl::body QuadDisplay::toggle_centerDotEnableAll {} {
    eval $itk_component(ul) toggle_centerDotEnable
    eval $itk_component(ur) toggle_centerDotEnable
    eval $itk_component(ll) toggle_centerDotEnable
    eval $itk_component(lr) toggle_centerDotEnable
}

::itcl::body QuadDisplay::toggle_scaleEnable {args} {
    switch -- $args {
	ul -
	ur -
	ll -
	lr {
	    eval $itk_component($args) toggle_scaleEnable
	}
	default {
	    eval $itk_component($itk_option(-pane)) toggle_scaleEnable
	}
    }
}

::itcl::body QuadDisplay::toggle_scaleEnableAll {} {
    eval $itk_component(ul) toggle_scaleEnable
    eval $itk_component(ur) toggle_scaleEnable
    eval $itk_component(ll) toggle_scaleEnable
    eval $itk_component(lr) toggle_scaleEnable
}

::itcl::body QuadDisplay::zclipAll {args} {
    eval $itk_component(ul) zclip $args
    eval $itk_component(ur) zclip $args
    eval $itk_component(ll) zclip $args
    eval $itk_component(lr) zclip $args
}

::itcl::body QuadDisplay::setCenterDotEnable {args} {
    set ve [eval $itk_component($itk_option(-pane)) configure -centerDotEnable $args]

    # we must be doing a "get"
    if {$ve != ""} {
	return [lindex $ve 4]
    }
}

::itcl::body QuadDisplay::setScaleEnable {args} {
    set ve [eval $itk_component($itk_option(-pane)) configure -scaleEnable $args]

    # we must be doing a "get"
    if {$ve != ""} {
	return [lindex $ve 4]
    }
}

::itcl::body QuadDisplay::setViewAxesEnable {args} {
    set ve [eval $itk_component($itk_option(-pane)) configure -viewAxesEnable $args]

    # we must be doing a "get"
    if {$ve != ""} {
	return [lindex $ve 4]
    }
}

::itcl::body QuadDisplay::setModelAxesEnable {args} {
    set me [eval $itk_component($itk_option(-pane)) configure -modelAxesEnable $args]

    # we must be doing a "get"
    if {$me != ""} {
	return [lindex $me 4]
    }
}

::itcl::body QuadDisplay::setViewAxesPosition {args} {
    set vap [eval $itk_component($itk_option(-pane)) configure -viewAxesPosition $args]

    # we must be doing a "get"
    if {$vap != ""} {
	return [lindex $vap 4]
    }
}

::itcl::body QuadDisplay::setModelAxesPosition {args} {
    set map [eval $itk_component($itk_option(-pane)) configure -modelAxesPosition $args]

    # we must be doing a "get"
    if {$map != ""} {
	set mm2local [$itk_component($itk_option(-pane)) base2local]
	set map [lindex $map 4]
	set x [lindex $map 0]
	set y [lindex $map 1]
	set z [lindex $map 2]

	return [list [expr {$x * $mm2local}] \
		    [expr {$y * $mm2local}] \
		    [expr {$z * $mm2local}]]
    }
}

::itcl::body QuadDisplay::setModelAxesTickEnable {args} {
    set te [eval $itk_component($itk_option(-pane)) configure -modelAxesTickEnable $args]

    # we must be doing a "get"
    if {$te != ""} {
	return [lindex $te 4]
    }
}

::itcl::body QuadDisplay::setModelAxesTickInterval {args} {
    set ti [eval $itk_component($itk_option(-pane)) configure -modelAxesTickInterval $args]

    # we must be doing a "get"
    if {$ti != ""} {
	set ti [lindex $ti 4]
	return [expr {$ti * [$itk_component($itk_option(-pane)) base2local]}]
    }
}

::itcl::body QuadDisplay::setModelAxesTicksPerMajor {args} {
    set tpm [eval $itk_component($itk_option(-pane)) configure -modelAxesTicksPerMajor $args]

    # we must be doing a "get"
    if {$tpm != ""} {
	return [lindex $tpm 4]
    }
}

::itcl::body QuadDisplay::idle_mode {} {
    eval $itk_component($itk_option(-pane)) idle_mode
}

::itcl::body QuadDisplay::rotate_mode {x y} {
    eval $itk_component($itk_option(-pane)) rotate_mode $x $y
}

::itcl::body QuadDisplay::translate_mode {x y} {
    eval $itk_component($itk_option(-pane)) translate_mode $x $y
}

::itcl::body QuadDisplay::scale_mode {x y} {
    eval $itk_component($itk_option(-pane)) scale_mode $x $y
}

::itcl::body QuadDisplay::orotate_mode {x y func obj kx ky kz} {
    eval $itk_component($itk_option(-pane)) orotate_mode $x $y $func $obj $kx $ky $kz
}

::itcl::body QuadDisplay::oscale_mode {x y func obj kx ky kz} {
    eval $itk_component($itk_option(-pane)) oscale_mode $x $y $func $obj $kx $ky $kz
}

::itcl::body QuadDisplay::otranslate_mode {x y func obj} {
    eval $itk_component($itk_option(-pane)) otranslate_mode $x $y $func $obj
}

::itcl::body QuadDisplay::screen2model {x y} {
    eval $itk_component($itk_option(-pane)) screen2model $x $y
}

::itcl::body QuadDisplay::screen2view {x y} {
    eval $itk_component($itk_option(-pane)) screen2view $x $y
}

::itcl::body QuadDisplay::transparency {args} {
    eval $itk_component($itk_option(-pane)) transparency $args
}

::itcl::body QuadDisplay::transparencyAll {args} {
    eval $itk_component(ul) transparency $args
    eval $itk_component(ur) transparency $args
    eval $itk_component(ll) transparency $args
    eval $itk_component(lr) transparency $args
}

::itcl::body QuadDisplay::zbuffer {args} {
    eval $itk_component($itk_option(-pane)) zbuffer $args
}

::itcl::body QuadDisplay::zbufferAll {args} {
    eval $itk_component(ul) zbuffer $args
    eval $itk_component(ur) zbuffer $args
    eval $itk_component(ll) zbuffer $args
    eval $itk_component(lr) zbuffer $args
}

::itcl::body QuadDisplay::depthMask {args} {
    eval $itk_component($itk_option(-pane)) depthMask $args
}

::itcl::body QuadDisplay::light {args} {
    eval $itk_component($itk_option(-pane)) light $args
}

::itcl::body QuadDisplay::lightAll {args} {
    eval $itk_component(ul) light $args
    eval $itk_component(ur) light $args
    eval $itk_component(ll) light $args
    eval $itk_component(lr) light $args
}

::itcl::body QuadDisplay::perspective {args} {
    eval $itk_component($itk_option(-pane)) perspective $args
}

::itcl::body QuadDisplay::perspective_angle {args} {
    eval $itk_component($itk_option(-pane)) perspective_angle $args
}

::itcl::body QuadDisplay::sync {} {
    $itk_component($itk_option(-pane)) sync
}

::itcl::body QuadDisplay::png {args} {
    eval $itk_component($itk_option(-pane)) png $args
}

::itcl::body QuadDisplay::bg {args} {
    set result [eval $itk_component($itk_option(-pane)) bg $args]
    if {$args != ""} {
	refresh
	return
    }
    return $result
}

::itcl::body QuadDisplay::bgAll {args} {
    set result [eval $itk_component(ul) bg $args]
    eval $itk_component(ur) bg $args
    eval $itk_component(ll) bg $args
    eval $itk_component(lr) bg $args

    if {$args != ""} {
	refreshAll
	return
    }

    return $result
}

::itcl::body QuadDisplay::bounds {args} {
    eval $itk_component($itk_option(-pane)) bounds $args
}

::itcl::body QuadDisplay::boundsAll {args} {
    eval $itk_component(ul) bounds $args
    eval $itk_component(ur) bounds $args
    eval $itk_component(ll) bounds $args
    eval $itk_component(lr) bounds $args
}

if {$tcl_platform(os) != "Windows NT"} {
}
::itcl::body QuadDisplay::fb_active {args} {
    eval $itk_component($itk_option(-pane)) fb_active $args
}

::itcl::body QuadDisplay::fb_observe {args} {
    eval $itk_component($itk_option(-pane)) fb_observe $args
}

::itcl::body QuadDisplay::mouse_nirt {x y} {
    eval $itk_component($itk_option(-pane)) mouse_nirt $x $y
}

::itcl::body QuadDisplay::nirt {args} {
    eval $itk_component($itk_option(-pane)) nirt $args
}

::itcl::body QuadDisplay::vnirt {vx vy} {
    eval $itk_component($itk_option(-pane)) vnirt $vx $vy
}

::itcl::body QuadDisplay::qray {args} {
    eval $itk_component($itk_option(-pane)) qray $args
}

::itcl::body QuadDisplay::rt {args} {
    eval $itk_component($itk_option(-pane)) rt $args
}

::itcl::body QuadDisplay::rtabort {{gi 0}} {
    $itk_component($itk_option(-pane)) rtabort $gi
}

::itcl::body QuadDisplay::rtarea {args} {
    eval $itk_component($itk_option(-pane)) rtarea $args
}

::itcl::body QuadDisplay::rtcheck {args} {
    eval $itk_component($itk_option(-pane)) rtcheck $args
}

::itcl::body QuadDisplay::rtedge {args} {
    eval $itk_component($itk_option(-pane)) rtedge $args
}

::itcl::body QuadDisplay::rtweight {args} {
    eval $itk_component($itk_option(-pane)) rtweight $args
}

::itcl::body QuadDisplay::resetAll {} {
    reset
    $itk_component(upw) reset
    $itk_component(lpw) reset
}

::itcl::body QuadDisplay::default_views {} {
    $itk_component(ul) ae 0 90 0
    $itk_component(ur) ae 35 25 0
    $itk_component(ll) ae 0 0 0
    $itk_component(lr) ae 90 0 0
}

::itcl::body QuadDisplay::attach_view {} {
    $itk_component($itk_option(-pane)) attach_view
}

::itcl::body QuadDisplay::attach_viewAll {} {
    $itk_component(ul) attach_view
    $itk_component(ur) attach_view
    $itk_component(ll) attach_view
    $itk_component(lr) attach_view
}

::itcl::body QuadDisplay::attach_drawable {dg} {
    $itk_component($itk_option(-pane)) attach_drawable $dg
}

::itcl::body QuadDisplay::attach_drawableAll {dg} {
    $itk_component(ul) attach_drawable $dg
    $itk_component(ur) attach_drawable $dg
    $itk_component(ll) attach_drawable $dg
    $itk_component(lr) attach_drawable $dg
}

::itcl::body QuadDisplay::detach_view {} {
    $itk_component($itk_option(-pane)) detach_view
}

::itcl::body QuadDisplay::detach_viewAll {} {
    $itk_component(ul) detach_view
    $itk_component(ur) detach_view
    $itk_component(ll) detach_view
    $itk_component(lr) detach_view
}

::itcl::body QuadDisplay::detach_drawable {dg} {
    $itk_component($itk_option(-pane)) detach_drawable $dg
}

::itcl::body QuadDisplay::detach_drawableAll {dg} {
    $itk_component(ul) detach_drawable $dg
    $itk_component(ur) detach_drawable $dg
    $itk_component(ll) detach_drawable $dg
    $itk_component(lr) detach_drawable $dg
}

::itcl::body QuadDisplay::toggle_multi_pane {} {
    if {$priv_multi_pane} {
	set itk_option(-multi_pane) 0
	set priv_multi_pane 0

	switch $itk_option(-pane) {
	    ul {
		iwidgets::Panedwindow::hide lower
		$itk_component(upw) hide urp
	    }
	    ur {
		iwidgets::Panedwindow::hide lower
		$itk_component(upw) hide ulp
	    }
	    ll {
		iwidgets::Panedwindow::hide upper
		$itk_component(lpw) hide lrp
	    }
	    lr {
		iwidgets::Panedwindow::hide upper
		$itk_component(lpw) hide llp
	    }
	}
    } else {
	set itk_option(-multi_pane) 1
	set priv_multi_pane 1

	switch $itk_option(-pane) {
	    ul {
		iwidgets::Panedwindow::show lower
		$itk_component(upw) show urp
	    }
	    ur {
		iwidgets::Panedwindow::show lower
		$itk_component(upw) show ulp
	    }
	    ll {
		iwidgets::Panedwindow::show upper
		$itk_component(lpw) show lrp
	    }
	    lr {
		iwidgets::Panedwindow::show upper
		$itk_component(lpw) show llp
	    }
	}
    }
}

::itcl::body QuadDisplay::resetBindings {} {
    $itk_component($itk_option(-pane)) resetBindings
}

::itcl::body QuadDisplay::resetBindingsAll {} {
    $itk_component(ul) resetBindings
    $itk_component(ur) resetBindings
    $itk_component(ll) resetBindings
    $itk_component(lr) resetBindings
}

::itcl::body QuadDisplay::? {} {
    return [$itk_component(ur) ?]
}

::itcl::body QuadDisplay::apropos {key} {
    return [$itk_component(ur) apropos $key]
}

::itcl::body QuadDisplay::getUserCmds {} {
    return [$itk_component(ur) getUserCmds]
}

::itcl::body QuadDisplay::help {args} {
    return [eval $itk_component(ur) help $args]
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
