#                      L E G E N D . T C L
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
#       Widget for displaying colors and their corresponding values.
#

option add *Legend.height 30 widgetDefault

::itcl::class cadwidgets::Legend {
    inherit itk::Widget

    constructor {args} {}
    destructor {}

    itk_option define -range range Range {0.0 1.0}
    itk_option define -rgbRange rgbRange RgbRange {{255 255 255} {255 0 0}}
    itk_option define -slots slots Slots 10
    itk_option define -colorFunc colorFunc ColorFunc ""

    public method drawToCanvas {c x y w h tags}
    public method update {}
    public method getColor {val}
    public method rampRGB {val}
    public method postscript {args}
    public method rgbValid {r g b}

    private variable low 0.0
    private variable high 1.0
    private variable dv 1.0
    private variable invDv 1.0

    # colors to ramp between
    private variable r1 255
    private variable g1 255
    private variable b1 255
    private variable r2 255
    private variable g2 0
    private variable b2 0

    # ranges for r, g and b
    private variable dR 0
    private variable dG -255
    private variable dB -255

    # used for clamping
    private variable lowR 255
    private variable lowG 0
    private variable lowB 0
    private variable highR 255
    private variable highG 255
    private variable highB 255

    # x and y offsets for drawing legend
    private variable xoff 20
    private variable yoff 15
}

::itcl::configbody cadwidgets::Legend::range {
    if {[llength $itk_option(-range)] != 2} {
	error "range: must specify min and max"
    }

    set min [lindex $itk_option(-range) 0]
    set max [lindex $itk_option(-range) 1]

    if {![string is double $min]} {
	error "range: bad value - $min"
    }

    if {![string is double $max]} {
	error "range: bad value - $max"
    }

    if {$max <= $min} {
	error "range: max <= min not allowed"
    }

    set low $min
    set high $max
    set dv [expr {$high - $low}]
    set invDv [expr {1.0 / $dv}]

    cadwidgets::Legend::update
}

::itcl::configbody cadwidgets::Legend::rgbRange {
    if {[llength $itk_option(-rgbRange)] != 2} {
	error "rgbRange: must specify two RGB's"
    }

    set rgb1 [lindex $itk_option(-rgbRange) 0]
    set rgb2 [lindex $itk_option(-rgbRange) 1]

    # must have three values for each color
    if {[llength $rgb1] != 3} {
	error "rgbRange: must specify R, G, and B for each color"
    }

    if {[llength $rgb2] != 3} {
	error "rgbRange: must specify R, G, and B for each color"
    }

    if {![eval cadwidgets::Legend::rgbValid $rgb1]} {
	error "Improper color specification - $rgb1"
    }

    if {![eval cadwidgets::Legend::rgbValid $rgb2]} {
	error "Improper color specification - $rgb2"
    }

    set r1 [lindex $rgb1 0]
    set g1 [lindex $rgb1 1]
    set b1 [lindex $rgb1 2]
    set r2 [lindex $rgb2 0]
    set g2 [lindex $rgb2 1]
    set b2 [lindex $rgb2 2]

    set dR [expr $r2 - $r1]
    set dG [expr $g2 - $g1]
    set dB [expr $b2 - $b1]

    if {$r1 <= $r2} {
	set lowR $r1
	set highR $r2
    } else {
	set lowR $r2
	set highR $r1
    }

    if {$g1 <= $g2} {
	set lowG $g1
	set highG $g2
    } else {
	set lowG $g2
	set highG $g1
    }

    if {$b1 <= $b2} {
	set lowB $b1
	set highB $b2
    } else {
	set lowB $b2
	set highB $b1
    }

    cadwidgets::Legend::update
}

::itcl::configbody cadwidgets::Legend::slots {
    if {$itk_option(-slots) < 2} {
	error "Must be 2 or greater"
    }

    cadwidgets::Legend::update
}

::itcl::body cadwidgets::Legend::constructor {args} {
    itk_component add canvas {
	::canvas $itk_interior.canvas
    } {
	usual
	keep -width -height
    }

    eval itk_initialize $args
    ::bind $itk_component(canvas) <Configure> [::itcl::code $this update]
    pack $itk_component(canvas) -expand yes -fill both
}

::itcl::body cadwidgets::Legend::destructor {} {
}

::itcl::body cadwidgets::Legend::drawToCanvas {c x y w h tags} {
    # calculate slot increment
    set si [expr {$w / double($itk_option(-slots))}]

    # calculate value increment
    set vi [expr {$dv / double($itk_option(-slots) - 1)}]

    set y1 $y
    set y2 [expr {$h + $y1}]
    for {set i 0} {$i < $itk_option(-slots)} {incr i} {
	set x1 [expr {int($i * $si) + $x}]
	set x2 [expr {int(($i + 1) * $si) + $x}]
	set val [expr {$vi * $i + $low}]
	if {$itk_option(-colorFunc) == ""} {
	    set rgb [eval format "#%.2x%.2x%.2x" [rampRGB $val]]
	} else {
	    set rgb [eval format "#%.2x%.2x%.2x" [eval $itk_option(-colorFunc) $low $high $val]]
	}
	$c create rectangle $x1 $y1 $x2 $y2 \
		-outline "" -fill $rgb -tags $tags
    }
    $c create text $x $y -text $low -anchor s -tags $tags
    $c create text [expr {$w + $x}] $y -text $high -anchor s -tags $tags

    return
}

::itcl::body cadwidgets::Legend::update {} {
    $itk_component(canvas) delete all
    set w [expr {[winfo width $itk_component(canvas)] - (2 * $xoff)}]
    set h [expr {[winfo height $itk_component(canvas)] - $yoff}]
    drawToCanvas $itk_component(canvas) $xoff $yoff $w $h legend
}

::itcl::body cadwidgets::Legend::getColor {val} {
    if {$itk_option(-colorFunc) != ""} {
	return [$itk_option(-colorFunc) $low $high $val]
    } else {
	return [rampRGB $val]
    }
}

::itcl::body cadwidgets::Legend::rampRGB {val} {
    set sv [expr {($val - $low) * $invDv}]
    set r [expr {int($r1 + $sv * $dR)}]
    set g [expr {int($g1 + $sv * $dG)}]
    set b [expr {int($b1 + $sv * $dB)}]

    # clamp r, g and b
    if {$r < $lowR} {
	set r $lowR
    } elseif {$highR < $r} {
	set r $highR
    }

    if {$g < $lowG} {
	set g $lowG
    } elseif {$highG < $g} {
	set g $highG
    }

    if {$b < $lowB} {
	set b $lowB
    } elseif {$highB < $b} {
	set b $highB
    }

    return "$r $g $b"
}

::itcl::body cadwidgets::Legend::postscript {args} {
    eval $itk_component(canvas) postscript $args
}

::itcl::body cadwidgets::Legend::rgbValid {r g b} {
    if {![string is integer $r]} {
	    return 0
    }
    if {![string is integer $g]} {
	    return 0
    }
    if {![string is integer $b]} {
	    return 0
    }
    if {$r < 0 || 255 < $r} {
	return 0
    }
    if {$g < 0 || 255 < $g} {
	return 0
    }
    if {$b < 0 || 255 < $b} {
	return 0
    }

    return 1
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
