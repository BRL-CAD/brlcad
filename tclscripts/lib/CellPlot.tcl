##                 C E L L P L O T . T C L
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
#	As the name indicates, instances of CellPlot are intended
#       to be used for cell plots.
#
class cadwidgets::CellPlot {
    inherit iwidgets::Scrolledcanvas

    constructor {args} {}
    destructor {}

    itk_option define -range range Range {0.0 1.0}

    public method createCell {x1 y1 x2 y2 args}

    private method transform {x1 y1 x2 y2}
    private method configureWinCB {width height}

    private variable min 0.0
    private variable max 1.0
    private variable sf 1.0
}

configbody cadwidgets::CellPlot::range {
    if {[llength $itk_option(-range)] != 2} {
	error "range: two arguments are required"
    }

    set _min [lindex $itk_option(-range) 0]
    set _max [lindex $itk_option(-range) 1]

    if {![string is double $_min]} {
	error "range: values must be doubles"
    }

    if {![string is double $_max]} {
	error "range: values must be doubles"
    }

    if {$_max <= $_min} {
	error "range: bad values, max <= min"
    }

    set min $_min
    set max $_max
    set sf [expr {1.0 / double($max - $min)}]
}

body cadwidgets::CellPlot::constructor {args} {
    eval itk_initialize $args
    ::bind [childsite] <Configure> [code $this configureWinCB %w %h]
}

## - createCell
#
# Transform the data coodinates into canvas coordinates,
# then draw the cell in the specified color.
#
body cadwidgets::CellPlot::createCell {x1 y1 x2 y2 args} {
    eval create rectangle [transform $x1 $y1 $x2 $y2] $args
}

## - transform
#
# Transform data coordinates into canvas coordinates.
#
body cadwidgets::CellPlot::transform {x1 y1 x2 y2} {
    set tx1 [expr {($x1 - $min) * $sf * $itk_option(-width)}]
    set tx2 [expr {($x2 - $min) * $sf * $itk_option(-width)}]
    set ty1 [expr {$itk_option(-height) - \
	    (($y1 - $min) * $sf * $itk_option(-width))}]
    set ty2 [expr {$itk_option(-height) - \
	    (($y2 - $min) * $sf * $itk_option(-width))}]
    return "$tx1 $ty1 $tx2 $ty2"
}

## - configureWinCB
#
# Keep width and height options in sync
# with actual window size.
#
body cadwidgets::CellPlot::configureWinCB {width height} {
    configure -width $width -height $height
}
