##                 D M . T C L
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
#	The Dm class wraps LIBDM's display manager object.
#
class Dm {
    protected variable dm ""
    protected variable width 512
    protected variable height 512
    protected variable invWidth ""
    protected variable aspect ""
    private variable initializing 1

    public variable name ""
    public variable istoplevel 1
    public variable type X
    public variable size 512
    public variable title ""
    public variable listen -1
    public variable fb_active 0
    public variable fb_update 1
    public variable bg "0 0 0"
    public variable light 0
    public variable zclip 0
    public variable zbuffer 0
    public variable perspective 0
    public variable debug 0
    public variable linewidth 0

    constructor {args} {
	# process options
	eval configure $args

	# create default name
	if {$name == ""} {
	    set name .[string map {:: ""} $this]
	}

	# create default title
	if {$title == ""} {
	    set title $this
	}

	# create display manager object
	set dm [dm_open $name $type -t $istoplevel -W $width -N $height]

	# initialize display manager object
	$dm clear
	eval bg $bg
	light $light
	zclip $zclip
	zbuffer $zbuffer
	debug $debug
	listen $listen
	linewidth $linewidth

	# event bindings
	doBindings

	if {$istoplevel} {
	    wm title $dm $title
	}

	set initializing 0
    }

    destructor {
	$dm listen -1
	$dm close
    }

    # methods for handling window events
    protected method toggle_zclip {}
    protected method toggle_zbuffer {}
    protected method toggle_light {}
    protected method toggle_perspective {}
    protected method doBindings {}

    # methods that wrap LIBDM display manager object commands
    public method observer {args}
    public method drawBegin {}
    public method drawEnd {}
    public method clear {}
    public method normal {}
    public method loadmat {mat eye}
    public method drawString {str x y size use_aspect}
    public method drawPoint {x y}
    public method drawLine {x1 y1 x2 y2}
    public method drawGeom {args}
    public method bg {args}
    public method fg {args}
    public method linewidth {args}
    public method linestyle {args}
    public method handle_configure {}
    public method zclip {args}
    public method zbuffer {args}
    public method light {args}
    public method perspective {args}
    public method bounds {args}
    public method debug {args}
    public method listen {args}
    public method refreshfb {}
    public method flush {}
    public method sync {}
    public method size {args}
    public method get_aspect {}

    # new methods
    public method get_name {}
    public method fb_active {args}
    public method fb_update {args}
}

configbody Dm::name {
    if {!$initializing} {
	return -code error "name is read-only"
    }
}

configbody Dm::size {
    # save size
    set s $size

    # For now, put back the old value.
    # If the size really does change, size will
    # be set in the handle_configure method.
    set size "$width $height"

    # request a size change
    eval Dm::size $s
}

configbody Dm::title {
    if {$istoplevel && [winfo exists $dm]} {
	wm title $dm $title
    }
}

configbody Dm::istoplevel {
    if {!$initializing} {
	return -code error "istoplevel is read-only"
    }
}

configbody Dm::type {
    if {!$initializing} {
	return -code error "type is read-only"
    }
    
    switch $type {
	X -
	ogl {
	}
	default {
	    set type X
	}
    }
}

configbody Dm::listen {
    if {!$initializing} {
	Dm::listen $listen
    }
}

configbody Dm::fb_active {
    Dm::fb_active $fb_active
}

configbody Dm::fb_update {
    Dm::fb_update $fb_update
}

configbody Dm::bg {
    if {!$initializing} {
	Dm::bg $bg
    }
}

configbody Dm::light {
    if {!$initializing} {
	Dm::light $light
    }
}

configbody Dm::zclip {
    if {!$initializing} {
	Dm::zclip $zclip
    }
}

configbody Dm::zbuffer {
    if {!$initializing} {
	Dm::zbuffer $zbuffer
    }
}

configbody Dm::perspective {
    if {!$initializing} {
	Dm::perspective $perspective
    }
}

configbody Dm::debug {
    if {!$initializing} {
	Dm::debug $debug
    }
}

configbody Dm::linewidth {
    if {!$initializing} {
	Dm::linewidth $linewidth
    }
}

body Dm::observer {args} {
    eval $dm observer $args
}

body Dm::drawBegin {} {
    $dm drawBegin
}

body Dm::drawEnd {} {
    $dm drawEnd
}

# Clear the display manager window
body Dm::clear {} {
    $dm clear
}

body Dm::normal {} {
    $dm normal
}

body Dm::loadmat {mat eye} {
    $dm loadmat $mat $eye
}

body Dm::drawString {str x y size use_aspect} {
    $dm drawString $str $x $y $size $use_aspect
}

body Dm::drawPoint {x y} {
    $dm drawPoint $x $y
}

body Dm::drawLine {x1 y1 x2 y2} {
    $dm drawLine $x1 $y1 $x2 $y2
}

body Dm::drawGeom {args} {
    eval $dm drawGeom $args
}

# Get/set the background color
body Dm::bg {args} {
    if {$args == ""} {
	return $bg
    }

    $dm bg $args
    set bg $args
}

# Get/set the foreground color
body Dm::fg {args} {
    if {$args == ""} {
	$dm fg
    } else {
	$dm fg $args
    }
}

# Get/set the line width
body Dm::linewidth {args} {
    if {$args == ""} {
	return $linewidth
    }

    $dm linewidth $args
    set linewidth $args
}

# Get/set the line style
body Dm::linestyle {args} {
    if {$args == ""} {
	$dm linestyle
    } else {
	$dm linestyle $args
    }
}

body Dm::handle_configure {} {
    $dm configure
    set size [$dm size]
    set width [lindex $size 0]
    set height [lindex $size 1]
    set invWidth [expr 1.0 / $width]
    set aspect [expr (1.0 * $width) / $height]
}

body Dm::zclip {args} {
    if {$args == ""} {
	return $zclip
    }

    $dm zclip $args
    set zclip $args
}

body Dm::zbuffer {args} {
    if {$args == ""} {
	return $zbuffer
    }

    $dm zbuffer $args
    set zbuffer $args
}

# Get/set light
body Dm::light {args} {
    if {$args == ""} {
	return $light
    }

    $dm light $args
    set light $args
}

body Dm::perspective {args} {
    if {$args == ""} {
	return $perspective
    }

    $dm perspective $args
    set perspective $args
}

body Dm::bounds {args} {
    eval $dm bounds $args
}

body Dm::debug {args} {
    if {$args == ""} {
	return $debug
    }

    $dm debug $args
    set debug $args
}

body Dm::listen {args} {
    if {$args == ""} {
	return $listen
    }

    $dm listen $args
    set listen $args
}


body Dm::refreshfb {} {
    $dm refreshfb
}

body Dm::flush {} {
    $dm flush
}

body Dm::sync {} {
    $dm sync
}

body Dm::size {args} {
    set nargs [llength $args]

    # get display manager window size
    if {$nargs == 0} {
	return $size
    }

    if {$nargs == 1} {
	set w $args
	set h $args
    } elseif {$nargs == 2} {
	set w [lindex $args 0]
	set h [lindex $args 1]
    } else {
	return -code error "size: bad size - $args"
    }

    if {$initializing} {
	set width $w
	set height $h
	set size $args
    } else {
	# make request to set display manager window size
	if {$istoplevel} {
	    wm geometry $dm $w\x$h
	} else {
	    $dm size $w $h
	}
    }
}

body Dm::get_aspect {} {
    $dm get_aspect
}

body Dm::get_name {} {
    return $dm
}

body Dm::fb_active {args} {
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

    return $fb_active
}

body Dm::fb_update {args} {
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

body Dm::toggle_zclip {} {
    if {$zclip} {
	$dm zclip 0
	set zclip 0
    } else {
	$dm zclip 1
	set zclip 1
    }
}

body Dm::toggle_zbuffer {} {
    if {$zbuffer} {
	$dm zbuffer 0
	set zbuffer 0
    } else {
	$dm zbuffer 1
	set zbuffer 1
    }
}

body Dm::toggle_light {} {
    if {$light} {
	$dm light 0
	set light 0
    } else {
	$dm light 1
	set light 1
    }
}

body Dm::toggle_perspective {} {
    if {$perspective} {
	$dm perspective 0
	set perspective 0
    } else {
	$dm perspective 1
	set perspective 1
    }
}

body Dm::doBindings {} {
    bind $dm <Enter> "focus $dm;"
    bind $dm <Configure> "[code $this Dm::handle_configure]; break"

    # Key Bindings
    bind $dm <F2> "$this Dm::toggle_zclip; break"
    bind $dm <F3> "$this Dm::toggle_perspective; break"
    bind $dm <F4> "$this Dm::toggle_zbuffer; break"
    bind $dm <F5> "$this Dm::toggle_light; break"
}
