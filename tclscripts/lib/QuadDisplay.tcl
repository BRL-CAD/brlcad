##                 Q U A D D I S P L A Y . T C L
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
#	The QuadDisplay class is comprised of four Display objects. This class
#       keeps track of the current Display object and provides the means to toggle
#       between showing all four Display objects or just the current one.
#

option add *Pane*margin 0 widgetDefault
option add *QuadDisplay.width 400 widgetDefault
option add *QuadDisplay.height 400 widgetDefault

class QuadDisplay {
    inherit iwidgets::Panedwindow

    itk_option define -pane pane Pane ur
    itk_option define -multi_pane multi_pane Multi_pane 1

    constructor {args} {}
    destructor {}

    public method pane {args}
    public method multi_pane {args}
    public method refresh {}
    public method refreshall {}

    # methods for controlling the view object
    public method aet {args}
    public method center {args}
    public method coord {args}
    public method keypoint {args}
    public method rot {args}
    public method rotate_about {args}
    public method size {args}
    public method slew {args}
    public method tra {args}
    public method zoom {sf}

    public method autoview {{gindex 0}}
    public method autoviewall {{gindex 0}}

    public method add {glist}
    public method addall {glist}
    public method remove {glist}
    public method removeall {glist}
    public method contents {}

    public method bg {args}
    public method fb_active {args}
    public method fb_observe {args}
    public method light {args}
    public method linestyle {args}
    public method linewidth {args}
    public method listen {args}
    public method perspective {args}
    public method perspective_angle {args}
    public method rt {args}
    public method rtabort {{gi 0}}
    public method rtcheck {args}
    public method zbuffer {args}
    public method zclip {args}

    public method resetall {}
    public method default_views {}
    public method attach_view {}
    public method attach_viewall {}
    public method attach_drawable {dg}
    public method attach_drawableall {dg}
    public method detach_view {}
    public method detach_viewall {}
    public method detach_drawable {dg}
    public method detach_drawableall {dg}

    public method ? {}
    public method apropos {key}
    public method getUserCmds {}
    public method help {args}

    protected method toggle_multi_pane {}

    private variable priv_pane ur
    private variable priv_multi_pane 1
}

body QuadDisplay::constructor {args} {
    iwidgets::Panedwindow::add upper
    iwidgets::Panedwindow::add lower

    # create two more panedwindows
    itk_component add upw {
	::iwidgets::Panedwindow [childsite upper].pw -orient vertical
    } {
	usual
	keep -sashwidth -sashheight -sashborderwidth
	keep -sashindent -thickness
    }

    itk_component add lpw {
	::iwidgets::Panedwindow [childsite lower].pw -orient vertical
    } {
	usual
	keep -sashwidth -sashheight -sashborderwidth
	keep -sashindent -thickness
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

configbody QuadDisplay::pane {
    pane $itk_option(-pane)
}

configbody QuadDisplay::multi_pane {
    multi_pane $itk_option(-multi_pane)
}

body QuadDisplay::pane {args} {
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
			hide upper
			$itk_component(upw) show urp
			show lower
			$itk_component(lpw) show llp
			$itk_component(lpw) hide lrp
		    }
		    lr {
			hide upper
			$itk_component(upw) show urp
			show lower
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
			hide upper
			$itk_component(upw) show ulp
			show lower
			$itk_component(lpw) show llp
			$itk_component(lpw) hide lrp
		    }
		    lr {
			hide upper
			$itk_component(upw) show ulp
			show lower
			$itk_component(lpw) hide llp
			$itk_component(lpw) show lrp
		    }
		}
	    }
	    ll {
		switch $itk_option(-pane) {
		    ul {
			hide lower
			$itk_component(lpw) show lrp
			show upper
			$itk_component(upw) hide urp
			$itk_component(upw) show ulp
		    }
		    ur {
			hide lower
			$itk_component(lpw) show lrp
			show upper
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
			hide lower
			$itk_component(lpw) show llp
			show upper
			$itk_component(upw) hide urp
			$itk_component(upw) show ulp
		    }
		    ur {
			hide lower
			$itk_component(lpw) show llp
			show upper
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

	set priv_pane $itk_option(-pane)
    }
}

body QuadDisplay::multi_pane {args} {
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
	    return -code error "mult_mode: bad value - $args"
	}
    }
}

body QuadDisplay::refresh {} {
    $itk_component($itk_option(-pane)) refresh
}

body QuadDisplay::refreshall {} {
    $itk_component(ul) refresh
    $itk_component(ur) refresh
    $itk_component(ll) refresh
    $itk_component(lr) refresh
}

body QuadDisplay::aet {args} {
    eval $itk_component($itk_option(-pane)) aet $args
}

body QuadDisplay::center {args} {
    eval $itk_component($itk_option(-pane)) center $args
}

body QuadDisplay::coord {args} {
    eval $itk_component($itk_option(-pane)) coord $args
}

body QuadDisplay::keypoint {args} {
    eval $itk_component($itk_option(-pane)) keypoint $args
}

body QuadDisplay::rot {args} {
    eval $itk_component($itk_option(-pane)) rot $args
}

body QuadDisplay::rotate_about {args} {
    eval $itk_component($itk_option(-pane)) rotate_about $args
}

body QuadDisplay::slew {args} {
    eval $itk_component($itk_option(-pane)) slew $args
}

body QuadDisplay::tra {args} {
    eval $itk_component($itk_option(-pane)) tra $args
}

body QuadDisplay::size {args} {
    eval $itk_component($itk_option(-pane)) size $args
}

body QuadDisplay::zoom {args} {
    eval $itk_component($itk_option(-pane)) zoom $args
}

body QuadDisplay::autoview {{gindex 0}} {
    $itk_component($itk_option(-pane)) autoview $gindex
}

body QuadDisplay::autoviewall {{gindex 0}} {
    $itk_component(ul) autoview $gindex
    $itk_component(ur) autoview $gindex
    $itk_component(ll) autoview $gindex
    $itk_component(lr) autoview $gindex
}

body QuadDisplay::add {glist} {
    $itk_component($itk_option(-pane)) add $glist
}

body QuadDisplay::addall {glist} {
    $itk_component(ul) add $glist
    $itk_component(ur) add $glist
    $itk_component(ll) add $glist
    $itk_component(lr) add $glist
}

body QuadDisplay::remove {glist} {
    $itk_component($itk_option(-pane)) remove $glist
}

body QuadDisplay::removeall {glist} {
    $itk_component(ul) remove $glist
    $itk_component(ur) remove $glist
    $itk_component(ll) remove $glist
    $itk_component(lr) remove $glist
}

body QuadDisplay::contents {} {
    $itk_component($itk_option(-pane)) contents
}

body QuadDisplay::listen {args} {
    eval $itk_component($itk_option(-pane)) listen $args
}

body QuadDisplay::linewidth {args} {
    set result [eval $itk_component($itk_option(-pane)) linewidth $args]
    if {$args != ""} {
	refresh
	return
    }
    return $result
}

body QuadDisplay::linestyle {args} {
    eval $itk_component($itk_option(-pane)) linestyle $args
}

body QuadDisplay::zclip {args} {
    eval $itk_component($itk_option(-pane)) zclip $args
}

body QuadDisplay::zbuffer {args} {
    eval $itk_component($itk_option(-pane)) zbuffer $args
}

body QuadDisplay::light {args} {
    eval $itk_component($itk_option(-pane)) light $args
}

body QuadDisplay::perspective {args} {
    eval $itk_component($itk_option(-pane)) perspective $args
}

body QuadDisplay::perspective_angle {args} {
    eval $itk_component($itk_option(-pane)) perspective_angle $args
}

body QuadDisplay::bg {args} {
    set result [eval $itk_component($itk_option(-pane)) bg $args]
    if {$args != ""} {
	refresh
	return
    }
    return $result
}

body QuadDisplay::fb_active {args} {
    eval $itk_component($itk_option(-pane)) fb_active $args
}

body QuadDisplay::fb_observe {args} {
    eval $itk_component($itk_option(-pane)) fb_observe $args
}

body QuadDisplay::rt {args} {
    eval $itk_component($itk_option(-pane)) rt $args
}

body QuadDisplay::rtabort {{gi 0}} {
    $itk_component($itk_option(-pane)) rtabort $gi
}

body QuadDisplay::rtcheck {args} {
    eval $itk_component($itk_option(-pane)) rtcheck $args
}

body QuadDisplay::resetall {} {
    reset
    $itk_component(upw) reset
    $itk_component(lpw) reset
}

body QuadDisplay::default_views {} {
    $itk_component(ul) aet 0 90 0
    $itk_component(ur) aet 35 25 0
    $itk_component(ll) aet 0 0 0
    $itk_component(lr) aet 90 0 0
}

body QuadDisplay::attach_view {} {
    $itk_component($itk_option(-pane)) attach_view
}

body QuadDisplay::attach_viewall {} {
    $itk_component(ul) attach_view
    $itk_component(ur) attach_view
    $itk_component(ll) attach_view
    $itk_component(lr) attach_view
}

body QuadDisplay::attach_drawable {dg} {
    $itk_component($itk_option(-pane)) attach_drawable $dg
}

body QuadDisplay::attach_drawableall {dg} {
    $itk_component(ul) attach_drawable $dg
    $itk_component(ur) attach_drawable $dg
    $itk_component(ll) attach_drawable $dg
    $itk_component(lr) attach_drawable $dg
}

body QuadDisplay::detach_view {} {
    $itk_component($itk_option(-pane)) detach_view
}

body QuadDisplay::detach_viewall {} {
    $itk_component(ul) detach_view
    $itk_component(ur) detach_view
    $itk_component(ll) detach_view
    $itk_component(lr) detach_view
}

body QuadDisplay::detach_drawable {dg} {
    $itk_component($itk_option(-pane)) detach_drawable $dg
}

body QuadDisplay::detach_drawableall {dg} {
    $itk_component(ul) detach_drawable $dg
    $itk_component(ur) detach_drawable $dg
    $itk_component(ll) detach_drawable $dg
    $itk_component(lr) detach_drawable $dg
}

body QuadDisplay::toggle_multi_pane {} {
    if {$priv_multi_pane} {
	set itk_option(-multi_pane) 0
	set priv_multi_pane 0

	switch $itk_option(-pane) {
	    ul {
		hide lower
		$itk_component(upw) hide urp
	    }
	    ur {
		hide lower
		$itk_component(upw) hide ulp
	    }
	    ll {
		hide upper
		$itk_component(lpw) hide lrp
	    }
	    lr {
		hide upper
		$itk_component(lpw) hide llp
	    }
	}
    } else {
	set itk_option(-multi_pane) 1
	set priv_multi_pane 1

	switch $itk_option(-pane) {
	    ul {
		show lower
		$itk_component(upw) show urp
	    }
	    ur {
		show lower
		$itk_component(upw) show ulp
	    }
	    ll {
		show upper
		$itk_component(lpw) show lrp
	    }
	    lr {
		show upper
		$itk_component(lpw) show llp
	    }
	}
    }
}

body QuadDisplay::? {} {
    return [$itk_component(ur) ?]
}

body QuadDisplay::apropos {key} {
    return [$itk_component(ur) apropos $key]
}

body QuadDisplay::getUserCmds {} {
    return [$itk_component(ur) getUserCmds]
}

body QuadDisplay::help {args} {
    return [eval $itk_component(ur) help $args]
}

