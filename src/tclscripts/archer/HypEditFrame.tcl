#                H Y P E D I T F R A M E . T C L
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
#    The class for editing elliptical hyperboloids within Archer.
#

::itcl::class HypEditFrame {
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
	common setH  1
	common setHV 2
	common setA  3
	common setB  4
	common setC  5
	common rotH  6

	variable mVx ""
	variable mVy ""
	variable mVz ""
	variable mHx ""
	variable mHy ""
	variable mHz ""
	variable mAx ""
	variable mAy ""
	variable mAz ""
	variable mB ""
	variable mC ""

	# Methods used by the constructor.
	# Override methods in GeometryEditFrame.
	method buildUpperPanel
	method buildLowerPanel

	# Override what's in GeometryEditFrame.
	method updateGeometryIfMod {}
	method initEditState {}
    }

    private {}
}


# ------------------------------------------------------------
#                      CONSTRUCTOR
# ------------------------------------------------------------

::itcl::body HypEditFrame::constructor {args} {
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
::itcl::body HypEditFrame::initGeometry {gdata} {
    set _V [bu_get_value_by_keyword V $gdata]
    set mVx [lindex $_V 0]
    set mVy [lindex $_V 1]
    set mVz [lindex $_V 2]
    set _H [bu_get_value_by_keyword H $gdata]
    set mHx [lindex $_H 0]
    set mHy [lindex $_H 1]
    set mHz [lindex $_H 2]
    set _A [bu_get_value_by_keyword A $gdata]
    set mAx [lindex $_A 0]
    set mAy [lindex $_A 1]
    set mAz [lindex $_A 2]
    set mB [bu_get_value_by_keyword b $gdata]
    set mC [bu_get_value_by_keyword bnr $gdata]

    GeometryEditFrame::initGeometry $gdata
}

::itcl::body HypEditFrame::updateGeometry {} {
    if {$itk_option(-mged) == "" ||
	$itk_option(-geometryObject) == ""} {
	return
    }

    $itk_option(-mged) adjust $itk_option(-geometryObject) \
	V [list $mVx $mVy $mVz] \
	H [list $mHx $mHy $mHz] \
	A [list $mAx $mAy $mAz] \
	b $mB \
	bnr $mC

    GeometryEditFrame::updateGeometry
}

::itcl::body HypEditFrame::createGeometry {obj} {
    if {![GeometryEditFrame::createGeometry $obj]} {
	return
    }

    $itk_option(-mged) put $obj hyp \
	V [list $mCenterX $mCenterY $mCenterZ] \
	H [list 0 0 $mDelta] \
	A [list 0 [expr {$mDelta * 0.5}] 0] \
	b [expr {$mDelta * 0.25} \
	bnr [expr {$mDelta * 0.1}]
}

::itcl::body HypEditFrame::p {obj args} {
    switch -- $GeometryEditFrame::mEditClass \
	$GeometryEditFrame::EDIT_CLASS_SCALE {
	    if {[llength $args] != 1 || ![string is double $args]} {
		return "Usage: p sf"
	    }
	} \
	$GeometryEditFrame::EDIT_CLASS_ROT {
	    if {[llength $args] != 3 ||
		![string is double [lindex $args 0]] ||
		![string is double [lindex $args 1]] ||
		![string is double [lindex $args 2]]} {
		return "Usage: p rx ry rz"
	    }
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
	$setH {
	    $::ArcherCore::application p_pscale $obj h $args
	} \
	$setHV {
	    $::ArcherCore::application p_pscale $obj hv $args
	} \
	$rotH {
	    $::ArcherCore::application p_protate $obj h $args
	}

    return ""
}


# ------------------------------------------------------------
#                      PROTECTED METHODS
# ------------------------------------------------------------

::itcl::body HypEditFrame::buildUpperPanel {} {
    set parent [$this childsite]
    itk_component add hypType {
	::ttk::label $parent.hyptype \
	    -text "Hyp:" \
	    -anchor e
    } {}
    itk_component add hypName {
	::ttk::label $parent.hypname \
	    -textvariable [::itcl::scope itk_option(-geometryObject)] \
	    -anchor w
    } {}

    # Create header labels
    itk_component add hypXL {
	::ttk::label $parent.hypXL \
	    -text "X"
    } {}
    itk_component add hypYL {
	::ttk::label $parent.hypYL \
	    -text "Y"
    } {}
    itk_component add hypZL {
	::ttk::label $parent.hypZL \
	    -text "Z"
    } {}

    # create widgets for attributes
    itk_component add hypVL {
	::ttk::label $parent.hypVL \
	    -text "V:" \
	    -anchor e
    } {}
    itk_component add hypVxE {
	::ttk::entry $parent.hypVxE \
	    -textvariable [::itcl::scope mVx] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add hypVyE {
	::ttk::entry $parent.hypVyE \
	    -textvariable [::itcl::scope mVy] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add hypVzE {
	::ttk::entry $parent.hypVzE \
	    -textvariable [::itcl::scope mVz] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add hypVUnitsL {
	::ttk::label $parent.hypVUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add hypHL {
	::ttk::label $parent.hypHL \
	    -text "H:" \
	    -anchor e
    } {}
    itk_component add hypHxE {
	::ttk::entry $parent.hypHxE \
	    -textvariable [::itcl::scope mHx] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add hypHyE {
	::ttk::entry $parent.hypHyE \
	    -textvariable [::itcl::scope mHy] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add hypHzE {
	::ttk::entry $parent.hypHzE \
	    -textvariable [::itcl::scope mHz] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add hypHUnitsL {
	::ttk::label $parent.hypHUnitsL \
	    -anchor e
    } {}
    itk_component add hypAL {
	::ttk::label $parent.hypAL \
	    -text "A:" \
	    -anchor e
    } {}
    itk_component add hypAxE {
	::ttk::entry $parent.hypAxE \
	    -textvariable [::itcl::scope mAx] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add hypAyE {
	::ttk::entry $parent.hypAyE \
	    -textvariable [::itcl::scope mAy] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add hypAzE {
	::ttk::entry $parent.hypAzE \
	    -textvariable [::itcl::scope mAz] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add hypAUnitsL {
	::ttk::label $parent.hypAUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add hypBL {
	::ttk::label $parent.hypBL \
	    -text "b:" \
	    -anchor e
    } {}
    itk_component add hypBE {
	::ttk::entry $parent.hypBE \
	    -textvariable [::itcl::scope mB] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add hypBUnitsL {
	::ttk::label $parent.hypBVUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add hypCL {
	::ttk::label $parent.hypCL \
	    -text "bnr:" \
	    -anchor e
    } {}
    itk_component add hypCE {
	::ttk::entry $parent.hypCE \
	    -textvariable [::itcl::scope mC] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add hypCUnitsL {
	::ttk::label $parent.hypCUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}

    set row 0
    grid $itk_component(hypType) \
	-row $row \
	-column 0 \
	-sticky nsew
    grid $itk_component(hypName) \
	-row $row \
	-column 1 \
	-columnspan 3 \
	-sticky nsew
    incr row
    grid x $itk_component(hypXL) \
	$itk_component(hypYL) \
	$itk_component(hypZL)
    incr row
    grid $itk_component(hypVL) \
	$itk_component(hypVxE) \
	$itk_component(hypVyE) \
	$itk_component(hypVzE) \
	$itk_component(hypVUnitsL) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(hypHL) \
	$itk_component(hypHxE) \
	$itk_component(hypHyE) \
	$itk_component(hypHzE) \
	$itk_component(hypHUnitsL) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(hypAL) \
	$itk_component(hypAxE) \
	$itk_component(hypAyE) \
	$itk_component(hypAzE) \
	$itk_component(hypAUnitsL) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(hypBL) $itk_component(hypBE) \
	-row $row \
	-sticky nsew
    grid $itk_component(hypBUnitsL) \
	-row $row \
	-column 4 \
	-sticky nsew
    incr row
    grid $itk_component(hypCL) $itk_component(hypCE) \
	-row $row \
	-sticky nsew
    grid $itk_component(hypCUnitsL) \
	-row $row \
	-column 4 \
	-sticky nsew
    grid columnconfigure $parent 1 -weight 1
    grid columnconfigure $parent 2 -weight 1
    grid columnconfigure $parent 3 -weight 1
    pack $parent -expand yes -fill x -anchor n

    # Set up bindings
    bind $itk_component(hypVxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(hypVyE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(hypVzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(hypHxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(hypHyE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(hypHzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(hypAxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(hypAyE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(hypAzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(hypBE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(hypCE) <Return> [::itcl::code $this updateGeometryIfMod]
}

::itcl::body HypEditFrame::buildLowerPanel {} {
    set parent [$this childsite lower]

    set alist [list H set Set HV set Set A set Set B set Set C set Set H rot Rotate]
    foreach {attribute op opLabel} $alist {
	itk_component add $op$attribute {
	    ::ttk::radiobutton $parent.$op\_$attribute \
		-variable [::itcl::scope mEditMode] \
		-value [subst $[subst $op$attribute]] \
		-text "$opLabel $attribute" \
		-command [::itcl::code $this initEditState]
	} {}

	pack $itk_component($op$attribute) \
	    -anchor w \
	    -expand yes
    }
}

::itcl::body HypEditFrame::updateGeometryIfMod {} {
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
    set _H [bu_get_value_by_keyword H $gdata]
    set _Hx [lindex $_H 0]
    set _Hy [lindex $_H 1]
    set _Hz [lindex $_H 2]
    set _A [bu_get_value_by_keyword A $gdata]
    set _Ax [lindex $_A 0]
    set _Ay [lindex $_A 1]
    set _Az [lindex $_A 2]
    set _B [bu_get_value_by_keyword b $gdata]
    set _C [bu_get_value_by_keyword bnr $gdata]

    if {$mVx == ""  ||
	$mVx == "-" ||
	$mVy == ""  ||
	$mVy == "-" ||
	$mVz == ""  ||
	$mVz == "-" ||
	$mHx == ""  ||
	$mHx == "-" ||
	$mHy == ""  ||
	$mHy == "-" ||
	$mHz == ""  ||
	$mHz == "-" ||
	$mAx == ""  ||
	$mAx == "-" ||
	$mAy == ""  ||
	$mAy == "-" ||
	$mAz == ""  ||
	$mAz == "-" ||
	$mB == ""  ||
	$mB == "-" ||
	$mC == ""  ||
	$mC == "-"} {
	# Not valid
	return
    }

    if {$_Vx != $mVx ||
	$_Vy != $mVy ||
	$_Vz != $mVz ||
	$_Hx != $mHx ||
	$_Hy != $mHy ||
	$_Hz != $mHz ||
	$_Ax != $mAx ||
	$_Ay != $mAy ||
	$_Az != $mAz ||
	$_B != $mB ||
	$_C != $mC} {
	updateGeometry
    }
}

::itcl::body HypEditFrame::initEditState {} {
    set mEditPCommand [::itcl::code $this p]
    configure -valueUnits "mm"

    switch -- $mEditMode \
	$setA {
	    set mEditCommand pscale
	    set mEditClass $EDIT_CLASS_SCALE
	    set mEditParam1 a
	} \
	$setB {
	    set mEditCommand pscale
	    set mEditClass $EDIT_CLASS_SCALE
	    set mEditParam1 b
	} \
	$setC {
	    set mEditCommand pscale
	    set mEditClass $EDIT_CLASS_SCALE
	    set mEditParam1 c
	} \
	$setH {
	    set mEditCommand pscale
	    set mEditClass $EDIT_CLASS_SCALE
	    set mEditParam1 h
	} \
	$setHV {
	    set mEditCommand pscale
	    set mEditClass $EDIT_CLASS_SCALE
	    set mEditParam1 hv
	} \
	$rotH {
	    set mEditCommand protate
	    set mEditClass $EDIT_CLASS_ROT
	    set mEditParam1 h
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
