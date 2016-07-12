#                E P A E D I T F R A M E . T C L
# BRL-CAD
#
# Copyright (c) 2002-2016 United States Government as represented by
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
#    The class for editing epas within Archer.
#
##############################################################

::itcl::class EpaEditFrame {
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
	common setH 1
	common setA 2
	common setB 3

	variable mVx ""
	variable mVy ""
	variable mVz ""
	variable mHx ""
	variable mHy ""
	variable mHz ""
	variable mAx ""
	variable mAy ""
	variable mAz ""
	variable mR_1 ""
	variable mR_2 ""

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
	variable cmR_1 ""
	variable cmR_2 ""


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

::itcl::body EpaEditFrame::constructor {args} {
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
::itcl::body EpaEditFrame::initGeometry {gdata} {
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
    set mR_1 [bu_get_value_by_keyword r_1 $gdata]
    set mR_2 [bu_get_value_by_keyword r_2 $gdata]

    GeometryEditFrame::initGeometry $gdata
    set curr_name $itk_option(-geometryObject)
    if {$cmVx == "" || "$checkpointed_name" != "$curr_name"} {checkpointGeometry}

}

::itcl::body EpaEditFrame::updateGeometry {} {
    if {$itk_option(-mged) == "" ||
	$itk_option(-geometryObject) == ""} {
	return
    }

    $itk_option(-mged) adjust $itk_option(-geometryObject) \
	V [list $mVx $mVy $mVz] \
	H [list $mHx $mHy $mHz] \
	A [list $mAx $mAy $mAz] \
	r_1 $mR_1 \
	r_2 $mR_2

    GeometryEditFrame::updateGeometry
}

::itcl::body EpaEditFrame::checkpointGeometry {} {
    set checkpointed_name $itk_option(-geometryObject)
    set cmVx  $mVx
    set cmVy  $mVy
    set cmVz  $mVz
    set cmHx  $mHx
    set cmHy  $mHy
    set cmHz  $mHz
    set cmAx  $mAx
    set cmAy  $mAy
    set cmAz  $mAz
    set cmR_1 $mR_1
    set cmR_2 $mR_2
}

::itcl::body EpaEditFrame::revertGeometry {} {
    set mVx  $cmVx
    set mVy  $cmVy
    set mVz  $cmVz
    set mHx  $cmHx
    set mHy  $cmHy
    set mHz  $cmHz
    set mAx  $cmAx
    set mAy  $cmAy
    set mAz  $cmAz
    set mR_1 $cmR_1
    set mR_2 $cmR_2

    updateGeometry
}

::itcl::body EpaEditFrame::createGeometry {obj} {
    if {![GeometryEditFrame::createGeometry $obj]} {
	return
    }

    $itk_option(-mged) put $obj epa \
	V [list $mCenterX $mCenterY $mCenterZ] \
	H [list 0 0 $mDelta] \
	A {1 0 0} \
	r_1 $mDelta \
	r_2 $mDelta
}

::itcl::body EpaEditFrame::p {obj args} {
    if {[llength $args] != 1 || ![string is double $args]} {
	return "Usage: p sf"
    }

    switch -- $mEditMode \
	$setH {
	    $::ArcherCore::application p_pscale $obj h $args
	} \
	$setA {
	    $::ArcherCore::application p_pscale $obj a $args
	} \
	$setB {
	    $::ArcherCore::application p_pscale $obj b $args
	}

    return ""
}

# ------------------------------------------------------------
#                      PROTECTED METHODS
# ------------------------------------------------------------

::itcl::body EpaEditFrame::buildUpperPanel {} {
    set parent [$this childsite]
    itk_component add epaType {
	::ttk::label $parent.epatype \
	    -text "Epa:" \
	    -anchor e
    } {}
    itk_component add epaName {
	::ttk::label $parent.epaname \
	    -textvariable [::itcl::scope itk_option(-geometryObject)] \
	    -anchor w
    } {}

    # Create header labels
    itk_component add epaXL {
	::ttk::label $parent.epaXL \
	    -text "X"
    } {}
    itk_component add epaYL {
	::ttk::label $parent.epaYL \
	    -text "Y"
    } {}
    itk_component add epaZL {
	::ttk::label $parent.epaZL \
	    -text "Z"
    } {}

    # create widgets for vertices
    itk_component add epaVL {
	::ttk::label $parent.epaVL \
	    -text "V:" \
	    -anchor e
    } {}
    itk_component add epaVxE {
	::ttk::entry $parent.epaVxE \
	    -textvariable [::itcl::scope mVx] \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add epaVyE {
	::ttk::entry $parent.epaVyE \
	    -textvariable [::itcl::scope mVy] \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add epaVzE {
	::ttk::entry $parent.epaVzE \
	    -textvariable [::itcl::scope mVz] \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add epaVUnitsL {
	::ttk::label $parent.epaVUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add epaHL {
	::ttk::label $parent.epaHL \
	    -text "H:" \
	    -anchor e
    } {}
    itk_component add epaHxE {
	::ttk::entry $parent.epaHxE \
	    -textvariable [::itcl::scope mHx] \
	    -state disabled \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add epaHyE {
	::ttk::entry $parent.epaHyE \
	    -textvariable [::itcl::scope mHy] \
	    -state disabled \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add epaHzE {
	::ttk::entry $parent.epaHzE \
	    -textvariable [::itcl::scope mHz] \
	    -state disabled \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add epaHUnitsL {
	::ttk::label $parent.epaHUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add epaAL {
	::ttk::label $parent.epaAL \
	    -text "A:" \
	    -anchor e
    } {}
    itk_component add epaAxE {
	::ttk::entry $parent.epaAxE \
	    -textvariable [::itcl::scope mAx] \
	    -state disabled \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add epaAyE {
	::ttk::entry $parent.epaAyE \
	    -textvariable [::itcl::scope mAy] \
	    -state disabled \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add epaAzE {
	::ttk::entry $parent.epaAzE \
	    -textvariable [::itcl::scope mAz] \
	    -state disabled \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add epaAUnitsL {
	::ttk::label $parent.epaAUnitsL \
	    -anchor e
    } {}
    itk_component add epaR_1L {
	::ttk::label $parent.epaR_1L \
	    -text "r_1:" \
	    -anchor e
    } {}
    itk_component add epaR_1E {
	::ttk::entry $parent.epaR_1E \
	    -textvariable [::itcl::scope mR_1] \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add epaR_1UnitsL {
	::ttk::label $parent.epaR_1UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add epaR_2L {
	::ttk::label $parent.epaR_2L \
	    -text "r_2:" \
	    -anchor e
    } {}
    itk_component add epaR_2E {
	::ttk::entry $parent.epaR_2E \
	    -textvariable [::itcl::scope mR_2] \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add epaR_2UnitsL {
	::ttk::label $parent.epaR_2UnitsL \
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
    grid $itk_component(epaType) \
	-row $row \
	-column 0 \
	-sticky nsew
    grid $itk_component(epaName) \
	-row $row \
	-column 1 \
	-columnspan 3 \
	-sticky nsew
    incr row
    grid x $itk_component(epaXL) \
	$itk_component(epaYL) \
	$itk_component(epaZL)
    incr row
    grid $itk_component(epaVL) \
	$itk_component(epaVxE) \
	$itk_component(epaVyE) \
	$itk_component(epaVzE) \
	$itk_component(epaVUnitsL) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(epaHL) \
	$itk_component(epaHxE) \
	$itk_component(epaHyE) \
	$itk_component(epaHzE) \
	$itk_component(epaHUnitsL) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(epaAL) \
	$itk_component(epaAxE) \
	$itk_component(epaAyE) \
	$itk_component(epaAzE) \
	$itk_component(epaAUnitsL) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(epaR_1L) $itk_component(epaR_1E) \
	-row $row \
	-sticky nsew
    grid $itk_component(epaR_1UnitsL) \
	-row $row \
	-column 4 \
	-sticky nsew
    incr row
    grid $itk_component(epaR_2L) $itk_component(epaR_2E) \
	-row $row \
	-sticky nsew
    grid $itk_component(epaR_2UnitsL) \
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
    bind $itk_component(epaVxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(epaVyE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(epaVzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(epaHxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(epaHyE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(epaHzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(epaAxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(epaAyE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(epaAzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(epaR_1E) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(epaR_2E) <Return> [::itcl::code $this updateGeometryIfMod]
}

::itcl::body EpaEditFrame::buildLowerPanel {} {
    set parent [$this childsite lower]
    set row 0
    foreach attribute {H A B} {
	itk_component add set$attribute {
	    ::ttk::radiobutton $parent.set_$attribute \
		-variable [::itcl::scope mEditMode] \
		-value [subst $[subst set$attribute]] \
		-text "Set $attribute" \
		-command [::itcl::code $this initEditState]
	} {}

	grid $itk_component(set$attribute) -row $row -column 0 -sticky nsew
	incr row
    }
}

::itcl::body EpaEditFrame::updateGeometryIfMod {} {
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
    set _R_1 [bu_get_value_by_keyword r_1 $gdata]
    set _R_2 [bu_get_value_by_keyword r_2 $gdata]

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
	$mR_1 == ""  ||
	$mR_1 == "-" ||
	$mR_2 == ""  ||
	$mR_2 == "-"} {
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
	$_R_1 != $mR_1 ||
	$_R_2 != $mR_2} {
	updateGeometry
    }
}

::itcl::body EpaEditFrame::initEditState {} {
    set mEditCommand pscale
    set mEditPCommand [::itcl::code $this p]
    configure -valueUnits "mm"

    switch -- $mEditMode \
	$setH {
	    set mEditParam1 h
	    set mEditClass $EDIT_CLASS_SCALE
	} \
	$setA {
	    set mEditParam1 a
	    set mEditClass $EDIT_CLASS_SCALE
	} \
	$setB {
	    set mEditParam1 b
	    set mEditClass $EDIT_CLASS_SCALE
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
