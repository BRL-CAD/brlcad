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
#	The Display class inherits from View and Dm. 
#       This class is capable of displaying one or more drawable
#       geometry objects.
#
class Display {
    inherit View Dm

    private variable x ""
    private variable y ""
    private variable geolist ""
    private variable perspective_angle_index 0
    private variable perspective_angles {90 60 45 30}

    public variable rscale 0.4
    public variable sscale 2.0

    constructor {view_args dm_args args} {
	eval View::constructor $view_args
	eval Dm::constructor $dm_args
    } {
	# process options
	eval configure $args

	attach

	handle_configure
	doBindings
    }

    destructor {}

    public method update {obj}
    public method refresh {}
    public method rt {args}
    public method autoview {}

    # methods for maintaining the list of geometry objects
    public method add {glist}
    public method remove {glist}
    public method contents {}

    # methods that override inherited methods
    public method slew {x y}
    public method fb_active {args}
    public method zclip {args}
    public method zbuffer {args}
    public method light {args}
    public method perspective {args}
    public method toggle_zclip {}
    public method toggle_zbuffer {}
    public method toggle_light {}
    public method toggle_perspective {}
    public method toggle_perspective_angle {}

    protected method attach {}
    protected method detach {}
    protected method idle_mode {}
    protected method rotate_mode {x y}
    protected method translate_mode {x y}
    protected method scale_mode {x y}
    protected method constrain_rmode {coord x y}
    protected method constrain_tmode {coord x y}
    protected method handle_rotation {x y}
    protected method handle_translation {x y}
    protected method handle_scale {x y}
    protected method handle_constrain_rot {coord x y}
    protected method handle_constrain_tran {coord x y}
    protected method handle_configure {}
    protected method handle_expose {}
    protected method doBindings {}
}

########################### ###########################
########################### Public/Interface Methods ###########################
body Display::update {obj} {
    refresh
}

body Display::refresh {} {
    Dm::drawBegin

    if {$perspective} {
	Dm::loadmat [View::pmodel2view] 0
    } else {
	Dm::loadmat [View::model2view] 0
    }

    if {$fb_active < 2} {
	if {$fb_active} {
	    # underlay
	    Dm::refreshfb
	}

	foreach geo $geolist {
	    Dm::drawGeom $geo
	}
    } else {
	# overlay
	Dm::refreshfb
    }
    Dm::drawEnd
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

    set v_obj [View::get_name]
    eval $geo rt $v_obj -F $listen -w $width -n $height -V $aspect $args
}


body Display::autoview {} {
    if [llength $geolist] {
	set geo [lindex $geolist 0]
	set aview [$geo get_autoview]
	eval [lrange $aview 0 1]
	eval [lrange $aview 2 3]
    }
}

body Display::add {glist} {
    if [llength $geolist] {
	set blank 0
    } else {
	set blank 1
    }

    foreach geo $glist {
	set index [lsearch $geolist $geo]
	if {$index != -1} {
	    continue
	}

	lappend geolist $geo
	$geo observer attach $this
    }

    if {$blank} {
	detach
	autoview
	attach
    }

    refresh
}

body Display::remove {glist} {
    foreach geo $glist {
	set index [lsearch $geolist $geo]
	if {$index == -1} {
	    continue
	}

	set geolist [lreplace $geolist $index $index]
	$geo observer detach $this
    }

    refresh
}

body Display::contents {} {
    return $geolist
}

########################### Public Methods That Override ###########################
body Display::slew {x1 y1} {
    set x2 [expr $width * 0.5]
    set y2 [expr $height * 0.5]
    set sf [expr 2.0 * $invWidth]

    set _x [expr ($x1 - $x2) * $sf]
    set _y [expr (-1.0 * $y1 + $y2) * $sf]

    View::slew $_x $_y
}

body Display::fb_active {args} {
    eval Dm::fb_active $args
    refresh
    return $fb_active
}

body Display::zclip {args} {
    eval Dm::zclip $args
    refresh
    return $zclip
}

body Display::zbuffer {args} {
    eval Dm::zbuffer $args
    refresh
    return $zbuffer
}

body Display::light {args} {
    eval Dm::light $args
    refresh
    return $light
}

body Display::perspective {args} {
    eval Dm::perspective $args
    refresh
    return $perspective
}

body Display::toggle_zclip {} {
    Dm::toggle_zclip
    refresh
    return $zclip
}

body Display::toggle_zbuffer {} {
    Dm::toggle_zbuffer
    refresh
    return $zbuffer
}

body Display::toggle_light {} {
    Dm::toggle_light
    refresh
    return $light
}

body Display::toggle_perspective {} {
    Dm::toggle_perspective
    refresh
    return $perspective
}

body Display::toggle_perspective_angle {} {
    if {$perspective_angle_index == 3} {
	set perspective_angle_index 0
    } else {
	incr perspective_angle_index
    }

    View::perspective [lindex $perspective_angles $perspective_angle_index]
}

########################### Protected Methods ###########################
body Display::attach {} {
    Dm::observer attach $this
    View::observer attach $this
}

body Display::detach {} {
    Dm::observer detach $this
    View::observer detach $this
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
    rot "$dx $dy 0"
    refresh

    #update instance variables x and y
    set x $_x
    set y $_y
}

body Display::handle_translation {_x _y} {
    set dx [expr ($x - $_x) * $invWidth * $size]
    set dy [expr ($_y - $y) * $invWidth * $size]
    tra "$dx $dy 0"
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

    zoom $f
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
	    rot "$f 0 0"
	}
	y {
	    rot "0 $f 0"
	}
	z {
	    rot "0 0 $f"
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
	    tra "$f 0 0"
	}
	y {
	    tra "0 $f 0"
	}
	z {
	    tra "0 0 $f"
	}
    }
    refresh

    #update instance variables x and y
    set x $_x
    set y $_y
}

body Display::handle_configure {} {
    Dm::handle_configure
    refresh
}

body Display::handle_expose {} {
    refresh
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
    bind $dm <F2> "[code $this toggle_zclip]; break"
    bind $dm <F3> "[code $this toggle_perspective]; break"
    bind $dm <F4> "[code $this toggle_zbuffer]; break"
    bind $dm <F5> "[code $this toggle_light]; break"
    bind $dm <F6> "[code $this toggle_perspective_angle]; break"
}
