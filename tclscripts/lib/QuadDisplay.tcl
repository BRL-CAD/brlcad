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
#       between showing all four Display objects or just the current Display object.
#
class QuadDisplay {
    inherit iwidgets::Panedwindow

    constructor {type args} {}
    destructor {}

    itk_option define -pane pane Pane ur
    itk_option define -multi_pane multi_pane Multi_pane 1
    itk_option define -margin margin Margin 0

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
    public method fb_active {args}
    public method fb_update {args}
    public method rt {args}
    public method rtcheck {args}

    public method dm_size {args}
    public method dm_name {}
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

    private variable initializing 1
    private variable priv_multi_pane 0
    private variable upper ""
    private variable lower ""
    private variable ul ""
    private variable ur ""
    private variable ll ""
    private variable lr ""
}

body QuadDisplay::constructor {type args} {
    Panedwindow::add upper
    Panedwindow::add lower

    set upper [childsite upper].pane
    set lower [childsite lower].pane

    # create two more panedwindows
    panedwindow $upper -orient vertical
    panedwindow $lower -orient vertical

    $upper Panedwindow::add left
    $upper Panedwindow::add right
    $lower Panedwindow::add left
    $lower Panedwindow::add right

    set ul [$upper childsite left].ul
    set ur [$upper childsite right].ur
    set ll [$lower childsite left].ll
    set lr [$lower childsite right].lr

    # create four instances of Display
    Display $ul $type
    Display $ur $type
    Display $ll $type
    Display $lr $type

    # initialize the views
    default_views

    pack $ul -fill both -expand yes
    pack $ur -fill both -expand yes
    pack $ll -fill both -expand yes
    pack $lr -fill both -expand yes

    pack $upper -fill both -expand yes
    pack $lower -fill both -expand yes

    set initializing 0
    eval itk_initialize $args

    # set the margins
    paneconfigure 0 -margin $itk_option(-margin)
    paneconfigure 1 -margin $itk_option(-margin)
    margin $itk_option(-margin)
}

body QuadDisplay::destructor {} {
    ::delete object $ul
    ::delete object $ur
    ::delete object $ll
    ::delete object $lr

    ::delete object $upper
    ::delete object $lower
}

configbody QuadDisplay::pane {
    pane $itk_option(-pane)
}

configbody QuadDisplay::multi_pane {
    if {$initializing} {
	set priv_multi_pane $itk_option(-multi_pane)
    } else {
	multi_pane $itk_option(-multi_pane)
    }
}

configbody QuadDisplay::margin {
    margin $itk_option(-margin)
}

body QuadDisplay::pane {args} {
    # get the active pane
    if {$args == ""} {
	return $itk_option(-pane)
    }

    set prev_pane $itk_option(-pane)

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

    if {!$initializing && !$itk_option(-multi_pane)} {
	# nothing to do
	if {$prev_pane == $itk_option(-pane)} {
	    return
	}

	switch $prev_pane {
	    ul {
		switch $itk_option(-pane) {
		    ur {
			$upper hide left
			$upper show right
		    }
		    ll {
			hide upper
			$upper show right
			show lower
			$lower show left
			$lower hide right
		    }
		    lr {
			hide upper
			$upper show right
			show lower
			$lower hide left
			$lower show right
		    }
		}
	    }
	    ur {
		switch $itk_option(-pane) {
		    ul {
			$upper hide right
			$upper show left
		    }
		    ll {
			hide upper
			$upper show left
			show lower
			$lower show left
			$lower hide right
		    }
		    lr {
			hide upper
			$upper show left
			show lower
			$lower hide left
			$lower show right
		    }
		}
	    }
	    ll {
		switch $itk_option(-pane) {
		    ul {
			hide lower
			$lower show right
			show upper
			$upper hide right
			$upper show left
		    }
		    ur {
			hide lower
			$lower show right
			show upper
			$upper hide left
			$upper show right
		    }
		    lr {
			$lower hide left
			$lower show right
		    }
		}
	    }
	    lr {
		switch $itk_option(-pane) {
		    ul {
			hide lower
			$lower show left
			show upper
			$upper hide right
			$upper show left
		    }
		    ur {
			hide lower
			$lower show left
			show upper
			$upper hide left
			$upper show right
		    }
		    ll {
			$lower hide right
			$lower show left
		    }
		}
	    }
	}
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
		$upper hide right
	    }
	    ur {
		hide lower
		$upper hide left
	    }
	    ll {
		hide upper
		$lower hide right
	    }
	    lr {
		hide upper
		$lower hide left
	    }
	}
    } else {
	set itk_option(-multi_pane) 1
	set priv_multi_pane 1

	switch $itk_option(-pane) {
	    ul {
		show lower
		$upper show right
	    }
	    ur {
		show lower
		$upper show left
	    }
	    ll {
		show upper
		$lower show right
	    }
	    lr {
		show upper
		$lower show left
	    }
	}
    }
}

body QuadDisplay::refresh {} {
    [subst $[subst $itk_option(-pane)]] refresh
}

body QuadDisplay::refreshall {} {
    $ul refresh
    $ur refresh
    $ll refresh
    $lr refresh
}

body QuadDisplay::aet {args} {
    eval [subst $[subst $itk_option(-pane)]] aet $args
}

body QuadDisplay::center {args} {
    eval [subst $[subst $itk_option(-pane)]] center $args
}

body QuadDisplay::rot {args} {
    eval [subst $[subst $itk_option(-pane)]] rot $args
}

body QuadDisplay::slew {args} {
    eval [subst $[subst $itk_option(-pane)]] slew $args
}

body QuadDisplay::tra {args} {
    eval [subst $[subst $itk_option(-pane)]] tra $args
}

body QuadDisplay::size {args} {
    eval [subst $[subst $itk_option(-pane)]] size $args
}

body QuadDisplay::scale {args} {
    eval [subst $[subst $itk_option(-pane)]] scale $args
}

body QuadDisplay::zoom {args} {
    eval [subst $[subst $itk_option(-pane)]] zoom $args
}

body QuadDisplay::autoview {} {
    [subst $[subst $itk_option(-pane)]] autoview
}

body QuadDisplay::autoviewall {} {
    $ul autoview
    $ur autoview
    $ll autoview
    $lr autoview
}

body QuadDisplay::add {glist} {
    [subst $[subst $itk_option(-pane)]] add $glist
}

body QuadDisplay::addall {glist} {
    $ul add $glist
    $ur add $glist
    $ll add $glist
    $lr add $glist
}

body QuadDisplay::remove {glist} {
    [subst $[subst $itk_option(-pane)]] remove $glist
}

body QuadDisplay::removeall {glist} {
    $ul remove $glist
    $ur remove $glist
    $ll remove $glist
    $lr remove $glist
}

body QuadDisplay::contents {} {
    [subst $[subst $itk_option(-pane)]] contents
}

body QuadDisplay::listen {args} {
    eval [subst $[subst $itk_option(-pane)]] listen $args
}

body QuadDisplay::fb_active {args} {
    eval eval [subst $[subst $itk_option(-pane)]] fb_active $args
}

body QuadDisplay::fb_update {args} {
    eval [subst $[subst $itk_option(-pane)]] fb_update $args
}

body QuadDisplay::rt {args} {
    eval [subst $[subst $itk_option(-pane)]] rt $args
}

body QuadDisplay::rtcheck {args} {
    eval [subst $[subst $itk_option(-pane)]] rtcheck $args
}

body QuadDisplay::dm_size {args} {
    eval [subst $[subst $itk_option(-pane)]] Dm::size $args
}

body QuadDisplay::dm_name {} {
    eval [subst $[subst $itk_option(-pane)]] Dm::get_name
}

body QuadDisplay::margin {margin} {
    $upper paneconfigure 0 -margin $margin
    $upper paneconfigure 1 -margin $margin
    $lower paneconfigure 0 -margin $margin
    $lower paneconfigure 1 -margin $margin
}

body QuadDisplay::resetall {} {
    reset
    $upper reset
    $lower reset
}

body QuadDisplay::default_views {} {
    $ul aet 0 90 0
    $ur aet 35 25 0
    $ll aet 0 0 0
    $lr aet 90 0 0
}

body QuadDisplay::attach_view {} {
    [subst $[subst $itk_option(-pane)]] attach_view
}

body QuadDisplay::attach_viewall {} {
    $ul attach_view
    $ur attach_view
    $ll attach_view
    $lr attach_view
}

body QuadDisplay::attach_drawable {dg} {
    [subst $[subst $itk_option(-pane)]] attach_drawable $dg
}

body QuadDisplay::attach_drawableall {dg} {
    $ul attach_drawable $dg
    $ur attach_drawable $dg
    $ll attach_drawable $dg
    $lr attach_drawable $dg
}

body QuadDisplay::detach_view {} {
    [subst $[subst $itk_option(-pane)]] detach_view
}

body QuadDisplay::detach_viewall {} {
    $ul detach_view
    $ur detach_view
    $ll detach_view
    $lr detach_view
}

body QuadDisplay::detach_drawable {dg} {
    [subst $[subst $itk_option(-pane)]] detach_drawable $dg
}

body QuadDisplay::detach_drawableall {dg} {
    $ul detach_drawable $dg
    $ur detach_drawable $dg
    $ll detach_drawable $dg
    $lr detach_drawable $dg
}
