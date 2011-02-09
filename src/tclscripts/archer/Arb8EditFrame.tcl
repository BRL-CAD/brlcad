#               A R B 8 E D I T F R A M E . T C L
# BRL-CAD
#
# Copyright (c) 2002-2011 United States Government as represented by
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

    public {
	# Override what's in GeometryEditFrame
	method initGeometry {gdata}
	method updateGeometry {}
	method createGeometry {obj}
	method p {obj args}
    }

    protected {
	common mRotationPointDialog ""
	common mRotationPointCB ""

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

	# Methods used by the constructor
	method buildMoveEdgePanel {parent}
	method buildMoveFacePanel {parent}
	method buildRotateFacePanel {parent}
	# Override methods in GeometryEditFrame
	method buildUpperPanel {}
	method buildLowerPanel {}

	# Override what's in GeometryEditFrame
	method updateUpperPanel {normal disabled}
	method updateGeometryIfMod {}

	method initEditState {}

	method buildRotationPointDialog {}
	method invokeRotationPointDialog {_choices}
    }

    private {}
}


# ------------------------------------------------------------
#                      CONSTRUCTOR
# ------------------------------------------------------------

::itcl::body Arb8EditFrame::constructor {args} {
    buildRotationPointDialog

    eval itk_initialize $args
}

::itcl::body Arb8EditFrame::buildUpperPanel {} {
    set parent [$this childsite upper]
    itk_component add arb8Type {
	::ttk::label $parent.arb8type \
	    -text "Arb8:" \
	    -anchor e
    } {}
    itk_component add arb8Name {
	::ttk::label $parent.arb8name \
	    -textvariable [::itcl::scope itk_option(-geometryObject)] \
	    -anchor w
    } {}

    # Create header labels
    itk_component add arb8XL {
	::ttk::label $parent.arb8XL \
	    -text "X"
    } {}
    itk_component add arb8YL {
	::ttk::label $parent.arb8YL \
	    -text "Y"
    } {}
    itk_component add arb8ZL {
	::ttk::label $parent.arb8ZL \
	    -text "Z"
    } {}

    # create widgets for vertices
    itk_component add arb8V1L {
	::ttk::label $parent.arb8V1L \
	    -text "V1:" \
	    -anchor e
    } {}
    itk_component add arb8V1xE {
	::ttk::entry $parent.arb8V1xE \
	    -textvariable [::itcl::scope mV1x] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb8V1yE {
	::ttk::entry $parent.arb8V1yE \
	    -textvariable [::itcl::scope mV1y] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb8V1zE {
	::ttk::entry $parent.arb8V1zE \
	    -textvariable [::itcl::scope mV1z] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb8V1UnitsL {
	::ttk::label $parent.arb8V1UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add arb8V2L {
	::ttk::label $parent.arb8V2L \
	    -text "V2:" \
	    -anchor e
    } {}
    itk_component add arb8V2xE {
	::ttk::entry $parent.arb8V2xE \
	    -textvariable [::itcl::scope mV2x] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb8V2yE {
	::ttk::entry $parent.arb8V2yE \
	    -textvariable [::itcl::scope mV2y] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb8V2zE {
	::ttk::entry $parent.arb8V2zE \
	    -textvariable [::itcl::scope mV2z] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb8V2UnitsL {
	::ttk::label $parent.arb8V2UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add arb8V3L {
	::ttk::label $parent.arb8V3L \
	    -text "V3:" \
	    -anchor e
    } {}
    itk_component add arb8V3xE {
	::ttk::entry $parent.arb8V3xE \
	    -textvariable [::itcl::scope mV3x] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb8V3yE {
	::ttk::entry $parent.arb8V3yE \
	    -textvariable [::itcl::scope mV3y] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb8V3zE {
	::ttk::entry $parent.arb8V3zE \
	    -textvariable [::itcl::scope mV3z] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb8V3UnitsL {
	::ttk::label $parent.arb8V3UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add arb8V4L {
	::ttk::label $parent.arb8V4L \
	    -text "V4:" \
	    -anchor e
    } {}
    itk_component add arb8V4xE {
	::ttk::entry $parent.arb8V4xE \
	    -textvariable [::itcl::scope mV4x] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb8V4yE {
	::ttk::entry $parent.arb8V4yE \
	    -textvariable [::itcl::scope mV4y] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb8V4zE {
	::ttk::entry $parent.arb8V4zE \
	    -textvariable [::itcl::scope mV4z] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb8V4UnitsL {
	::ttk::label $parent.arb8V4UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add arb8V5L {
	::ttk::label $parent.arb8V5L \
	    -text "V5:" \
	    -anchor e
    } {}
    itk_component add arb8V5xE {
	::ttk::entry $parent.arb8V5xE \
	    -textvariable [::itcl::scope mV5x] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb8V5yE {
	::ttk::entry $parent.arb8V5yE \
	    -textvariable [::itcl::scope mV5y] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb8V5zE {
	::ttk::entry $parent.arb8V5zE \
	    -textvariable [::itcl::scope mV5z] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb8V5UnitsL {
	::ttk::label $parent.arb8V5UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add arb8V6L {
	::ttk::label $parent.arb8V6L \
	    -text "V6:" \
	    -anchor e
    } {}
    itk_component add arb8V6xE {
	::ttk::entry $parent.arb8V6xE \
	    -textvariable [::itcl::scope mV6x] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb8V6yE {
	::ttk::entry $parent.arb8V6yE \
	    -textvariable [::itcl::scope mV6y] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb8V6zE {
	::ttk::entry $parent.arb8V6zE \
	    -textvariable [::itcl::scope mV6z] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb8V6UnitsL {
	::ttk::label $parent.arb8V6UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add arb8V7L {
	::ttk::label $parent.arb8V7L \
	    -text "V7:" \
	    -anchor e
    } {}
    itk_component add arb8V7xE {
	::ttk::entry $parent.arb8V7xE \
	    -textvariable [::itcl::scope mV7x] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb8V7yE {
	::ttk::entry $parent.arb8V7yE \
	    -textvariable [::itcl::scope mV7y] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb8V7zE {
	::ttk::entry $parent.arb8V7zE \
	    -textvariable [::itcl::scope mV7z] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb8V7UnitsL {
	::ttk::label $parent.arb8V7UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add arb8V8L {
	::ttk::label $parent.arb8V8L \
	    -text "V8:" \
	    -anchor e
    } {}
    itk_component add arb8V8xE {
	::ttk::entry $parent.arb8V8xE \
	    -textvariable [::itcl::scope mV8x] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb8V8yE {
	::ttk::entry $parent.arb8V8yE \
	    -textvariable [::itcl::scope mV8y] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb8V8zE {
	::ttk::entry $parent.arb8V8zE \
	    -textvariable [::itcl::scope mV8z] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb8V8UnitsL {
	::ttk::label $parent.arb8V8UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}

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
	    ::ttk::radiobutton $parent.me$edge \
		-variable [::itcl::scope mEditMode] \
		-value [subst $[subst moveEdge$edge]] \
		-text "Move edge $edge" \
		-command [::itcl::code $this initEditState]
	} {}

	pack $itk_component(moveEdge$edge) \
	    -anchor w \
	    -expand yes
    }
}

::itcl::body Arb8EditFrame::buildMoveFacePanel {parent} {
    foreach face {1234 5678 1584 2376 1265 4378} {
	itk_component add moveFace$face {
	    ::ttk::radiobutton $parent.mf$face \
		-variable [::itcl::scope mEditMode] \
		-value [subst $[subst moveFace$face]] \
		-text "Move face $face" \
		-command [::itcl::code $this initEditState]
	} {}

	pack $itk_component(moveFace$face) \
	    -anchor w \
	    -expand yes
    }
}

::itcl::body Arb8EditFrame::buildRotateFacePanel {parent} {
    foreach face {1234 5678 1584 2376 1265 4378} {
	itk_component add rotateFace$face {
	    ::ttk::radiobutton $parent.rf$face \
		-variable [::itcl::scope mEditMode] \
		-value [subst $[subst rotateFace$face]] \
		-text "Rotate face $face" \
		-command [::itcl::code $this initEditState]
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

::itcl::body Arb8EditFrame::p {obj args} {
    if {[llength $args] != 3 ||
	![string is double [lindex $args 0]] ||
	![string is double [lindex $args 1]] ||
	![string is double [lindex $args 2]]} {
	return "Usage: p x y z"
    }

    switch -- $mEditMode \
	$moveEdge12 {
	    $::ArcherCore::application p_move_arb_edge $obj 1 $args
	} \
	$moveEdge23 {
	    $::ArcherCore::application p_move_arb_edge $obj 2 $args
	} \
	$moveEdge34 {
	    $::ArcherCore::application p_move_arb_edge $obj 3 $args
	} \
	$moveEdge14 {
	    $::ArcherCore::application p_move_arb_edge $obj 4 $args
	} \
	$moveEdge15 {
	    $::ArcherCore::application p_move_arb_edge $obj 5 $args
	} \
	$moveEdge26 {
	    $::ArcherCore::application p_move_arb_edge $obj 6 $args
	} \
	$moveEdge56 {
	    $::ArcherCore::application p_move_arb_edge $obj 7 $args
	} \
	$moveEdge67 {
	    $::ArcherCore::application p_move_arb_edge $obj 8 $args
	} \
	$moveEdge78 {
	    $::ArcherCore::application p_move_arb_edge $obj 9 $args
	} \
	$moveEdge58 {
	    $::ArcherCore::application p_move_arb_edge $obj 10 $args
	} \
	$moveEdge37 {
	    $::ArcherCore::application p_move_arb_edge $obj 11 $args
	} \
	$moveEdge48 {
	    $::ArcherCore::application p_move_arb_edge $obj 12 $args
	} \
	$moveFace1234 {
	    $::ArcherCore::application p_move_arb_face $obj 1 $args
	} \
	$moveFace5678 {
	    $::ArcherCore::application p_move_arb_face $obj 2 $args
	} \
	$moveFace1584 {
	    $::ArcherCore::application p_move_arb_face $obj 3 $args
	} \
	$moveFace2376 {
	    $::ArcherCore::application p_move_arb_face $obj 4 $args
	} \
	$moveFace1265 {
	    $::ArcherCore::application p_move_arb_face $obj 5 $args
	} \
	$moveFace4378 {
	    $::ArcherCore::application p_move_arb_face $obj 6 $args
	} \
	$rotateFace1234 {
	    $::ArcherCore::application p_rotate_arb_face $obj 1 $mEditParam2 $args
	} \
	$rotateFace5678 {
	    $::ArcherCore::application p_rotate_arb_face $obj 2 $mEditParam2 $args
	} \
	$rotateFace1584 {
	    $::ArcherCore::application p_rotate_arb_face $obj 3 $mEditParam2 $args
	} \
	$rotateFace2376 {
	    $::ArcherCore::application p_rotate_arb_face $obj 4 $mEditParam2 $args
	} \
	$rotateFace1265 {
	    $::ArcherCore::application p_rotate_arb_face $obj 5 $mEditParam2 $args
	} \
	$rotateFace4378 {
	    $::ArcherCore::application p_rotate_arb_face $obj 6 $mEditParam2 $args
	}

    return ""
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

::itcl::body Arb8EditFrame::initEditState {} {
    set mEditPCommand [::itcl::code $this p]
    configure -valueUnits "mm"

    switch -- $mEditMode \
	$moveEdge12 {
	    set mEditCommand move_arb_edge
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditParam1 1
	    updateUpperPanel {} {1 2 3 4 5 6 7 8}
	} \
	$moveEdge23 {
	    set mEditCommand move_arb_edge
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditParam1 2
	    updateUpperPanel {} {1 2 3 4 5 6 7 8}
	} \
	$moveEdge34 {
	    set mEditCommand move_arb_edge
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditParam1 3
	    updateUpperPanel {} {1 2 3 4 5 6 7 8}
	} \
	$moveEdge14 {
	    set mEditCommand move_arb_edge
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditParam1 4
	    updateUpperPanel {} {1 2 3 4 5 6 7 8}
	} \
	$moveEdge15 {
	    set mEditCommand move_arb_edge
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditParam1 5
	    updateUpperPanel {} {1 2 3 4 5 6 7 8}
	} \
	$moveEdge26 {
	    set mEditCommand move_arb_edge
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditParam1 6
	    updateUpperPanel {} {1 2 3 4 5 6 7 8}
	} \
	$moveEdge56 {
	    set mEditCommand move_arb_edge
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditParam1 7
	    updateUpperPanel {} {1 2 3 4 5 6 7 8}
	} \
	$moveEdge67 {
	    set mEditCommand move_arb_edge
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditParam1 8
	    updateUpperPanel {} {1 2 3 4 5 6 7 8}
	} \
	$moveEdge78 {
	    set mEditCommand move_arb_edge
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditParam1 9
	    updateUpperPanel {} {1 2 3 4 5 6 7 8}
	}  \
	$moveEdge58 {
	    set mEditCommand move_arb_edge
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditParam1 10
	    updateUpperPanel {} {1 2 3 4 5 6 7 8}
	} \
	$moveEdge37 {
	    set mEditCommand move_arb_edge
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditParam1 11
	    updateUpperPanel {} {1 2 3 4 5 6 7 8}
	} \
	$moveEdge48 {
	    set mEditCommand move_arb_edge
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditParam1 12
	    updateUpperPanel {} {1 2 3 4 5 6 7 8}
	} \
	$moveFace1234 {
	    set mEditCommand move_arb_face
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditParam1 1
	    updateUpperPanel {} {1 2 3 4 5 6 7 8}
	} \
	$moveFace5678 {
	    set mEditCommand move_arb_face
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditParam1 2
	    updateUpperPanel {} {1 2 3 4 5 6 7 8}
	} \
	$moveFace1584 {
	    set mEditCommand move_arb_face
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditParam1 3
	    updateUpperPanel {} {1 2 3 4 5 6 7 8}
	} \
	$moveFace2376 {
	    set mEditCommand move_arb_face
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditParam1 4
	    updateUpperPanel {} {1 2 3 4 5 6 7 8}
	} \
	$moveFace1265 {
	    set mEditCommand move_arb_face
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditParam1 5
	    updateUpperPanel {} {1 2 3 4 5 6 7 8}
	} \
	$moveFace4378 {
	    set mEditCommand move_arb_face
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditParam1 6
	    updateUpperPanel {} {1 2 3 4 5 6 7 8}
	} \
	$rotateFace1234 {
	    set mEditCommand rotate_arb_face
	    set mEditClass $EDIT_CLASS_ROT
	    set mEditParam1 1
	    set mEditParam2 1
	    invokeRotationPointDialog {1 2 3 4}
	    updateUpperPanel {1 2 3 4} {5 6 7 8}
	} \
	$rotateFace5678 {
	    set mEditCommand rotate_arb_face
	    set mEditClass $EDIT_CLASS_ROT
	    set mEditParam1 2
	    set mEditParam2 5
	    invokeRotationPointDialog {5 6 7 8}
	    updateUpperPanel {5 6 7 8} {1 2 3 4}
	} \
	$rotateFace1584 {
	    set mEditCommand rotate_arb_face
	    set mEditClass $EDIT_CLASS_ROT
	    set mEditParam1 3
	    set mEditParam2 1
	    invokeRotationPointDialog {1 4 5 8}
	    updateUpperPanel {1 5 8 4} {2 3 6 7}
	} \
	$rotateFace2376 {
	    set mEditCommand rotate_arb_face
	    set mEditClass $EDIT_CLASS_ROT
	    set mEditParam1 4
	    set mEditParam2 2
	    invokeRotationPointDialog {2 3 6 7}
	    updateUpperPanel {2 3 6 7} {1 5 8 4}
	} \
	$rotateFace1265 {
	    set mEditCommand rotate_arb_face
	    set mEditClass $EDIT_CLASS_ROT
	    set mEditParam1 5
	    set mEditParam2 1
	    invokeRotationPointDialog {1 2 5 6}
	    updateUpperPanel {1 2 5 6} {3 4 7 8}
	} \
	$rotateFace4378 {
	    set mEditCommand rotate_arb_face
	    set mEditClass $EDIT_CLASS_ROT
	    set mEditParam1 6
	    set mEditParam2 3
	    invokeRotationPointDialog {3 4 7 8}
	    updateUpperPanel {3 4 7 8} {1 2 5 6}
	}

    GeometryEditFrame::initEditState
}

::itcl::body Arb8EditFrame::buildRotationPointDialog {} {
    if {$mRotationPointDialog != ""} {
	return
    }

    set dialog [::iwidgets::dialog .\#auto \
		    -modality application \
		    -title "Select Rotation Point" \
		    -background $::ArcherCore::SystemButtonFace]
    set mRotationPointDialog $dialog

    $dialog configure -background $::ArcherCore::LABEL_BACKGROUND_COLOR

    $dialog hide 1
    $dialog hide 2
    $dialog hide 3

    $dialog configure \
	-thickness 2 \
	-buttonboxpady 0

    $dialog configure \
	-thickness 2 \
	-buttonboxpady 0
    $dialog buttonconfigure 0 \
	-defaultring yes \
	-defaultringpad 3 \
	-borderwidth 1 \
	-pady 0

    # ITCL can be nasty
    set win [$dialog component bbox component OK component hull]
    after idle "$win configure -relief flat"

    # Build combobox
    set parent [$dialog childsite]
    ::ttk::label $parent.rpointL \
	-text "Rotation Point:"

    ::ttk::frame $parent.rpointF \
	-relief sunken

    ::ttk::combobox $parent.rpointF.rpointCB \
	-state readonly \
	-textvariable ::GeometryEditFrame::mEditParam2 \
	-values {}
    set mRotationPointCB $parent.rpointF.rpointCB

    pack $parent.rpointF.rpointCB -expand yes -fill both

    set row 0
    grid $parent.rpointL \
	-row $row \
	-column 0 \
	-sticky ne
    grid $parent.rpointF \
	-row $row \
	-column 1 \
	-sticky nsew
    grid columnconfigure $parent 1 -weight 1

    $dialog buttonconfigure OK -command "$dialog deactivate"
    wm protocol $dialog WM_DELETE_WINDOW "$dialog deactivate"

    # Lame tcl/tk won't center the window properly
    # the first time unless we call update.
    update

    after idle "$dialog center"
}
    
::itcl::body Arb8EditFrame::invokeRotationPointDialog {_choices} {
    $mRotationPointCB configure -values $_choices
    $mRotationPointDialog activate
}


# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
