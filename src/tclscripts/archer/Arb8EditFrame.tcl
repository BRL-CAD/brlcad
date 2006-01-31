#               A R B 8 E D I T F R A M E . T C L
# BRL-CAD
#
# Copyright (c) 2002-2006 United States Government as represented by
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
#    The class for editing arb8s within Archer.
#
##############################################################

::itcl::class Arb8EditFrame {
    inherit GeometryEditFrame

    constructor {args} {}
    destructor {}

    # Methods used by the constructor
    protected {
	method buildMoveEdgePanel {parent}
	method buildMoveFacePanel {parent}
	method buildRotateFacePanel {parent}

	# override methods in GeometryEditFrame
	method buildUpperPanel
	method buildLowerPanel
	method buildValuePanel
    }

    public {
	# Override what's in GeometryEditFrame
	method initGeometry {gdata}
	method updateGeometry {}
	method createGeometry {obj}

	method moveEdge {edge}
	method moveFace {face}
	method rotateFace {face}
    }

    protected {
	variable mVIndex 0
	variable mValueX ""
	variable mValueY ""
	variable mValueZ ""
	variable mV1x ""
	variable mV1y ""
	variable mV1z ""
	variable mV2x ""
	variable mV2y ""
	variable mV2z ""
	variable mV3x ""
	variable mV3y ""
	variable mV3z ""
	variable mV4x ""
	variable mV4y ""
	variable mV4z ""
	variable mV5x ""
	variable mV5y ""
	variable mV5z ""
	variable mV6x ""
	variable mV6y ""
	variable mV6z ""
	variable mV7x ""
	variable mV7y ""
	variable mV7z ""
	variable mV8x ""
	variable mV8y ""
	variable mV8z ""

	variable moveEdge12 1
	variable moveEdge23 2
	variable moveEdge34 3
	variable moveEdge14 4
	variable moveEdge15 5
	variable moveEdge26 6
	variable moveEdge56 7
	variable moveEdge67 8
	variable moveEdge78 9
	variable moveEdge58 10
	variable moveEdge37 11
	variable moveEdge48 12
	variable moveFace1234 13
	variable moveFace5678 14
	variable moveFace1584 15
	variable moveFace2376 16
	variable moveFace1265 17
	variable moveFace4378 18
	variable rotateFace1234 19
	variable rotateFace5678 20
	variable rotateFace1584 21
	variable rotateFace2376 22
	variable rotateFace1265 23
	variable rotateFace4378 24

	# Override what's in GeometryEditFrame
	method updateUpperPanel {normal disabled}
	method updateValuePanel {}
	method updateGeometryIfMod {}

	method initValuePanel {}

	method editGeometry {}
    }

    private {}
}


# ------------------------------------------------------------
#                      CONSTRUCTOR
# ------------------------------------------------------------

::itcl::body Arb8EditFrame::constructor {args} {
    eval itk_initialize $args
}

::itcl::body Arb8EditFrame::buildUpperPanel {} {
    set parent [$this childsite upper]
    itk_component add arb8Type {
	::label $parent.arb8type \
	    -text "Arb8:" \
	    -anchor e
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }
    itk_component add arb8Name {
	::label $parent.arb8name \
	    -textvariable [::itcl::scope itk_option(-geometryObject)] \
	    -anchor w
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }

    # Create header labels
    itk_component add arb8XL {
	::label $parent.arb8XL \
	    -text "X"
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }
    itk_component add arb8YL {
	::label $parent.arb8YL \
	    -text "Y"
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }
    itk_component add arb8ZL {
	::label $parent.arb8ZL \
	    -text "Z"
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }

    # create widgets for vertices
    itk_component add arb8V1L {
	::radiobutton $parent.arb8V1L \
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
    $itk_component(arb8V1L) configure \
	-disabledforeground [$itk_component(arb8V1L) cget -foreground] \
	-selectcolor  [$itk_component(arb8V1L) cget -background]
    itk_component add arb8V1xE {
	::entry $parent.arb8V1xE \
	    -textvariable [::itcl::scope mV1x] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb8V1yE {
	::entry $parent.arb8V1yE \
	    -textvariable [::itcl::scope mV1y] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb8V1zE {
	::entry $parent.arb8V1zE \
	    -textvariable [::itcl::scope mV1z] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb8V1UnitsL {
	::label $parent.arb8V1UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add arb8V2L {
	::radiobutton $parent.arb8V2L \
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
    $itk_component(arb8V2L) configure \
	-disabledforeground [$itk_component(arb8V2L) cget -foreground] \
	-selectcolor  [$itk_component(arb8V1L) cget -background]
    itk_component add arb8V2xE {
	::entry $parent.arb8V2xE \
	    -textvariable [::itcl::scope mV2x] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb8V2yE {
	::entry $parent.arb8V2yE \
	    -textvariable [::itcl::scope mV2y] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb8V2zE {
	::entry $parent.arb8V2zE \
	    -textvariable [::itcl::scope mV2z] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb8V2UnitsL {
	::label $parent.arb8V2UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add arb8V3L {
	::radiobutton $parent.arb8V3L \
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
    $itk_component(arb8V3L) configure \
	-disabledforeground [$itk_component(arb8V3L) cget -foreground] \
	-selectcolor  [$itk_component(arb8V1L) cget -background]
    itk_component add arb8V3xE {
	::entry $parent.arb8V3xE \
	    -textvariable [::itcl::scope mV3x] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb8V3yE {
	::entry $parent.arb8V3yE \
	    -textvariable [::itcl::scope mV3y] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb8V3zE {
	::entry $parent.arb8V3zE \
	    -textvariable [::itcl::scope mV3z] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb8V3UnitsL {
	::label $parent.arb8V3UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add arb8V4L {
	::radiobutton $parent.arb8V4L \
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
    $itk_component(arb8V4L) configure \
	-disabledforeground [$itk_component(arb8V4L) cget -foreground] \
	-selectcolor  [$itk_component(arb8V1L) cget -background]
    itk_component add arb8V4xE {
	::entry $parent.arb8V4xE \
	    -textvariable [::itcl::scope mV4x] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb8V4yE {
	::entry $parent.arb8V4yE \
	    -textvariable [::itcl::scope mV4y] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb8V4zE {
	::entry $parent.arb8V4zE \
	    -textvariable [::itcl::scope mV4z] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb8V4UnitsL {
	::label $parent.arb8V4UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add arb8V5L {
	::radiobutton $parent.arb8V5L \
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
    $itk_component(arb8V5L) configure \
	-disabledforeground [$itk_component(arb8V5L) cget -foreground] \
	-selectcolor  [$itk_component(arb8V1L) cget -background]
    itk_component add arb8V5xE {
	::entry $parent.arb8V5xE \
	    -textvariable [::itcl::scope mV5x] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb8V5yE {
	::entry $parent.arb8V5yE \
	    -textvariable [::itcl::scope mV5y] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb8V5zE {
	::entry $parent.arb8V5zE \
	    -textvariable [::itcl::scope mV5z] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb8V5UnitsL {
	::label $parent.arb8V5UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add arb8V6L {
	::radiobutton $parent.arb8V6L \
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
    $itk_component(arb8V6L) configure \
	-disabledforeground [$itk_component(arb8V6L) cget -foreground] \
	-selectcolor  [$itk_component(arb8V1L) cget -background]
    itk_component add arb8V6xE {
	::entry $parent.arb8V6xE \
	    -textvariable [::itcl::scope mV6x] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb8V6yE {
	::entry $parent.arb8V6yE \
	    -textvariable [::itcl::scope mV6y] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb8V6zE {
	::entry $parent.arb8V6zE \
	    -textvariable [::itcl::scope mV6z] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb8V6UnitsL {
	::label $parent.arb8V6UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add arb8V7L {
	::radiobutton $parent.arb8V7L \
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
    $itk_component(arb8V7L) configure \
	-disabledforeground [$itk_component(arb8V7L) cget -foreground] \
	-selectcolor  [$itk_component(arb8V1L) cget -background]
    itk_component add arb8V7xE {
	::entry $parent.arb8V7xE \
	    -textvariable [::itcl::scope mV7x] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb8V7yE {
	::entry $parent.arb8V7yE \
	    -textvariable [::itcl::scope mV7y] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb8V7zE {
	::entry $parent.arb8V7zE \
	    -textvariable [::itcl::scope mV7z] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb8V7UnitsL {
	::label $parent.arb8V7UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add arb8V8L {
	::radiobutton $parent.arb8V8L \
	    -text "V8:" \
	    -indicatoron 0 \
	    -variable [::itcl::scope mVIndex] \
	    -value 8 \
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
    $itk_component(arb8V8L) configure \
	-disabledforeground [$itk_component(arb8V8L) cget -foreground] \
	-selectcolor  [$itk_component(arb8V1L) cget -background]
    itk_component add arb8V8xE {
	::entry $parent.arb8V8xE \
	    -textvariable [::itcl::scope mV8x] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb8V8yE {
	::entry $parent.arb8V8yE \
	    -textvariable [::itcl::scope mV8y] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb8V8zE {
	::entry $parent.arb8V8zE \
	    -textvariable [::itcl::scope mV8z] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb8V8UnitsL {
	::label $parent.arb8V8UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }

    set row 0
    set col 0
    grid $itk_component(arb8Type) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb8Name) \
	-row $row \
	-column $col \
	-columnspan 3 \
	-sticky nsew
    incr row
    grid x $itk_component(arb8XL) \
	$itk_component(arb8YL) \
	$itk_component(arb8ZL)
    incr row
    set col 0
    grid $itk_component(arb8V1L) \
	-row $row \
	-column $col \
	-sticky ne
    incr col
    grid $itk_component(arb8V1xE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb8V1yE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb8V1zE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb8V1UnitsL) \
	-row $row \
	-column $col \
	-sticky nsew
    incr row
    set col 0
    grid $itk_component(arb8V2L) \
	-row $row \
	-column $col \
	-sticky ne
    incr col
    grid $itk_component(arb8V2xE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb8V2yE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb8V2zE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb8V2UnitsL) \
	-row $row \
	-column $col \
	-sticky nsew
    incr row
    set col 0
    grid $itk_component(arb8V3L) \
	-row $row \
	-column $col \
	-sticky ne
    incr col
    grid $itk_component(arb8V3xE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb8V3yE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb8V3zE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb8V3UnitsL) \
	-row $row \
	-column $col \
	-sticky nsew
    incr row
    set col 0
    grid $itk_component(arb8V4L) \
	-row $row \
	-column $col \
	-sticky ne
    incr col
    grid $itk_component(arb8V4xE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb8V4yE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb8V4zE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb8V4UnitsL) \
	-row $row \
	-column $col \
	-sticky nsew
    incr row
    set col 0
    grid $itk_component(arb8V5L) \
	-row $row \
	-column $col \
	-sticky ne
    incr col
    grid $itk_component(arb8V5xE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb8V5yE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb8V5zE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb8V5UnitsL) \
	-row $row \
	-column $col \
	-sticky nsew
    incr row
    set col 0
    grid $itk_component(arb8V6L) \
	-row $row \
	-column $col \
	-sticky ne
    incr col
    grid $itk_component(arb8V6xE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb8V6yE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb8V6zE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb8V6UnitsL) \
	-row $row \
	-column $col \
	-sticky nsew
    incr row
    set col 0
    grid $itk_component(arb8V7L) \
	-row $row \
	-column $col \
	-sticky ne
    incr col
    grid $itk_component(arb8V7xE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb8V7yE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb8V7zE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb8V7UnitsL) \
	-row $row \
	-column $col \
	-sticky nsew
    incr row
    set col 0
    grid $itk_component(arb8V8L) \
	-row $row \
	-column $col \
	-sticky ne
    incr col
    grid $itk_component(arb8V8xE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb8V8yE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb8V8zE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb8V8UnitsL) \
	-row $row \
	-column $col \
	-sticky nsew
    grid columnconfigure $parent 1 -weight 1
    grid columnconfigure $parent 2 -weight 1
    grid columnconfigure $parent 3 -weight 1
    pack $parent -expand yes -fill x -anchor n

    # Set up bindings
    bind $itk_component(arb8V1xE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb8V1yE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb8V1zE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb8V2xE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb8V2yE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb8V2zE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb8V3xE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb8V3yE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb8V3zE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb8V4xE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb8V4yE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb8V4zE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb8V5xE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb8V5yE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb8V5zE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb8V6xE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb8V6yE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb8V6zE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb8V7xE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb8V7yE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb8V7zE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb8V8xE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb8V8yE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb8V8zE) <Return> [::itcl::code $this updateGeometryIfMod]
}

::itcl::body Arb8EditFrame::buildMoveEdgePanel {parent} {
    foreach edge {12 23 34 14 15 26 56 67 78 58 37 48} {
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
}

::itcl::body Arb8EditFrame::buildMoveFacePanel {parent} {
    foreach face {1234 5678 1584 2376 1265 4378} {
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

::itcl::body Arb8EditFrame::buildRotateFacePanel {parent} {
    foreach face {1234 5678 1584 2376 1265 4378} {
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

::itcl::body Arb8EditFrame::buildLowerPanel {} {
    set parent [$this childsite lower]

    buildArrow $parent moveEdge "Move Edges" [::itcl::code $this buildMoveEdgePanel]
    buildArrow $parent moveFace "Move Faces" [::itcl::code $this buildMoveFacePanel]
    buildArrow $parent rotateFace "Rotate Faces" [::itcl::code $this buildRotateFacePanel]

    set row 0
    grid $itk_component(moveEdge) -row $row -column 0 -sticky nsew
    incr row
    grid $itk_component(moveFace) -row $row -column 0 -sticky nsew
    incr row
    grid $itk_component(rotateFace) -row $row -column 0 -sticky nsew
    grid columnconfigure $parent 0 -weight 1
}

::itcl::body Arb8EditFrame::buildValuePanel {} {
    set parent [$this childsite value]
    itk_component add valueX {
	::entry $parent.valueX \
	    -textvariable [::itcl::scope mValueX] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add valueY {
	::entry $parent.valueY \
	    -textvariable [::itcl::scope mValueY] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add valueZ {
	::entry $parent.valueZ \
	    -textvariable [::itcl::scope mValueZ] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}

    set row 0
    grid $itk_component(valueX) \
	$itk_component(valueY) \
	$itk_component(valueZ) \
	-row $row \
	-sticky nsew
    grid columnconfigure $parent 0 -weight 1
    grid columnconfigure $parent 1 -weight 1
    grid columnconfigure $parent 2 -weight 1

    bind $itk_component(valueX) <Return> [::itcl::code $this editGeometry]
    bind $itk_component(valueY) <Return> [::itcl::code $this editGeometry]
    bind $itk_component(valueZ) <Return> [::itcl::code $this editGeometry]
}


# ------------------------------------------------------------
#                        OPTIONS
# ------------------------------------------------------------


# ------------------------------------------------------------
#                      PUBLIC METHODS
# ------------------------------------------------------------

## - initGeometry
#
# Initialize the variables containing the object's specification.
#
::itcl::body Arb8EditFrame::initGeometry {gdata} {
    set _V1 [bu_get_value_by_keyword V1 $gdata]
    set mV1x [lindex $_V1 0]
    set mV1y [lindex $_V1 1]
    set mV1z [lindex $_V1 2]
    set _V2 [bu_get_value_by_keyword V2 $gdata]
    set mV2x [lindex $_V2 0]
    set mV2y [lindex $_V2 1]
    set mV2z [lindex $_V2 2]
    set _V3 [bu_get_value_by_keyword V3 $gdata]
    set mV3x [lindex $_V3 0]
    set mV3y [lindex $_V3 1]
    set mV3z [lindex $_V3 2]
    set _V4 [bu_get_value_by_keyword V4 $gdata]
    set mV4x [lindex $_V4 0]
    set mV4y [lindex $_V4 1]
    set mV4z [lindex $_V4 2]
    set _V5 [bu_get_value_by_keyword V5 $gdata]
    set mV5x [lindex $_V5 0]
    set mV5y [lindex $_V5 1]
    set mV5z [lindex $_V5 2]
    set _V6 [bu_get_value_by_keyword V6 $gdata]
    set mV6x [lindex $_V6 0]
    set mV6y [lindex $_V6 1]
    set mV6z [lindex $_V6 2]
    set _V7 [bu_get_value_by_keyword V7 $gdata]
    set mV7x [lindex $_V7 0]
    set mV7y [lindex $_V7 1]
    set mV7z [lindex $_V7 2]
    set _V8 [bu_get_value_by_keyword V8 $gdata]
    set mV8x [lindex $_V8 0]
    set mV8y [lindex $_V8 1]
    set mV8z [lindex $_V8 2]

    GeometryEditFrame::initGeometry $gdata
}

::itcl::body Arb8EditFrame::updateGeometry {} {
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
	V8 [list $mV8x $mV8y $mV8z]

    if {$itk_option(-geometryChangedCallback) != ""} {
	$itk_option(-geometryChangedCallback)
    }
}

::itcl::body Arb8EditFrame::createGeometry {obj} {
    if {![GeometryEditFrame::createGeometry $obj]} {
	return
    }

    $itk_option(-mged) put $obj arb8 \
	V1 [list $mXmin $mYmin $mZmin] \
	V2 [list $mXmax $mYmin $mZmin] \
	V3 [list $mXmax $mYmin $mZmax] \
	V4 [list $mXmin $mYmin $mZmax] \
	V5 [list $mXmin $mYmax $mZmin] \
	V6 [list $mXmax $mYmax $mZmin] \
	V7 [list $mXmax $mYmax $mZmax] \
	V8 [list $mXmin $mYmax $mZmax]
}

::itcl::body Arb8EditFrame::moveEdge {edge} {
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
	"78" {
	    set edgeIndex 9
	}
	"58" {
	    set edgeIndex 10
	}
	"37" {
	    set edgeIndex 11
	}
	"48" {
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

::itcl::body Arb8EditFrame::moveFace {face} {
    switch -- $face {
	"1234" {
	    set faceIndex 1
	}
	"5678" {
	    set faceIndex 2
	}
	"1584" {
	    set faceIndex 3
	}
	"2376" {
	    set faceIndex 4
	}
	"1265" {
	    set faceIndex 5
	}
	"4378" {
	    set faceIndex 6
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

::itcl::body Arb8EditFrame::rotateFace {face} {
    switch -- $face {
	"1234" {
	    set faceIndex 1
	}
	"5678" {
	    set faceIndex 2
	}
	"1584" {
	    set faceIndex 3
	}
	"2376" {
	    set faceIndex 4
	}
	"1265" {
	    set faceIndex 5
	}
	"4378" {
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

    set saveVIndex $mVIndex
    initGeometry $gdata
    set mVIndex $saveVIndex

    if {$itk_option(-geometryChangedCallback) != ""} {
	$itk_option(-geometryChangedCallback)
    }
}

# ------------------------------------------------------------
#                      PROTECTED METHODS
# ------------------------------------------------------------

::itcl::body Arb8EditFrame::updateUpperPanel {normal disabled} {
    foreach rb $normal {
	$itk_component(arb8V$rb\L) configure -state normal
    }

    foreach rb $disabled {
	$itk_component(arb8V$rb\L) configure -state disabled
    }
}

::itcl::body Arb8EditFrame::updateValuePanel {} {
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
	$moveEdge78 { \
	    set mValueX $mV7x; \
	    set mValueY $mV7y; \
	    set mValueZ $mV7z \
	} \
	$moveEdge58 { \
	    set mValueX $mV5x; \
	    set mValueY $mV5y; \
	    set mValueZ $mV5z \
	} \
	$moveEdge37 { \
	    set mValueX $mV3x; \
	    set mValueY $mV3y; \
	    set mValueZ $mV3z \
	} \
	$moveEdge48 { \
	    set mValueX $mV4x; \
	    set mValueY $mV4y; \
	    set mValueZ $mV4z \
	} \
	$moveFace1234 { \
	    set mValueX $mV1x; \
	    set mValueY $mV1y; \
	    set mValueZ $mV1z \
	} \
	$moveFace5678 { \
	    set mValueX $mV5x; \
	    set mValueY $mV5y; \
	    set mValueZ $mV5z \
	} \
	$moveFace1584 { \
	    set mValueX $mV1x; \
	    set mValueY $mV1y; \
	    set mValueZ $mV1z \
	} \
	$moveFace2376 { \
	    set mValueX $mV2x; \
	    set mValueY $mV2y; \
	    set mValueZ $mV2z \
	} \
	$moveFace1265 { \
	    set mValueX $mV1x; \
	    set mValueY $mV1y; \
	    set mValueZ $mV1z \
	} \
	$moveFace4378 { \
	    set mValueX $mV4x; \
	    set mValueY $mV4y; \
	    set mValueZ $mV4z \
	} \
	$rotateFace1234 { \
	    set mValueX 0; \
	    set mValueY 0; \
	    set mValueZ 0 \
	} \
	$rotateFace5678 { \
	    set mValueX 0; \
	    set mValueY 0; \
	    set mValueZ 0 \
	} \
	$rotateFace1584 { \
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
	$rotateFace4378 { \
	    set mValueX 0; \
	    set mValueY 0; \
	    set mValueZ 0 \
	}
}

::itcl::body Arb8EditFrame::updateGeometryIfMod {} {
    if {$itk_option(-mged) == "" ||
	$itk_option(-geometryObject) == ""} {
	return
    }

    set gdata [$itk_option(-mged) get $itk_option(-geometryObject)]
    set gdata [lrange $gdata 1 end]

    set _V1 [bu_get_value_by_keyword V1 $gdata]
    set _V1x [lindex $_V1 0]
    set _V1y [lindex $_V1 1]
    set _V1z [lindex $_V1 2]
    set _V2 [bu_get_value_by_keyword V2 $gdata]
    set _V2x [lindex $_V2 0]
    set _V2y [lindex $_V2 1]
    set _V2z [lindex $_V2 2]
    set _V3 [bu_get_value_by_keyword V3 $gdata]
    set _V3x [lindex $_V3 0]
    set _V3y [lindex $_V3 1]
    set _V3z [lindex $_V3 2]
    set _V4 [bu_get_value_by_keyword V4 $gdata]
    set _V4x [lindex $_V4 0]
    set _V4y [lindex $_V4 1]
    set _V4z [lindex $_V4 2]
    set _V5 [bu_get_value_by_keyword V5 $gdata]
    set _V5x [lindex $_V5 0]
    set _V5y [lindex $_V5 1]
    set _V5z [lindex $_V5 2]
    set _V6 [bu_get_value_by_keyword V6 $gdata]
    set _V6x [lindex $_V6 0]
    set _V6y [lindex $_V6 1]
    set _V6z [lindex $_V6 2]
    set _V7 [bu_get_value_by_keyword V7 $gdata]
    set _V7x [lindex $_V7 0]
    set _V7y [lindex $_V7 1]
    set _V7z [lindex $_V7 2]
    set _V8 [bu_get_value_by_keyword V8 $gdata]
    set _V8x [lindex $_V8 0]
    set _V8y [lindex $_V8 1]
    set _V8z [lindex $_V8 2]

    if {$mV1x == ""  ||
	$mV1x == "-" ||
	$mV1y == ""  ||
	$mV1y == "-" ||
	$mV1z == ""  ||
	$mV1z == "-" ||
	$mV2x == ""  ||
	$mV2x == "-" ||
	$mV2y == ""  ||
	$mV2y == "-" ||
	$mV2z == ""  ||
	$mV2z == "-" ||
	$mV3x == ""  ||
	$mV3x == "-" ||
	$mV3y == ""  ||
	$mV3y == "-" ||
	$mV3z == ""  ||
	$mV3z == "-" ||
	$mV4x == ""  ||
	$mV4x == "-" ||
	$mV4y == ""  ||
	$mV4y == "-" ||
	$mV4z == ""  ||
	$mV4z == "-" ||
	$mV5x == ""  ||
	$mV5x == "-" ||
	$mV5y == ""  ||
	$mV5y == "-" ||
	$mV5z == ""  ||
	$mV5z == "-" ||
	$mV6x == ""  ||
	$mV6x == "-" ||
	$mV6y == ""  ||
	$mV6y == "-" ||
	$mV6z == ""  ||
	$mV6z == "-" ||
	$mV7x == ""  ||
	$mV7x == "-" ||
	$mV7y == ""  ||
	$mV7y == "-" ||
	$mV7z == ""  ||
	$mV7z == "-" ||
	$mV8x == ""  ||
	$mV8x == "-" ||
	$mV8y == ""  ||
	$mV8y == "-" ||
	$mV8z == ""  ||
	$mV8z == "-"} {
	# Not valid
	return
    }

    if {$_V1x != $mV1x ||
	$_V1y != $mV1y ||
	$_V1z != $mV1z ||
	$_V2x != $mV2x ||
	$_V2y != $mV2y ||
	$_V2z != $mV2z ||
	$_V3x != $mV3x ||
	$_V3y != $mV3y ||
	$_V3z != $mV3z ||
	$_V4x != $mV4x ||
	$_V4y != $mV4y ||
	$_V4z != $mV4z ||
	$_V5x != $mV5x ||
	$_V5y != $mV5y ||
	$_V5z != $mV5z ||
	$_V6x != $mV6x ||
	$_V6y != $mV6y ||
	$_V6z != $mV6z ||
	$_V7x != $mV7x ||
	$_V7y != $mV7y ||
	$_V7z != $mV7z ||
	$_V8x != $mV8x ||
	$_V8y != $mV8y ||
	$_V8z != $mV8z} {
	updateGeometry
    }
}

::itcl::body Arb8EditFrame::initValuePanel {} {
    switch -- $mEditMode \
	$moveEdge12 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6 7 8} \
	} \
	$moveEdge23 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6 7 8} \
	} \
	$moveEdge34 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6 7 8} \
	} \
	$moveEdge14 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6 7 8} \
	} \
	$moveEdge15 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6 7 8} \
	} \
	$moveEdge26 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6 7 8} \
	} \
	$moveEdge56 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6 7 8} \
	} \
	$moveEdge67 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6 7 8} \
	} \
	$moveEdge78 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6 7 8} \
	} \
	$moveEdge58 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6 7 8} \
	} \
	$moveEdge37 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6 7 8} \
	} \
	$moveEdge48 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6 7 8} \
	} \
	$moveFace1234 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6 7 8} \
	} \
	$moveFace5678 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6 7 8} \
	} \
	$moveFace1584 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6 7 8} \
	} \
	$moveFace2376 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6 7 8} \
	} \
	$moveFace1265 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6 7 8} \
	} \
	$moveFace4378 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6 7 8} \
	} \
	$rotateFace1234 { \
	    set mVIndex 1; \
	    configure -valueUnits "deg"; \
	    updateUpperPanel {1 2 3 4} {5 6 7 8} \
	} \
	$rotateFace5678 { \
	    set mVIndex 5; \
	    configure -valueUnits "deg"; \
	    updateUpperPanel {5 6 7 8} {1 2 3 4} \
	} \
	$rotateFace1584 { \
	    set mVIndex 1; \
	    configure -valueUnits "deg"; \
	    updateUpperPanel {1 5 8 4} {2 3 6 7} \
	} \
	$rotateFace2376 { \
	    set mVIndex 2; \
	    configure -valueUnits "deg"; \
	    updateUpperPanel {2 3 6 7} {1 5 8 4} \
	} \
	$rotateFace1265 { \
	    set mVIndex 1; \
	    configure -valueUnits "deg"; \
	    updateUpperPanel {1 2 5 6} {3 4 7 8} \
	} \
	$rotateFace4378 { \
	    set mVIndex 4; \
	    configure -valueUnits "deg"; \
	    updateUpperPanel {3 4 7 8} {1 2 5 6} \
	}

    updateValuePanel
}

::itcl::body Arb8EditFrame::editGeometry {} {
    switch -- $mEditMode \
	$moveEdge12 {moveEdge 12} \
	$moveEdge23 {moveEdge 23} \
	$moveEdge34 {moveEdge 34} \
	$moveEdge14 {moveEdge 14} \
	$moveEdge15 {moveEdge 15} \
	$moveEdge26 {moveEdge 26} \
	$moveEdge56 {moveEdge 56} \
	$moveEdge67 {moveEdge 67} \
	$moveEdge78 {moveEdge 78} \
	$moveEdge58 {moveEdge 58} \
	$moveEdge37 {moveEdge 37} \
	$moveEdge48 {moveEdge 48} \
	$moveFace1234 {moveFace 1234} \
	$moveFace5678 {moveFace 5678} \
	$moveFace1584 {moveFace 1584} \
	$moveFace2376 {moveFace 2376} \
	$moveFace1265 {moveFace 1265} \
	$moveFace4378 {moveFace 4378} \
	$rotateFace1234 {rotateFace 1234} \
	$rotateFace5678 {rotateFace 5678} \
	$rotateFace1584 {rotateFace 1584} \
	$rotateFace2376 {rotateFace 2376} \
	$rotateFace1265 {rotateFace 1265} \
	$rotateFace4378 {rotateFace 4378}
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
