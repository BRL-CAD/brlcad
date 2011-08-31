#                E T O E D I T F R A M E . T C L
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
#    The class for editing etos within Archer.
#

::itcl::class EtoEditFrame {
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
	common setr 1
	common setr_d 2
	common setC 3
	common rotC 4

	variable mVx ""
	variable mVy ""
	variable mVz ""
	variable mNx ""
	variable mNy ""
	variable mNz ""
	variable mCx ""
	variable mCy ""
	variable mCz ""
	variable mR ""
	variable mR_d ""

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

::itcl::body EtoEditFrame::constructor {args} {
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
::itcl::body EtoEditFrame::initGeometry {gdata} {
    set _V [bu_get_value_by_keyword V $gdata]
    set mVx [lindex $_V 0]
    set mVy [lindex $_V 1]
    set mVz [lindex $_V 2]
    set _N [bu_get_value_by_keyword N $gdata]
    set mNx [lindex $_N 0]
    set mNy [lindex $_N 1]
    set mNz [lindex $_N 2]
    set _C [bu_get_value_by_keyword C $gdata]
    set mCx [lindex $_C 0]
    set mCy [lindex $_C 1]
    set mCz [lindex $_C 2]
    set mR [bu_get_value_by_keyword r $gdata]
    set mR_d [bu_get_value_by_keyword r_d $gdata]

    GeometryEditFrame::initGeometry $gdata
}

::itcl::body EtoEditFrame::updateGeometry {} {
    if {$itk_option(-mged) == "" ||
	$itk_option(-geometryObject) == ""} {
	return
    }

    $itk_option(-mged) adjust $itk_option(-geometryObject) \
	V [list $mVx $mVy $mVz] \
	N [list $mNx $mNy $mNz] \
	C [list $mCx $mCy $mCz] \
	r $mR \
	r_d $mR_d

    GeometryEditFrame::updateGeometry
}

::itcl::body EtoEditFrame::createGeometry {obj} {
    if {![GeometryEditFrame::createGeometry $obj]} {
	return
    }

    $itk_option(-mged) put $obj eto \
	V [list $mCenterX $mCenterY $mCenterZ] \
	N [list $mDelta 0 0] \
	C [list $mDelta 0 0] \
	r $mDelta \
	r_d [expr {$mDelta * 0.1}]
}

::itcl::body EtoEditFrame::p {obj args} {
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
	$setr {
	    $::ArcherCore::application p_pscale $obj r $args
	} \
	$setr_d {
	    $::ArcherCore::application p_pscale $obj d $args
	} \
	$setC {
	    $::ArcherCore::application p_pscale $obj c $args
	} \
	$rotC {
	    $::ArcherCore::application p_protate $obj c $args
	}

    return ""
}


# ------------------------------------------------------------
#                      PROTECTED METHODS
# ------------------------------------------------------------

::itcl::body EtoEditFrame::buildUpperPanel {} {
    set parent [$this childsite]
    itk_component add etoType {
	::ttk::label $parent.etotype \
	    -text "Eto:" \
	    -anchor e
    } {}
    itk_component add etoName {
	::ttk::label $parent.etoname \
	    -textvariable [::itcl::scope itk_option(-geometryObject)] \
	    -anchor w
    } {}

    # Create header labels
    itk_component add etoXL {
	::ttk::label $parent.etoXL \
	    -text "X"
    } {}
    itk_component add etoYL {
	::ttk::label $parent.etoYL \
	    -text "Y"
    } {}
    itk_component add etoZL {
	::ttk::label $parent.etoZL \
	    -text "Z"
    } {}

    # create widgets for vertices
    itk_component add etoVL {
	::ttk::label $parent.etoVL \
	    -text "V:" \
	    -anchor e
    } {}
    itk_component add etoVxE {
	::ttk::entry $parent.etoVxE \
	    -textvariable [::itcl::scope mVx] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add etoVyE {
	::ttk::entry $parent.etoVyE \
	    -textvariable [::itcl::scope mVy] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add etoVzE {
	::ttk::entry $parent.etoVzE \
	    -textvariable [::itcl::scope mVz] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add etoVUnitsL {
	::ttk::label $parent.etoVUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add etoNL {
	::ttk::label $parent.etoNL \
	    -text "N:" \
	    -anchor e
    } {}
    itk_component add etoNxE {
	::ttk::entry $parent.etoNxE \
	    -textvariable [::itcl::scope mNx] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add etoNyE {
	::ttk::entry $parent.etoNyE \
	    -textvariable [::itcl::scope mNy] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add etoNzE {
	::ttk::entry $parent.etoNzE \
	    -textvariable [::itcl::scope mNz] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add etoNUnitsL {
	::ttk::label $parent.etoNUnitsL \
	    -anchor e
    } {}
    itk_component add etoCL {
	::ttk::label $parent.etoCL \
	    -text "C:" \
	    -anchor e
    } {}
    itk_component add etoCxE {
	::ttk::entry $parent.etoCxE \
	    -textvariable [::itcl::scope mCx] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add etoCyE {
	::ttk::entry $parent.etoCyE \
	    -textvariable [::itcl::scope mCy] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add etoCzE {
	::ttk::entry $parent.etoCzE \
	    -textvariable [::itcl::scope mCz] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add etoCUnitsL {
	::ttk::label $parent.etoCUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add etoRL {
	::ttk::label $parent.etoRL \
	    -text "r:" \
	    -anchor e
    } {}
    itk_component add etoRE {
	::ttk::entry $parent.etoRE \
	    -textvariable [::itcl::scope mR] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add etoRUnitsL {
	::ttk::label $parent.etoRVUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add etoR_dL {
	::ttk::label $parent.etoR_dL \
	    -text "r_d:" \
	    -anchor e
    } {}
    itk_component add etoR_dE {
	::ttk::entry $parent.etoR_dE \
	    -textvariable [::itcl::scope mR_d] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add etoR_dUnitsL {
	::ttk::label $parent.etoR_dUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}

    set row 0
    grid $itk_component(etoType) \
	-row $row \
	-column 0 \
	-sticky nsew
    grid $itk_component(etoName) \
	-row $row \
	-column 1 \
	-columnspan 3 \
	-sticky nsew
    incr row
    grid x $itk_component(etoXL) \
	$itk_component(etoYL) \
	$itk_component(etoZL)
    incr row
    grid $itk_component(etoVL) \
	$itk_component(etoVxE) \
	$itk_component(etoVyE) \
	$itk_component(etoVzE) \
	$itk_component(etoVUnitsL) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(etoNL) \
	$itk_component(etoNxE) \
	$itk_component(etoNyE) \
	$itk_component(etoNzE) \
	$itk_component(etoNUnitsL) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(etoCL) \
	$itk_component(etoCxE) \
	$itk_component(etoCyE) \
	$itk_component(etoCzE) \
	$itk_component(etoCUnitsL) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(etoRL) $itk_component(etoRE) \
	-row $row \
	-sticky nsew
    grid $itk_component(etoRUnitsL) \
	-row $row \
	-column 4 \
	-sticky nsew
    incr row
    grid $itk_component(etoR_dL) $itk_component(etoR_dE) \
	-row $row \
	-sticky nsew
    grid $itk_component(etoR_dUnitsL) \
	-row $row \
	-column 4 \
	-sticky nsew
    grid columnconfigure $parent 1 -weight 1
    grid columnconfigure $parent 2 -weight 1
    grid columnconfigure $parent 3 -weight 1
    pack $parent -expand yes -fill x -anchor n

    # Set up bindings
    bind $itk_component(etoVxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(etoVyE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(etoVzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(etoNxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(etoNyE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(etoNzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(etoCxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(etoCyE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(etoCzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(etoRE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(etoR_dE) <Return> [::itcl::code $this updateGeometryIfMod]
}

::itcl::body EtoEditFrame::buildLowerPanel {} {
    set parent [$this childsite lower]

    foreach {attribute op opLabel} {r set Set r_d set Set C set Set C rot Rotate} {
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

::itcl::body EtoEditFrame::updateGeometryIfMod {} {
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
    set _N [bu_get_value_by_keyword N $gdata]
    set _Nx [lindex $_N 0]
    set _Ny [lindex $_N 1]
    set _Nz [lindex $_N 2]
    set _C [bu_get_value_by_keyword C $gdata]
    set _Cx [lindex $_C 0]
    set _Cy [lindex $_C 1]
    set _Cz [lindex $_C 2]
    set _R [bu_get_value_by_keyword r $gdata]
    set _R_d [bu_get_value_by_keyword r_d $gdata]

    if {$mVx == ""  ||
	$mVx == "-" ||
	$mVy == ""  ||
	$mVy == "-" ||
	$mVz == ""  ||
	$mVz == "-" ||
	$mNx == ""  ||
	$mNx == "-" ||
	$mNy == ""  ||
	$mNy == "-" ||
	$mNz == ""  ||
	$mNz == "-" ||
	$mCx == ""  ||
	$mCx == "-" ||
	$mCy == ""  ||
	$mCy == "-" ||
	$mCz == ""  ||
	$mCz == "-" ||
	$mR == ""  ||
	$mR == "-" ||
	$mR_d == ""  ||
	$mR_d == "-"} {
	# Not valid
	return
    }

    if {$_Vx != $mVx ||
	$_Vy != $mVy ||
	$_Vz != $mVz ||
	$_Nx != $mNx ||
	$_Ny != $mNy ||
	$_Nz != $mNz ||
	$_Cx != $mCx ||
	$_Cy != $mCy ||
	$_Cz != $mCz ||
	$_R != $mR ||
	$_R_d != $mR_d} {
	updateGeometry
    }
}

::itcl::body EtoEditFrame::initEditState {} {
    set mEditPCommand [::itcl::code $this p]
    configure -valueUnits "mm"

    switch -- $mEditMode \
	$setr {
	    set mEditCommand pscale
	    set mEditClass $EDIT_CLASS_SCALE
	    set mEditParam1 r
	} \
	$setr_d {
	    set mEditCommand pscale
	    set mEditClass $EDIT_CLASS_SCALE
	    set mEditParam1 d
	} \
	$setC {
	    set mEditCommand pscale
	    set mEditClass $EDIT_CLASS_SCALE
	    set mEditParam1 c
	} \
	$rotC {
	    set mEditCommand protate
	    set mEditClass $EDIT_CLASS_ROT
	    set mEditParam1 c
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
