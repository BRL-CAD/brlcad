#               A R B 7 E D I T F R A M E . T C L
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
#    The class for editing arb7s within Archer.
#
##############################################################

::itcl::class Arb7EditFrame {
    inherit Arb8EditFrame

    constructor {args} {}
    destructor {}

    public {
	# Override what's in Arb8EditFrame
	method updateGeometry {}
	method createGeometry {obj}
	method p {obj args}
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

	# Methods used by the constructor
	method buildMoveEdgePanel {parent}
	method buildMoveFacePanel {parent}
	method buildRotateFacePanel {parent}

	# Override what's in Arb8EditFrame
	method buildUpperPanel {}
	method updateUpperPanel {normal disabled}

	method initEditState {}
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
	::ttk::label $parent.arb7type \
	    -text "Arb7:" \
	    -anchor e
    } {}
    itk_component add arb7Name {
	::ttk::label $parent.arb7name \
	    -textvariable [::itcl::scope itk_option(-geometryObject)] \
	    -anchor w
    } {}

    # Create header labels
    itk_component add arb7XL {
	::ttk::label $parent.arb7XL \
	    -text "X"
    } {}
    itk_component add arb7YL {
	::ttk::label $parent.arb7YL \
	    -text "Y"
    } {}
    itk_component add arb7ZL {
	::ttk::label $parent.arb7ZL \
	    -text "Z"
    } {}

    # create widgets for vertices
    itk_component add arb7V1L {
	::ttk::label $parent.arb7V1L \
	    -text "V1:" \
	    -anchor e
    } {}
    itk_component add arb7V1xE {
	::ttk::entry $parent.arb7V1xE \
	    -textvariable [::itcl::scope mV1x] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb7V1yE {
	::ttk::entry $parent.arb7V1yE \
	    -textvariable [::itcl::scope mV1y] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb7V1zE {
	::ttk::entry $parent.arb7V1zE \
	    -textvariable [::itcl::scope mV1z] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb7V1UnitsL {
	::ttk::label $parent.arb7V1UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add arb7V2L {
	::ttk::label $parent.arb7V2L \
	    -text "V2:" \
	    -anchor e
    } {}
    itk_component add arb7V2xE {
	::ttk::entry $parent.arb7V2xE \
	    -textvariable [::itcl::scope mV2x] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb7V2yE {
	::ttk::entry $parent.arb7V2yE \
	    -textvariable [::itcl::scope mV2y] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb7V2zE {
	::ttk::entry $parent.arb7V2zE \
	    -textvariable [::itcl::scope mV2z] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb7V2UnitsL {
	::ttk::label $parent.arb7V2UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add arb7V3L {
	::ttk::label $parent.arb7V3L \
	    -text "V3:" \
	    -anchor e
    } {}
    itk_component add arb7V3xE {
	::ttk::entry $parent.arb7V3xE \
	    -textvariable [::itcl::scope mV3x] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb7V3yE {
	::ttk::entry $parent.arb7V3yE \
	    -textvariable [::itcl::scope mV3y] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb7V3zE {
	::ttk::entry $parent.arb7V3zE \
	    -textvariable [::itcl::scope mV3z] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb7V3UnitsL {
	::ttk::label $parent.arb7V3UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add arb7V4L {
	::ttk::label $parent.arb7V4L \
	    -text "V4:" \
	    -anchor e
    } {}
    itk_component add arb7V4xE {
	::ttk::entry $parent.arb7V4xE \
	    -textvariable [::itcl::scope mV4x] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb7V4yE {
	::ttk::entry $parent.arb7V4yE \
	    -textvariable [::itcl::scope mV4y] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb7V4zE {
	::ttk::entry $parent.arb7V4zE \
	    -textvariable [::itcl::scope mV4z] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb7V4UnitsL {
	::ttk::label $parent.arb7V4UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add arb7V5L {
	::ttk::label $parent.arb7V5L \
	    -text "V5:" \
	    -anchor e
    } {}
    itk_component add arb7V5xE {
	::ttk::entry $parent.arb7V5xE \
	    -textvariable [::itcl::scope mV5x] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb7V5yE {
	::ttk::entry $parent.arb7V5yE \
	    -textvariable [::itcl::scope mV5y] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb7V5zE {
	::ttk::entry $parent.arb7V5zE \
	    -textvariable [::itcl::scope mV5z] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb7V5UnitsL {
	::ttk::label $parent.arb7V5UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add arb7V6L {
	::ttk::label $parent.arb7V6L \
	    -text "V6:" \
	    -anchor e
    } {}
    itk_component add arb7V6xE {
	::ttk::entry $parent.arb7V6xE \
	    -textvariable [::itcl::scope mV6x] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb7V6yE {
	::ttk::entry $parent.arb7V6yE \
	    -textvariable [::itcl::scope mV6y] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb7V6zE {
	::ttk::entry $parent.arb7V6zE \
	    -textvariable [::itcl::scope mV6z] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb7V6UnitsL {
	::ttk::label $parent.arb7V6UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add arb7V7L {
	::ttk::label $parent.arb7V7L \
	    -text "V7:" \
	    -anchor e
    } {}
    itk_component add arb7V7xE {
	::ttk::entry $parent.arb7V7xE \
	    -textvariable [::itcl::scope mV7x] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb7V7yE {
	::ttk::entry $parent.arb7V7yE \
	    -textvariable [::itcl::scope mV7y] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb7V7zE {
	::ttk::entry $parent.arb7V7zE \
	    -textvariable [::itcl::scope mV7z] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb7V7UnitsL {
	::ttk::label $parent.arb7V7UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}

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

    foreach point {5} {
	itk_component add movePoint$point {
	    ::ttk::radiobutton $parent.me$point \
		-variable [::itcl::scope mEditMode] \
		-value [subst $[subst movePoint$point]] \
		-text "Move point $point" \
		-command [::itcl::code $this initEditState]
	} {}

	pack $itk_component(movePoint$point) \
	    -anchor w \
	    -expand yes
    }
}

::itcl::body Arb7EditFrame::buildMoveFacePanel {parent} {
    foreach face {1234 2376} {
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

::itcl::body Arb7EditFrame::buildRotateFacePanel {parent} {
    foreach face {1234 567 145 2376 1265 4375} {
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

    GeometryEditFrame::updateGeometry
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

::itcl::body Arb7EditFrame::p {obj args} {
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
	$moveEdge37 {
	    $::ArcherCore::application p_move_arb_edge $obj 9 $args
	} \
	$moveEdge57 {
	    $::ArcherCore::application p_move_arb_edge $obj 10 $args
	} \
	$moveEdge45 {
	    $::ArcherCore::application p_move_arb_edge $obj 11 $args
	} \
	$movePoint5 {
	    $::ArcherCore::application p_move_arb_edge $obj 12 $args
	} \
	$moveFace1234 {
	    $::ArcherCore::application p_move_arb_face $obj 1 $args
	} \
	$moveFace2376 {
	    $::ArcherCore::application p_move_arb_face $obj 4 $args
	} \
	$rotateFace1234 {
	    $::ArcherCore::application p_rotate_arb_face $obj 1 $mEditParam2 $args
	} \
	$rotateFace567 {
	    $::ArcherCore::application p_rotate_arb_face $obj 2 $mEditParam2 $args
	} \
	$rotateFace145 {
	    $::ArcherCore::application p_rotate_arb_face $obj 3 $mEditParam2 $args
	} \
	$rotateFace2376 {
	    $::ArcherCore::application p_rotate_arb_face $obj 4 $mEditParam2 $args
	} \
	$rotateFace1265 {
	    $::ArcherCore::application p_rotate_arb_face $obj 5 $mEditParam2 $args
	} \
	$rotateFace4375 {
	    $::ArcherCore::application p_rotate_arb_face $obj 6 $mEditParam2 $args
	}

    return ""
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

::itcl::body Arb7EditFrame::initEditState {} {
    set mEditPCommand [::itcl::code $this p]
    configure -valueUnits "mm"

    switch -- $mEditMode \
	$moveEdge12 {
	    set mEditCommand move_arb_edge
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditParam1 1
	    configure -valueUnits "mm"
	    updateUpperPanel {} {1 2 3 4 5 6 7}
	} \
	$moveEdge23 {
	    set mEditCommand move_arb_edge
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditParam1 2
	    configure -valueUnits "mm"
	    updateUpperPanel {} {1 2 3 4 5 6 7}
	} \
	$moveEdge34 {
	    set mEditCommand move_arb_edge
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditParam1 3
	    configure -valueUnits "mm"
	    updateUpperPanel {} {1 2 3 4 5 6 7}
	} \
	$moveEdge14 {
	    set mEditCommand move_arb_edge
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditParam1 4
	    configure -valueUnits "mm"
	    updateUpperPanel {} {1 2 3 4 5 6 7}
	} \
	$moveEdge15 {
	    set mEditCommand move_arb_edge
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditParam1 5
	    configure -valueUnits "mm"
	    updateUpperPanel {} {1 2 3 4 5 6 7}
	} \
	$moveEdge26 {
	    set mEditCommand move_arb_edge
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditParam1 6
	    configure -valueUnits "mm"
	    updateUpperPanel {} {1 2 3 4 5 6 7}
	} \
	$moveEdge56 {
	    set mEditCommand move_arb_edge
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditParam1 7
	    configure -valueUnits "mm"
	    updateUpperPanel {} {1 2 3 4 5 6 7}
	} \
	$moveEdge67 {
	    set mEditCommand move_arb_edge
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditParam1 8
	    configure -valueUnits "mm"
	    updateUpperPanel {} {1 2 3 4 5 6 7}
	} \
	$moveEdge37 {
	    set mEditCommand move_arb_edge
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditParam1 9
	    configure -valueUnits "mm"
	    updateUpperPanel {} {1 2 3 4 5 6 7}
	} \
	$moveEdge57 {
	    set mEditCommand move_arb_edge
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditParam1 10
	    configure -valueUnits "mm"
	    updateUpperPanel {} {1 2 3 4 5 6 7}
	} \
	$moveEdge45 {
	    set mEditCommand move_arb_edge
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditParam1 11
	    configure -valueUnits "mm"
	    updateUpperPanel {} {1 2 3 4 5 6 7}
	} \
	$movePoint5 {
	    set mEditCommand move_arb_edge
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditParam1 12
	    configure -valueUnits "mm"
	    updateUpperPanel {} {1 2 3 4 5 6 7}
	} \
	$moveFace1234 {
	    set mEditCommand move_arb_face
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditParam1 1
	    configure -valueUnits "mm"
	    updateUpperPanel {} {1 2 3 4 5 6 7}
	} \
	$moveFace2376 {
	    set mEditCommand move_arb_face
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditParam1 4
	    configure -valueUnits "mm"
	    updateUpperPanel {} {1 2 3 4 5 6 7}
	} \
	$rotateFace1234 {
	    set mEditCommand rotate_arb_face
	    set mEditClass $EDIT_CLASS_ROT
	    set mEditParam1 1
	    set mEditParam2 1
	    invokeRotationPointDialog {1 2 3 4}
	    configure -valueUnits "deg"
	    updateUpperPanel {1 2 3 4} {5 6 7}
	} \
	$rotateFace567 {
	    set mEditCommand rotate_arb_face
	    set mEditClass $EDIT_CLASS_ROT
	    set mEditParam1 2
	    set mEditParam2 5
	    configure -valueUnits "deg"
	    updateUpperPanel {5} {1 2 3 4 6 7}
	} \
	$rotateFace145 {
	    set mEditCommand rotate_arb_face
	    set mEditClass $EDIT_CLASS_ROT
	    set mEditParam1 3
	    set mEditParam2 5
	    configure -valueUnits "deg"
	    updateUpperPanel {5} {1 2 3 4 6 7}
	} \
	$rotateFace2376 {
	    set mEditCommand rotate_arb_face
	    set mEditClass $EDIT_CLASS_ROT
	    set mEditParam1 4
	    set mEditParam2 2
	    invokeRotationPointDialog {2 3 6 7}
	    configure -valueUnits "deg"
	    updateUpperPanel {2 3 6 7} {1 4 5}
	} \
	$rotateFace1265 {
	    set mEditCommand rotate_arb_face
	    set mEditClass $EDIT_CLASS_ROT
	    set mEditParam1 5
	    set mEditParam2 5
	    configure -valueUnits "deg"
	    updateUpperPanel {5} {1 2 3 4 6 7}
	} \
	$rotateFace4375 {
	    set mEditCommand rotate_arb_face
	    set mEditClass $EDIT_CLASS_ROT
	    set mEditParam1 6
	    set mEditParam2 5
	    configure -valueUnits "deg"
	    updateUpperPanel {5} {1 2 3 4 6 7}
	}

    GeometryEditFrame::initEditState
}


# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
