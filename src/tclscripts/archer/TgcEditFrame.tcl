#                T G C E D I T F R A M E . T C L
# BRL-CAD
#
# Copyright (c) 2002-2014 United States Government as represented by
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
#    The class for editing tgc's within Archer.
#
##############################################################

::itcl::class TgcEditFrame {
    inherit GeometryEditFrame

    constructor {args} {}
    destructor {}

    public {
	# Override what's in GeometryEditFrame
	method initGeometry {gdata}
	method updateGeometry {}
	method checkpointGeometry {}
	method revertGeometry {}
	method createGeometry {obj}
	method moveElement {_dm _obj _vx _vy _ocenter}
	method p {obj args}
    }

    protected {
	common setA     1
	common setB     2
	common setC     3
	common setD     4
	common setAB    5
	common setCD    6
	common setABCD  7
	common setH     8
	common setHCD   9
	common setHV   10
	common setHVAB 11
	common rotH    12
	common rotAB  13
	common moveHR  14
	common moveH  15

	variable mVx ""
	variable mVy ""
	variable mVz ""
	variable mHx ""
	variable mHy ""
	variable mHz ""
	variable mAx ""
	variable mAy ""
	variable mAz ""
	variable mBx ""
	variable mBy ""
	variable mBz ""
	variable mCx ""
	variable mCy ""
	variable mCz ""
	variable mDx ""
	variable mDy ""
	variable mDz ""

	# Checkpoint values
	variable checkpointed_name ""
	variable cmVx ""
	variable cmVy ""
	variable cmVz ""
	variable cmHx ""
	variable cmHy ""
	variable cmHz ""
	variable cmAx ""
	variable cmAy ""
	variable cmAz ""
	variable cmBx ""
	variable cmBy ""
	variable cmBz ""
	variable cmCx ""
	variable cmCy ""
	variable cmCz ""
	variable cmDx ""
	variable cmDy ""
	variable cmDz ""


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

::itcl::body TgcEditFrame::constructor {args} {
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
::itcl::body TgcEditFrame::initGeometry {gdata} {
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
    set _B [bu_get_value_by_keyword B $gdata]
    set mBx [lindex $_B 0]
    set mBy [lindex $_B 1]
    set mBz [lindex $_B 2]
    set _C [bu_get_value_by_keyword C $gdata]
    set mCx [lindex $_C 0]
    set mCy [lindex $_C 1]
    set mCz [lindex $_C 2]
    set _D [bu_get_value_by_keyword D $gdata]
    set mDx [lindex $_D 0]
    set mDy [lindex $_D 1]
    set mDz [lindex $_D 2]

    GeometryEditFrame::initGeometry $gdata
    set curr_name $itk_option(-geometryObject)
    if {$cmVx == "" || "$checkpointed_name" != "$curr_name"} {checkpointGeometry}
}

::itcl::body TgcEditFrame::updateGeometry {} {
    if {$itk_option(-mged) == "" ||
	$itk_option(-geometryObject) == ""} {
	return
    }

    $itk_option(-mged) adjust $itk_option(-geometryObject) \
	V [list $mVx $mVy $mVz] \
	H [list $mHx $mHy $mHz] \
	A [list $mAx $mAy $mAz] \
	B [list $mBx $mBy $mBz] \
	C [list $mCx $mCy $mCz] \
	D [list $mDx $mDy $mDz]

    GeometryEditFrame::updateGeometry
}

::itcl::body TgcEditFrame::checkpointGeometry {} {
    set checkpointed_name $itk_option(-geometryObject)
    set cmVx $mVx
    set cmVy $mVy
    set cmVz $mVz
    set cmHx $mHx
    set cmHy $mHy
    set cmHz $mHz
    set cmAx $mAx
    set cmAy $mAy
    set cmAz $mAz
    set cmBx $mBx
    set cmBy $mBy
    set cmBz $mBz
    set cmCx $mCx
    set cmCy $mCy
    set cmCz $mCz
    set cmDx $mDx
    set cmDy $mDy
    set cmDz $mDz
}

::itcl::body TgcEditFrame::revertGeometry {} {
    set mVx $cmVx
    set mVy $cmVy
    set mVz $cmVz
    set mHx $cmHx
    set mHy $cmHy
    set mHz $cmHz
    set mAx $cmAx
    set mAy $cmAy
    set mAz $cmAz
    set mBx $cmBx
    set mBy $cmBy
    set mBz $cmBz
    set mCx $cmCx
    set mCy $cmCy
    set mCz $cmCz
    set mDx $cmDx
    set mDy $cmDy
    set mDz $cmDz

    updateGeometry
}

::itcl::body TgcEditFrame::createGeometry {obj} {
    if {![GeometryEditFrame::createGeometry $obj]} {
	return
    }

    $itk_option(-mged) put $obj tgc \
	V [list $mCenterX $mCenterY $mCenterZ] \
	H [list 0 0 [expr {$mDelta * 3.0}]] \
	A [list $mDelta 0 0] \
	B [list 0 $mDelta 0] \
	C [list $mDelta 0 0] \
	D [list 0 $mDelta 0]
}


::itcl::body TgcEditFrame::moveElement {_dm _obj _vx _vy _ocenter} {
    set param1 [string tolower $GeometryEditFrame::mEditParam1]
    switch -- $param1 {
	h -
	hr {
	    set h [$itk_option(-mged) get $itk_option(-geometryObject) H]
	    set v [$itk_option(-mged) get $itk_option(-geometryObject) V]
	    set model_h [::vadd2 $h $v]
	    set view_h [$itk_option(-mged) pane_m2v_point $_dm $model_h]
	    set vz [lindex $view_h 2]
	    set new_view_h [list $_vx $_vy $vz]
	    set new_model_h [$itk_option(-mged) pane_v2m_point $_dm $new_view_h]
	    $itk_option(-mged) ptranslate $_obj $mEditParam1 $new_model_h
	}
	default {
	    $::ArcherCore::application putString "TgcEditFrame:moveTgcElement: bad parameter - $mEditParam1"
	}
    }
}


::itcl::body TgcEditFrame::p {obj args} {
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
	} \
	$GeometryEditFrame::EDIT_CLASS_TRANS {
	    if {[llength $args] != 3 ||
		![string is double [lindex $args 0]] ||
		![string is double [lindex $args 1]] ||
		![string is double [lindex $args 2]]} {
		return "Usage: p tx ty tz"
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
	$setD {
	    $::ArcherCore::application p_pscale $obj d $args
	} \
	$setAB {
	    $::ArcherCore::application p_pscale $obj ab $args
	} \
	$setCD {
	    $::ArcherCore::application p_pscale $obj cd $args
	} \
	$setABCD {
	    $::ArcherCore::application p_pscale $obj abcd $args
	} \
	$setH {
	    $::ArcherCore::application p_pscale $obj h $args
	} \
	$setHCD {
	    $::ArcherCore::application p_pscale $obj hcd $args
	} \
	$setHV {
	    $::ArcherCore::application p_pscale $obj hv $args
	} \
	$setHVAB {
	    $::ArcherCore::application p_pscale $obj hvab $args
	} \
	$rotH {
	    $::ArcherCore::application p_protate $obj h $args
	} \
	$rotAB {
	    $::ArcherCore::application p_protate $obj hab $args
	} \
	$moveHR {
	    $::ArcherCore::application p_ptranslate $obj hr $args
	} \
	$moveH {
	    $::ArcherCore::application p_ptranslate $obj h $args
	}

    return ""
}

# ------------------------------------------------------------
#                      PROTECTED METHODS
# ------------------------------------------------------------

::itcl::body TgcEditFrame::buildUpperPanel {} {
    set parent [$this childsite]
    itk_component add tgcType {
	::ttk::label $parent.tgctype \
	    -text "Tgc:" \
	    -anchor e
    } {}
    itk_component add tgcName {
	::ttk::label $parent.tgcname \
	    -textvariable [::itcl::scope itk_option(-geometryObject)] \
	    -anchor w
    } {}

    # Create header labels
    itk_component add tgcXL {
	::ttk::label $parent.tgcXL \
	    -text "X"
    } {}
    itk_component add tgcYL {
	::ttk::label $parent.tgcYL \
	    -text "Y"
    } {}
    itk_component add tgcZL {
	::ttk::label $parent.tgcZL \
	    -text "Z"
    } {}

    # create widgets for vertices
    itk_component add tgcVL {
	::ttk::label $parent.tgcVL \
	    -text "V:" \
	    -anchor e
    } {}
    itk_component add tgcVxE {
	::ttk::entry $parent.tgcVxE \
	    -textvariable [::itcl::scope mVx] \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add tgcVyE {
	::ttk::entry $parent.tgcVyE \
	    -textvariable [::itcl::scope mVy] \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add tgcVzE {
	::ttk::entry $parent.tgcVzE \
	    -textvariable [::itcl::scope mVz] \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add tgcVUnitsL {
	::ttk::label $parent.tgcVUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add tgcHL {
	::ttk::label $parent.tgcHL \
	    -text "H:" \
	    -anchor e
    } {}
    itk_component add tgcHxE {
	::ttk::entry $parent.tgcHxE \
	    -textvariable [::itcl::scope mHx] \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add tgcHyE {
	::ttk::entry $parent.tgcHyE \
	    -textvariable [::itcl::scope mHy] \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add tgcHzE {
	::ttk::entry $parent.tgcHzE \
	    -textvariable [::itcl::scope mHz] \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add tgcHUnitsL {
	::ttk::label $parent.tgcHUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add tgcAL {
	::ttk::label $parent.tgcAL \
	    -text "A:" \
	    -anchor e
    } {}
    itk_component add tgcAxE {
	::ttk::entry $parent.tgcAxE \
	    -textvariable [::itcl::scope mAx] \
	    -state disabled \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add tgcAyE {
	::ttk::entry $parent.tgcAyE \
	    -textvariable [::itcl::scope mAy] \
	    -state disabled \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add tgcAzE {
	::ttk::entry $parent.tgcAzE \
	    -textvariable [::itcl::scope mAz] \
	    -state disabled \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add tgcAUnitsL {
	::ttk::label $parent.tgcAUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add tgcBL {
	::ttk::label $parent.tgcBL \
	    -text "B:" \
	    -anchor e
    } {}
    itk_component add tgcBxE {
	::ttk::entry $parent.tgcBxE \
	    -textvariable [::itcl::scope mBx] \
	    -state disabled \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add tgcByE {
	::ttk::entry $parent.tgcByE \
	    -textvariable [::itcl::scope mBy] \
	    -state disabled \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add tgcBzE {
	::ttk::entry $parent.tgcBzE \
	    -textvariable [::itcl::scope mBz] \
	    -state disabled \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add tgcBUnitsL {
	::ttk::label $parent.tgcBUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add tgcCL {
	::ttk::label $parent.tgcCL \
	    -text "C:" \
	    -anchor e
    } {}
    itk_component add tgcCxE {
	::ttk::entry $parent.tgcCxE \
	    -textvariable [::itcl::scope mCx] \
	    -state disabled \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add tgcCyE {
	::ttk::entry $parent.tgcCyE \
	    -textvariable [::itcl::scope mCy] \
	    -state disabled \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add tgcCzE {
	::ttk::entry $parent.tgcCzE \
	    -textvariable [::itcl::scope mCz] \
	    -state disabled \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add tgcCUnitsL {
	::ttk::label $parent.tgcCUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add tgcDL {
	::ttk::label $parent.tgcDL \
	    -text "D:" \
	    -anchor e
    } {}
    itk_component add tgcDxE {
	::ttk::entry $parent.tgcDxE \
	    -textvariable [::itcl::scope mDx] \
	    -state disabled \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add tgcDyE {
	::ttk::entry $parent.tgcDyE \
	    -textvariable [::itcl::scope mDy] \
	    -state disabled \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add tgcDzE {
	::ttk::entry $parent.tgcDzE \
	    -textvariable [::itcl::scope mDz] \
	    -state disabled \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add tgcDUnitsL {
	::ttk::label $parent.tgcDUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}

    itk_component add checkpointButton {
	::ttk::button $parent.checkpointButton \
	-text {CheckPoint} \
	-command "[::itcl::code $this checkpointGeometry]"
    } {}

    itk_component add revertButton {
	::ttk::button $parent.revertButton \
	-text {Revert} \
	-command "[::itcl::code $this revertGeometry]"
    } {}

    set row 0
    grid $itk_component(tgcType) \
	-row $row \
	-column 0 \
	-sticky nsew
    grid $itk_component(tgcName) \
	-row $row \
	-column 1 \
	-columnspan 3 \
	-sticky nsew
    incr row
    grid x $itk_component(tgcXL) \
	$itk_component(tgcYL) \
	$itk_component(tgcZL)
    incr row
    grid $itk_component(tgcVL) \
	$itk_component(tgcVxE) \
	$itk_component(tgcVyE) \
	$itk_component(tgcVzE) \
	$itk_component(tgcVUnitsL) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(tgcHL) \
	$itk_component(tgcHxE) \
	$itk_component(tgcHyE) \
	$itk_component(tgcHzE) \
	$itk_component(tgcHUnitsL) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(tgcAL) \
	$itk_component(tgcAxE) \
	$itk_component(tgcAyE) \
	$itk_component(tgcAzE) \
	$itk_component(tgcAUnitsL) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(tgcBL) \
	$itk_component(tgcBxE) \
	$itk_component(tgcByE) \
	$itk_component(tgcBzE) \
	$itk_component(tgcBUnitsL) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(tgcCL) \
	$itk_component(tgcCxE) \
	$itk_component(tgcCyE) \
	$itk_component(tgcCzE) \
	$itk_component(tgcCUnitsL) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(tgcDL) \
	$itk_component(tgcDxE) \
	$itk_component(tgcDyE) \
	$itk_component(tgcDzE) \
	$itk_component(tgcDUnitsL) \
	-row $row \
	-sticky nsew

    incr row
    set col 0
    grid $itk_component(checkpointButton) \
	-row $row \
	-column $col \
	-columnspan 2 \
	-sticky nsew
    incr col
    incr col
    grid $itk_component(revertButton) \
	-row $row \
	-column $col \
	-columnspan 2 \
	-sticky nsew

    grid columnconfigure $parent 1 -weight 1
    grid columnconfigure $parent 2 -weight 1
    grid columnconfigure $parent 3 -weight 1
    pack $parent -expand yes -fill x -anchor n

    # Set up bindings
    bind $itk_component(tgcVxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(tgcVyE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(tgcVzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(tgcHxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(tgcHyE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(tgcHzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(tgcAxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(tgcAyE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(tgcAzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(tgcBxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(tgcByE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(tgcBzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(tgcCxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(tgcCyE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(tgcCzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(tgcDxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(tgcDyE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(tgcDzE) <Return> [::itcl::code $this updateGeometryIfMod]
}

::itcl::body TgcEditFrame::buildLowerPanel {} {
    set parent [$this childsite lower]

    set alist [list \
		   A set Set B set Set C set Set D set Set \
		   AB set Set CD set Set ABCD set Set \
		   H set Set HV set Set HVAB set Set HCD set Set \
		   H rot Rotate AB rot Rotate HR move {Move End} \
		   H move {Move End} \
		  ]

    set row 0
    foreach {attribute op opLabel} $alist {
	itk_component add $op$attribute {
	    ::ttk::radiobutton $parent.$op\_$attribute \
		-variable [::itcl::scope mEditMode] \
		-value [subst $[subst $op$attribute]] \
		-text "$opLabel $attribute" \
		-command [::itcl::code $this initEditState]
	} {}

	grid $itk_component($op$attribute) -row $row -column 0 -sticky nsew
	incr row
    }

    grid rowconfigure $parent $row -weight 1
}

::itcl::body TgcEditFrame::updateGeometryIfMod {} {
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
    set _B [bu_get_value_by_keyword B $gdata]
    set _Bx [lindex $_B 0]
    set _By [lindex $_B 1]
    set _Bz [lindex $_B 2]
    set _C [bu_get_value_by_keyword C $gdata]
    set _Cx [lindex $_C 0]
    set _Cy [lindex $_C 1]
    set _Cz [lindex $_C 2]
    set _D [bu_get_value_by_keyword D $gdata]
    set _Dx [lindex $_D 0]
    set _Dy [lindex $_D 1]
    set _Dz [lindex $_D 2]

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
	$mCz == "-" ||
	$mDx == ""  ||
	$mDx == "-" ||
	$mDy == ""  ||
	$mDy == "-" ||
	$mDz == ""  ||
	$mDz == "-"} {
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
	$_Bx != $mBx ||
	$_By != $mBy ||
	$_Bz != $mBz ||
	$_Cx != $mCx ||
	$_Cy != $mCy ||
	$_Cz != $mCz ||
	$_Dx != $mDx ||
	$_Dy != $mDy ||
	$_Dz != $mDz} {
	updateGeometry
    }
}

::itcl::body TgcEditFrame::initEditState {} {
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
	$setD {
	    set mEditCommand pscale
	    set mEditClass $EDIT_CLASS_SCALE
	    set mEditParam1 d
	} \
	$setAB {
	    set mEditCommand pscale
	    set mEditClass $EDIT_CLASS_SCALE
	    set mEditParam1 ab
	} \
	$setCD {
	    set mEditCommand pscale
	    set mEditClass $EDIT_CLASS_SCALE
	    set mEditParam1 cd
	} \
	$setABCD {
	    set mEditCommand pscale
	    set mEditClass $EDIT_CLASS_SCALE
	    set mEditParam1 abcd
	} \
	$setH {
	    set mEditCommand pscale
	    set mEditClass $EDIT_CLASS_SCALE
	    set mEditParam1 h
	} \
	$setHCD {
	    set mEditCommand pscale
	    set mEditClass $EDIT_CLASS_SCALE
	    set mEditParam1 hcd
	} \
	$setHV {
	    set mEditCommand pscale
	    set mEditClass $EDIT_CLASS_SCALE
	    set mEditParam1 hv
	} \
	$setHVAB {
	    set mEditCommand pscale
	    set mEditClass $EDIT_CLASS_SCALE
	    set mEditParam1 hvab
	} \
	$rotH {
	    set mEditCommand protate
	    set mEditClass $EDIT_CLASS_ROT
	    set mEditParam1 h
	} \
	$rotAB {
	    set mEditCommand protate
	    set mEditClass $EDIT_CLASS_ROT
	    set mEditParam1 hab
	} \
	$moveHR {
	    set mEditCommand ptranslate
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditParam1 hr
	} \
	$moveH {
	    set mEditCommand ptranslate
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditParam1 h
	} \
	default {
	    set mEditCommand ""
	    set mEditPCommand ""
	    set mEditParam1 ""
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
