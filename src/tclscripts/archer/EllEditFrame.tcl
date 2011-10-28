#                E L L E D I T F R A M E . T C L
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
# Author:
#    Bob Parker
#
# Description:
#    The class for editing ells within Archer.
#

::itcl::class EllEditFrame {
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
	common setA 1
	common setB 2
	common setC 3
	common setABC 4

	variable mVx ""
	variable mVy ""
	variable mVz ""
	variable mAx ""
	variable mAy ""
	variable mAz ""
	variable mBx ""
	variable mBy ""
	variable mBz ""
	variable mCx ""
	variable mCy ""
	variable mCz ""

	variable mCurrentGridRow 0

	# Methods used by the constructor
	# override methods in GeometryEditFrame
	method buildUpperPanel
	method buildLowerPanel

	# Override what's in GeometryEditFrame
	method updateGeometryIfMod {}
	method initEditState {}
    }

    private {}
}


# ------------------------------------------------------------
#                      CONSTRUCTOR
# ------------------------------------------------------------

::itcl::body EllEditFrame::constructor {args} {
    eval itk_initialize $args
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
::itcl::body EllEditFrame::initGeometry {gdata} {
    set _V [bu_get_value_by_keyword V $gdata]
    set mVx [lindex $_V 0]
    set mVy [lindex $_V 1]
    set mVz [lindex $_V 2]
    set _A [bu_get_value_by_keyword A $gdata]
    set mAx [lindex $_A 0]
    set mAy [lindex $_A 1]
    set mAz [lindex $_A 2]
    set _B [bu_get_value_by_keyword B $gdata]
    set mBx [lindex $_B 0]
    set mBy [lindex $_B 1]
    set mBz [lindex $_B 2]
    set _C [bu_get_value_by_keyword C $gdata]
    set mCx [lindex $_C 0]
    set mCy [lindex $_C 1]
    set mCz [lindex $_C 2]

    GeometryEditFrame::initGeometry $gdata
}

::itcl::body EllEditFrame::updateGeometry {} {
    if {$itk_option(-mged) == "" ||
	$itk_option(-geometryObject) == ""} {
	return
    }

    $itk_option(-mged) adjust $itk_option(-geometryObject) \
	V [list $mVx $mVy $mVz] \
	A [list $mAx $mAy $mAz] \
	B [list $mBx $mBy $mBz] \
	C [list $mCx $mCy $mCz]

    GeometryEditFrame::updateGeometry
}

::itcl::body EllEditFrame::createGeometry {obj} {
    if {![GeometryEditFrame::createGeometry $obj]} {
	return
    }

    $itk_option(-mged) put $obj ell \
	V [list $mCenterX $mCenterY $mCenterZ] \
	A [list $mDelta 0 0] \
	B [list 0 $mDelta 0] \
	C [list 0 0 $mDelta]
}

::itcl::body EllEditFrame::p {obj args} {
    if {[llength $args] != 1 || ![string is double $args]} {
	return "Usage: p sf"
    }

    switch -- $mEditMode \
	$setA {
	    $::ArcherCore::application p_pscale $obj a $args
	} \
	$setB {
	    $::ArcherCore::application p_pscale $obj b $args
	} \
	$setC {
	    $::ArcherCore::application p_pscale $obj c $args
	} \
	$setABC {
	    $::ArcherCore::application p_pscale $obj abc $args
	}

    return ""
}


# ------------------------------------------------------------
#                      PROTECTED METHODS
# ------------------------------------------------------------

::itcl::body EllEditFrame::buildUpperPanel {} {
    set parent [$this childsite]
    itk_component add ellType {
	::ttk::label $parent.elltype \
	    -text "Ellipsoid:" \
	    -anchor e
    } {}
    itk_component add ellName {
	::ttk::label $parent.ellname \
	    -textvariable [::itcl::scope itk_option(-geometryObject)] \
	    -anchor w
    } {}

    # Create header labels
    itk_component add ellXL {
	::ttk::label $parent.ellXL \
	    -text "X"
    } {}
    itk_component add ellYL {
	::ttk::label $parent.ellYL \
	    -text "Y"
    } {}
    itk_component add ellZL {
	::ttk::label $parent.ellZL \
	    -text "Z"
    } {}

    # create widgets for vertices and vectors
    itk_component add ellVL {
	::ttk::label $parent.ellVL \
	    -text "V:" \
	    -anchor e
    } {}
    itk_component add ellVxE {
	::ttk::entry $parent.ellVxE \
	    -textvariable [::itcl::scope mVx] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add ellVyE {
	::ttk::entry $parent.ellVyE \
	    -textvariable [::itcl::scope mVy] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add ellVzE {
	::ttk::entry $parent.ellVzE \
	    -textvariable [::itcl::scope mVz] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add ellVUnitsL {
	::ttk::label $parent.ellVUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add ellAL {
	::ttk::label $parent.ellAL \
	    -state disabled \
	    -text "A:" \
	    -anchor e
    } {}
    itk_component add ellAxE {
	::ttk::entry $parent.ellAxE \
	    -textvariable [::itcl::scope mAx] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add ellAyE {
	::ttk::entry $parent.ellAyE \
	    -textvariable [::itcl::scope mAy] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add ellAzE {
	::ttk::entry $parent.ellAzE \
	    -textvariable [::itcl::scope mAz] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add ellAUnitsL {
	::ttk::label $parent.ellAUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add ellBL {
	::ttk::label $parent.ellBL \
	    -state disabled \
	    -text "B:" \
	    -anchor e
    } {}
    itk_component add ellBxE {
	::ttk::entry $parent.ellBxE \
	    -textvariable [::itcl::scope mBx] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add ellByE {
	::ttk::entry $parent.ellByE \
	    -textvariable [::itcl::scope mBy] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add ellBzE {
	::ttk::entry $parent.ellBzE \
	    -textvariable [::itcl::scope mBz] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add ellBUnitsL {
	::ttk::label $parent.ellBUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add ellCL {
	::ttk::label $parent.ellCL \
	    -state disabled \
	    -text "C:" \
	    -anchor e
    } {}
    itk_component add ellCxE {
	::ttk::entry $parent.ellCxE \
	    -textvariable [::itcl::scope mCx] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add ellCyE {
	::ttk::entry $parent.ellCyE \
	    -textvariable [::itcl::scope mCy] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add ellCzE {
	::ttk::entry $parent.ellCzE \
	    -textvariable [::itcl::scope mCz] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add ellCUnitsL {
	::ttk::label $parent.ellCUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}

    grid $itk_component(ellType) \
	-row $mCurrentGridRow \
	-column 0 \
	-sticky nsew
    grid $itk_component(ellName) \
	-row $mCurrentGridRow \
	-column 1 \
	-columnspan 3 \
	-sticky nsew
    incr mCurrentGridRow
    grid x $itk_component(ellXL) \
	$itk_component(ellYL) \
	$itk_component(ellZL)
    incr mCurrentGridRow
    grid $itk_component(ellVL) \
	$itk_component(ellVxE) \
	$itk_component(ellVyE) \
	$itk_component(ellVzE) \
	$itk_component(ellVUnitsL) \
	-row $mCurrentGridRow \
	-sticky nsew
    incr mCurrentGridRow
    grid $itk_component(ellAL) \
	$itk_component(ellAxE) \
	$itk_component(ellAyE) \
	$itk_component(ellAzE) \
	$itk_component(ellAUnitsL) \
	-row $mCurrentGridRow \
	-sticky nsew
    incr mCurrentGridRow
    grid $itk_component(ellBL) \
	$itk_component(ellBxE) \
	$itk_component(ellByE) \
	$itk_component(ellBzE) \
	$itk_component(ellBUnitsL) \
	-row $mCurrentGridRow \
	-sticky nsew
    incr mCurrentGridRow
    grid $itk_component(ellCL) \
	$itk_component(ellCxE) \
	$itk_component(ellCyE) \
	$itk_component(ellCzE) \
	$itk_component(ellCUnitsL) \
	-row $mCurrentGridRow \
	-sticky nsew
    grid columnconfigure $parent 1 -weight 1
    grid columnconfigure $parent 2 -weight 1
    grid columnconfigure $parent 3 -weight 1
    pack $parent -expand yes -fill x -anchor n

    # Set up bindings
    bind $itk_component(ellVxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(ellVyE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(ellVzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(ellAxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(ellAyE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(ellAzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(ellBxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(ellByE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(ellBzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(ellCxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(ellCyE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(ellCzE) <Return> [::itcl::code $this updateGeometryIfMod]
}

::itcl::body EllEditFrame::buildLowerPanel {} {
    set parent [$this childsite lower]

    foreach attribute {A B C ABC} {
	itk_component add set$attribute {
	    ::ttk::radiobutton $parent.set_$attribute \
		-variable [::itcl::scope mEditMode] \
		-value [subst $[subst set$attribute]] \
		-text "Set $attribute" \
		-command [::itcl::code $this initEditState]
	} {}

	pack $itk_component(set$attribute) \
	    -anchor w \
	    -expand yes
    }
}

::itcl::body EllEditFrame::updateGeometryIfMod {} {
    if {$itk_option(-mged) == "" ||
	$itk_option(-geometryObject) == ""} {
	return
    }

    set gdata [$itk_option(-mged) get $itk_option(-geometryObject)]
    set gdata [lrange $gdata 1 end]

    set _V [bu_get_value_by_keyword V $gdata]
    set _Vx [lindex $_V 0]
    set _Vy [lindex $_V 1]
    set _Vz [lindex $_V 2]
    set _A [bu_get_value_by_keyword A $gdata]
    set _Ax [lindex $_A 0]
    set _Ay [lindex $_A 1]
    set _Az [lindex $_A 2]
    set _B [bu_get_value_by_keyword B $gdata]
    set _Bx [lindex $_B 0]
    set _By [lindex $_B 1]
    set _Bz [lindex $_B 2]
    set _C [bu_get_value_by_keyword C $gdata]
    set _Cx [lindex $_C 0]
    set _Cy [lindex $_C 1]
    set _Cz [lindex $_C 2]

    if {$mVx == ""  ||
	$mVx == "-" ||
	$mVy == ""  ||
	$mVy == "-" ||
	$mVz == ""  ||
	$mVz == "-" ||
	$mAx == ""  ||
	$mAx == "-" ||
	$mAy == ""  ||
	$mAy == "-" ||
	$mAz == ""  ||
	$mAz == "-" ||
	$mBx == ""  ||
	$mBx == "-" ||
	$mBy == ""  ||
	$mBy == "-" ||
	$mBz == ""  ||
	$mBz == "-" ||
	$mCx == ""  ||
	$mCx == "-" ||
	$mCy == ""  ||
	$mCy == "-" ||
	$mCz == ""  ||
	$mCz == "-"} {
	# Not valid
	return
    }

    if {$_Vx != $mVx ||
	$_Vy != $mVy ||
	$_Vz != $mVz ||
	$_Ax != $mAx ||
	$_Ay != $mAy ||
	$_Az != $mAz ||
	$_Bx != $mBx ||
	$_By != $mBy ||
	$_Bz != $mBz ||
	$_Cx != $mCx ||
	$_Cy != $mCy ||
	$_Cz != $mCz} {
	updateGeometry
    }
}

::itcl::body EllEditFrame::initEditState {} {
    set mEditCommand pscale
    set mEditClass $EDIT_CLASS_SCALE
    set mEditPCommand [::itcl::code $this p]
    configure -valueUnits "mm"

    switch -- $mEditMode \
	$setA {
	    set mEditParam1 a
	} \
	$setB {
	    set mEditParam1 b
	} \
	$setC {
	    set mEditParam1 c
	} \
	$setABC {
	    set mEditParam1 abc
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
