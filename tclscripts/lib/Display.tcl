##                 D I S P L A Y . T C L
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
#	The Display class is comprised of a display manager object and
#       a view object. This class is capable of displaying one or more drawable
#       geometry objects.
#
class Display {
    private common dlist ""
    private variable dm ""
    private variable view ""
    private variable x ""
    private variable y ""
    private variable invWidth ""
    private variable aspect ""
    private variable initializing 1
    private variable width 512
    private variable height 512

    public variable dm_name ""
    public variable istoplevel 1
    public variable type X
    public variable dm_size 512
    public variable size 2000
    public variable rscale 0.4
    public variable sscale 2.0
    public variable title ""
    public variable geolist ""
    public variable listen -1
    public variable fb_active 0
    public variable fb_update 1

    constructor {args} {
	# process options
	eval configure $args

	# create default dm_name
	if {$dm_name == ""} {
	    set dm_name .[string map {:: ""} $this]
	}

	# create display manager object
	set dm [dm_open $dm_name $type -t $istoplevel -W $width -N $height]

	# create view object
	set view [v_open $this\_view]
	$view center {0 0 0}
	$view scale [expr 0.5 * $size]

	# initialize display manager object's view
	$dm clear
	$dm loadmat [$view model2view] 0

	# event bindings
	doBindings

	if {$istoplevel} {
	    wm title $dm $title
	}

	if {$geolist != ""} {
	    autoview
	}

	if {$listen != -1} {
	    listen $listen
	}

	# append to list of Display objects
	lappend dlist $this

	set initializing 0
    }

    destructor {
	if {$listen >= 0} {
	    $dm listen -1
	}
	$dm close
	$view close

	# remove from list of Display objects
	set index [lsearch $dlist $this]
	set dlist [lreplace $dlist $index $index]
    }

    method refresh {}
    method clear {}

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

    # methods for maintaining the list of geometry objects
    method add {glist}
    method remove {glist}
    method contents {}

    method listen {args}
    method fb_active {args}
    method fb_update {args}
    method rt {args}

    private method toggle_zclip {}
    private method toggle_zbuffer {}
    private method toggle_light {}
    private method idle_mode {}
    private method rotate_mode {_x _y}
    private method translate_mode {_x _y}
    private method scale_mode {_x _y}
    private method constrain_rmode {coord _x _y}
    private method constrain_tmode {coord _x _y}
    private method handle_rotation {_x _y}
    private method handle_translation {_x _y}
    private method handle_scale {_x _y}
    private method handle_constrain_rot {coord _x _y}
    private method handle_constrain_tran {coord _x _y}
    private method handle_configure {}
    private method handle_expose {}
    private method doBindings {}

    method dm_size {args}
    method dm_name {}
}

configbody Display::dm_name {
    if {!$initializing} {
	return -code error "dm_name is read-only"
    }
}

configbody Display::dm_size {
    # save dm_size
    set s $dm_size

    # For now, put back the old value.
    # If the size really does change, dm_size will
    # be set in the handle_configure method.
    set dm_size "$width $height"

    # request a size change
    eval dm_size $s
}

configbody Display::title {
    if {$istoplevel && [winfo exists $dm]} {
	wm title $dm $title
    }
}

configbody Display::istoplevel {
    if {!$initializing} {
	return -code error "istoplevel is read-only"
    }
}

configbody Display::type {
    if {!$initializing} {
	return -code error "type is read-only"
    }
}

configbody Display::geolist {
    if {!$initializing} {
	autoview
    }
}

configbody Display::listen {
    if {!$initializing} {
	listen $listen
    }
}

configbody Display::fb_active {
    fb_active $fb_active
}

configbody Display::fb_update {
    fb_update $fb_update
}

configbody Display::size {
    if {!$initializing} {
	size $size
    }
}

body Display::refresh {} {
    $dm drawBegin
    $dm loadmat [$view model2view] 0
    if {$fb_active < 2} {
	if {$fb_active} {
	    # underlay
	    $dm refreshfb
	}

	foreach geo $geolist {
	    $dm drawGeom $geo
	}
    } else {
	# overlay
	$dm refreshfb
    }
    $dm drawEnd
}

# Clear the display manager window
body Display::clear {} {
    $dm clear
}

body Display::aet {args} {
    set len [llength $args]

    # get aet
    if {$len == 0} {
	return [$view aet]
    }

    # set aet
    $view aet $args
    refresh
}

body Display::center {args} {
    set len [llength $args]

    # get center
    if {$len == 0} {
	return [$view center]
    }

    # set center
    $view center $args
    refresh
}

body Display::rot {args} {
    # rotate view
    $view rot $args
    refresh
}

body Display::slew {args} {
    if {[llength $args] != 2} {
	return -code error "Display::slew - need two numbers"
    }

    set x2 [expr $width * 0.5]
    set y2 [expr $height * 0.5]
    set sf [expr 2.0 * $invWidth]

    set _x [expr ([lindex $args 0] - $x2) * $sf]
    set _y [expr (-1.0 * [lindex $args 1] + $y2) * $sf]

    $view slew "$_x $_y"
    refresh
}

body Display::tra {args} {
    # translate view
    $view tra $args
    refresh
}

body Display::size {args} {
    set len [llength $args]

    # get size
    if {$len == 0} {
	return [$view size]
    }

    # set size
    $view size $args

    set size [$view size]
    refresh
}

body Display::scale {args} {
    set len [llength $args]

    # get scale
    if {$len == 0} {
	return [$view scale]
    }

    # set scale
    $view scale $args

    set size [$view size]
    refresh
}

body Display::zoom {sf} {
    $view zoom $sf
    set size [$view size]
    refresh
}

body Display::autoview {} {
    if [llength $geolist] {
	set geo [lindex $geolist 0]
	set aview [$geo get_autoview]
	eval $view [lrange $aview 0 1]
	eval $view [lrange $aview 2 3]
	set size [$view size]
	refresh
    }
}

body Display::add {glist} {
    if [llength $geolist] {
	set empty_screen 0
    } else {
	set empty_screen 1
    }
    eval lappend geolist $glist

    if $empty_screen {
	autoview
    } else {
	refresh
    }
}

body Display::remove {glist} {
    foreach geo $glist {
	set index [lsearch $geolist $geo]
	if {$index == -1} {
	    continue
	}

	set geolist [lreplace $geolist $index $index]
    }
    refresh
}

body Display::contents {} {
    return $geolist
}

body Display::toggle_zclip {} {
    set zclip [$dm zclip]

    if $zclip {
	$dm zclip 0
    } else {
	$dm zclip 1
    }

    refresh
}

body Display::toggle_zbuffer {} {
    set zbuffer [$dm zbuffer]

    if $zbuffer {
	$dm zbuffer 0
    } else {
	$dm zbuffer 1
    }

    refresh
}

body Display::toggle_light {} {
    set light [$dm light]

    if $light {
	$dm light 0
    } else {
	$dm light 1
    }

    refresh
}

body Display::idle_mode {} {
    # stop receiving motion events
    bind $dm <Motion> {}
}

body Display::rotate_mode {_x _y} {
    set x $_x
    set y $_y

    # start receiving motion events
    bind $dm <Motion> "[code $this handle_rotation %x %y]; break"
}

body Display::translate_mode {_x _y} {
    set x $_x
    set y $_y

    # start receiving motion events
    bind $dm <Motion> "[code $this handle_translation %x %y]; break"
}

body Display::scale_mode {_x _y} {
    set x $_x
    set y $_y

    # start receiving motion events
    bind $dm <Motion> "[code $this handle_scale %x %y]; break"
}

body Display::constrain_rmode {coord _x _y} {
    set x $_x
    set y $_y

    # start receiving motion events
    bind $dm <Motion> "[code $this handle_constrain_rot $coord %x %y]; break"
}

body Display::constrain_tmode {coord _x _y} {
    set x $_x
    set y $_y

    # start receiving motion events
    bind $dm <Motion> "[code $this handle_constrain_tran $coord %x %y]; break"
}

body Display::handle_rotation {_x _y} {
    set dx [expr ($y - $_y) * $rscale]
    set dy [expr ($x - $_x) * $rscale]
    $view rot "$dx $dy 0"
    refresh

    #update instance variables x and y
    set x $_x
    set y $_y
}

body Display::handle_translation {_x _y} {
    set dx [expr ($x - $_x) * $invWidth * $size]
    set dy [expr ($_y - $y) * $invWidth * $size]
    $view tra "$dx $dy 0"
    refresh

    #update instance variables x and y
    set x $_x
    set y $_y
}

body Display::handle_scale {_x _y} {
    set dx [expr ($_x - $x) * $invWidth * $sscale]
    set dy [expr ($y - $_y) * $invWidth * $sscale]

    if [expr abs($dx) > abs($dy)] {
	set f [expr 1.0 + $dx]
    } else {
	set f [expr 1.0 + $dy]
    }

    $view zoom $f
    refresh

    #update instance variables x and y
    set x $_x
    set y $_y
}

body Display::handle_constrain_rot {coord _x _y} {
    set dx [expr ($x - $_x) * $rscale]
    set dy [expr ($_y - $y) * $rscale]

    if [expr abs($dx) > abs($dy)] {
	set f $dx
    } else {
	set f $dy
    }
    switch $coord {
	x {
	    $view rot "$f 0 0"
	}
	y {
	    $view rot "0 $f 0"
	}
	z {
	    $view rot "0 0 $f"
	}
    }
    refresh

    #update instance variables x and y
    set x $_x
    set y $_y
}

body Display::handle_constrain_tran {coord _x _y} {
    set dx [expr ($x - $_x) * $invWidth * $size]
    set dy [expr ($_y - $y) * $invWidth * $size]

    if [expr abs($dx) > abs($dy)] {
	set f $dx
    } else {
	set f $dy
    }
    switch $coord {
	x {
	    $view tra "$f 0 0"
	}
	y {
	    $view tra "0 $f 0"
	}
	z {
	    $view tra "0 0 $f"
	}
    }
    refresh

    #update instance variables x and y
    set x $_x
    set y $_y
}

body Display::doBindings {} {
    bind $dm <Enter> "focus $dm;"
    bind $dm <Configure> "[code $this handle_configure]; break"
    bind $dm <Expose> "[code $this handle_expose]; break"

    # Mouse Bindings
    bind $dm <1> "$this zoom 0.5; break"
    bind $dm <2> "$this slew %x %y; break"
    bind $dm <3> "$this zoom 2.0; break"

    # Idle Mode
    bind $dm <ButtonRelease> "[code $this idle_mode]; break"
    bind $dm <KeyRelease-Control_L> "[code $this idle_mode]; break"
    bind $dm <KeyRelease-Control_R> "[code $this idle_mode]; break"
    bind $dm <KeyRelease-Shift_L> "[code $this idle_mode]; break"
    bind $dm <KeyRelease-Shift_R> "[code $this idle_mode]; break"
    bind $dm <KeyRelease-Alt_L> "[code $this idle_mode]; break"
    bind $dm <KeyRelease-Alt_R> "[code $this idle_mode]; break"

    # Rotate Mode
    bind $dm <Control-ButtonPress-1> "[code $this rotate_mode %x %y]; break"
    bind $dm <Control-ButtonPress-2> "[code $this rotate_mode %x %y]; break"
    bind $dm <Control-ButtonPress-3> "[code $this rotate_mode %x %y]; break"

    # Translate Mode
    bind $dm <Shift-ButtonPress-1> "[code $this translate_mode %x %y]; break"
    bind $dm <Shift-ButtonPress-2> "[code $this translate_mode %x %y]; break"
    bind $dm <Shift-ButtonPress-3> "[code $this translate_mode %x %y]; break"

    # Scale Mode
    bind $dm <Control-Shift-ButtonPress-1> "[code $this scale_mode %x %y]; break"
    bind $dm <Control-Shift-ButtonPress-2> "[code $this scale_mode %x %y]; break"
    bind $dm <Control-Shift-ButtonPress-3> "[code $this scale_mode %x %y]; break"

    # Constrained Rotate Mode
    bind $dm <Alt-Control-ButtonPress-1> "[code $this constrain_rmode x %x %y]; break"
    bind $dm <Alt-Control-ButtonPress-2> "[code $this constrain_rmode y %x %y]; break"
    bind $dm <Alt-Control-ButtonPress-3> "[code $this constrain_rmode z %x %y]; break"

    # Constrained Translate Mode
    bind $dm <Alt-Shift-ButtonPress-1> "[code $this constrain_tmode x %x %y]; break"
    bind $dm <Alt-Shift-ButtonPress-2> "[code $this constrain_tmode y %x %y]; break"
    bind $dm <Alt-Shift-ButtonPress-3> "[code $this constrain_tmode z %x %y]; break"

    # Constrained Scale Mode
    bind $dm <Alt-Control-Shift-ButtonPress-1> "[code $this scale_mode %x %y]; break"
    bind $dm <Alt-Control-Shift-ButtonPress-2> "[code $this scale_mode %x %y]; break"
    bind $dm <Alt-Control-Shift-ButtonPress-3> "[code $this scale_mode %x %y]; break"

    # Key Bindings
    bind $dm 3 "$this aet \"35 25 0\"; break"
    bind $dm 4 "$this aet \"45 45 0\"; break"
    bind $dm f "$this aet \"0 0 0\"; break"
    bind $dm R "$this aet \"180 0 0\"; break"
    bind $dm r "$this aet \"270 0 0\"; break"
    bind $dm l "$this aet \"90 0 0\"; break"
    bind $dm t "$this aet \"0 90 0\"; break"
    bind $dm b "$this aet \"0 270 0\"; break"
    bind $dm <F2> "$this toggle_zclip; break"
    bind $dm <F4> "$this toggle_zbuffer; break"
    bind $dm <F5> "$this toggle_light; break"
}

body Display::handle_configure {} {
    $dm configure
    set dm_size [$dm size]
    set width [lindex $dm_size 0]
    set height [lindex $dm_size 1]
    set invWidth [expr 1.0 / $width]
    set aspect [expr (1.0 * $width) / $height]
    refresh
}

body Display::handle_expose {} {
    refresh
}

body Display::listen {args} {
    set len [llength $args]

    if {$len > 1} {
	return "Usage: $this listen \[port\]"
    }

    if {$len} {
	set port [lindex $args 0]
	set listen [$dm listen $port]
    }

    return $listen
}

body Display::rt {args} {
    if {$listen < 0} {
	return "rt: not listening"
    }

    set len [llength $args]

    if {$len > 1 && [lindex $args 0] == "-geo"} {
	set index [lindex $args 1]
	set args [lrange $args 2 end]
	set geo [lindex $geolist $index]
    } else {
	set geo [lindex $geolist 0]
    }

    if {$geo == ""} {
	return "rt: bad geometry index"
    }

    eval $geo rt $view -F $listen -w $width -n $height -V $aspect $args
#    eval $geo rt $view -F $listen $args
}

body Display::fb_active {args} {
    set len [llength $args]

    if {$len > 1} {
	return "Usage: $this fb_active \[0|1|2\]"
    }

    if {$len} {
	set fba [lindex $args 0]
	if {$fba < 0 || 2 < $fba} {
	    return -code error "Usage: $this fb_active \[0|1|2\]"
	}

	# update saved value
	set fb_active $fba
    }

    refresh
    return $fb_active
}

body Display::fb_update {args} {
    set len [llength $args]

    if {$len > 1} {
	return "Usage: $this fb_update \[0|1\]"
    }

    if {$len} {
	set fbu [lindex $args 0]
	if {$fbu < 0 || 1 < $fbu} {
	    return -code error "Usage: $this fb_update \[0|1\]"
	}

	# update saved value
	set fb_update $fbu
    }

    return $fb_update
}

body Display::dm_size {args} {
    set nargs [llength $args]

    # get display manager window size
    if {$nargs == 0} {
	return $dm_size
    }

    if {$nargs == 1} {
	set w $args
	set h $args
    } elseif {$nargs == 2} {
	set w [lindex $args 0]
	set h [lindex $args 1]
    } else {
	return -code error "dm_size: bad size - $args"
    }

    if {$initializing} {
	set width $w
	set height $h
	set dm_size $args
    } else {
	# make request to set display manager window size
	if {$istoplevel} {
	    wm geometry $dm $w\x$h
	} else {
	    $dm size $w $h
	}
    }
}

body Display::dm_name {} {
    return $dm_name
}

## - fbs_callback
#
# This is called by the display manager object
# when it's framebuffer receives data.
#
proc fbs_callback {dm_obj} {
    foreach obj $::Display::dlist {
	if {$dm_obj == [$obj dm_name]} {
	    if [$obj fb_update] {
		$obj refresh
	    }
	    break
	}
    }
}

## - dgo_callback
#
# This is called by the drawable geometry object.
#
proc dgo_callback {dg_obj} {
    foreach obj $::Display::dlist {
	set contents [$obj contents]
	if {[lsearch $contents $dg_obj] != -1} {
	    $obj refresh
	}
    }
}
