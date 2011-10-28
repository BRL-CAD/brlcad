#               A R B 4 E D I T F R A M E . T C L
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
#    The class for editing arb4s within Archer.
#
##############################################################

::itcl::class Arb4EditFrame {
    inherit Arb8EditFrame

    constructor {args} {}
    destructor {}

    public {
	# Override what's in GeometryEditFrame
	method updateGeometry {}
	method createGeometry {obj}
	method p {obj args}
    }

    protected {
	variable movePoint1 1
	variable movePoint2 2
	variable movePoint3 3
	variable movePoint4 4
	variable moveFace123 5
	variable moveFace124 6
	variable moveFace234 7
	variable moveFace134 8
	variable rotateFace123 9
	variable rotateFace124 10
	variable rotateFace234 11
	variable rotateFace134 12

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

::itcl::body Arb4EditFrame::constructor {args} {
    eval itk_initialize $args
}

::itcl::body Arb4EditFrame::buildUpperPanel {} {
    set parent [$this childsite upper]
    itk_component add arb4Type {
	::ttk::label $parent.arb4type \
	    -text "Arb4:" \
	    -anchor e
    } {}
    itk_component add arb4Name {
	::ttk::label $parent.arb4name \
	    -textvariable [::itcl::scope itk_option(-geometryObject)] \
	    -anchor w
    } {}

    # Create header labels
    itk_component add arb4XL {
	::ttk::label $parent.arb4XL \
	    -text "X"
    } {}
    itk_component add arb4YL {
	::ttk::label $parent.arb4YL \
	    -text "Y"
    } {}
    itk_component add arb4ZL {
	::ttk::label $parent.arb4ZL \
	    -text "Z"
    } {}

    # create widgets for vertices
    itk_component add arb4V1L {
	::ttk::label $parent.arb4V1L \
	    -text "V1:" \
	    -anchor e
    } {}
    itk_component add arb4V1xE {
	::ttk::entry $parent.arb4V1xE \
	    -textvariable [::itcl::scope mV1x] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb4V1yE {
	::ttk::entry $parent.arb4V1yE \
	    -textvariable [::itcl::scope mV1y] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb4V1zE {
	::ttk::entry $parent.arb4V1zE \
	    -textvariable [::itcl::scope mV1z] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb4V1UnitsL {
	::ttk::label $parent.arb4V1UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add arb4V2L {
	::ttk::label $parent.arb4V2L \
	    -text "V2:" \
	    -anchor e
    } {}
    itk_component add arb4V2xE {
	::ttk::entry $parent.arb4V2xE \
	    -textvariable [::itcl::scope mV2x] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb4V2yE {
	::ttk::entry $parent.arb4V2yE \
	    -textvariable [::itcl::scope mV2y] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb4V2zE {
	::ttk::entry $parent.arb4V2zE \
	    -textvariable [::itcl::scope mV2z] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb4V2UnitsL {
	::ttk::label $parent.arb4V2UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add arb4V3L {
	::ttk::label $parent.arb4V3L \
	    -text "V3:" \
	    -anchor e
    } {}
    itk_component add arb4V3xE {
	::ttk::entry $parent.arb4V3xE \
	    -textvariable [::itcl::scope mV3x] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb4V3yE {
	::ttk::entry $parent.arb4V3yE \
	    -textvariable [::itcl::scope mV3y] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb4V3zE {
	::ttk::entry $parent.arb4V3zE \
	    -textvariable [::itcl::scope mV3z] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb4V3UnitsL {
	::ttk::label $parent.arb4V3UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add arb4V4L {
	::ttk::label $parent.arb4V4L \
	    -text "V4:" \
	    -anchor e
    } {}
    itk_component add arb4V4xE {
	::ttk::entry $parent.arb4V4xE \
	    -textvariable [::itcl::scope mV5x] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb4V4yE {
	::ttk::entry $parent.arb4V4yE \
	    -textvariable [::itcl::scope mV5y] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb4V4zE {
	::ttk::entry $parent.arb4V4zE \
	    -textvariable [::itcl::scope mV5z] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb4V4UnitsL {
	::ttk::label $parent.arb4V4UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}

    set row 0
    grid $itk_component(arb4Type) \
	-row $row \
	-column 0 \
	-sticky nsew
    grid $itk_component(arb4Name) \
	-row $row \
	-column 1 \
	-columnspan 3 \
	-sticky nsew
    incr row
    grid x $itk_component(arb4XL) \
	$itk_component(arb4YL) \
	$itk_component(arb4ZL)
    incr row
    set col 0
    grid $itk_component(arb4V1L) \
	-row $row \
	-column $col \
	-sticky ne
    incr col
    grid $itk_component(arb4V1xE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb4V1yE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb4V1zE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb4V1UnitsL) \
	-row $row \
	-column $col \
	-sticky nsew
    incr row
    set col 0
    grid $itk_component(arb4V2L) \
	-row $row \
	-column $col \
	-sticky ne
    incr col
    grid $itk_component(arb4V2xE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb4V2yE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb4V2zE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb4V2UnitsL) \
	-row $row \
	-column $col \
	-sticky nsew
    incr row
    set col 0
    grid $itk_component(arb4V3L) \
	-row $row \
	-column $col \
	-sticky ne
    incr col
    grid $itk_component(arb4V3xE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb4V3yE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb4V3zE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb4V3UnitsL) \
	-row $row \
	-column $col \
	-sticky nsew
    incr row
    set col 0
    grid $itk_component(arb4V4L) \
	-row $row \
	-column $col \
	-sticky ne
    incr col
    grid $itk_component(arb4V4xE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb4V4yE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb4V4zE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb4V4UnitsL) \
	-row $row \
	-column $col \
	-sticky nsew
    grid columnconfigure $parent 1 -weight 1
    grid columnconfigure $parent 2 -weight 1
    grid columnconfigure $parent 3 -weight 1
    pack $parent -expand yes -fill x -anchor n

    # Set up bindings
    bind $itk_component(arb4V1xE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb4V1yE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb4V1zE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb4V2xE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb4V2yE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb4V2zE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb4V3xE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb4V3yE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb4V3zE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb4V4xE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb4V4yE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb4V4zE) <Return> [::itcl::code $this updateGeometryIfMod]
}

::itcl::body Arb4EditFrame::buildMoveEdgePanel {parent} {
    foreach point {1 2 3 4} {
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

::itcl::body Arb4EditFrame::buildMoveFacePanel {parent} {
    foreach face {123 124 234 134} {
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

::itcl::body Arb4EditFrame::buildRotateFacePanel {parent} {
    foreach face {123 124 234 134} {
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

::itcl::body Arb4EditFrame::updateGeometry {} {
    if {$itk_option(-mged) == "" ||
	$itk_option(-geometryObject) == ""} {
	return
    }

    $itk_option(-mged) adjust $itk_option(-geometryObject) \
	V1 [list $mV1x $mV1y $mV1z] \
	V2 [list $mV2x $mV2y $mV2z] \
	V3 [list $mV3x $mV3y $mV3z] \
	V4 [list $mV1x $mV1y $mV1z] \
	V5 [list $mV5x $mV5y $mV5z] \
	V6 [list $mV5x $mV5y $mV5z] \
	V7 [list $mV5x $mV5y $mV5z] \
	V8 [list $mV5x $mV5y $mV5z]

    # update unseen (by the user via the upper panel) variables
    set mV4x $mV1x
    set mV4y $mV1y
    set mV4z $mV1z
    set mV6x $mV5x
    set mV6y $mV5y
    set mV6z $mV5z
    set mV7x $mV5x
    set mV7y $mV5y
    set mV7z $mV5z
    set mV8x $mV5x
    set mV8y $mV5y
    set mV8z $mV5z

    GeometryEditFrame::updateGeometry
}

::itcl::body Arb4EditFrame::createGeometry {obj} {
    if {![GeometryEditFrame::createGeometry $obj]} {
	return
    }

    #    $itk_option(-mged) put $obj arb8 \
	V1 [list $mXmin $mYmin $mZmin] \
	V2 [list $mXmax $mYmin $mZmin] \
	V3 [list $mXmax $mYmin $mZmax] \
	V4 [list $mXmin $mYmin $mZmin] \
	V5 [list $mXmax $mYmax $mZmin] \
	V6 [list $mXmax $mYmax $mZmin] \
	V7 [list $mXmax $mYmax $mZmin] \
	V8 [list $mXmax $mYmax $mZmin]
    $itk_option(-mged) put $obj arb8 \
	V1 [list $mXmin $mYmin $mZmin] \
	V2 [list $mXmax $mYmin $mZmin] \
	V3 [list $mXmax $mYmin $mZmax] \
	V4 [list $mXmin $mYmin $mZmin] \
	V5 [list $mXmax $mYmax $mZmin] \
	V6 [list $mXmax $mYmax $mZmin] \
	V7 [list $mXmax $mYmax $mZmin] \
	V8 [list $mXmax $mYmax $mZmin]
}

::itcl::body Arb4EditFrame::p {obj args} {
    if {[llength $args] != 3 ||
	![string is double [lindex $args 0]] ||
	![string is double [lindex $args 1]] ||
	![string is double [lindex $args 2]]} {
	return "Usage: p x y z"
    }

    switch -- $mEditMode \
	$movePoint1 {
	    $::ArcherCore::application p_move_arb_edge $obj 1 $args
	} \
	$movePoint2 {
	    $::ArcherCore::application p_move_arb_edge $obj 2 $args
	} \
	$movePoint3 {
	    $::ArcherCore::application p_move_arb_edge $obj 3 $args
	} \
	$movePoint4 {
	    $::ArcherCore::application p_move_arb_edge $obj 5 $args
	} \
	$moveFace123 {
	    $::ArcherCore::application p_move_arb_face $obj 1 $args
	} \
	$moveFace124 {
	    $::ArcherCore::application p_move_arb_face $obj 2 $args
	} \
	$moveFace234 {
	    $::ArcherCore::application p_move_arb_face $obj 3 $args
	} \
	$moveFace134 {
	    $::ArcherCore::application p_move_arb_face $obj 4 $args
	} \
	$rotateFace123 {
	    $::ArcherCore::application p_rotate_arb_face $obj 1 $mEditParam2 $args
	} \
	$rotateFace124 {
	    $::ArcherCore::application p_rotate_arb_face $obj 2 $mEditParam2 $args
	} \
	$rotateFace234 {
	    $::ArcherCore::application p_rotate_arb_face $obj 3 $mEditParam2 $args
	} \
	$rotateFace134 {
	    $::ArcherCore::application p_rotate_arb_face $obj 4 $mEditParam2 $args
	}

    return ""
}


# ------------------------------------------------------------
#                      PROTECTED METHODS
# ------------------------------------------------------------

::itcl::body Arb4EditFrame::updateUpperPanel {normal disabled} {
    foreach rb $normal {
	$itk_component(arb4V$rb\L) configure -state normal
    }

    foreach rb $disabled {
	$itk_component(arb4V$rb\L) configure -state disabled
    }
}

::itcl::body Arb4EditFrame::initEditState {} {
    set mEditPCommand [::itcl::code $this p]
    configure -valueUnits "mm"

    switch -- $mEditMode \
	$movePoint1 { 
	    set mEditCommand move_arb_edge
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditParam1 1
	    updateUpperPanel {} {1 2 3 4}
	} \
	$movePoint2 { 
	    set mEditCommand move_arb_edge
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditParam1 2
	    updateUpperPanel {} {1 2 3 4} 
	} \
	$movePoint3 { 
	    set mEditCommand move_arb_edge
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditParam1 3
	    updateUpperPanel {} {1 2 3 4} 
	} \
	$movePoint4 { 
	    set mEditCommand move_arb_edge
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditParam1 5
	    updateUpperPanel {} {1 2 3 4} 
	} \
	$moveFace123 { 
	    set mEditCommand move_arb_face
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditParam1 1
	    updateUpperPanel {} {1 2 3 4} 
	} \
	$moveFace124 { 
	    set mEditCommand move_arb_face
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditParam1 2
	    updateUpperPanel {} {1 2 3 4} 
	} \
	$moveFace234 { 
	    set mEditCommand move_arb_face
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditParam1 3
	    updateUpperPanel {} {1 2 3 4} 
	} \
	$moveFace134 { 
	    set mEditCommand move_arb_face
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditParam1 4
	    updateUpperPanel {} {1 2 3 4} 
	} \
	$rotateFace123 { 
	    set mEditCommand rotate_arb_face
	    set mEditClass $EDIT_CLASS_ROT
	    set mEditParam1 1
	    set mEditParam2 1
	    invokeRotationPointDialog {1 2 3}
	    updateUpperPanel {1 2 3} {4} 
	} \
	$rotateFace124 { 
	    set mEditCommand rotate_arb_face
	    set mEditClass $EDIT_CLASS_ROT
	    set mEditParam1 2
	    set mEditParam2 1
	    invokeRotationPointDialog {1 2 4}
	    updateUpperPanel {1 2 4} {3} 
	} \
	$rotateFace234 { 
	    set mEditCommand rotate_arb_face
	    set mEditClass $EDIT_CLASS_ROT
	    set mEditParam1 3
	    set mEditParam2 2
	    invokeRotationPointDialog {2 3 4}
	    updateUpperPanel {2 3 4} {1} 
	} \
	$rotateFace134 { 
	    set mEditCommand rotate_arb_face
	    set mEditClass $EDIT_CLASS_ROT
	    set mEditParam1 4
	    set mEditParam2 1
	    invokeRotationPointDialog {1 3 4}
	    updateUpperPanel {1 3 4} {2} 
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
