#                     D I S P L A Y . T C L
# BRL-CAD
#
# Copyright (c) 1998-2007 United States Government as represented by
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
#
# Author -
#	Bob Parker
#
# Source -
#	The U. S. Army Research Laboratory
#	Aberdeen Proving Ground, Maryland  21005
#
#
#
# Description -
#	The Display class inherits from View and Dm. This
#       class also maintains a list of drawable geometry objects
#       which it can display. It now becomes possible to bind
#       view commands to window events to automatically update the
#       Dm window when the view changes.
#

#
# Usual options.
#
::itk::usual Display {
    keep -linewidth
    keep -rscale
    keep -sscale
    keep -usePhony
    keep -type

    keep -primitiveLabels
    keep -primitiveLabelColor

    keep -centerDotEnable
    keep -centerDotColor

    keep -modelAxesEnable
    keep -modelAxesLineWidth
    keep -modelAxesPosition
    keep -modelAxesSize
    keep -modelAxesColor
    keep -modelAxesLabelColor
    keep -modelAxesTripleColor

    keep -modelAxesTickEnable
    keep -modelAxesTickLength
    keep -modelAxesTickMajorLength
    keep -modelAxesTickInterval
    keep -modelAxesTicksPerMajor
    keep -modelAxesTickColor
    keep -modelAxesTickMajorColor
    keep -modelAxesTickThreshold

    keep -viewAxesEnable
    keep -viewAxesLineWidth
    keep -viewAxesPosition
    keep -viewAxesSize
    keep -viewAxesColor
    keep -viewAxesLabelColor
    keep -viewAxesTripleColor

    keep -showViewingParams
    keep -viewingParamsColor

    keep -scaleEnable
    keep -scaleColor
}

::itcl::class Display {
    inherit Dm View

    itk_option define -rscale rscale Rscale 0.4
    itk_option define -sscale sscale Sscale 2.0

    itk_option define -usePhony usePhony UsePhony 0

    itk_option define -centerDotEnable centerDotEnable CenterDotEnable 1
    itk_option define -centerDotColor centerDotColor CenterDotColor {255 255 0}

    itk_option define -scaleEnable scaleEnable ScaleEnable 0
    itk_option define -scaleColor scaleColor ScaleColor {255 255 0}

    itk_option define -primitiveLabels primitiveLabels PrimitiveLabels {}
    itk_option define -primitiveLabelColor primitiveLabelColor PrimitiveLabelColor {255 255 0}

    itk_option define -modelAxesEnable modelAxesEnable AxesEnable 0
    itk_option define -modelAxesLineWidth modelAxesLineWidth AxesLineWidth 0
    itk_option define -modelAxesPosition modelAxesPosition AxesPosition {0 0 0}
    itk_option define -modelAxesSize modelAxesSize AxesSize 2.0
    itk_option define -modelAxesColor modelAxesColor AxesColor {255 255 255}
    itk_option define -modelAxesLabelColor modelAxesLabelColor AxesLabelColor {255 255 0}
    itk_option define -modelAxesTripleColor modelAxesTripleColor AxesTripleColor 0

    itk_option define -modelAxesTickEnable modelAxesTickEnable AxesTickEnable 1
    itk_option define -modelAxesTickLength modelAxesTickLength AxesTickLength 4
    itk_option define -modelAxesTickMajorLength modelAxesTickMajorLength AxesTickMajorLength 8
    itk_option define -modelAxesTickInterval modelAxesTickInterval AxesTickInterval 100
    itk_option define -modelAxesTicksPerMajor modelAxesTicksPerMajor AxesTicksPerMajor 10
    itk_option define -modelAxesTickColor modelAxesTickColor AxesTickColor {255 255 0}
    itk_option define -modelAxesTickMajorColor modelAxesTickMajorColor AxesTickMajorColor {255 0 0}
    itk_option define -modelAxesTickThreshold modelAxesTickThreshold AxesTickThreshold 8

    itk_option define -viewAxesEnable viewAxesEnable AxesEnable 1
    itk_option define -viewAxesLineWidth viewAxesLineWidth AxesLineWidth 0
    itk_option define -viewAxesPosition viewAxesPosition AxesPosition {-0.85 -0.85 0}
    itk_option define -viewAxesSize viewAxesSize AxesSize 0.2
    itk_option define -viewAxesColor viewAxesColor AxesColor {255 255 255}
    itk_option define -viewAxesLabelColor viewAxesLabelColor AxesLabelColor {255 255 0}
    itk_option define -viewAxesTripleColor viewAxesTripleColor AxesTripleColor 1

    itk_option define -showViewingParams showViewingParams ShowViewingParams 0
    itk_option define -viewingParamsColor viewingParamsColor ViewingParamsColor {255 255 0}

    constructor {args} {
	Dm::constructor
	View::constructor
    } {}
    destructor {}

    public method mouse_nirt {_x _y {gi 0}}
    public method nirt {args}
    public method vnirt {vx vy {gi 0}}
    public method qray {args}
    public method refresh {}
    public method rt {args}
    public method rtabort {{gi 0}}
    public method rtarea {args}
    public method rtcheck {args}
    public method rtedge {args}
    public method rtweight {args}
    public method autoview {{g_index 0}}
    public method attach_view {}
    public method attach_drawable {dg}
    public method detach_view {}
    public method detach_drawable {dg}
    public method update {obj}

    # methods for maintaining the list of geometry objects
    public method add {glist}
    public method contents {}
    public method remove {glist}

    # methods that override methods inherited from View
    public method slew {args}
    public method perspective_angle {args}

    # methods that override methods inherited from Dm
    public method bounds {args}
    public method depthMask {args}
    public method perspective {args}
    public method light {args}
    public method transparency {args}
    public method zbuffer {args}
    public method zclip {args}
    if {$tcl_platform(os) != "Windows NT"} {
	public method fb_active {args}
    }

    public method toggle_modelAxesEnable {}
    public method toggle_modelAxesTickEnable {}
    public method toggle_viewAxesEnable {}
    public method toggle_centerDotEnable {}
    public method toggle_scaleEnable {}

    public method ? {}
    public method apropos {key}
    public method help {args}
    public method getUserCmds {}

    protected method toggle_zclip {}
    protected method toggle_zbuffer {}
    protected method toggle_light {}
    protected method toggle_perspective {}
    protected method toggle_perspective_angle {}
    protected method toggle_transparency {}

    public method idle_mode {}
    public method rotate_mode {x y}
    public method scale_mode {x y}
    public method translate_mode {x y}
    public method orotate_mode {x y func obj kx ky kz}
    public method oscale_mode {x y func obj kx ky kz}
    public method otranslate_mode {x y func obj}
    public method screen2model {x y}
    public method screen2view {x y}

    protected method constrain_rmode {coord x y}
    protected method constrain_tmode {coord x y}
    protected method handle_rotation {x y}
    protected method handle_translation {x y}
    protected method handle_scale {x y}
    protected method handle_orotation {x y}
    protected method handle_oscale {x y}
    protected method handle_otranslation {x y}
    protected method handle_constrain_rot {coord x y}
    protected method handle_constrain_tran {coord x y}
    protected method handle_configure {}
    protected method handle_expose {}
    protected method doBindings {}
    public method resetBindings {}

    protected variable minScale 0.0001
    protected variable minAxesSize 0.1
    protected variable minAxesLineWidth 0
    protected variable minAxesTickLength 1
    protected variable minAxesTickMajorLength 1
    protected variable minMouseDelta -20
    protected variable maxMouseDelta 20

    protected {
	variable keyPointX 0
	variable keyPointY 0
	variable keyPointZ 0

	variable object ""
	variable mouseFunc ""
    }

    private variable prevMouseX ""
    private variable prevMouseY ""
    private variable geolist ""
    private variable perspective_angle_index 0
    private variable perspective_angles {90 60 45 30}
    private variable doingInit 1
}

########################### Public/Interface Methods ###########################

::itcl::body Display::constructor {args} {
    attach_view
    doBindings
    handle_configure
    eval itk_initialize $args
    set doingInit 0
}

::itcl::configbody Display::rscale {
    if {$itk_option(-rscale) < $minScale} {
	error "rscale must be >= $minScale"
    }
}

::itcl::configbody Display::sscale {
    if {$itk_option(-sscale) < $minScale} {
	error "sscale must be >= $minScale"
    }
}

::itcl::configbody Display::centerDotEnable {
    if {$itk_option(-centerDotEnable) != 0 &&
        $itk_option(-centerDotEnable) != 1} {
	error "value must be 0, 1"
    }

    refresh
}

::itcl::configbody Display::centerDotColor {
    if {[llength $itk_option(-centerDotColor)] != 3} {
	error "values must be {r g b} where 0 <= r/g/b <= 255"
    }

    set r [lindex $itk_option(-centerDotColor) 0]
    set g [lindex $itk_option(-centerDotColor) 1]
    set b [lindex $itk_option(-centerDotColor) 2]

    # validate color
    if {![string is digit $r] ||
	![string is digit $g] ||
        ![string is digit $b] ||
        $r < 0 || 255 < $r ||
        $g < 0 || 255 < $g ||
        $b < 0 || 255 < $b} {

	error "values must be {r g b} where 0 <= r/g/b <= 255"
    }

    refresh
}

#::itcl::configbody Display::showViewingParams {
#    if {$itk_option(-showViewingParams) != 0 &&
#        $itk_option(-showViewingParams) != 1} {
#	error "value must be 0, 1"
#    }
#
#    refresh
#}
#
#::itcl::configbody Display::scaleEnable {
#    if {$itk_option(-scaleEnable) != 0 &&
#        $itk_option(-scaleEnable) != 1} {
#	error "value must be 0, 1"
#    }
#
#    refresh
#}

::itcl::configbody Display::scaleColor {
    if {[llength $itk_option(-scaleColor)] != 3} {
	error "values must be {r g b} where 0 <= r/g/b <= 255"
    }

    set r [lindex $itk_option(-scaleColor) 0]
    set g [lindex $itk_option(-scaleColor) 1]
    set b [lindex $itk_option(-scaleColor) 2]

    # validate color
    if {![string is digit $r] ||
	![string is digit $g] ||
        ![string is digit $b] ||
        $r < 0 || 255 < $r ||
        $g < 0 || 255 < $g ||
        $b < 0 || 255 < $b} {

	error "values must be {r g b} where 0 <= r/g/b <= 255"
    }

    refresh
}

::itcl::configbody Display::primitiveLabelColor {
    if {[llength $itk_option(-primitiveLabelColor)] != 3} {
	error "values must be {r g b} where 0 <= r/g/b <= 255"
    }

    set r [lindex $itk_option(-primitiveLabelColor) 0]
    set g [lindex $itk_option(-primitiveLabelColor) 1]
    set b [lindex $itk_option(-primitiveLabelColor) 2]

    # validate color
    if {![string is digit $r] ||
	![string is digit $g] ||
        ![string is digit $b] ||
        $r < 0 || 255 < $r ||
        $g < 0 || 255 < $g ||
        $b < 0 || 255 < $b} {

	error "values must be {r g b} where 0 <= r/g/b <= 255"
    }

    refresh
}

::itcl::configbody Display::viewingParamsColor {
    if {[llength $itk_option(-viewingParamsColor)] != 3} {
	error "values must be {r g b} where 0 <= r/g/b <= 255"
    }

    set r [lindex $itk_option(-viewingParamsColor) 0]
    set g [lindex $itk_option(-viewingParamsColor) 1]
    set b [lindex $itk_option(-viewingParamsColor) 2]

    # validate color
    if {![string is digit $r] ||
	![string is digit $g] ||
        ![string is digit $b] ||
        $r < 0 || 255 < $r ||
        $g < 0 || 255 < $g ||
        $b < 0 || 255 < $b} {

	error "values must be {r g b} where 0 <= r/g/b <= 255"
    }

    refresh
}

::itcl::configbody Display::viewAxesEnable {
    if {$itk_option(-viewAxesEnable) != 0 &&
        $itk_option(-viewAxesEnable) != 1} {
	error "value must be 0, 1"
    }

    refresh
}

::itcl::configbody Display::modelAxesEnable {
    if {$itk_option(-modelAxesEnable) != 0 &&
        $itk_option(-modelAxesEnable) != 1} {
	error "value must be 0, 1"
    }

    refresh
}

::itcl::configbody Display::viewAxesSize {
    # validate size
    if {![string is double $itk_option(-viewAxesSize)] ||
        $itk_option(-viewAxesSize) < $minAxesSize} {
	    error "-viewAxesSize must be >= $minAxesSize"
    }

    refresh
}

::itcl::configbody Display::modelAxesSize {
    # validate size
    if {![string is double $itk_option(-modelAxesSize)] ||
        $itk_option(-modelAxesSize) < $minAxesSize} {
	    error "-modelAxesSize must be >= $minAxesSize"
    }

    refresh
}

::itcl::configbody Display::viewAxesPosition {
    if {[llength $itk_option(-viewAxesPosition)] != 3} {
	error "values must be {x y z} where x, y and z are numeric"
    }

    set x [lindex $itk_option(-viewAxesPosition) 0]
    set y [lindex $itk_option(-viewAxesPosition) 1]
    set z [lindex $itk_option(-viewAxesPosition) 2]

    # validate center
    if {![string is double $x] ||
	![string is double $y] ||
        ![string is double $z]} {

	error "values must be {x y z} where x, y and z are numeric"
    }

    refresh
}

::itcl::configbody Display::modelAxesPosition {
    if {[llength $itk_option(-modelAxesPosition)] != 3} {
	error "values must be {x y z} where x, y and z are numeric"
    }

    set x [lindex $itk_option(-modelAxesPosition) 0]
    set y [lindex $itk_option(-modelAxesPosition) 1]
    set z [lindex $itk_option(-modelAxesPosition) 2]

    # validate center
    if {![string is double $x] ||
	![string is double $y] ||
        ![string is double $z]} {

	error "values must be {x y z} where x, y and z are numeric"
    }

    # convert to mm
    set local2mm [local2base]
    set itk_option(-modelAxesPosition) [list [expr {$local2mm * $x}] \
	                                     [expr {$local2mm * $y}] \
					     [expr {$local2mm * $z}]]

    refresh
}

::itcl::configbody Display::viewAxesLineWidth {
    # validate line width
    if {![string is digit $itk_option(-viewAxesLineWidth)] ||
        $itk_option(-viewAxesLineWidth) < $minAxesLineWidth} {
	    error "-viewAxesLineWidth must be >= $minAxesLineWidth"
    }

    refresh
}

::itcl::configbody Display::modelAxesLineWidth {
    # validate line width
    if {![string is digit $itk_option(-modelAxesLineWidth)] ||
        $itk_option(-modelAxesLineWidth) < $minAxesLineWidth} {
	    error "-modelAxesLineWidth must be >= $minAxesLineWidth"
    }

    refresh
}

::itcl::configbody Display::viewAxesTripleColor {
    if {$itk_option(-viewAxesTripleColor) != 0 &&
        $itk_option(-viewAxesTripleColor) != 1} {
	error "value must be 0 or 1"
    }

    refresh
}

::itcl::configbody Display::modelAxesTripleColor {
    if {$itk_option(-modelAxesTripleColor) != 0 &&
        $itk_option(-modelAxesTripleColor) != 1} {
	error "value must be 0 or 1"
    }

    refresh
}

::itcl::configbody Display::viewAxesColor {
    if {[llength $itk_option(-viewAxesColor)] != 3} {
	error "values must be {r g b} where 0 <= r/g/b <= 255"
    }

    set r [lindex $itk_option(-viewAxesColor) 0]
    set g [lindex $itk_option(-viewAxesColor) 1]
    set b [lindex $itk_option(-viewAxesColor) 2]

    # validate color
    if {![string is digit $r] ||
	![string is digit $g] ||
        ![string is digit $b] ||
        $r < 0 || 255 < $r ||
        $g < 0 || 255 < $g ||
        $b < 0 || 255 < $b} {

	error "values must be {r g b} where 0 <= r/g/b <= 255"
    }

    refresh
}

::itcl::configbody Display::modelAxesColor {
    if {[llength $itk_option(-modelAxesColor)] != 3} {
	error "values must be {r g b} where 0 <= r/g/b <= 255"
    }

    set r [lindex $itk_option(-modelAxesColor) 0]
    set g [lindex $itk_option(-modelAxesColor) 1]
    set b [lindex $itk_option(-modelAxesColor) 2]

    # validate color
    if {![string is digit $r] ||
	![string is digit $g] ||
	![string is digit $b] ||
        $r < 0 || 255 < $r ||
        $g < 0 || 255 < $g ||
        $b < 0 || 255 < $b} {

	error "values must be {r g b} where 0 <= r/g/b <= 255"
    }

    refresh
}

::itcl::configbody Display::viewAxesLabelColor {
    if {[llength $itk_option(-viewAxesLabelColor)] != 3} {
	error "values must be {r g b} where 0 <= r/g/b <= 255"
    }

    set r [lindex $itk_option(-viewAxesLabelColor) 0]
    set g [lindex $itk_option(-viewAxesLabelColor) 1]
    set b [lindex $itk_option(-viewAxesLabelColor) 2]

    # validate color
    if {![string is digit $r] ||
	![string is digit $g] ||
        ![string is digit $b] ||
        $r < 0 || 255 < $r ||
        $g < 0 || 255 < $g ||
        $b < 0 || 255 < $b} {

	error "values must be {r g b} where 0 <= r/g/b <= 255"
    }

    refresh
}

::itcl::configbody Display::modelAxesLabelColor {
    if {[llength $itk_option(-modelAxesLabelColor)] != 3} {
	error "values must be {r g b} where 0 <= r/g/b <= 255"
    }

    set r [lindex $itk_option(-modelAxesLabelColor) 0]
    set g [lindex $itk_option(-modelAxesLabelColor) 1]
    set b [lindex $itk_option(-modelAxesLabelColor) 2]

    # validate color
    if {![string is digit $r] ||
	![string is digit $g] ||
	![string is digit $b] ||
        $r < 0 || 255 < $r ||
        $g < 0 || 255 < $g ||
        $b < 0 || 255 < $b} {

	error "values must be {r g b} where 0 <= r/g/b <= 255"
    }

    refresh
}

::itcl::configbody Display::modelAxesTickEnable {
    if {$itk_option(-modelAxesTickEnable) != 0 &&
        $itk_option(-modelAxesTickEnable) != 1} {
	error "value must be 0, 1"
    }

    refresh
}

::itcl::configbody Display::modelAxesTickLength {
    # validate tick length
    if {![string is digit $itk_option(-modelAxesTickLength)] ||
        $itk_option(-modelAxesTickLength) < $minAxesTickLength} {
	    error "-modelAxesTickLength must be >= $minAxesTickLength"
    }

    refresh
}

::itcl::configbody Display::modelAxesTickMajorLength {
    # validate major tick length
    if {![string is digit $itk_option(-modelAxesTickMajorLength)] ||
        $itk_option(-modelAxesTickMajorLength) < $minAxesTickMajorLength} {
	    error "-modelAxesTickMajorLength must be >= $minAxesTickMajorLength"
    }

    refresh
}

::itcl::configbody Display::modelAxesTickInterval {
    if {![string is double $itk_option(-modelAxesTickInterval)] ||
        $itk_option(-modelAxesTickInterval) <= 0} {
	error "-modelAxesTickInterval must be > 0"
    }

    # convert to mm
    set itk_option(-modelAxesTickInterval) [expr {[local2base] * $itk_option(-modelAxesTickInterval)}]
    refresh
}

::itcl::configbody Display::modelAxesTicksPerMajor {
    if {![string is digit $itk_option(-modelAxesTicksPerMajor)]} {
	error "-modelAxesTicksPerMajor must be > 0"
    }

    refresh
}

::itcl::configbody Display::modelAxesTickColor {
    if {[llength $itk_option(-modelAxesTickColor)] != 3} {
	error "values must be {r g b} where 0 <= r/g/b <= 255"
    }

    set r [lindex $itk_option(-modelAxesTickColor) 0]
    set g [lindex $itk_option(-modelAxesTickColor) 1]
    set b [lindex $itk_option(-modelAxesTickColor) 2]

    # validate color
    if {![string is digit $r] ||
	![string is digit $g] ||
	![string is digit $b] ||
        $r < 0 || 255 < $r ||
        $g < 0 || 255 < $g ||
        $b < 0 || 255 < $b} {

	error "values must be {r g b} where 0 <= r/g/b <= 255"
    }

    refresh
}

::itcl::configbody Display::modelAxesTickMajorColor {
    if {[llength $itk_option(-modelAxesTickMajorColor)] != 3} {
	error "values must be {r g b} where 0 <= r/g/b <= 255"
    }

    set r [lindex $itk_option(-modelAxesTickMajorColor) 0]
    set g [lindex $itk_option(-modelAxesTickMajorColor) 1]
    set b [lindex $itk_option(-modelAxesTickMajorColor) 2]

    # validate color
    if {![string is digit $r] ||
	![string is digit $g] ||
	![string is digit $b] ||
        $r < 0 || 255 < $r ||
        $g < 0 || 255 < $g ||
        $b < 0 || 255 < $b} {

	error "values must be {r g b} where 0 <= r/g/b <= 255"
    }

    refresh
}

::itcl::configbody Display::modelAxesTickThreshold {
    if {![string is digit $itk_option(-modelAxesTickThreshold)]} {
	error "-modelAxesTickThreshold must be > 1"
    }

    refresh
}

::itcl::body Display::update {obj} {
    refresh
}

::itcl::body Display::refresh {} {
    global tcl_platform

    if {$doingInit} {
	return
    }

    Dm::drawBegin

    if {$itk_option(-perspective)} {
	Dm::loadmat [View::pmodel2view] 0
    } else {
	Dm::loadmat [View::model2view] 0
    }

    if {![info exists itk_option(-fb_active)] || $itk_option(-fb_active) < 2} {

	if {$tcl_platform(os) != "Windows NT"} {
	    if {$itk_option(-fb_active)} {
		# underlay
		Dm::refreshfb
	    }
	}

	foreach geo $geolist {
	    Dm::drawGeom $geo
	}

	Dm::normal

	if {$itk_option(-showViewingParams)} {
	    #set ae [View::ae]
	    set azel [View::ae]
	    set cent [View::center]
	    #set vstr [format "units:%s  size:%g  center:(%g %g %g)  az:%g  el:%g  tw::%g"
	    set vstr [format "units:%s  size:%.2f  center:(%.2f, %.2f, %.2f)  az:%.2f  el:%.2f  tw::%.2f" \
			  [View::units] \
			  [View::size] \
			  [lindex $cent 0] [lindex $cent 1] [lindex $cent 2] \
			  [lindex $azel 0] \
			  [lindex $azel 1] \
			  [lindex $azel 2]]
	    #Dm::drawString $vstr 0.0 -0.965 10 0
	    eval Dm::fg $itk_option(-viewingParamsColor)
	    Dm::drawString $vstr -0.98 -0.965 10 0
	}

	if {$itk_option(-scaleEnable)} {
	    Dm::drawScale [View::size] $itk_option(-scaleColor)
	}

	if {$itk_option(-primitiveLabels) != {}} {
	    ####
	    #XXX At the moment primitive labels are only supported
	    #    for the first drawable geometry object in the list.
	    #
	    set geo [lindex $geolist 0]
	    eval Dm::drawLabels $geo [list $itk_option(-primitiveLabelColor)] $itk_option(-primitiveLabels)
	}

	if {$itk_option(-viewAxesEnable) ||
	    $itk_option(-modelAxesEnable)} {
		set vsize [expr {[View::local2base] * [View::size]}]
		set rmat [View::rmat]
		set model2view [View::model2view]

		if {$itk_option(-viewAxesEnable)} {
		    set x [lindex $itk_option(-viewAxesPosition) 0]
		    set y [lindex $itk_option(-viewAxesPosition) 1]
		    set z [lindex $itk_option(-viewAxesPosition) 2]
		    set y [expr {$y * $invAspect}]
		    set modVAP "$x $y $z"

		    Dm::drawViewAxes $vsize $rmat $modVAP \
			    $itk_option(-viewAxesSize) $itk_option(-viewAxesColor) \
			    $itk_option(-viewAxesLabelColor) $itk_option(-viewAxesLineWidth) \
			    1 $itk_option(-viewAxesTripleColor)
		}

		if {$itk_option(-modelAxesEnable)} {
		    Dm::drawModelAxes $vsize $rmat $itk_option(-modelAxesPosition) \
			    $itk_option(-modelAxesSize) $itk_option(-modelAxesColor) \
			    $itk_option(-modelAxesLabelColor) $itk_option(-modelAxesLineWidth) \
			    0 $itk_option(-modelAxesTripleColor) \
			    $model2view \
			    $itk_option(-modelAxesTickEnable) \
			    $itk_option(-modelAxesTickLength) \
			    $itk_option(-modelAxesTickMajorLength) \
			    $itk_option(-modelAxesTickInterval) \
			    $itk_option(-modelAxesTicksPerMajor) \
			    $itk_option(-modelAxesTickColor) \
			    $itk_option(-modelAxesTickMajorColor) \
			    $itk_option(-modelAxesTickThreshold)
		}
	}

	if {$itk_option(-centerDotEnable)} {
	    Dm::drawCenterDot $itk_option(-centerDotColor)
	}

    } elseif {$tcl_platform(os) != "Windows NT"} {
	# overlay
	Dm::refreshfb
    }

    Dm::drawEnd
}

::itcl::body Display::mouse_nirt {_x _y {gi 0}} {
    set geo [lindex $geolist $gi]

    if {$geo == ""} {
	return "mouse_nirt: bad geometry index - $gi"
    }

    # transform X screen coordinates into normalized view coordinates
    set nvx [expr ($_x * $invWidth - 0.5) * 2.0]
    set nvy [expr (0.5 - $_y * $invHeight) * 2.0 * $invAspect]

    # transform normalized view coordinates into model coordinates
    set mc [mat4x3pnt [view2model] "$nvx $nvy 0"]
    set mc [vscale $mc [base2local]]

    # finally, call nirt (backing out of geometry)
    set v_obj [View::get_viewname]
    eval $geo nirt $v_obj -b $mc
}

::itcl::body Display::nirt {args} {
    set len [llength $args]

    if {$len > 1 && [lindex $args 0] == "-geo"} {
	set index [lindex $args 1]
	set args [lrange $args 2 end]
	set geo [lindex $geolist $index]
    } else {
	set geo [lindex $geolist 0]
    }

    if {$geo == ""} {
	return "nirt: bad geometry index"
    }

    set v_obj [View::get_viewname]
    eval $geo nirt $v_obj $args
}

::itcl::body Display::vnirt {vx vy {gi 0}} {
    set geo [lindex $geolist $gi]

    if {$geo == ""} {
	return "vnirt: bad geometry index - $gi"
    }

    # finally, call vnirt (backing out of geometry)
    set v_obj [View::get_viewname]
    eval $geo vnirt $v_obj -b $vx $vy
}

::itcl::body Display::qray {args} {
    set len [llength $args]

    if {$len > 1 && [lindex $args 0] == "-geo"} {
	set index [lindex $args 1]
	set args [lrange $args 2 end]
	set geo [lindex $geolist $index]
    } else {
	set geo [lindex $geolist 0]
    }

    if {$geo == ""} {
	return "qray: bad geometry index"
    }

    eval $geo qray $args
}

::itcl::body Display::rt {args} {
    global tcl_platform

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

    set v_obj [View::get_viewname]
    if {$tcl_platform(os) != "Windows NT"} {
	eval $geo rt $v_obj -F $itk_option(-listen) -w $width -n $height -V $aspect $args
    } else {
	eval $geo rt $v_obj $args
    }
}

::itcl::body Display::rtabort {{gi 0}} {
    set geo [lindex $geolist $gi]

    if {$geo == ""} {
	return "rtabort: bad geometry index"
    }

    $geo rtabort
}

::itcl::body Display::rtarea {args} {
    set len [llength $args]

    if {$len > 1 && [lindex $args 0] == "-geo"} {
	set index [lindex $args 1]
	set args [lrange $args 2 end]
	set geo [lindex $geolist $index]
    } else {
	set geo [lindex $geolist 0]
    }

    if {$geo == ""} {
	return "rtarea: bad geometry index"
    }

    set v_obj [View::get_viewname]
    eval $geo rtarea $v_obj -V $aspect $args
}

::itcl::body Display::rtcheck {args} {
    global tcl_platform

    set len [llength $args]

    if {$len > 1 && [lindex $args 0] == "-geo"} {
	set index [lindex $args 1]
	set args [lrange $args 2 end]
	set geo [lindex $geolist $index]
    } else {
	set geo [lindex $geolist 0]
    }

    if {$geo == ""} {
	return "rtcheck: bad geometry index"
    }

    set v_obj [View::get_viewname]
    eval $geo rtcheck $v_obj $args
}

::itcl::body Display::rtedge {args} {
    global tcl_platform

    set len [llength $args]

    if {$len > 1 && [lindex $args 0] == "-geo"} {
	set index [lindex $args 1]
	set args [lrange $args 2 end]
	set geo [lindex $geolist $index]
    } else {
	set geo [lindex $geolist 0]
    }

    if {$geo == ""} {
	return "rtedge: bad geometry index"
    }

    set v_obj [View::get_viewname]
    if {$tcl_platform(os) != "Windows NT"} {
	eval $geo rtedge $v_obj -F $itk_option(-listen) -w $width -n $height -V $aspect $args
    } else {
	eval $geo rtedge $v_obj $args
    }
}

::itcl::body Display::rtweight {args} {
    set len [llength $args]

    if {$len > 1 && [lindex $args 0] == "-geo"} {
	set index [lindex $args 1]
	set args [lrange $args 2 end]
	set geo [lindex $geolist $index]
    } else {
	set geo [lindex $geolist 0]
    }

    if {$geo == ""} {
	return "rtweight: bad geometry index"
    }

    set v_obj [View::get_viewname]
    eval $geo rtweight $v_obj -V $aspect $args
}

::itcl::body Display::autoview {{g_index 0}} {
    if {$g_index < [llength $geolist]} {
	set geo [lindex $geolist $g_index]
	if {$itk_option(-usePhony)} {
	    set aview [$geo get_autoview -p]
	} else {
	    set aview [$geo get_autoview]
	}
	catch {eval [lrange $aview 0 1]}
	catch {eval [lrange $aview 2 3]}
    }
}

::itcl::body Display::attach_view {} {
    View::observer attach $this
}

::itcl::body Display::attach_drawable {dg} {
    $dg observer attach $this
}

::itcl::body Display::detach_view {} {
    View::observer detach $this
}

::itcl::body Display::detach_drawable {dg} {
    $dg observer detach $this
}

::itcl::body Display::add {glist} {
    if [llength $geolist] {
	set blank 0
    } else {
	set blank 1
    }

    foreach geo $glist {
	set index [lsearch $geolist $geo]

	# already in list
	if {$index != -1} {
	    continue
	}

	lappend geolist $geo
	attach_drawable $geo
    }

    if {$blank} {
	detach_view
	autoview
	attach_view
    }

    refresh
}

::itcl::body Display::remove {glist} {
    foreach geo $glist {
	set index [lsearch $geolist $geo]
	if {$index == -1} {
	    continue
	}

	set geolist [lreplace $geolist $index $index]
	detach_drawable $geo
    }

    refresh
}

::itcl::body Display::contents {} {
    return $geolist
}

########################### Public Methods That Override ###########################
::itcl::body Display::slew {args} {
    if {[llength $args] == 2} {
	set x1 [lindex $args 0]
	set y1 [lindex $args 1]

	set x2 [expr $width * 0.5]
	set y2 [expr $height * 0.5]
	set sf [expr 2.0 * $invWidth]

	set _x [expr ($x1 - $x2) * $sf]
	set _y [expr ($y2 - $y1) * $sf]
	View::slew $_x $_y
    } else {
	eval View::slew $args
    }
}

::itcl::body Display::perspective_angle {args} {
    if {$args == ""} {
	# get perspective angle
	return $perspective_angle
    } else {
	# set perspective angle
	View::perspective $args
    }

    if {$perspective_angle > 0} {
	# turn perspective mode on
	Dm::perspective 1
    } else {
	# turn perspective mode off
	Dm::perspective 0
    }

    refresh
    return $perspective_angle
}

::itcl::body Display::perspective {args} {
    eval Dm::perspective $args

    if {$itk_option(-perspective)} {
	View::perspective [lindex $perspective_angles $perspective_angle_index]
    } else {
	View::perspective -1
    }

    refresh
    return $itk_option(-perspective)
}

if {$tcl_platform(os) != "Windows NT"} {
    ::itcl::body Display::fb_active {args} {
	if {$args == ""} {
	    return $itk_option(-fb_active)
	} else {
	    eval Dm::fb_active $args
	    refresh
	}
    }
}

::itcl::body Display::light {args} {
    eval Dm::light $args
    refresh
    return $itk_option(-light)
}

::itcl::body Display::transparency {args} {
    eval Dm::transparency $args
    refresh
    return $itk_option(-transparency)
}

::itcl::body Display::bounds {args} {
    if {$args == ""} {
	return [Dm::bounds]
    }

    eval Dm::bounds $args
    refresh
}

::itcl::body Display::depthMask {args} {
    eval Dm::depthMask $args
    refresh
    return $itk_option(-depthMask)
}

::itcl::body Display::zbuffer {args} {
    eval Dm::zbuffer $args
    refresh
    return $itk_option(-zbuffer)
}

::itcl::body Display::zclip {args} {
    eval Dm::zclip $args
    refresh
    return $itk_option(-zclip)
}

########################### Protected Methods ###########################
::itcl::body Display::toggle_modelAxesEnable {} {
    if {$itk_option(-modelAxesEnable)} {
	set itk_option(-modelAxesEnable) 0
    } else {
	set itk_option(-modelAxesEnable) 1
    }

    refresh
}

::itcl::body Display::toggle_modelAxesTickEnable {} {
    if {$itk_option(-modelAxesTickEnable)} {
	set itk_option(-modelAxesTickEnable) 0
    } else {
	set itk_option(-modelAxesTickEnable) 1
    }

    refresh
}

::itcl::body Display::toggle_viewAxesEnable {} {
    if {$itk_option(-viewAxesEnable)} {
	set itk_option(-viewAxesEnable) 0
    } else {
	set itk_option(-viewAxesEnable) 1
    }

    refresh
}

::itcl::body Display::toggle_centerDotEnable {} {
    if {$itk_option(-centerDotEnable)} {
	set itk_option(-centerDotEnable) 0
    } else {
	set itk_option(-centerDotEnable) 1
    }

    refresh
}

::itcl::body Display::toggle_scaleEnable {} {
    if {$itk_option(-scaleEnable)} {
	set itk_option(-scaleEnable) 0
    } else {
	set itk_option(-scaleEnable) 1
    }

    refresh
}

::itcl::body Display::? {} {
    return "[View::?][Dm::?]"
}

::itcl::body Display::apropos {key} {
    return "[View::apropos $key] [Dm::apropos $key]"
}

::itcl::body Display::help {args} {
    if {[llength $args] && [lindex $args 0] != {}} {
	if {[catch {eval View::help $args} result]} {
	    set result [eval Dm::help $args]
	}

	return $result
    }

    # list all help messages for QuadDisplay and Db
    return "[View::help][Dm::help]"
}

::itcl::body Display::getUserCmds {} {
    eval lappend cmds [View::getUserCmds] [Dm::getUserCmds]
    return $cmds
}

::itcl::body Display::toggle_zclip {} {
    Dm::toggle_zclip
    refresh
    return $itk_option(-zclip)
}

::itcl::body Display::toggle_zbuffer {} {
    Dm::toggle_zbuffer
    refresh
    return $itk_option(-zbuffer)
}

::itcl::body Display::toggle_light {} {
    Dm::toggle_light
    refresh
    return $itk_option(-light)
}

::itcl::body Display::toggle_perspective {} {
    Dm::toggle_perspective

    if {$itk_option(-perspective)} {
	View::perspective [lindex $perspective_angles $perspective_angle_index]
    } else {
	View::perspective -1
    }

    refresh
    return $itk_option(-perspective)
}

::itcl::body Display::toggle_perspective_angle {} {
    if {$perspective_angle_index == 3} {
	set perspective_angle_index 0
    } else {
	incr perspective_angle_index
    }

    if {$itk_option(-perspective)} {
	View::perspective [lindex $perspective_angles $perspective_angle_index]
    }
}

::itcl::body Display::toggle_transparency {} {
    Dm::toggle_transparency
    refresh
    return $itk_option(-transparency)
}

::itcl::body Display::idle_mode {} {
    # stop receiving motion events
    bind $itk_component(dm) <Motion> {}
}

::itcl::body Display::rotate_mode {_x _y} {
    set prevMouseX $_x
    set prevMouseY $_y

    # start receiving motion events
    bind $itk_component(dm) <Motion> "[::itcl::code $this handle_rotation %x %y]; break"
}

::itcl::body Display::translate_mode {_x _y} {
    set prevMouseX $_x
    set prevMouseY $_y

    # start receiving motion events
    bind $itk_component(dm) <Motion> "[::itcl::code $this handle_translation %x %y]; break"
}

::itcl::body Display::scale_mode {_x _y} {
    set prevMouseX $_x
    set prevMouseY $_y

    # start receiving motion events
    bind $itk_component(dm) <Motion> "[::itcl::code $this handle_scale %x %y]; break"
}

::itcl::body Display::orotate_mode {_x _y func obj kx ky kz} {
    set mouseFunc $func
    set object $obj
    set keyPointX $kx
    set keyPointY $ky
    set keyPointZ $kz

    set prevMouseX $_x
    set prevMouseY $_y

    # start receiving motion events
    bind $itk_component(dm) <Motion> "[::itcl::code $this handle_orotation %x %y]; break"
}

::itcl::body Display::oscale_mode {_x _y func obj kx ky kz} {
    set mouseFunc $func
    set object $obj
    set keyPointX $kx
    set keyPointY $ky
    set keyPointZ $kz

    set prevMouseX $_x
    set prevMouseY $_y

    # start receiving motion events
    bind $itk_component(dm) <Motion> "[::itcl::code $this handle_oscale %x %y]; break"
}

::itcl::body Display::otranslate_mode {_x _y func obj} {
    set mouseFunc $func
    set object $obj

    set prevMouseX $_x
    set prevMouseY $_y

    # start receiving motion events
    bind $itk_component(dm) <Motion> "[::itcl::code $this handle_otranslation %x %y]; break"
}

::itcl::body Display::screen2model {x y} {
    return [eval v2mPoint [screen2view $x $y]]
}

::itcl::body Display::screen2view {x y} {
    set vx [expr {$x * $invWidth * 2.0 - 1.0}]
    set vy [expr {(-$y * $invHeight * 2.0 + 1.0) * $invAspect}]

    return [list $vx $vy 0]
}

::itcl::body Display::constrain_rmode {coord _x _y} {
    set prevMouseX $_x
    set prevMouseY $_y

    # start receiving motion events
    bind $itk_component(dm) <Motion> "[::itcl::code $this handle_constrain_rot $coord %x %y]; break"
}

::itcl::body Display::constrain_tmode {coord _x _y} {
    set prevMouseX $_x
    set prevMouseY $_y

    # start receiving motion events
    bind $itk_component(dm) <Motion> "[::itcl::code $this handle_constrain_tran $coord %x %y]; break"
}

::itcl::body Display::handle_rotation {_x _y} {
    set dx [expr {$prevMouseY - $_y}]
    set dy [expr {$prevMouseX - $_x}]

    set prevMouseX $_x
    set prevMouseY $_y

    if {$dx < $minMouseDelta} {
	set dx $minMouseDelta
    } elseif {$maxMouseDelta < $dx} {
	set dx $maxMouseDelta
    }

    if {$dy < $minMouseDelta} {
	set dy $minMouseDelta
    } elseif {$maxMouseDelta < $dy} {
	set dy $maxMouseDelta
    }

    set dx [expr {$dx * $itk_option(-rscale)}]
    set dy [expr {$dy * $itk_option(-rscale)}]

    catch {vrot $dx $dy 0}
}

::itcl::body Display::handle_translation {_x _y} {
    set dx [expr {$prevMouseX - $_x}]
    set dy [expr {$_y - $prevMouseY}]

    set prevMouseX $_x
    set prevMouseY $_y

    if {$dx < $minMouseDelta} {
	set dx $minMouseDelta
    } elseif {$maxMouseDelta < $dx} {
	set dx $maxMouseDelta
    }

    if {$dy < $minMouseDelta} {
	set dy $minMouseDelta
    } elseif {$maxMouseDelta < $dy} {
	set dy $maxMouseDelta
    }

    set size [View::size]
    set dx [expr {$dx * $invWidth * $size}]
    set dy [expr {$dy * $invWidth * $size}]

    catch {vtra $dx $dy 0}
}

::itcl::body Display::handle_scale {_x _y} {
    set dx [expr {$_x - $prevMouseX}]
    set dy [expr {$prevMouseY - $_y}]

    set prevMouseX $_x
    set prevMouseY $_y

    if {$dx < $minMouseDelta} {
	set dx $minMouseDelta
    } elseif {$maxMouseDelta < $dx} {
	set dx $maxMouseDelta
    }

    if {$dy < $minMouseDelta} {
	set dy $minMouseDelta
    } elseif {$maxMouseDelta < $dy} {
	set dy $maxMouseDelta
    }

    set dx [expr {$dx * $invWidth * $itk_option(-sscale)}]
    set dy [expr {$dy * $invWidth * $itk_option(-sscale)}]

    if {[expr {abs($dx) > abs($dy)}]} {
	set f [expr 1.0 + $dx]
    } else {
	set f [expr 1.0 + $dy]
    }

    catch {zoom $f}
}

::itcl::body Display::handle_orotation {_x _y} {
    set dx [expr {$_y - $prevMouseY}]
    set dy [expr {$_x - $prevMouseX}]

    set prevMouseX $_x
    set prevMouseY $_y

    if {$dx < $minMouseDelta} {
	set dx $minMouseDelta
    } elseif {$maxMouseDelta < $dx} {
	set dx $maxMouseDelta
    }

    if {$dy < $minMouseDelta} {
	set dy $minMouseDelta
    } elseif {$maxMouseDelta < $dy} {
	set dy $maxMouseDelta
    }

    set dx [expr {$dx * $itk_option(-rscale)}]
    set dy [expr {$dy * $itk_option(-rscale)}]
    set model [mrotPoint $dx $dy 0]
    set mx [lindex $model 0]
    set my [lindex $model 1]
    set mz [lindex $model 2]

    catch {$mouseFunc $object $mx $my $mz $keyPointX $keyPointY $keyPointZ}
}

::itcl::body Display::handle_oscale {_x _y} {
    set dx [expr {$_x - $prevMouseX}]
    set dy [expr {$prevMouseY - $_y}]

    set prevMouseX $_x
    set prevMouseY $_y

    if {$dx < $minMouseDelta} {
	set dx $minMouseDelta
    } elseif {$maxMouseDelta < $dx} {
	set dx $maxMouseDelta
    }

    if {$dy < $minMouseDelta} {
	set dy $minMouseDelta
    } elseif {$maxMouseDelta < $dy} {
	set dy $maxMouseDelta
    }

    set dx [expr {$dx * $invWidth * $itk_option(-sscale)}]
    set dy [expr {$dy * $invWidth * $itk_option(-sscale)}]

    if {[expr {abs($dx) > abs($dy)}]} {
	set sf [expr 1.0 + $dx]
    } else {
	set sf [expr 1.0 + $dy]
    }

    catch {$mouseFunc $object $sf $keyPointX $keyPointY $keyPointZ}
}

::itcl::body Display::handle_otranslation {_x _y} {
    set dx [expr {$_x - $prevMouseX}]
    set dy [expr {$prevMouseY - $_y}]

    set prevMouseX $_x
    set prevMouseY $_y

    if {$dx < $minMouseDelta} {
	set dx $minMouseDelta
    } elseif {$maxMouseDelta < $dx} {
	set dx $maxMouseDelta
    }

    if {$dy < $minMouseDelta} {
	set dy $minMouseDelta
    } elseif {$maxMouseDelta < $dy} {
	set dy $maxMouseDelta
    }

    set size [View::size]
    set dx [expr {$dx * $invWidth * $size}]
    set dy [expr {$dy * $invWidth * $size}]
    set model [mrotPoint $dx $dy 0]
    set mx [lindex $model 0]
    set my [lindex $model 1]
    set mz [lindex $model 2]

    catch {$mouseFunc $object $mx $my $mz}
}

::itcl::body Display::handle_constrain_rot {coord _x _y} {
    set dx [expr {$prevMouseX - $_x}]
    set dy [expr {$_y - $prevMouseY}]

    set prevMouseX $_x
    set prevMouseY $_y

    if {$dx < $minMouseDelta} {
	set dx $minMouseDelta
    } elseif {$maxMouseDelta < $dx} {
	set dx $maxMouseDelta
    }

    if {$dy < $minMouseDelta} {
	set dy $minMouseDelta
    } elseif {$maxMouseDelta < $dy} {
	set dy $maxMouseDelta
    }

    set dx [expr {$dx * $itk_option(-rscale)}]
    set dy [expr {$dy * $itk_option(-rscale)}]

    if [expr abs($dx) > abs($dy)] {
	set f $dx
    } else {
	set f $dy
    }
    switch $coord {
	x {
	    catch {rot $f 0 0}
	}
	y {
	    catch {rot 0 $f 0}
	}
	z {
	    catch {rot 0 0 $f}
	}
    }
}

::itcl::body Display::handle_constrain_tran {coord _x _y} {
    set dx [expr {$prevMouseX - $_x}]
    set dy [expr {$_y - $prevMouseY}]

    set prevMouseX $_x
    set prevMouseY $_y

    if {$dx < $minMouseDelta} {
	set dx $minMouseDelta
    } elseif {$maxMouseDelta < $dx} {
	set dx $maxMouseDelta
    }

    if {$dy < $minMouseDelta} {
	set dy $minMouseDelta
    } elseif {$maxMouseDelta < $dy} {
	set dy $maxMouseDelta
    }

    set dx [expr {$dx * $invWidth * [View::size]}]
    set dy [expr {$dy * $invWidth * [View::size]}]

    if {[expr {abs($dx) > abs($dy)}]} {
	set f $dx
    } else {
	set f $dy
    }
    switch $coord {
	x {
	    catch {tra $f 0 0}
	}
	y {
	    catch {tra 0 $f 0}
	}
	z {
	    catch {tra 0 0 $f}
	}
    }
}

::itcl::body Display::handle_configure {} {
    Dm::handle_configure
    refresh
}

::itcl::body Display::handle_expose {} {
    refresh
}

::itcl::body Display::doBindings {} {
    global tcl_platform

    if {$tcl_platform(os) != "Windows NT"} {
	bind $itk_component(dm) <Enter> "focus $itk_component(dm);"
    }

    bind $itk_component(dm) <Configure> "[::itcl::code $this handle_configure]; break"
    bind $itk_component(dm) <Expose> "[::itcl::code $this handle_expose]; break"

    # Mouse Bindings
    bind $itk_component(dm) <1> "$this zoom 0.5; break"
    bind $itk_component(dm) <2> "$this slew %x %y; break"
    bind $itk_component(dm) <3> "$this zoom 2.0; break"

    # Idle Mode
    bind $itk_component(dm) <ButtonRelease> "[::itcl::code $this idle_mode]; break"
    bind $itk_component(dm) <KeyRelease-Control_L> "[::itcl::code $this idle_mode]; break"
    bind $itk_component(dm) <KeyRelease-Control_R> "[::itcl::code $this idle_mode]; break"
    bind $itk_component(dm) <KeyRelease-Shift_L> "[::itcl::code $this idle_mode]; break"
    bind $itk_component(dm) <KeyRelease-Shift_R> "[::itcl::code $this idle_mode]; break"
    bind $itk_component(dm) <KeyRelease-Alt_L> "[::itcl::code $this idle_mode]; break"
    bind $itk_component(dm) <KeyRelease-Alt_R> "[::itcl::code $this idle_mode]; break"

    # Rotate Mode
    bind $itk_component(dm) <Control-ButtonPress-1> "[::itcl::code $this rotate_mode %x %y]; break"
    bind $itk_component(dm) <Control-ButtonPress-2> "[::itcl::code $this rotate_mode %x %y]; break"
    bind $itk_component(dm) <Control-ButtonPress-3> "[::itcl::code $this rotate_mode %x %y]; break"

    # Translate Mode
    bind $itk_component(dm) <Shift-ButtonPress-1> "[::itcl::code $this translate_mode %x %y]; break"
    bind $itk_component(dm) <Shift-ButtonPress-2> "[::itcl::code $this translate_mode %x %y]; break"
    bind $itk_component(dm) <Shift-ButtonPress-3> "[::itcl::code $this translate_mode %x %y]; break"

    # Scale Mode
    bind $itk_component(dm) <Control-Shift-ButtonPress-1> "[::itcl::code $this scale_mode %x %y]; break"
    bind $itk_component(dm) <Control-Shift-ButtonPress-2> "[::itcl::code $this scale_mode %x %y]; break"
    bind $itk_component(dm) <Control-Shift-ButtonPress-3> "[::itcl::code $this scale_mode %x %y]; break"

    # Constrained Rotate Mode
    bind $itk_component(dm) <Alt-Control-ButtonPress-1> "[::itcl::code $this constrain_rmode x %x %y]; break"
    bind $itk_component(dm) <Alt-Control-ButtonPress-2> "[::itcl::code $this constrain_rmode y %x %y]; break"
    bind $itk_component(dm) <Alt-Control-ButtonPress-3> "[::itcl::code $this constrain_rmode z %x %y]; break"

    # Constrained Translate Mode
    bind $itk_component(dm) <Alt-Shift-ButtonPress-1> "[::itcl::code $this constrain_tmode x %x %y]; break"
    bind $itk_component(dm) <Alt-Shift-ButtonPress-2> "[::itcl::code $this constrain_tmode y %x %y]; break"
    bind $itk_component(dm) <Alt-Shift-ButtonPress-3> "[::itcl::code $this constrain_tmode z %x %y]; break"

    # Constrained Scale Mode
    bind $itk_component(dm) <Alt-Control-Shift-ButtonPress-1> "[::itcl::code $this scale_mode %x %y]; break"
    bind $itk_component(dm) <Alt-Control-Shift-ButtonPress-2> "[::itcl::code $this scale_mode %x %y]; break"
    bind $itk_component(dm) <Alt-Control-Shift-ButtonPress-3> "[::itcl::code $this scale_mode %x %y]; break"

    # Key Bindings
    bind $itk_component(dm) 3 "$this ae \"35 25 0\"; break"
    bind $itk_component(dm) 4 "$this ae \"45 45 0\"; break"
    bind $itk_component(dm) f "$this ae \"0 0 0\"; break"
    bind $itk_component(dm) R "$this ae \"180 0 0\"; break"
    bind $itk_component(dm) r "$this ae \"270 0 0\"; break"
    bind $itk_component(dm) l "$this ae \"90 0 0\"; break"
    bind $itk_component(dm) t "$this ae \"0 90 0\"; break"
    bind $itk_component(dm) b "$this ae \"0 270 0\"; break"
    bind $itk_component(dm) m "[::itcl::code $this toggle_modelAxesEnable]; break"
    bind $itk_component(dm) T "[::itcl::code $this toggle_modelAxesTickEnable]; break"
    bind $itk_component(dm) v "[::itcl::code $this toggle_viewAxesEnable]; break"
    bind $itk_component(dm) <F2> "[::itcl::code $this toggle_zclip]; break"
    bind $itk_component(dm) <F3> "[::itcl::code $this toggle_perspective]; break"
    bind $itk_component(dm) <F4> "[::itcl::code $this toggle_zbuffer]; break"
    bind $itk_component(dm) <F5> "[::itcl::code $this toggle_light]; break"
    bind $itk_component(dm) <F6> "[::itcl::code $this toggle_perspective_angle]; break"
    bind $itk_component(dm) <F10> "[::itcl::code $this toggle_transparency]; break"
}

::itcl::body Display::resetBindings {} {
    Dm::doBindings
    doBindings
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
