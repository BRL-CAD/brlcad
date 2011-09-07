#                          D M . T C L
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
# DEPRECATED: This widget is deprecated and should no longer be used,
# use ArcherCore instead.
#####################################################################

#
# Description -
#	The Dm class wraps LIBDM's display manager object.
#
::itcl::class Dm {
    inherit itk::Widget

    itk_option define -bg bg Bg {0 0 0}
    itk_option define -debug debug Debug 0
    itk_option define -depthMask depthMask DepthMask 1
    itk_option define -dmsize dmsize Dmsize {512 512}
    itk_option define -light light Light 0
    itk_option define -linestyle linestyle Linestyle 0
    itk_option define -linewidth linewidth Linewidth 1
    itk_option define -perspective perspective Perspective 0
    itk_option define -transparency transparency Transparency 0
    itk_option define -type type Type wgl
    itk_option define -zbuffer zbuffer Zbuffer 0
    itk_option define -zclip zclip Zclip 0

    if {$tcl_platform(os) != "Windows NT"} {
    }
    itk_option define -fb_active fb_active Fb_active 0
    itk_option define -fb_observe fb_observe Fb_observe 1
    itk_option define -listen listen Listen -1

    constructor {args} {}
    destructor {}

    # methods that wrap LIBDM-display-manager-object commands
    public method bg {args}
    public method bounds {args}
    public method clear {}
    public method debug {args}
    public method depthMask {args}
    public method dmsize {args}
    public method drawBegin {}
    public method drawEnd {}
    public method drawGeom {args}
    public method drawLabels {args}
    public method drawLine {x1 y1 x2 y2}
    public method drawPoint {x y}
    public method drawString {str x y size use_aspect}
    public method drawDataAxes {args}
    public method drawModelAxes {args}
    public method drawViewAxes {args}
    public method drawCenterDot {args}
    public method drawScale {args}
    public method fg {args}
    public method flush {}
    public method get_aspect {}
    public method getDrawLabelsHook {args}
    public method light {args}
    public method linestyle {args}
    public method linewidth {args}
    public method loadmat {mat eye}
    public method normal {}
    public method observer {args}
    public method perspective {args}
    public method png {args}
    public method setDrawLabelsHook {args}
    public method sync {}
    public method transparency {args}
    public method zbuffer {args}
    public method zclip {args}

    if {$tcl_platform(os) != "Windows NT"} {
    }
    public method listen {args}
    public method refreshfb {}
    public method fb_active {args}
    public method fb_observe {args}

    public method ? {}
    public method apropos {key}
    public method help {args}
    public method getUserCmds {}

    # methods for handling window events
    protected method toggle_zclip {}
    protected method toggle_zbuffer {}
    protected method toggle_light {}
    protected method toggle_perspective {}
    protected method toggle_transparency {}
    protected method handle_configure {}
    protected method doBindings {}
    protected method changeType {type}
    protected method createDm {type}
    protected method initDm {}

    protected variable width 512
    protected variable height 512
    protected variable invWidth 0.001953125
    protected variable invHeight 0.001953125
    protected variable aspect 1.0
    protected variable invAspect 1.0

    private method helpInit {}

    private variable initializing 1
    private variable tkwin

    if {$tcl_platform(os) != "Windows NT"} {
	private variable priv_type X
    } else {
	private variable priv_type wgl
    }

    private variable help
}

::itcl::body Dm::constructor {args} {
    global tcl_platform
    global env

    puts "DEPRECATION WARNING: The Dm widget should no longer be used.  Use the Ged widget instead."

    catch {set display $env(DISPLAY)}
    if {![info exists display] || $display == ""} {
	set display :0
    }

    if {[catch {dm_bestXType $display} priv_type]} {
	if {$tcl_platform(os) != "Windows NT"} {
	    set priv_type X
	} else {
	    set priv_type wgl
	}
    }

    # process options now (i.e. -type may have been specified)
    eval itk_initialize $args
    set initializing 0

    set itk_option(-type) $priv_type
    createDm $itk_option(-type)
    initDm
    helpInit
}

::itcl::body Dm::destructor {} {
    #    $tkwin listen -1
    rename $tkwin ""

    catch {delete object $help}
}

::itcl::configbody Dm::dmsize {
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

if {$tcl_platform(os) != "Windows NT"} {
}
::itcl::configbody Dm::listen {
    if {!$initializing} {
	Dm::listen $itk_option(-listen)
    }
}

::itcl::configbody Dm::fb_active {
    if {!$initializing} {
	Dm::fb_active $itk_option(-fb_active)
    }
}

::itcl::configbody Dm::fb_observe {
    if {!$initializing} {
	Dm::fb_observe $itk_option(-fb_observe)
    }
}

::itcl::configbody Dm::bg {
    if {!$initializing} {
	eval Dm::bg $itk_option(-bg)
    }
}

::itcl::configbody Dm::light {
    if {!$initializing} {
	Dm::light $itk_option(-light)
    }
}

::itcl::configbody Dm::depthMask {
    if {!$initializing} {
	Dm::depthMask $itk_option(-depthMask)
    }
}

::itcl::configbody Dm::transparency {
    if {!$initializing} {
	Dm::transparency $itk_option(-transparency)
    }
}

::itcl::configbody Dm::zclip {
    if {!$initializing} {
	Dm::zclip $itk_option(-zclip)
    }
}

::itcl::configbody Dm::zbuffer {
    if {!$initializing} {
	Dm::zbuffer $itk_option(-zbuffer)
    }
}

::itcl::configbody Dm::perspective {
    if {!$initializing} {
	Dm::perspective $itk_option(-perspective)
    }
}

::itcl::configbody Dm::debug {
    if {!$initializing} {
	Dm::debug $itk_option(-debug)
    }
}

::itcl::configbody Dm::linewidth {
    if {!$initializing} {
	Dm::linewidth $itk_option(-linewidth)
    }
}

::itcl::configbody Dm::type {
    switch $itk_option(-type) {
	X -
	ogl -
	wgl {
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

::itcl::body Dm::observer {args} {
    eval $itk_component(dm) observer $args
}

::itcl::body Dm::drawBegin {} {
    $itk_component(dm) drawBegin
}

::itcl::body Dm::drawEnd {} {
    $itk_component(dm) drawEnd
}

# Clear the display manager window
::itcl::body Dm::clear {} {
    $itk_component(dm) clear
}

::itcl::body Dm::normal {} {
    $itk_component(dm) normal
}

::itcl::body Dm::loadmat {mat eye} {
    $itk_component(dm) loadmat $mat $eye
}

::itcl::body Dm::drawGeom {args} {
    eval $itk_component(dm) drawGeom $args
}

::itcl::body Dm::drawLabels {args} {
    eval $itk_component(dm) drawLabels $args
}

::itcl::body Dm::drawLine {x1 y1 x2 y2} {
    $itk_component(dm) drawLine $x1 $y1 $x2 $y2
}

::itcl::body Dm::drawPoint {x y} {
    $itk_component(dm) drawPoint $x $y
}

::itcl::body Dm::drawString {str x y size use_aspect} {
    $itk_component(dm) drawString $str $x $y $size $use_aspect
}

::itcl::body Dm::drawDataAxes {args} {
    eval $itk_component(dm) drawDataAxes $args
}

::itcl::body Dm::drawModelAxes {args} {
    eval $itk_component(dm) drawModelAxes $args
}

::itcl::body Dm::drawViewAxes {args} {
    eval $itk_component(dm) drawViewAxes $args
}

::itcl::body Dm::drawCenterDot {args} {
    eval $itk_component(dm) drawCenterDot $args
}

::itcl::body Dm::drawScale {args} {
    eval $itk_component(dm) drawScale $args
}

# Get/set the background color
::itcl::body Dm::bg {args} {
    if {$args == ""} {
	return $itk_option(-bg)
    }

    $itk_component(dm) bg $args
    set itk_option(-bg) $args
}

# Get/set the foreground color
::itcl::body Dm::fg {args} {
    if {$args == ""} {
	$itk_component(dm) fg
    } else {
	$itk_component(dm) fg $args
    }
}

# Get/set the line width
::itcl::body Dm::linewidth {args} {
    if {$args == ""} {
	return $itk_option(-linewidth)
    }

    $itk_component(dm) linewidth $args
    set itk_option(-linewidth) $args
}

# Get/set the line style
::itcl::body Dm::linestyle {args} {
    if {$args == ""} {
	return $itk_option(-linestyle)
    }

    $itk_component(dm) linestyle $args
    set itk_option(-linestyle) $args
}

::itcl::body Dm::zclip {args} {
    if {$args == ""} {
	return $itk_option(-zclip)
    }

    $itk_component(dm) zclip $args
    set itk_option(-zclip) $args
}

::itcl::body Dm::zbuffer {args} {
    if {$args == ""} {
	return $itk_option(-zbuffer)
    }

    $itk_component(dm) zbuffer $args
    set itk_option(-zbuffer) $args
}

# Get/set light
::itcl::body Dm::light {args} {
    if {$args == ""} {
	return $itk_option(-light)
    }

    $itk_component(dm) light $args
    set itk_option(-light) $args
}

# Get/set depthMask
::itcl::body Dm::depthMask {args} {
    if {$args == ""} {
	return $itk_option(-depthMask)
    }

    $itk_component(dm) depthMask $args
    set itk_option(-depthMask) $args
}

# Get/set transparency
::itcl::body Dm::transparency {args} {
    if {$args == ""} {
	return $itk_option(-transparency)
    }

    $itk_component(dm) transparency $args
    set itk_option(-transparency) $args
}

::itcl::body Dm::perspective {args} {
    if {$args == ""} {
	return $itk_option(-perspective)
    }

    $itk_component(dm) perspective $args
    set itk_option(-perspective) $args
}

::itcl::body Dm::png {args} {
    eval $itk_component(dm) png $args
}

::itcl::body Dm::getDrawLabelsHook {args} {
    eval $itk_component(dm) getDrawLabelsHook $args
}

::itcl::body Dm::setDrawLabelsHook {args} {
    eval $itk_component(dm) setDrawLabelsHook $args
}

::itcl::body Dm::bounds {args} {
    eval $itk_component(dm) bounds $args
}

::itcl::body Dm::debug {args} {
    if {$args == ""} {
	return $itk_option(-debug)
    }

    $itk_component(dm) debug $args
    set itk_option(-debug) $args
}

if {$tcl_platform(os) != "Windows NT"} {
}
::itcl::body Dm::listen {args} {
    if {$args == ""} {
	return $itk_option(-listen)
    }

    set itk_option(-listen) [$itk_component(dm) listen $args]
}

::itcl::body Dm::refreshfb {} {
    $itk_component(dm) refreshfb
}

::itcl::body Dm::flush {} {
    $itk_component(dm) flush
}

::itcl::body Dm::sync {} {
    $itk_component(dm) sync
}

::itcl::body Dm::dmsize {args} {
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

::itcl::body Dm::get_aspect {} {
    $itk_component(dm) get_aspect
}

::itcl::body Dm::fb_active {args} {
    if {$args == ""} {
	return $itk_option(-fb_active)
    }

    if {$args < 0 || 2 < $args} {
	error "Usage: fb_active \[0|1|2\]"
    }

    # update saved value
    set itk_option(-fb_active) $args
}

::itcl::body Dm::fb_observe {args} {
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

::itcl::body Dm::toggle_zclip {} {
    if {$itk_option(-zclip)} {
	$itk_component(dm) zclip 0
	set itk_option(-zclip) 0
    } else {
	$itk_component(dm) zclip 1
	set itk_option(-zclip) 1
    }
}

::itcl::body Dm::toggle_zbuffer {} {
    if {$itk_option(-zbuffer)} {
	$itk_component(dm) zbuffer 0
	set itk_option(-zbuffer) 0
    } else {
	$itk_component(dm) zbuffer 1
	set itk_option(-zbuffer) 1
    }
}

::itcl::body Dm::toggle_light {} {
    if {$itk_option(-light)} {
	$itk_component(dm) light 0
	set itk_option(-light) 0
    } else {
	$itk_component(dm) light 1
	set itk_option(-light) 1
    }
}

::itcl::body Dm::toggle_perspective {} {
    if {$itk_option(-perspective)} {
	$itk_component(dm) perspective 0
	set itk_option(-perspective) 0
    } else {
	$itk_component(dm) perspective 1
	set itk_option(-perspective) 1
    }
}

::itcl::body Dm::toggle_transparency {} {
    if {$itk_option(-transparency)} {
	$itk_component(dm) transparency 0
	set itk_option(-transparency) 0
    } else {
	$itk_component(dm) transparency 1
	set itk_option(-transparency) 1
    }
}

::itcl::body Dm::handle_configure {} {
    $itk_component(dm) configure
    #    [namespace tail $itk_component(dm)] configure

    set itk_option(-dmsize) [$itk_component(dm) size]
    set width [lindex $itk_option(-dmsize) 0]
    set height [lindex $itk_option(-dmsize) 1]
    set invWidth [expr 1.0 / $width]
    set invHeight [expr 1.0 / $height]
    set aspect [get_aspect]
    set invAspect [expr 1.0 / $aspect]
}

::itcl::body Dm::changeType {type} {
    if {$type != $priv_type} {
	#	$itk_component(dm) listen -1

	# the close method no longer exists
	#$itk_component(dm) close
	rename $itk_component(dm) ""

	createDm $type
	initDm
	set priv_type $type
    }
}

::itcl::body Dm::createDm {type} {
    itk_component add dm {
	dm_open $itk_interior.dm $type -t 0 -W $width -N $height
    } {}

    pack $itk_component(dm) -fill both -expand yes
    set tkwin $itk_component(dm)
}

::itcl::body Dm::initDm {} {
    global tcl_platform

    eval Dm::dmsize $itk_option(-dmsize)
    Dm::fb_active $itk_option(-fb_active)
    Dm::fb_observe $itk_option(-fb_observe)
    Dm::listen $itk_option(-listen)

    eval Dm::bg $itk_option(-bg)
    Dm::light $itk_option(-light)
    Dm::zclip $itk_option(-zclip)
    Dm::zbuffer $itk_option(-zbuffer)
    Dm::perspective $itk_option(-perspective)
    Dm::debug $itk_option(-debug)
    Dm::linewidth $itk_option(-linewidth)
    Dm::linestyle $itk_option(-linestyle)
    Dm::depthMask $itk_option(-depthMask)
    Dm::transparency $itk_option(-transparency)

    # event bindings
    doBindings
}

::itcl::body Dm::doBindings {} {
    global tcl_platform

    if {$tcl_platform(os) != "Windows NT"} {
	bind $itk_component(dm) <Enter> "focus $itk_component(dm);"
    }

    bind $itk_component(dm) <Configure> "[::itcl::code $this Dm::handle_configure]; break"

    # Key Bindings
    bind $itk_component(dm) <F2> "[::itcl::code $this Dm::toggle_zclip]; break"
    bind $itk_component(dm) <F3> "[::itcl::code $this Dm::toggle_perspective]; break"
    bind $itk_component(dm) <F4> "[::itcl::code $this Dm::toggle_zbuffer]; break"
    bind $itk_component(dm) <F5> "[::itcl::code $this Dm::toggle_light]; break"
    bind $itk_component(dm) <F10> "[::itcl::code $this Dm::toggle_transparency]; break"
}

::itcl::body Dm::? {} {
    return [$help ? 20 4]
}

::itcl::body Dm::apropos {key} {
    return [$help apropos $key]
}

::itcl::body Dm::help {args} {
    return [eval $help get $args]
}

::itcl::body Dm::getUserCmds {} {
    return {png}
}

::itcl::body Dm::helpInit {} {
    set help [cadwidgets::Help \#auto]

    $help add png		{{file} {Dump contents of window to a png file}}
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
