##                 L E G E N D . T C L
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
#       Widget for displaying colors and their corresponding values.
#

option add *Legend.height 30 widgetDefault

class cadwidgets::Legend {
    inherit itk::Widget

    constructor {args} {}
    destructor {}

    itk_option define -range range Range {0.0 1.0}
    itk_option define -rgbRange rgbRange RgbRange {{255 255 255} {255 0 0}}
    itk_option define -slots slots Slots 10
    itk_option define -colorFunc colorFunc ColorFunc ""

    public method drawToCanvas {c w h tags}
    public method update {}
    public method getColor {val}
    public method rampRGB {val}
    public method postscript {args}

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

configbody cadwidgets::Legend::range {
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

configbody cadwidgets::Legend::rgbRange {
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

configbody cadwidgets::Legend::slots {
    if {$itk_option(-slots) < 2} {
	error "Must be 2 or greater"
    }

    cadwidgets::Legend::update
}

body cadwidgets::Legend::constructor {args} {
    itk_component add canvas {
	::canvas $itk_interior.canvas
    } {
	usual
	keep -width -height
    }

    eval itk_initialize $args
    ::bind $itk_component(canvas) <Configure> [code $this update]
    pack $itk_component(canvas) -expand yes -fill both
}

body cadwidgets::Legend::destructor {} {
}

body cadwidgets::Legend::drawToCanvas {c w h tags} {
    # calculate slot increment
    set si [expr {$w / double($itk_option(-slots))}]

    # calculate value increment
    set vi [expr {$dv / double($itk_option(-slots) - 1)}]

    set y1 $yoff
    set y2 [expr {$h + $y1}]
    for {set i 0} {$i < $itk_option(-slots)} {incr i} {
	set x1 [expr {int($i * $si) + $xoff}]
	set x2 [expr {int(($i + 1) * $si) + $xoff}]
	set val [expr {$vi * $i + $low}]
	if {$itk_option(-colorFunc) == ""} {
	    set rgb [eval format "#%.2x%.2x%.2x" [rampRGB $val]]
	} else {
	    set rgb [eval format "#%.2x%.2x%.2x" [eval $itk_option(-colorFunc) $low $high $val]]
	}
	$c create rectangle $x1 $y1 $x2 $y2 \
		-outline "" -fill $rgb -tags $tags
    }
    $c create text $xoff $yoff -text $low -anchor s -tags $tags
    $c create text [expr {$w + $xoff}] $yoff -text $high -anchor s -tags $tags

    return
}

body cadwidgets::Legend::update {} {
    $itk_component(canvas) delete all
    set w [expr {[winfo width $itk_component(canvas)] - (2 * $xoff)}]
    set h [expr {[winfo height $itk_component(canvas)] - $yoff}]
    drawToCanvas $itk_component(canvas) $w $h legend
}

body cadwidgets::Legend::getColor {val} {
    if {$itk_option(-colorFunc) != ""} {
	return [$itk_option(-colorFunc) $low $high $val]
    } else {
	return [rampRGB $val]
    }
}

body cadwidgets::Legend::rampRGB {val} {
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

body cadwidgets::Legend::postscript {args} {
    eval $itk_component(canvas) postscript $args
}
