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
#       This software is Copyright (C) 1998-2004 by the United States Army
#       in all countries except the USA.  All rights reserved.
#
# Description -
#	As the name indicates, instances of CellPlot are intended
#       to be used for cell plots. This widget provide methods for
#       creating transformed (scaled and translated) cells.
#

#
# Usual options.
#
::itk::usual CellPlot {
    keep -range -plotWidth -plotHeight
}

::itcl::class cadwidgets::CellPlot {
    inherit iwidgets::Scrolledcanvas

    constructor {args} {}
    destructor {}

    # range of cell data
    itk_option define -range range Range {0.0 1.0}

    itk_option define -plotWidth plotWidth PlotWidth 512
    itk_option define -plotHeight plotHeight PlotHeight 512

    public method createCell {x1 y1 x2 y2 args}
    public method createCellInCanvas {c x1 y1 x2 y2 args}

    private method transform {x1 y1 x2 y2}

    private variable min 0.0
    private variable max 1.0
    private variable sf 1.0
}

::itcl::configbody cadwidgets::CellPlot::range {
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

::itcl::body cadwidgets::CellPlot::constructor {args} {
    eval itk_initialize $args
    ::bind [childsite] <Configure> \
	    [::itcl::code $this configure -width %w -height %h]
}

## - createCell
#
# Transform the data coodinates into canvas coordinates,
# then draw the cell in the specified color.
#
::itcl::body cadwidgets::CellPlot::createCell {x1 y1 x2 y2 args} {
    eval cadwidgets::CellPlot::createCellInCanvas $this $x1 $y1 $x2 $y2 $args
}

::itcl::body cadwidgets::CellPlot::createCellInCanvas {c x1 y1 x2 y2 args} {
    eval $c create rectangle [transform $x1 $y1 $x2 $y2] $args
}

## - transform
#
# Transform data coordinates into canvas coordinates.
#
::itcl::body cadwidgets::CellPlot::transform {x1 y1 x2 y2} {
    set tx1 [expr {($x1 - $min) * $sf * $itk_option(-plotWidth)}]
    set tx2 [expr {($x2 - $min) * $sf * $itk_option(-plotWidth)}]
    set ty1 [expr {$itk_option(-plotHeight) - \
	    (($y1 - $min) * $sf * $itk_option(-plotWidth))}]
    set ty2 [expr {$itk_option(-plotHeight) - \
	    (($y2 - $min) * $sf * $itk_option(-plotWidth))}]
    return "$tx1 $ty1 $tx2 $ty2"
}
