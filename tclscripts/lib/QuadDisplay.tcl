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
class QuadDisplay {
    inherit iwidgets::Panedwindow

    itk_option define -pane pane Pane ur
    itk_option define -multi_pane multi_pane Multi_pane 1
    itk_option define -margin margin Margin 0

    constructor {{type X} args} {}
    destructor {}

    public method pane {args}
    public method multi_pane {args}
    public method toggle_multi_pane {}
    public method refresh {}
    public method refreshall {}

    # methods for controlling the view object
    public method aet {args}
    public method center {args}
    public method rot {args}
    public method slew {args}
    public method tra {args}
    public method size {args}
    public method scale {args}
    public method zoom {sf}
    public method autoview {}
    public method autoviewall {}

    public method add {glist}
    public method addall {glist}
    public method remove {glist}
    public method removeall {glist}
    public method contents {}

    public method listen {args}
    public method linewidth {args}
    public method linestyle {args}
    public method zclip {args}
    public method zbuffer {args}
    public method light {args}
    public method perspective {args}
    public method bg {args}
    public method fb_active {args}
    public method fb_update {args}
    public method rt {args}
    public method rtcheck {args}
    public method get_dm_name {}

    public method margin {val}
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

    private variable priv_pane ur
    private variable priv_multi_pane 1
}

body QuadDisplay::constructor {{type X} args} {
    Panedwindow::add upper
    Panedwindow::add lower

    # create two more panedwindows
    itk_component add upperpane {
	panedwindow [childsite upper].pane -orient vertical
    } {
	usual
	keep -sashwidth -sashheight -sashborderwidth
	keep -sashindent -thickness
    }

    itk_component add lowerpane {
	panedwindow [childsite lower].pane -orient vertical
    } {
	usual
	keep -sashwidth -sashheight -sashborderwidth
	keep -sashindent -thickness
    }

    $itk_component(upperpane) add left
    $itk_component(upperpane) add right
    $itk_component(lowerpane) add left
    $itk_component(lowerpane) add right

    # create four instances of Display
    itk_component add ul {
	Display [$itk_component(upperpane) childsite left].ul $type
    } {
	keep -rscale -sscale -type
    }

    itk_component add ur {
	Display [$itk_component(upperpane) childsite right].ur $type
    } {
	keep -rscale -sscale -type
    }

    itk_component add ll {
	Display [$itk_component(lowerpane) childsite left].ll $type
    } {
	keep -rscale -sscale -type
    }

    itk_component add lr {
	Display [$itk_component(lowerpane) childsite right].lr $type
    } {
	keep -rscale -sscale -type
    }

    # initialize the views
    default_views

    pack $itk_component(ul) -fill both -expand yes
    pack $itk_component(ur) -fill both -expand yes
    pack $itk_component(ll) -fill both -expand yes
    pack $itk_component(lr) -fill both -expand yes

    pack $itk_component(upperpane) -fill both -expand yes
    pack $itk_component(lowerpane) -fill both -expand yes

    eval itk_initialize $args

    # set the margins
    margin $itk_option(-margin)
}

body QuadDisplay::destructor {} {
    ::delete object $itk_component(ul)
    ::delete object $itk_component(ur)
    ::delete object $itk_component(ll)
    ::delete object $itk_component(lr)

    ::delete object $itk_component(upperpane)
    ::delete object $itk_component(lowerpane)
}

configbody QuadDisplay::pane {
    pane $itk_option(-pane)
}

configbody QuadDisplay::multi_pane {
    multi_pane $itk_option(-multi_pane)
}

configbody QuadDisplay::margin {
    margin $itk_option(-margin)
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
			$itk_component(upperpane) hide left
			$itk_component(upperpane) show right
		    }
		    ll {
			hide upper
			$itk_component(upperpane) show right
			show lower
			$itk_component(lowerpane) show left
			$itk_component(lowerpane) hide right
		    }
		    lr {
			hide upper
			$itk_component(upperpane) show right
			show lower
			$itk_component(lowerpane) hide left
			$itk_component(lowerpane) show right
		    }
		}
	    }
	    ur {
		switch $itk_option(-pane) {
		    ul {
			$itk_component(upperpane) hide right
			$itk_component(upperpane) show left
		    }
		    ll {
			hide upper
			$itk_component(upperpane) show left
			show lower
			$itk_component(lowerpane) show left
			$itk_component(lowerpane) hide right
		    }
		    lr {
			hide upper
			$itk_component(upperpane) show left
			show lower
			$itk_component(lowerpane) hide left
			$itk_component(lowerpane) show right
		    }
		}
	    }
	    ll {
		switch $itk_option(-pane) {
		    ul {
			hide lower
			$itk_component(lowerpane) show right
			show upper
			$itk_component(upperpane) hide right
			$itk_component(upperpane) show left
		    }
		    ur {
			hide lower
			$itk_component(lowerpane) show right
			show upper
			$itk_component(upperpane) hide left
			$itk_component(upperpane) show right
		    }
		    lr {
			$itk_component(lowerpane) hide left
			$itk_component(lowerpane) show right
		    }
		}
	    }
	    lr {
		switch $itk_option(-pane) {
		    ul {
			hide lower
			$itk_component(lowerpane) show left
			show upper
			$itk_component(upperpane) hide right
			$itk_component(upperpane) show left
		    }
		    ur {
			hide lower
			$itk_component(lowerpane) show left
			show upper
			$itk_component(upperpane) hide left
			$itk_component(upperpane) show right
		    }
		    ll {
			$itk_component(lowerpane) hide right
			$itk_component(lowerpane) show left
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

body QuadDisplay::toggle_multi_pane {} {
    if {$priv_multi_pane} {
	set itk_option(-multi_pane) 0
	set priv_multi_pane 0

	switch $itk_option(-pane) {
	    ul {
		hide lower
		$itk_component(upperpane) hide right
	    }
	    ur {
		hide lower
		$itk_component(upperpane) hide left
	    }
	    ll {
		hide upper
		$itk_component(lowerpane) hide right
	    }
	    lr {
		hide upper
		$itk_component(lowerpane) hide left
	    }
	}
    } else {
	set itk_option(-multi_pane) 1
	set priv_multi_pane 1

	switch $itk_option(-pane) {
	    ul {
		show lower
		$itk_component(upperpane) show right
	    }
	    ur {
		show lower
		$itk_component(upperpane) show left
	    }
	    ll {
		show upper
		$itk_component(lowerpane) show right
	    }
	    lr {
		show upper
		$itk_component(lowerpane) show left
	    }
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

body QuadDisplay::rot {args} {
    eval $itk_component($itk_option(-pane)) rot $args
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

body QuadDisplay::scale {args} {
    eval $itk_component($itk_option(-pane)) scale $args
}

body QuadDisplay::zoom {args} {
    eval $itk_component($itk_option(-pane)) zoom $args
}

body QuadDisplay::autoview {} {
    $itk_component($itk_option(-pane)) autoview
}

body QuadDisplay::autoviewall {} {
    $itk_component(ul) autoview
    $itk_component(ur) autoview
    $itk_component(ll) autoview
    $itk_component(lr) autoview
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

body QuadDisplay::fb_update {args} {
    eval $itk_component($itk_option(-pane)) fb_update $args
}

body QuadDisplay::rt {args} {
    eval $itk_component($itk_option(-pane)) rt $args
}

body QuadDisplay::rtcheck {args} {
    eval $itk_component($itk_option(-pane)) rtcheck $args
}

body QuadDisplay::get_dm_name {} {
    eval $itk_component($itk_option(-pane)) Dm::get_name
}

body QuadDisplay::margin {margin} {
    set itk_option(-margin) $margin
    paneconfigure 0 -margin $margin
    paneconfigure 1 -margin $margin
    $itk_component(upperpane) paneconfigure 0 -margin $margin
    $itk_component(upperpane) paneconfigure 1 -margin $margin
    $itk_component(lowerpane) paneconfigure 0 -margin $margin
    $itk_component(lowerpane) paneconfigure 1 -margin $margin
}

body QuadDisplay::resetall {} {
    reset
    $itk_component(upperpane) reset
    $itk_component(lowerpane) reset
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
