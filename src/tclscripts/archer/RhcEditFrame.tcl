#                R H C E D I T F R A M E . T C L
# BRL-CAD
#
# Copyright (c) 2002-2020 United States Government as represented by
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
#    The class for editing rhcs within Archer.
#
##############################################################

::itcl::class RhcEditFrame {
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
	common setB 1
	common setH 2
	common setr 3
	common setc 4

	variable mVx ""
	variable mVy ""
	variable mVz ""
	variable mHx ""
	variable mHy ""
	variable mHz ""
	variable mBx ""
	variable mBy ""
	variable mBz ""
	variable mR ""
	variable mC ""

	# Checkpoint values
	variable checkpointed_name ""
	variable cmVx ""
	variable cmVy ""
	variable cmVz ""
	variable cmHx ""
	variable cmHy ""
	variable cmHz ""
	variable cmBx ""
	variable cmBy ""
	variable cmBz ""
	variable cmR ""
	variable cmC ""

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

::itcl::body RhcEditFrame::constructor {args} {
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
::itcl::body RhcEditFrame::initGeometry {gdata} {
    set _V [bu_get_value_by_keyword V $gdata]
    set mVx [lindex $_V 0]
    set mVy [lindex $_V 1]
    set mVz [lindex $_V 2]
    set _H [bu_get_value_by_keyword H $gdata]
    set mHx [lindex $_H 0]
    set mHy [lindex $_H 1]
    set mHz [lindex $_H 2]
    set _B [bu_get_value_by_keyword B $gdata]
    set mBx [lindex $_B 0]
    set mBy [lindex $_B 1]
    set mBz [lindex $_B 2]
    set mR [bu_get_value_by_keyword r $gdata]
    set mC [bu_get_value_by_keyword c $gdata]

    GeometryEditFrame::initGeometry $gdata
    set curr_name $itk_option(-geometryObject)
    if {$cmVx == "" || "$checkpointed_name" != "$curr_name"} {checkpointGeometry}
}

::itcl::body RhcEditFrame::updateGeometry {} {
    if {$itk_option(-mged) == "" ||
	$itk_option(-geometryObject) == ""} {
	return
    }

    $itk_option(-mged) adjust $itk_option(-geometryObject) \
	V [list $mVx $mVy $mVz] \
	H [list $mHx $mHy $mHz] \
	B [list $mBx $mBy $mBz] \
	r $mR \
	c $mC

    GeometryEditFrame::updateGeometry
}

::itcl::body RhcEditFrame::checkpointGeometry {} {
    set checkpointed_name $itk_option(-geometryObject)
    set cmVx $mVx
    set cmVy $mVy
    set cmVz $mVz
    set cmHx $mHx
    set cmHy $mHy
    set cmHz $mHz
    set cmBx $mBx
    set cmBy $mBy
    set cmBz $mBz
    set cmR  $mR
    set cmC  $mC
}

::itcl::body RhcEditFrame::revertGeometry {} {
    set mVx $cmVx
    set mVy $cmVy
    set mVz $cmVz
    set mHx $cmHx
    set mHy $cmHy
    set mHz $cmHz
    set mBx $cmBx
    set mBy $cmBy
    set mBz $cmBz
    set mR  $cmR
    set mC  $cmC

    updateGeometry
}

::itcl::body RhcEditFrame::createGeometry {obj} {
    if {![GeometryEditFrame::createGeometry $obj]} {
	return
    }

    $itk_option(-mged) put $obj rhc \
	V [list $mCenterX $mCenterY $mCenterZ] \
	H [list 0 0 $mDelta] \
	B [list $mDelta 0 0] \
	r $mDelta \
	c $mDelta
}

::itcl::body RhcEditFrame::p {obj args} {
    if {[llength $args] != 1 || ![string is double $args]} {
	return "Usage: p sf"
    }

    switch -- $mEditMode \
	$setB {
	    $::ArcherCore::application p_pscale $obj b $args
	} \
	$setH {
	    $::ArcherCore::application p_pscale $obj h $args
	} \
	$setr {
	    $::ArcherCore::application p_pscale $obj r $args
	} \
	$setc {
	    $::ArcherCore::application p_pscale $obj c $args
	}

    return ""
}


# ------------------------------------------------------------
#                      PROTECTED METHODS
# ------------------------------------------------------------

::itcl::body RhcEditFrame::buildUpperPanel {} {
    set parent [$this childsite]
    itk_component add rhcType {
	::ttk::label $parent.rhctype \
	    -text "Rhc:" \
	    -anchor e
    } {}
    itk_component add rhcName {
	::ttk::label $parent.rhcname \
	    -textvariable [::itcl::scope itk_option(-geometryObject)] \
	    -anchor w
    } {}

    # Create header labels
    itk_component add rhcXL {
	::ttk::label $parent.rhcXL \
	    -text "X"
    } {}
    itk_component add rhcYL {
	::ttk::label $parent.rhcYL \
	    -text "Y"
    } {}
    itk_component add rhcZL {
	::ttk::label $parent.rhcZL \
	    -text "Z"
    } {}

    # create widgets for vertices
    itk_component add rhcVL {
	::ttk::label $parent.rhcVL \
	    -text "V:" \
	    -anchor e
    } {}
    itk_component add rhcVxE {
	::ttk::entry $parent.rhcVxE \
	    -textvariable [::itcl::scope mVx] \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add rhcVyE {
	::ttk::entry $parent.rhcVyE \
	    -textvariable [::itcl::scope mVy] \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add rhcVzE {
	::ttk::entry $parent.rhcVzE \
	    -textvariable [::itcl::scope mVz] \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add rhcVUnitsL {
	::ttk::label $parent.rhcVUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add rhcHL {
	::ttk::label $parent.rhcHL \
	    -text "H:" \
	    -anchor e
    } {}
    itk_component add rhcHxE {
	::ttk::entry $parent.rhcHxE \
	    -textvariable [::itcl::scope mHx] \
	    -state disabled \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add rhcHyE {
	::ttk::entry $parent.rhcHyE \
	    -textvariable [::itcl::scope mHy] \
	    -state disabled \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add rhcHzE {
	::ttk::entry $parent.rhcHzE \
	    -textvariable [::itcl::scope mHz] \
	    -state disabled \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add rhcHUnitsL {
	::ttk::label $parent.rhcHUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add rhcBL {
	::ttk::label $parent.rhcBL \
	    -text "B:" \
	    -anchor e
    } {}
    itk_component add rhcBxE {
	::ttk::entry $parent.rhcBxE \
	    -textvariable [::itcl::scope mBx] \
	    -state disabled \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add rhcByE {
	::ttk::entry $parent.rhcByE \
	    -textvariable [::itcl::scope mBy] \
	    -state disabled \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add rhcBzE {
	::ttk::entry $parent.rhcBzE \
	    -textvariable [::itcl::scope mBz] \
	    -state disabled \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add rhcBUnitsL {
	::ttk::label $parent.rhcBUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add rhcRL {
	::ttk::label $parent.rhcRL \
	    -text "r:" \
	    -anchor e
    } {}
    itk_component add rhcRE {
	::ttk::entry $parent.rhcRE \
	    -textvariable [::itcl::scope mR] \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add rhcRUnitsL {
	::ttk::label $parent.rhcRUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add rhcCL {
	::ttk::label $parent.rhcCL \
	    -text "c:" \
	    -anchor e
    } {}
    itk_component add rhcCE {
	::ttk::entry $parent.rhcCE \
	    -textvariable [::itcl::scope mC] \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add rhcCUnitsL {
	::ttk::label $parent.rhcCUnitsL \
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
    grid $itk_component(rhcType) \
	-row $row \
	-column 0 \
	-sticky nsew
    grid $itk_component(rhcName) \
	-row $row \
	-column 1 \
	-columnspan 3 \
	-sticky nsew
    incr row
    grid x $itk_component(rhcXL) \
	$itk_component(rhcYL) \
	$itk_component(rhcZL)
    incr row
    grid $itk_component(rhcVL) \
	$itk_component(rhcVxE) \
	$itk_component(rhcVyE) \
	$itk_component(rhcVzE) \
	$itk_component(rhcVUnitsL) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(rhcHL) \
	$itk_component(rhcHxE) \
	$itk_component(rhcHyE) \
	$itk_component(rhcHzE) \
	$itk_component(rhcHUnitsL) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(rhcBL) \
	$itk_component(rhcBxE) \
	$itk_component(rhcByE) \
	$itk_component(rhcBzE) \
	$itk_component(rhcBUnitsL) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(rhcRL) $itk_component(rhcRE) \
	-row $row \
	-sticky nsew
    grid $itk_component(rhcRUnitsL) \
	-row $row \
	-column 4 \
	-sticky nsew
    incr row
    grid $itk_component(rhcCL) $itk_component(rhcCE) \
	-row $row \
	-sticky nsew
    grid $itk_component(rhcCUnitsL) \
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
    bind $itk_component(rhcVxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(rhcVyE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(rhcVzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(rhcHxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(rhcHyE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(rhcHzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(rhcBxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(rhcByE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(rhcBzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(rhcRE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(rhcCE) <Return> [::itcl::code $this updateGeometryIfMod]
}

::itcl::body RhcEditFrame::buildLowerPanel {} {
    set parent [$this childsite lower]
    set row 0
    foreach attribute {B H r c} {
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

::itcl::body RhcEditFrame::updateGeometryIfMod {} {
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
    set _B [bu_get_value_by_keyword B $gdata]
    set _Bx [lindex $_B 0]
    set _By [lindex $_B 1]
    set _Bz [lindex $_B 2]
    set _R [bu_get_value_by_keyword r $gdata]
    set _C [bu_get_value_by_keyword c $gdata]

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
	$mBx == ""  ||
	$mBx == "-" ||
	$mBy == ""  ||
	$mBy == "-" ||
	$mBz == ""  ||
	$mBz == "-" ||
	$mR == ""   ||
	$mR == "-"  ||
	$mC == ""   ||
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
	$_Bx != $mBx ||
	$_By != $mBy ||
	$_Bz != $mBz ||
	$_R != $mR   ||
	$_C != $mC} {
	updateGeometry
    }
}

::itcl::body RhcEditFrame::initEditState {} {
    set mEditCommand pscale
    set mEditPCommand [::itcl::code $this p]
    configure -valueUnits "mm"

    switch -- $mEditMode \
	$setB {
	    set mEditParam1 b
	    set mEditClass $EDIT_CLASS_SCALE
	} \
	$setH {
	    set mEditParam1 h
	    set mEditClass $EDIT_CLASS_SCALE
	} \
	$setr {
	    set mEditParam1 r
	    set mEditClass $EDIT_CLASS_SCALE
	} \
	$setc {
	    set mEditParam1 c
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
