##                 M U L T I D I S P L A Y . T C L
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
#	The MultiDisplay class is comprised of four Display objects. 
#
class MultiDisplay {
    protected variable ul ""
    protected variable ur ""
    protected variable ll ""
    protected variable lr ""
    public variable pane ur
    public variable parent ""
    private variable destroy_parent 0
    public variable multi_mode 1
    public variable size ""
    private variable width ""
    private variable height ""
    private variable dm_big_size ""
    private variable dm_small_size ""
    private variable initializing 1

    constructor {args} {
	# process options
	eval configure $args

	# create default parent
	if {$parent == ""} {
	    set parent .[string map {:: ""} $this]
	}

	if ![winfo exists $parent] {
	    toplevel $parent
	    set destroy_parent 1
	}

	if {$size == ""} {
	    set size [winfo screenheight $parent]
	    set width $size
	    set height $size
	}

	# create frames
	frame $parent.ulf -bd 1 -relief sunken
	frame $parent.urf -bd 1 -relief sunken
	frame $parent.llf -bd 1 -relief sunken
	frame $parent.lrf -bd 1 -relief sunken

	set dm_big_size "[expr $width - 2] [expr $height - 2]"
	set dm_small_size "[expr $width / 2 - 2] [expr $height / 2 - 2]"

	if {$multi_mode} {
	    set dmsize $dm_small_size
	} else {
	    set dmsize $dm_big_size
	}

	# create Display objects
	set ul [Display #auto -type ogl -dm_size $dmsize -dm_name $parent.ul -istoplevel 0]
	set ur [Display #auto -type ogl -dm_size $dmsize -dm_name $parent.ur -istoplevel 0]
	set ll [Display #auto -type ogl -dm_size $dmsize -dm_name $parent.ll -istoplevel 0]
	set lr [Display #auto -type ogl -dm_size $dmsize -dm_name $parent.lr -istoplevel 0]

	# put each display manager window in a frame 
	grid $parent.ul -in $parent.ulf -sticky nsew
	grid $parent.ur -in $parent.urf -sticky nsew
	grid $parent.ll -in $parent.llf -sticky nsew
	grid $parent.lr -in $parent.lrf -sticky nsew
	grid rowconfigure $parent.ulf 0 -weight 1
	grid rowconfigure $parent.urf 0 -weight 1
	grid rowconfigure $parent.llf 0 -weight 1
	grid rowconfigure $parent.lrf 0 -weight 1
	grid columnconfigure $parent.ulf 0 -weight 1
	grid columnconfigure $parent.urf 0 -weight 1
	grid columnconfigure $parent.llf 0 -weight 1
	grid columnconfigure $parent.lrf 0 -weight 1

	if {$multi_mode} {
	    grid $parent.ulf -row 0 -col 0 -sticky nsew
	    grid $parent.urf -row 0 -col 1 -sticky nsew
	    grid $parent.llf -row 1 -col 0 -sticky nsew
	    grid $parent.lrf -row 1 -col 1 -sticky nsew
	} else {
	    grid $parent.[subst $pane]f -row 0 -col 0 -sticky nsew
	}

	grid rowconfigure $parent 0 -weight 1
	grid rowconfigure $parent 1 -weight 1
	grid columnconfigure $parent 0 -weight 1
	grid columnconfigure $parent 1 -weight 1

	# initialize the views
	$ul aet 0 90 0
	$ur aet 35 25 0
	$ll aet 0 0 0
	$lr aet 90 0 0

	# event bindings
	doBindings

	set initializing 0
    }

    destructor {
	delete object $ul
	delete object $ur
	delete object $ll
	delete object $lr

	if {$destroy_parent} {
	    destroy $parent
	}
    }

    method pane {args}
    method multi_mode {val}
    method toggle_multi_mode {}

    # methods for controlling the view object
    method aet {args}
    method center {args}
    method rot {args}
    method slew {args}
    method tra {args}
    method size {args}
    method scale {args}
    method zoom {sf}
    method autoview {}

    method add {glist}
    method remove {glist}
    method contents {}

    method listen {args}
    method fb_active {args}
    method fb_update {args}
    method rt {args}

    method dm_size {args}
    method dm_name {}

    private method doBindings {}
    private method handle_configure {}
}

configbody MultiDisplay::pane {
    pane $pane
}

configbody MultiDisplay::parent {
    if {!$initializing} {
	return -code error "parent is read-only"
    }
}

configbody MultiDisplay::multi_mode {
    multi_mode $multi_mode
}

configbody MultiDisplay::size {
    if {!$initializing} {
	return -code error "size is read-only"
    }

    set slen [llength $size]
    if {$slen == 1} {
	set width $size
	set height $size
    } elseif {$slen == 2} {
	set width [lindex $size 0]
	set height [lindex $size 1]
    }
}

body MultiDisplay::pane {args} {
    set len [llength $args]

    # get the active pane
    if {$len == 0} {
	return $pane
    }

    if {$len != 1} {
	return -code error "pane: too many arguments"
    }

    # set the active pane
    switch $args {
	ul -
	ur -
	ll -
	lr {
	    set pane $args
	}
	default {
	    return -code error "pane: bad value - $args"
	}
    }
}

body MultiDisplay::multi_mode {val} {
    # nothing to do
    if {$val == $multi_mode} {
	return
    }

    switch $val {
	0 -
	1 {
	    toggle_multi_mode
	}
	default {
	    return -code error "mult_mode: bad value - $val"
	}
    }
}

body MultiDisplay::toggle_multi_mode {} {
    if {$multi_mode} {
	set multi_mode 0
	grid forget $parent.ulf $parent.urf $parent.llf $parent.lrf
	eval [subst $[subst $pane]] dm_size $dm_big_size

	# reduces flashing by resizing before being packed
	update

	grid $parent.[subst $pane]f -row 0 -col 0 -sticky nsew
    } else {
	set multi_mode 1
	grid forget $parent.[subst $pane]f
	eval $ul dm_size $dm_small_size
	eval $ur dm_size $dm_small_size
	eval $ll dm_size $dm_small_size
	eval $lr dm_size $dm_small_size

	# reduces flashing by resizing before being packed
	update

	grid $parent.ulf -row 0 -col 0 -sticky nsew
	grid $parent.urf -row 0 -col 1 -sticky nsew
	grid $parent.llf -row 1 -col 0 -sticky nsew
	grid $parent.lrf -row 1 -col 1 -sticky nsew
    }
}

body MultiDisplay::aet {args} {
    eval [subst $[subst $pane]] aet $args
}

body MultiDisplay::center {args} {
    eval [subst $[subst $pane]] center $args
}

body MultiDisplay::rot {args} {
    eval [subst $[subst $pane]] rot $args
}

body MultiDisplay::slew {args} {
    eval [subst $[subst $pane]] slew $args
}

body MultiDisplay::tra {args} {
    eval [subst $[subst $pane]] tra $args
}

body MultiDisplay::size {args} {
    eval [subst $[subst $pane]] size $args
}

body MultiDisplay::scale {args} {
    eval [subst $[subst $pane]] scale $args
}

body MultiDisplay::zoom {args} {
    eval [subst $[subst $pane]] zoom $args
}

body MultiDisplay::autoview {} {
    [subst $[subst $pane]] autoview
}

body MultiDisplay::add {glist} {
    [subst $[subst $pane]] add $glist
}

body MultiDisplay::remove {glist} {
    [subst $[subst $pane]] remove $glist
}

body MultiDisplay::contents {} {
    [subst $[subst $pane]] contents
}

body MultiDisplay::listen {args} {
    eval [subst $[subst $pane]] listen $args
}

body MultiDisplay::fb_active {args} {
    eval eval [subst $[subst $pane]] fb_active $args
}

body MultiDisplay::fb_update {args} {
    eval [subst $[subst $pane]] fb_update $args
}

body MultiDisplay::rt {args} {
    eval [subst $[subst $pane]] rt $args
}

body MultiDisplay::dm_size {args} {
    eval [subst $[subst $pane]] dm_size $args
}

body MultiDisplay::dm_name {} {
    eval [subst $[subst $pane]] dm_name
}

body MultiDisplay::doBindings {} {
    bind $parent <Configure> [code $this handle_configure]
}

body MultiDisplay::handle_configure {} {
    set width [winfo width $parent]
    set height [winfo height $parent]
    set size "$width $height"
    set dm_big_size "[expr $width - 2] [expr $height - 2]"
    set dm_small_size "[expr $width / 2 - 2] [expr $height / 2 - 2]"
}
