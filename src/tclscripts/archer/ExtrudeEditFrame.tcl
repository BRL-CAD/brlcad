#            E X T R U D E E D I T F R A M E . T C L
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
#    The class for editing extrusions within Archer.
#
##############################################################

::itcl::class ExtrudeEditFrame {
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
	method p {obj args}
    }

    protected {
	common setH   1
	common moveHR 2
	common moveH  3

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
	variable mS ""

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
	variable cmS ""


	# Methods used by the constructor.
	# Override methods in GeometryEditFrame.
	method buildUpperPanel {}
	method buildLowerPanel {}

	# Override what's in GeometryEditFrame.
	method updateGeometryIfMod {}
	method initEditState {}
    }

    private {}
}


# ------------------------------------------------------------
#                      CONSTRUCTOR
# ------------------------------------------------------------

::itcl::body ExtrudeEditFrame::constructor {args} {
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
::itcl::body ExtrudeEditFrame::initGeometry {gdata} {
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
    set mS [bu_get_value_by_keyword S $gdata]

    GeometryEditFrame::initGeometry $gdata
    set curr_name $itk_option(-geometryObject)
    if {$cmVx == "" || "$checkpointed_name" != "$curr_name"} {checkpointGeometry}
}

::itcl::body ExtrudeEditFrame::updateGeometry {} {
    if {$itk_option(-mged) == "" ||
	$itk_option(-geometryObject) == ""} {
	return
    }

    $itk_option(-mged) adjust $itk_option(-geometryObject) \
	V [list $mVx $mVy $mVz] \
	H [list $mHx $mHy $mHz] \
	A [list $mAx $mAy $mAz] \
	B [list $mBx $mBy $mBz] \
	S $mS

    GeometryEditFrame::updateGeometry
}

::itcl::body ExtrudeEditFrame::checkpointGeometry {} {
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
    set cmS  $mS
}

::itcl::body ExtrudeEditFrame::revertGeometry {} {
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
    set mS  $cmS

    updateGeometry
}

::itcl::body ExtrudeEditFrame::createGeometry {obj} {
    if {![GeometryEditFrame::createGeometry $obj]} {
	return
    }

    $itk_option(-mged) put $obj extrude \
	V [list $mCenterX $mCenterY $mCenterZ] \
	H [list 0 0 $mDelta] \
	A [list $mDelta 0 0] \
	B [list 0 $mDelta 0] \
	S "none"
}

::itcl::body ExtrudeEditFrame::p {obj args} {
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
	$setH {
	    $::ArcherCore::application p_pscale $obj h $args
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

::itcl::body ExtrudeEditFrame::buildUpperPanel {} {
    set parent [$this childsite]
    itk_component add extrudeType {
	::ttk::label $parent.extrudetype \
	    -text "Extrude:" \
	    -anchor e
    } {}
    itk_component add extrudeName {
	::ttk::label $parent.extrudename \
	    -textvariable [::itcl::scope itk_option(-geometryObject)] \
	    -anchor w
    } {}

    # Create header labels
    itk_component add extrudeXL {
	::ttk::label $parent.extrudeXL \
	    -text "X"
    } {}
    itk_component add extrudeYL {
	::ttk::label $parent.extrudeYL \
	    -text "Y"
    } {}
    itk_component add extrudeZL {
	::ttk::label $parent.extrudeZL \
	    -text "Z"
    } {}

    # create widgets for vertices
    itk_component add extrudeVL {
	::ttk::label $parent.extrudeVL \
	    -text "V:" \
	    -anchor e
    } {}
    itk_component add extrudeVxE {
	::ttk::entry $parent.extrudeVxE \
	    -textvariable [::itcl::scope mVx] \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add extrudeVyE {
	::ttk::entry $parent.extrudeVyE \
	    -textvariable [::itcl::scope mVy] \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add extrudeVzE {
	::ttk::entry $parent.extrudeVzE \
	    -textvariable [::itcl::scope mVz] \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add extrudeVUnitsL {
	::ttk::label $parent.extrudeVUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add extrudeHL {
	::ttk::label $parent.extrudeHL \
	    -text "H:" \
	    -anchor e
    } {}
    itk_component add extrudeHxE {
	::ttk::entry $parent.extrudeHxE \
	    -textvariable [::itcl::scope mHx] \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add extrudeHyE {
	::ttk::entry $parent.extrudeHyE \
	    -textvariable [::itcl::scope mHy] \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add extrudeHzE {
	::ttk::entry $parent.extrudeHzE \
	    -textvariable [::itcl::scope mHz] \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add extrudeHUnitsL {
	::ttk::label $parent.extrudeHUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add extrudeAL {
	::ttk::label $parent.extrudeAL \
	    -text "A:" \
	    -anchor e
    } {}
    itk_component add extrudeAxE {
	::ttk::entry $parent.extrudeAxE \
	    -textvariable [::itcl::scope mAx] \
	    -state disabled \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add extrudeAyE {
	::ttk::entry $parent.extrudeAyE \
	    -textvariable [::itcl::scope mAy] \
	    -state disabled \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add extrudeAzE {
	::ttk::entry $parent.extrudeAzE \
	    -textvariable [::itcl::scope mAz] \
	    -state disabled \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add extrudeAUnitsL {
	::ttk::label $parent.extrudeAUnitsL \
	    -anchor e
    } {}
    itk_component add extrudeBL {
	::ttk::label $parent.extrudeBL \
	    -text "B:" \
	    -anchor e
    } {}
    itk_component add extrudeBxE {
	::ttk::entry $parent.extrudeBxE \
	    -textvariable [::itcl::scope mBx] \
	    -state disabled \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add extrudeByE {
	::ttk::entry $parent.extrudeByE \
	    -textvariable [::itcl::scope mBy] \
	    -state disabled \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add extrudeBzE {
	::ttk::entry $parent.extrudeBzE \
	    -textvariable [::itcl::scope mBz] \
	    -state disabled \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add extrudeBUnitsL {
	::ttk::label $parent.extrudeBUnitsL \
	    -anchor e
    } {}
    itk_component add extrudeSL {
	::ttk::label $parent.extrudeSL \
	    -text "S:" \
	    -anchor e
    } {}
    itk_component add extrudeSE {
	::ttk::entry $parent.extrudeSE \
	    -textvariable [::itcl::scope mS]
    } {}
    itk_component add extrudeSUnitsL {
	::ttk::label $parent.extrudeSUnitsL \
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
    grid $itk_component(extrudeType) \
	-row $row \
	-column 0 \
	-sticky nsew
    grid $itk_component(extrudeName) \
	-row $row \
	-column 1 \
	-columnspan 3 \
	-sticky nsew
    incr row
    grid x $itk_component(extrudeXL) \
	$itk_component(extrudeYL) \
	$itk_component(extrudeZL)
    incr row
    grid $itk_component(extrudeVL) \
	$itk_component(extrudeVxE) \
	$itk_component(extrudeVyE) \
	$itk_component(extrudeVzE) \
	$itk_component(extrudeVUnitsL) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(extrudeHL) \
	$itk_component(extrudeHxE) \
	$itk_component(extrudeHyE) \
	$itk_component(extrudeHzE) \
	$itk_component(extrudeHUnitsL) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(extrudeAL) \
	$itk_component(extrudeAxE) \
	$itk_component(extrudeAyE) \
	$itk_component(extrudeAzE) \
	$itk_component(extrudeAUnitsL) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(extrudeBL) \
	$itk_component(extrudeBxE) \
	$itk_component(extrudeByE) \
	$itk_component(extrudeBzE) \
	$itk_component(extrudeBUnitsL) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(extrudeSL) $itk_component(extrudeSE) \
	-row $row \
	-sticky nsew
    grid $itk_component(extrudeSUnitsL) \
	-row $row \
	-column 4 \
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
    bind $itk_component(extrudeHxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(extrudeHyE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(extrudeHzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(extrudeVxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(extrudeVyE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(extrudeVzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(extrudeAxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(extrudeAyE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(extrudeAzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(extrudeBxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(extrudeByE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(extrudeBzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(extrudeSE) <Return> [::itcl::code $this updateGeometryIfMod]
}

::itcl::body ExtrudeEditFrame::buildLowerPanel {} {
    set parent [$this childsite lower]

    set alist [list H set Set HR move {Move End} H move {Move End}]

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

::itcl::body ExtrudeEditFrame::updateGeometryIfMod {} {
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
    set _S [bu_get_value_by_keyword S $gdata]

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
	$mBz == "-"} {
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
	$_S != $mS} {
	updateGeometry
    }
}

::itcl::body ExtrudeEditFrame::initEditState {} {
    set mEditPCommand [::itcl::code $this p]
    configure -valueUnits "mm"

    switch -- $mEditMode \
	$setH {
	    set mEditCommand pscale
	    set mEditClass $EDIT_CLASS_SCALE
	    set mEditParam1 h
	} \
	$moveHR {
	    set mEditCommand ptranslate
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditLastTransMode $::ArcherCore::OBJECT_TRANSLATE_MODE
	    set mEditParam1 hr
	} \
	$moveH {
	    set mEditCommand ptranslate
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditLastTransMode $::ArcherCore::OBJECT_TRANSLATE_MODE
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
