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
    inherit itk::Widget

    itk_option define -bg bg Bg {0 0 0}
    itk_option define -debug debug Debug 0
    itk_option define -dmsize dmsize Dmsize {512 512}
    itk_option define -fb_active fb_active Fb_active 0
    itk_option define -fb_observe fb_observe Fb_observe 1
    itk_option define -light light Light 0
    itk_option define -linestyle linestyle Linestyle 0
    itk_option define -linewidth linewidth Linewidth 0
    itk_option define -listen listen Listen -1
    itk_option define -perspective perspective Perspective 0
    itk_option define -type type Type X
    itk_option define -zbuffer zbuffer Zbuffer 0
    itk_option define -zclip zclip Zclip 0

    constructor {args} {}
    destructor {}

    # methods that wrap LIBDM-display-manager-object commands
    public method bg {args}
    public method bounds {args}
    public method clear {}
    public method debug {args}
    public method dmsize {args}
    public method drawBegin {}
    public method drawEnd {}
    public method drawGeom {args}
    public method drawLine {x1 y1 x2 y2}
    public method drawPoint {x y}
    public method drawString {str x y size use_aspect}
    public method fg {args}
    public method flush {}
    public method get_aspect {}
    public method light {args}
    public method linestyle {args}
    public method linewidth {args}
    public method listen {args}
    public method loadmat {mat eye}
    public method normal {}
    public method observer {args}
    public method perspective {args}
    public method refreshfb {}
    public method sync {}
    public method zbuffer {args}
    public method zclip {args}

    public method fb_active {args}
    public method fb_observe {args}

    # methods for handling window events
    protected method toggle_zclip {}
    protected method toggle_zbuffer {}
    protected method toggle_light {}
    protected method toggle_perspective {}
    protected method handle_configure {}
    protected method doBindings {}
    protected method changeType {type}
    protected method createDm {type}
    protected method initDm {}

    protected variable width 512
    protected variable height 512
    protected variable invWidth ""
    protected variable aspect 1.0
    private variable initializing 1
    private variable priv_type X
    private variable tkwin
}

body Dm::constructor {args} {
    # process options now (i.e. -type may have been specified)
    eval itk_initialize $args
    set initializing 0

    createDm $itk_option(-type)
    initDm
}

body Dm::destructor {} {
#    $itk_component(dm) listen -1
#    $itk_component(dm) close

# Hack around problem that showed up in Itcl3.2
# 
    $tkwin listen -1
    $tkwin close
}

configbody Dm::dmsize {
    if {!$initializing} {
	# save size
	set s $itk_option(-dmsize)

	# For now, put back the old value.
	# If the size really does change, size will
	# be set in the handle_configure method.
	set itk_option(-dmsize) "$width $height"

	# request a size change
	eval Dm::dmsize $s
    }
}

configbody Dm::listen {
    if {!$initializing} {
	Dm::listen $itk_option(-listen)
    }
}

configbody Dm::fb_active {
    if {!$initializing} {
	Dm::fb_active $itk_option(-fb_active)
    }
}

configbody Dm::fb_observe {
    if {!$initializing} {
	Dm::fb_observe $itk_option(-fb_observe)
    }
}

configbody Dm::bg {
    if {!$initializing} {
	eval Dm::bg $itk_option(-bg)
    }
}

configbody Dm::light {
    if {!$initializing} {
	Dm::light $itk_option(-light)
    }
}

configbody Dm::zclip {
    if {!$initializing} {
	Dm::zclip $itk_option(-zclip)
    }
}

configbody Dm::zbuffer {
    if {!$initializing} {
	Dm::zbuffer $itk_option(-zbuffer)
    }
}

configbody Dm::perspective {
    if {!$initializing} {
	Dm::perspective $itk_option(-perspective)
    }
}

configbody Dm::debug {
    if {!$initializing} {
	Dm::debug $itk_option(-debug)
    }
}

configbody Dm::linewidth {
    if {!$initializing} {
	Dm::linewidth $itk_option(-linewidth)
    }
}

configbody Dm::type {
    switch $itk_option(-type) {
	X -
	ogl {
	    if {$initializing} {
		set priv_type $itk_option(-type)
	    } else {
		changeType $itk_option(-type)
	    }
	}
	default {
	    error "bad type - $itk_option(-type)"
	}
    }
}

body Dm::observer {args} {
    eval $itk_component(dm) observer $args
}

body Dm::drawBegin {} {
    $itk_component(dm) drawBegin
}

body Dm::drawEnd {} {
    $itk_component(dm) drawEnd
}

# Clear the display manager window
body Dm::clear {} {
    $itk_component(dm) clear
}

body Dm::normal {} {
    $itk_component(dm) normal
}

body Dm::loadmat {mat eye} {
    $itk_component(dm) loadmat $mat $eye
}

body Dm::drawString {str x y size use_aspect} {
    $itk_component(dm) drawString $str $x $y $size $use_aspect
}

body Dm::drawPoint {x y} {
    $itk_component(dm) drawPoint $x $y
}

body Dm::drawLine {x1 y1 x2 y2} {
    $itk_component(dm) drawLine $x1 $y1 $x2 $y2
}

body Dm::drawGeom {args} {
    eval $itk_component(dm) drawGeom $args
}

# Get/set the background color
body Dm::bg {args} {
    if {$args == ""} {
	return $itk_option(-bg)
    }

    $itk_component(dm) bg $args
    set itk_option(-bg) $args
}

# Get/set the foreground color
body Dm::fg {args} {
    if {$args == ""} {
	$itk_component(dm) fg
    } else {
	$itk_component(dm) fg $args
    }
}

# Get/set the line width
body Dm::linewidth {args} {
    if {$args == ""} {
	return $itk_option(-linewidth)
    }

    $itk_component(dm) linewidth $args
    set itk_option(-linewidth) $args
}

# Get/set the line style
body Dm::linestyle {args} {
    if {$args == ""} {
	return $itk_option(-linestyle)
    }

    $itk_component(dm) linestyle $args
    set itk_option(-linestyle) $args
}

body Dm::zclip {args} {
    if {$args == ""} {
	return $itk_option(-zclip)
    }

    $itk_component(dm) zclip $args
    set itk_option(-zclip) $args
}

body Dm::zbuffer {args} {
    if {$args == ""} {
	return $itk_option(-zbuffer)
    }

    $itk_component(dm) zbuffer $args
    set itk_option(-zbuffer) $args
}

# Get/set light
body Dm::light {args} {
    if {$args == ""} {
	return $itk_option(-light)
    }

    $itk_component(dm) light $args
    set itk_option(-light) $args
}

body Dm::perspective {args} {
    if {$args == ""} {
	return $itk_option(-perspective)
    }

    $itk_component(dm) perspective $args
    set itk_option(-perspective) $args
}

body Dm::bounds {args} {
    eval $itk_component(dm) bounds $args
}

body Dm::debug {args} {
    if {$args == ""} {
	return $itk_option(-debug)
    }

    $itk_component(dm) debug $args
    set itk_option(-debug) $args
}

body Dm::listen {args} {
    if {$args == ""} {
	return $itk_option(-listen)
    }

    $itk_component(dm) listen $args
    set itk_option(-listen) $args
}


body Dm::refreshfb {} {
    $itk_component(dm) refreshfb
}

body Dm::flush {} {
    $itk_component(dm) flush
}

body Dm::sync {} {
    $itk_component(dm) sync
}

body Dm::dmsize {args} {
    set nargs [llength $args]

    # get display manager window size
    if {$nargs == 0} {
	return $itk_option(-dmsize)
    }

    if {$nargs == 1} {
	set w $args
	set h $args
    } elseif {$nargs == 2} {
	set w [lindex $args 0]
	set h [lindex $args 1]
    } else {
	error "size: bad size - $args"
    }

    $itk_component(dm) size $w $h
}

body Dm::get_aspect {} {
    $itk_component(dm) get_aspect
}

body Dm::fb_active {args} {
    if {$args == ""} {
	return $itk_option(-fb_active)
    }

    if {$args < 0 || 2 < $args} {
	error "Usage: fb_active \[0|1|2\]"
    }

    # update saved value
    set itk_option(-fb_active) $args
}

body Dm::fb_observe {args} {
    if {$args == ""} {
	return $itk_option(-fb_observe)
    }

    if {$args != 0 && $args != 1} {
	error "Usage: fb_observe \[0|1\]"
    }

    # update saved value
    set itk_option(-fb_observe) $args

    switch $itk_option(-fb_observe) {
	0 {
	    catch {Dm::observer detach $this}
	    return ""
	}
	1 {
	    Dm::observer attach $this
	}
    }
}

body Dm::toggle_zclip {} {
    if {$itk_option(-zclip)} {
	$itk_component(dm) zclip 0
	set itk_option(-zclip) 0
    } else {
	$itk_component(dm) zclip 1
	set itk_option(-zclip) 1
    }
}

body Dm::toggle_zbuffer {} {
    if {$itk_option(-zbuffer)} {
	$itk_component(dm) zbuffer 0
	set itk_option(-zbuffer) 0
    } else {
	$itk_component(dm) zbuffer 1
	set itk_option(-zbuffer) 1
    }
}

body Dm::toggle_light {} {
    if {$itk_option(-light)} {
	$itk_component(dm) light 0
	set itk_option(-light) 0
    } else {
	$itk_component(dm) light 1
	set itk_opton(-light) 1
    }
}

body Dm::toggle_perspective {} {
    if {$itk_option(-perspective)} {
	$itk_component(dm) perspective 0
	set itk_option(-perspective) 0
    } else {
	$itk_component(dm) perspective 1
	set itk_option(-perspective) 1
    }
}

body Dm::handle_configure {} {
    $itk_component(dm) configure

    set itk_option(-dmsize) [$itk_component(dm) size]
    set width [lindex $itk_option(-dmsize) 0]
    set height [lindex $itk_option(-dmsize) 1]
    set invWidth [expr 1.0 / $width]
    set aspect [get_aspect]
}

body Dm::changeType {type} {
    if {$type != $priv_type} {
	$itk_component(dm) listen -1
	$itk_component(dm) close
	createDm $type
	initDm
	set priv_type $type
    }
}

body Dm::createDm {type} {
    itk_component add dm {
	dm_open $itk_interior.dm $type -t 0 -W $width -N $height
    } {}

    pack $itk_component(dm) -fill both -expand yes
} 

body Dm::initDm {} {
    eval Dm::dmsize $itk_option(-dmsize)
    Dm::listen $itk_option(-listen)
    Dm::fb_active $itk_option(-fb_active)
    Dm::fb_observe $itk_option(-fb_observe)
    eval Dm::bg $itk_option(-bg)
    Dm::light $itk_option(-light)
    Dm::zclip $itk_option(-zclip)
    Dm::zbuffer $itk_option(-zbuffer)
    Dm::perspective $itk_option(-perspective)
    Dm::debug $itk_option(-debug)
    Dm::linewidth $itk_option(-linewidth)
    Dm::linestyle $itk_option(-linestyle)

    # event bindings
    doBindings
}

body Dm::doBindings {} {
    bind $itk_component(dm) <Enter> "focus $itk_component(dm);"
    bind $itk_component(dm) <Configure> "[code $this Dm::handle_configure]; break"

    # Key Bindings
    bind $itk_component(dm) <F2> "$this Dm::toggle_zclip; break"
    bind $itk_component(dm) <F3> "$this Dm::toggle_perspective; break"
    bind $itk_component(dm) <F4> "$this Dm::toggle_zbuffer; break"
    bind $itk_component(dm) <F5> "$this Dm::toggle_light; break"
}
