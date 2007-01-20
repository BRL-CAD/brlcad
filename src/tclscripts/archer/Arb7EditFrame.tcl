#               A R B 7 E D I T F R A M E . T C L
# BRL-CAD
#
# Copyright (c) 2002-2007 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
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
# Author(s):
#    Bob Parker
#
# Description:
#    The class for editing arb7s within Archer.
#
##############################################################

::itcl::class Arb7EditFrame {
    inherit Arb8EditFrame

    constructor {args} {}
    destructor {}

    # Methods used by the constructor
    protected {
	method buildMoveEdgePanel {parent}
	method buildMoveFacePanel {parent}
	method buildRotateFacePanel {parent}

	# override methods in GeometryEditFrame
	method buildUpperPanel
    }

    public {
	# Override what's in GeometryEditFrame
	method updateGeometry {}
	method createGeometry {obj}

	method moveEdge {edge}
	method moveFace {face}
	method rotateFace {face}
    }

    protected {
	variable moveEdge12 1
	variable moveEdge23 2
	variable moveEdge34 3
	variable moveEdge14 4
	variable moveEdge15 5
	variable moveEdge26 6
	variable moveEdge56 7
	variable moveEdge67 8
	variable moveEdge37 9
	variable moveEdge57 10
	variable moveEdge45 11
	variable movePoint5 12
	variable moveFace1234 13
	variable moveFace2376 14
	variable rotateFace1234 15
	variable rotateFace567 16
	variable rotateFace145 17
	variable rotateFace2376 18
	variable rotateFace1265 19
	variable rotateFace4375 20

	# Override what's in Arb8EditFrame
	method updateUpperPanel {normal disabled}
	method updateValuePanel {}

	method initValuePanel {}

	method editGeometry {}
    }

    private {}
}


# ------------------------------------------------------------
#                      CONSTRUCTOR
# ------------------------------------------------------------

::itcl::body Arb7EditFrame::constructor {args} {
    eval itk_initialize $args
}

::itcl::body Arb7EditFrame::buildUpperPanel {} {
    set parent [$this childsite upper]
    itk_component add arb7Type {
	::label $parent.arb7type \
	    -text "Arb7:" \
	    -anchor e
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }
    itk_component add arb7Name {
	::label $parent.arb7name \
	    -textvariable [::itcl::scope itk_option(-geometryObject)] \
	    -anchor w
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }

    # Create header labels
    itk_component add arb7XL {
	::label $parent.arb7XL \
	    -text "X"
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }
    itk_component add arb7YL {
	::label $parent.arb7YL \
	    -text "Y"
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }
    itk_component add arb7ZL {
	::label $parent.arb7ZL \
	    -text "Z"
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }

    # create widgets for vertices
    itk_component add arb7V1L {
	::radiobutton $parent.arb7V1L \
	    -text "V1:" \
	    -indicatoron 0 \
	    -variable [::itcl::scope mVIndex] \
	    -value 1 \
	    -relief flat \
	    -overrelief raised \
	    -offrelief flat \
	    -borderwidth 1 \
	    -padx 0 \
	    -highlightthickness 0 \
	    -state disabled \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    $itk_component(arb7V1L) configure \
	-disabledforeground [$itk_component(arb7V1L) cget -foreground] \
	-selectcolor  [$itk_component(arb7V1L) cget -background]
    itk_component add arb7V1xE {
	::entry $parent.arb7V1xE \
	    -textvariable [::itcl::scope mV1x] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb7V1yE {
	::entry $parent.arb7V1yE \
	    -textvariable [::itcl::scope mV1y] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb7V1zE {
	::entry $parent.arb7V1zE \
	    -textvariable [::itcl::scope mV1z] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb7V1UnitsL {
	::label $parent.arb7V1UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add arb7V2L {
	::radiobutton $parent.arb7V2L \
	    -text "V2:" \
	    -indicatoron 0 \
	    -variable [::itcl::scope mVIndex] \
	    -value 2 \
	    -relief flat \
	    -overrelief raised \
	    -offrelief flat \
	    -borderwidth 1 \
	    -padx 0 \
	    -highlightthickness 0 \
	    -state disabled \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    $itk_component(arb7V2L) configure \
	-disabledforeground [$itk_component(arb7V2L) cget -foreground] \
	-selectcolor  [$itk_component(arb7V2L) cget -background]
    itk_component add arb7V2xE {
	::entry $parent.arb7V2xE \
	    -textvariable [::itcl::scope mV2x] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb7V2yE {
	::entry $parent.arb7V2yE \
	    -textvariable [::itcl::scope mV2y] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb7V2zE {
	::entry $parent.arb7V2zE \
	    -textvariable [::itcl::scope mV2z] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb7V2UnitsL {
	::label $parent.arb7V2UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add arb7V3L {
	::radiobutton $parent.arb7V3L \
	    -text "V3:" \
	    -indicatoron 0 \
	    -variable [::itcl::scope mVIndex] \
	    -value 3 \
	    -relief flat \
	    -overrelief raised \
	    -offrelief flat \
	    -borderwidth 1 \
	    -padx 0 \
	    -highlightthickness 0 \
	    -state disabled \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    $itk_component(arb7V3L) configure \
	-disabledforeground [$itk_component(arb7V3L) cget -foreground] \
	-selectcolor  [$itk_component(arb7V3L) cget -background]
    itk_component add arb7V3xE {
	::entry $parent.arb7V3xE \
	    -textvariable [::itcl::scope mV3x] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb7V3yE {
	::entry $parent.arb7V3yE \
	    -textvariable [::itcl::scope mV3y] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb7V3zE {
	::entry $parent.arb7V3zE \
	    -textvariable [::itcl::scope mV3z] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb7V3UnitsL {
	::label $parent.arb7V3UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add arb7V4L {
	::radiobutton $parent.arb7V4L \
	    -text "V4:" \
	    -indicatoron 0 \
	    -variable [::itcl::scope mVIndex] \
	    -value 4 \
	    -relief flat \
	    -overrelief raised \
	    -offrelief flat \
	    -borderwidth 1 \
	    -padx 0 \
	    -highlightthickness 0 \
	    -state disabled \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    $itk_component(arb7V4L) configure \
	-disabledforeground [$itk_component(arb7V4L) cget -foreground] \
	-selectcolor  [$itk_component(arb7V4L) cget -background]
    itk_component add arb7V4xE {
	::entry $parent.arb7V4xE \
	    -textvariable [::itcl::scope mV4x] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb7V4yE {
	::entry $parent.arb7V4yE \
	    -textvariable [::itcl::scope mV4y] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb7V4zE {
	::entry $parent.arb7V4zE \
	    -textvariable [::itcl::scope mV4z] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb7V4UnitsL {
	::label $parent.arb7V4UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add arb7V5L {
	::radiobutton $parent.arb7V5L \
	    -text "V5:" \
	    -indicatoron 0 \
	    -variable [::itcl::scope mVIndex] \
	    -value 5 \
	    -relief flat \
	    -overrelief raised \
	    -offrelief flat \
	    -borderwidth 1 \
	    -padx 0 \
	    -highlightthickness 0 \
	    -state disabled \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    $itk_component(arb7V5L) configure \
	-disabledforeground [$itk_component(arb7V5L) cget -foreground] \
	-selectcolor  [$itk_component(arb7V5L) cget -background]
    itk_component add arb7V5xE {
	::entry $parent.arb7V5xE \
	    -textvariable [::itcl::scope mV5x] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb7V5yE {
	::entry $parent.arb7V5yE \
	    -textvariable [::itcl::scope mV5y] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb7V5zE {
	::entry $parent.arb7V5zE \
	    -textvariable [::itcl::scope mV5z] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb7V5UnitsL {
	::label $parent.arb7V5UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add arb7V6L {
	::radiobutton $parent.arb7V6L \
	    -text "V6:" \
	    -indicatoron 0 \
	    -variable [::itcl::scope mVIndex] \
	    -value 6 \
	    -relief flat \
	    -overrelief raised \
	    -offrelief flat \
	    -borderwidth 1 \
	    -padx 0 \
	    -highlightthickness 0 \
	    -state disabled \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    $itk_component(arb7V6L) configure \
	-disabledforeground [$itk_component(arb7V6L) cget -foreground] \
	-selectcolor  [$itk_component(arb7V6L) cget -background]
    itk_component add arb7V6xE {
	::entry $parent.arb7V6xE \
	    -textvariable [::itcl::scope mV6x] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb7V6yE {
	::entry $parent.arb7V6yE \
	    -textvariable [::itcl::scope mV6y] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb7V6zE {
	::entry $parent.arb7V6zE \
	    -textvariable [::itcl::scope mV6z] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb7V6UnitsL {
	::label $parent.arb7V6UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add arb7V7L {
	::radiobutton $parent.arb7V7L \
	    -text "V7:" \
	    -indicatoron 0 \
	    -variable [::itcl::scope mVIndex] \
	    -value 7 \
	    -relief flat \
	    -overrelief raised \
	    -offrelief flat \
	    -borderwidth 1 \
	    -padx 0 \
	    -highlightthickness 0 \
	    -state disabled \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    $itk_component(arb7V7L) configure \
	-disabledforeground [$itk_component(arb7V7L) cget -foreground] \
	-selectcolor  [$itk_component(arb7V7L) cget -background]
    itk_component add arb7V7xE {
	::entry $parent.arb7V7xE \
	    -textvariable [::itcl::scope mV7x] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb7V7yE {
	::entry $parent.arb7V7yE \
	    -textvariable [::itcl::scope mV7y] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb7V7zE {
	::entry $parent.arb7V7zE \
	    -textvariable [::itcl::scope mV7z] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb7V7UnitsL {
	::label $parent.arb7V7UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }

    set row 0
    grid $itk_component(arb7Type) \
	-row $row \
	-column 0 \
	-sticky nsew
    grid $itk_component(arb7Name) \
	-row $row \
	-column 1 \
	-columnspan 3 \
	-sticky nsew
    incr row
    grid x $itk_component(arb7XL) \
	$itk_component(arb7YL) \
	$itk_component(arb7ZL)
    incr row
    set col 0
    grid $itk_component(arb7V1L) \
	-row $row \
	-column $col \
	-sticky ne
    incr col
    grid $itk_component(arb7V1xE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb7V1yE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb7V1zE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb7V1UnitsL) \
	-row $row \
	-column $col \
	-sticky nsew
    incr row
    set col 0
    grid $itk_component(arb7V2L) \
	-row $row \
	-column $col \
	-sticky ne
    incr col
    grid $itk_component(arb7V2xE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb7V2yE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb7V2zE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb7V2UnitsL) \
	-row $row \
	-column $col \
	-sticky nsew
    incr row
    set col 0
    grid $itk_component(arb7V3L) \
	-row $row \
	-column $col \
	-sticky ne
    incr col
    grid $itk_component(arb7V3xE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb7V3yE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb7V3zE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb7V3UnitsL) \
	-row $row \
	-column $col \
	-sticky nsew
    incr row
    set col 0
    grid $itk_component(arb7V4L) \
	-row $row \
	-column $col \
	-sticky ne
    incr col
    grid $itk_component(arb7V4xE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb7V4yE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb7V4zE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb7V4UnitsL) \
	-row $row \
	-column $col \
	-sticky nsew
    incr row
    set col 0
    grid $itk_component(arb7V5L) \
	-row $row \
	-column $col \
	-sticky ne
    incr col
    grid $itk_component(arb7V5xE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb7V5yE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb7V5zE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb7V5UnitsL) \
	-row $row \
	-column $col \
	-sticky nsew
    incr row
    set col 0
    grid $itk_component(arb7V6L) \
	-row $row \
	-column $col \
	-sticky ne
    incr col
    grid $itk_component(arb7V6xE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb7V6yE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb7V6zE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb7V6UnitsL) \
	-row $row \
	-column $col \
	-sticky nsew
    incr row
    set col 0
    grid $itk_component(arb7V7L) \
	-row $row \
	-column $col \
	-sticky ne
    incr col
    grid $itk_component(arb7V7xE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb7V7yE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb7V7zE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb7V7UnitsL) \
	-row $row \
	-column $col \
	-sticky nsew
    grid columnconfigure $parent 1 -weight 1
    grid columnconfigure $parent 2 -weight 1
    grid columnconfigure $parent 3 -weight 1
    pack $parent -expand yes -fill x -anchor n

    # Set up bindings
    bind $itk_component(arb7V1xE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb7V1yE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb7V1zE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb7V2xE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb7V2yE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb7V2zE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb7V3xE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb7V3yE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb7V3zE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb7V4xE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb7V4yE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb7V4zE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb7V5xE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb7V5yE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb7V5zE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb7V6xE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb7V6yE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb7V6zE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb7V7xE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb7V7yE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb7V7zE) <Return> [::itcl::code $this updateGeometryIfMod]
}

::itcl::body Arb7EditFrame::buildMoveEdgePanel {parent} {
    foreach edge {12 23 34 14 15 26 56 67 37 57 45} {
	itk_component add moveEdge$edge {
	    ::radiobutton $parent.me$edge \
		-variable [::itcl::scope mEditMode] \
		-value [subst $[subst moveEdge$edge]] \
		-text "Move edge $edge" \
		-command [::itcl::code $this initValuePanel]
	} {}

	pack $itk_component(moveEdge$edge) \
	    -anchor w \
	    -expand yes
    }

    foreach point {5} {
	itk_component add movePoint$point {
	    ::radiobutton $parent.me$point \
		-variable [::itcl::scope mEditMode] \
		-value [subst $[subst movePoint$point]] \
		-text "Move point $point" \
		-command [::itcl::code $this initValuePanel]
	} {}

	pack $itk_component(movePoint$point) \
	    -anchor w \
	    -expand yes
    }
}

::itcl::body Arb7EditFrame::buildMoveFacePanel {parent} {
    foreach face {1234 2376} {
	itk_component add moveFace$face {
	    ::radiobutton $parent.mf$face \
		-variable [::itcl::scope mEditMode] \
		-value [subst $[subst moveFace$face]] \
		-text "Move face $face" \
		-command [::itcl::code $this initValuePanel]
	} {}

	pack $itk_component(moveFace$face) \
	    -anchor w \
	    -expand yes
    }
}

::itcl::body Arb7EditFrame::buildRotateFacePanel {parent} {
    foreach face {1234 567 145 2376 1265 4375} {
	itk_component add rotateFace$face {
	    ::radiobutton $parent.rf$face \
		-variable [::itcl::scope mEditMode] \
		-value [subst $[subst rotateFace$face]] \
		-text "Rotate face $face" \
		-command [::itcl::code $this initValuePanel]
	} {}

	pack $itk_component(rotateFace$face) \
	    -anchor w \
	    -expand yes
    }
}


# ------------------------------------------------------------
#                        OPTIONS
# ------------------------------------------------------------


# ------------------------------------------------------------
#                      PUBLIC METHODS
# ------------------------------------------------------------

::itcl::body Arb7EditFrame::updateGeometry {} {
    if {$itk_option(-mged) == "" ||
	$itk_option(-geometryObject) == ""} {
	return
    }

    $itk_option(-mged) adjust $itk_option(-geometryObject) \
	V1 [list $mV1x $mV1y $mV1z] \
	V2 [list $mV2x $mV2y $mV2z] \
	V3 [list $mV3x $mV3y $mV3z] \
	V4 [list $mV4x $mV4y $mV4z] \
	V5 [list $mV5x $mV5y $mV5z] \
	V6 [list $mV6x $mV6y $mV6z] \
	V7 [list $mV7x $mV7y $mV7z] \
	V8 [list $mV5x $mV5y $mV5z]

    # update unseen (by the user via the upper panel) variables
    set mV8x $mV5x
    set mV8y $mV5y
    set mV8z $mV5z

    if {$itk_option(-geometryChangedCallback) != ""} {
	$itk_option(-geometryChangedCallback)
    }
}

::itcl::body Arb7EditFrame::createGeometry {obj} {
    if {![GeometryEditFrame::createGeometry $obj]} {
	return
    }

    set _z [expr {($mZmin + $mZmax) * 0.5}]
#    $itk_option(-mged) put $obj arb8 \
	V1 [list $mXmin $mYmin $mZmin] \
	V2 [list $mXmin $mYmax $mZmin] \
	V3 [list $mXmin $mYmax $mZmax] \
	V4 [list $mXmin $mYmin $_z] \
	V5 [list $mXmax $mYmin $mZmin] \
	V6 [list $mXmax $mYmax $mZmin] \
	V7 [list $mXmax $mYmax $_z] \
	V8 [list $mXmax $mYmin $mZmin]
    $itk_option(-mged) put $obj arb8 \
	V1 [list $mXmax $mYmin $mZmin] \
	V2 [list $mXmax $mYmax $mZmin] \
	V3 [list $mXmax $mYmax $mZmax] \
	V4 [list $mXmax $mYmin $_z] \
	V5 [list $mXmin $mYmin $mZmin] \
	V6 [list $mXmin $mYmax $mZmin] \
	V7 [list $mXmin $mYmax $_z] \
	V8 [list $mXmin $mYmin $mZmin]

#	V7 [list $mXmin $mYmax $mZmax]
}

::itcl::body Arb7EditFrame::moveEdge {edge} {
    switch -- $edge {
	"12" {
	    set edgeIndex 1
	}
	"23" {
	    set edgeIndex 2
	}
	"34" {
	    set edgeIndex 3
	}
	"14" {
	    set edgeIndex 4
	}
	"15" {
	    set edgeIndex 5
	}
	"26" {
	    set edgeIndex 6
	}
	"56" {
	    set edgeIndex 7
	}
	"67" {
	    set edgeIndex 8
	}
	"37" {
	    set edgeIndex 9
	}
	"57" {
	    set edgeIndex 10
	}
	"45" {
	    set edgeIndex 11
	}
	"5" {
	    set edgeIndex 12
	}
    }

    set gdata [$itk_option(-mged) move_arb_edge \
		   $itk_option(-geometryObject) \
		   $edgeIndex \
		   [list $mValueX $mValueY $mValueZ]]

    eval $itk_option(-mged) adjust \
	$itk_option(-geometryObject) \
	$gdata

    initGeometry $gdata

    if {$itk_option(-geometryChangedCallback) != ""} {
	$itk_option(-geometryChangedCallback)
    }
}

::itcl::body Arb7EditFrame::moveFace {face} {
    switch -- $face {
	"1234" {
	    set faceIndex 1
	}
	"2376" {
	    set faceIndex 4
	}
    }

    set gdata [$itk_option(-mged) move_arb_face \
		   $itk_option(-geometryObject) \
		   $faceIndex \
		   [list $mValueX $mValueY $mValueZ]]

    eval $itk_option(-mged) adjust \
	$itk_option(-geometryObject) \
	$gdata

    initGeometry $gdata

    if {$itk_option(-geometryChangedCallback) != ""} {
	$itk_option(-geometryChangedCallback)
    }
}

::itcl::body Arb7EditFrame::rotateFace {face} {
    switch -- $face {
	"1234" {
	    set faceIndex 1
	}
	"567" {
	    set faceIndex 2
	}
	"145" {
	    set faceIndex 3
	}
	"2376" {
	    set faceIndex 4
	}
	"1265" {
	    set faceIndex 5
	}
	"4375" {
	    set faceIndex 6
	}
    }

    set gdata [$itk_option(-mged) rotate_arb_face \
		   $itk_option(-geometryObject) \
		   $faceIndex \
		   $mVIndex \
		   [list $mValueX $mValueY $mValueZ]]

    eval $itk_option(-mged) adjust \
	$itk_option(-geometryObject) \
	$gdata

    initGeometry $gdata

    if {$itk_option(-geometryChangedCallback) != ""} {
	$itk_option(-geometryChangedCallback)
    }
}

# ------------------------------------------------------------
#                      PROTECTED METHODS
# ------------------------------------------------------------

::itcl::body Arb7EditFrame::updateUpperPanel {normal disabled} {
    foreach rb $normal {
	$itk_component(arb7V$rb\L) configure -state normal
    }

    foreach rb $disabled {
	$itk_component(arb7V$rb\L) configure -state disabled
    }
}

::itcl::body Arb7EditFrame::updateValuePanel {} {
    switch -- $mEditMode \
	$moveEdge12 { \
	    set mValueX $mV1x; \
	    set mValueY $mV1y; \
	    set mValueZ $mV1z \
	} \
	$moveEdge23 { \
	    set mValueX $mV2x; \
	    set mValueY $mV2y; \
	    set mValueZ $mV2z \
	} \
	$moveEdge34 { \
	    set mValueX $mV3x; \
	    set mValueY $mV3y; \
	    set mValueZ $mV3z \
	} \
	$moveEdge14 { \
	    set mValueX $mV1x; \
	    set mValueY $mV1y; \
	    set mValueZ $mV1z \
	} \
	$moveEdge15 { \
	    set mValueX $mV1x; \
	    set mValueY $mV1y; \
	    set mValueZ $mV1z \
	} \
	$moveEdge26 { \
	    set mValueX $mV2x; \
	    set mValueY $mV2y; \
	    set mValueZ $mV2z \
	} \
	$moveEdge56 { \
	    set mValueX $mV5x; \
	    set mValueY $mV5y; \
	    set mValueZ $mV5z \
	} \
	$moveEdge67 { \
	    set mValueX $mV6x; \
	    set mValueY $mV6y; \
	    set mValueZ $mV6z \
	} \
	$moveEdge37 { \
	    set mValueX $mV3x; \
	    set mValueY $mV3y; \
	    set mValueZ $mV3z \
	} \
	$moveEdge57 { \
	    set mValueX $mV5x; \
	    set mValueY $mV5y; \
	    set mValueZ $mV5z \
	} \
	$moveEdge45 { \
	    set mValueX $mV4x; \
	    set mValueY $mV4y; \
	    set mValueZ $mV4z \
	} \
	$movePoint5 { \
	    set mValueX $mV5x; \
	    set mValueY $mV5y; \
	    set mValueZ $mV5z \
	} \
	$moveFace1234 { \
	    set mValueX $mV1x; \
	    set mValueY $mV1y; \
	    set mValueZ $mV1z \
	} \
	$moveFace2376 { \
	    set mValueX $mV2x; \
	    set mValueY $mV2y; \
	    set mValueZ $mV2z \
	} \
	$rotateFace1234 { \
	    set mValueX 0; \
	    set mValueY 0; \
	    set mValueZ 0 \
	} \
	$rotateFace567 { \
	    set mValueX 0; \
	    set mValueY 0; \
	    set mValueZ 0 \
	} \
	$rotateFace145 { \
	    set mValueX 0; \
	    set mValueY 0; \
	    set mValueZ 0 \
	} \
	$rotateFace2376 { \
	    set mValueX 0; \
	    set mValueY 0; \
	    set mValueZ 0 \
	} \
	$rotateFace1265 { \
	    set mValueX 0; \
	    set mValueY 0; \
	    set mValueZ 0 \
	} \
	$rotateFace4375 { \
	    set mValueX 0; \
	    set mValueY 0; \
	    set mValueZ 0 \
	}
}

::itcl::body Arb7EditFrame::initValuePanel {} {
    switch -- $mEditMode \
	$moveEdge12 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6 7} \
	} \
	$moveEdge23 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6 7} \
	} \
	$moveEdge34 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6 7} \
	} \
	$moveEdge14 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6 7} \
	} \
	$moveEdge15 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6 7} \
	} \
	$moveEdge26 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6 7} \
	} \
	$moveEdge56 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6 7} \
	} \
	$moveEdge67 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6 7} \
	} \
	$moveEdge37 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6 7} \
	} \
	$moveEdge57 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6 7} \
	} \
	$moveEdge45 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6 7} \
	} \
	$movePoint5 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6 7} \
	} \
	$moveFace1234 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6 7} \
	} \
	$moveFace2376 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6 7} \
	} \
	$rotateFace1234 { \
	    set mVIndex 1; \
	    configure -valueUnits "deg"; \
	    updateUpperPanel {1 2 3 4} {5 6 7} \
	} \
	$rotateFace567 { \
	    set mVIndex 5; \
	    configure -valueUnits "deg"; \
	    updateUpperPanel {5} {1 2 3 4 6 7} \
	} \
	$rotateFace145 { \
	    set mVIndex 5; \
	    configure -valueUnits "deg"; \
	    updateUpperPanel {5} {1 2 3 4 6 7} \
	} \
	$rotateFace2376 { \
	    set mVIndex 2; \
	    configure -valueUnits "deg"; \
	    updateUpperPanel {2 3 6 7} {1 4 5} \
	} \
	$rotateFace1265 { \
	    set mVIndex 5; \
	    configure -valueUnits "deg"; \
	    updateUpperPanel {5} {1 2 3 4 6 7} \
	} \
	$rotateFace4375 { \
	    set mVIndex 5; \
	    configure -valueUnits "deg"; \
	    updateUpperPanel {5} {1 2 3 4 6 7} \
	}

    updateValuePanel
}

::itcl::body Arb7EditFrame::editGeometry {} {
    switch -- $mEditMode \
	$moveEdge12 {moveEdge 12} \
	$moveEdge23 {moveEdge 23} \
	$moveEdge34 {moveEdge 34} \
	$moveEdge14 {moveEdge 14} \
	$moveEdge15 {moveEdge 15} \
	$moveEdge26 {moveEdge 26} \
	$moveEdge56 {moveEdge 56} \
	$moveEdge67 {moveEdge 67} \
	$moveEdge37 {moveEdge 37} \
	$moveEdge57 {moveEdge 57} \
	$moveEdge45 {moveEdge 45} \
	$movePoint5 {moveEdge 5} \
	$moveFace1234 {moveFace 1234} \
	$moveFace2376 {moveFace 2376} \
	$rotateFace1234 {rotateFace 1234} \
	$rotateFace567 {rotateFace 567} \
	$rotateFace145 {rotateFace 145} \
	$rotateFace2376 {rotateFace 2376} \
	$rotateFace1265 {rotateFace 1265} \
	$rotateFace4375 {rotateFace 4375}
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
