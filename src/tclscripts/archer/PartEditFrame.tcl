#               P A R T E D I T F R A M E . T C L
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
#    The class for editing particles within Archer.
#
##############################################################

::itcl::class PartEditFrame {
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
	common setH 1
	common setr_v 2
	common setr_h 3

	variable mVx ""
	variable mVy ""
	variable mVz ""
	variable mHx ""
	variable mHy ""
	variable mHz ""
	variable mR_v ""
	variable mR_h ""

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

::itcl::body PartEditFrame::constructor {args} {
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
::itcl::body PartEditFrame::initGeometry {gdata} {
    set _V [bu_get_value_by_keyword V $gdata]
    set mVx [lindex $_V 0]
    set mVy [lindex $_V 1]
    set mVz [lindex $_V 2]
    set _H [bu_get_value_by_keyword H $gdata]
    set mHx [lindex $_H 0]
    set mHy [lindex $_H 1]
    set mHz [lindex $_H 2]
    set mR_v [bu_get_value_by_keyword r_v $gdata]
    set mR_h [bu_get_value_by_keyword r_h $gdata]

    GeometryEditFrame::initGeometry $gdata
}

::itcl::body PartEditFrame::updateGeometry {} {
    if {$itk_option(-mged) == "" ||
	$itk_option(-geometryObject) == ""} {
	return
    }

    $itk_option(-mged) adjust $itk_option(-geometryObject) \
	V [list $mVx $mVy $mVz] \
	H [list $mHx $mHy $mHz] \
	r_v $mR_v \
	r_h $mR_h

    GeometryEditFrame::updateGeometry
}

::itcl::body PartEditFrame::createGeometry {obj} {
    if {![GeometryEditFrame::createGeometry $obj]} {
	return
    }

    set r1 [expr {$mDelta * 0.5}]
    set r2 [expr {$mDelta * 0.25}]

    $itk_option(-mged) put $obj part \
	V [list $mCenterX $mCenterY $mCenterZ] \
	H [list $mDelta 0 0] \
	r_v $r1 \
	r_h $r2
}

::itcl::body PartEditFrame::p {obj args} {
    if {[llength $args] != 1 || ![string is double $args]} {
	return "Usage: p sf"
    }

    switch -- $mEditMode \
	$setH {
	    $::ArcherCore::application p_pscale $obj H $args
	} \
	$setr_v {
	    $::ArcherCore::application p_pscale $obj v $args
	} \
	$setr_h {
	    $::ArcherCore::application p_pscale $obj h $args
	}

    return ""
}

# ------------------------------------------------------------
#                      PROTECTED METHODS
# ------------------------------------------------------------

::itcl::body PartEditFrame::buildUpperPanel {} {
    set parent [$this childsite]
    itk_component add partType {
	::ttk::label $parent.parttype \
	    -text "Part:" \
	    -anchor e
    } {}
    itk_component add partName {
	::ttk::label $parent.partname \
	    -textvariable [::itcl::scope itk_option(-geometryObject)] \
	    -anchor w
    } {}

    # Create header labels
    itk_component add partXL {
	::ttk::label $parent.partXL \
	    -text "X"
    } {}
    itk_component add partYL {
	::ttk::label $parent.partYL \
	    -text "Y"
    } {}
    itk_component add partZL {
	::ttk::label $parent.partZL \
	    -text "Z"
    } {}

    # create widgets for vertices
    itk_component add partVL {
	::ttk::label $parent.partVL \
	    -text "V:" \
	    -anchor e
    } {}
    itk_component add partVxE {
	::ttk::entry $parent.partVxE \
	    -textvariable [::itcl::scope mVx] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add partVyE {
	::ttk::entry $parent.partVyE \
	    -textvariable [::itcl::scope mVy] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add partVzE {
	::ttk::entry $parent.partVzE \
	    -textvariable [::itcl::scope mVz] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add partVUnitsL {
	::ttk::label $parent.partVUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add partHL {
	::ttk::label $parent.partHL \
	    -text "H:" \
	    -anchor e
    } {}
    itk_component add partHxE {
	::ttk::entry $parent.partHxE \
	    -textvariable [::itcl::scope mHx] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add partHyE {
	::ttk::entry $parent.partHyE \
	    -textvariable [::itcl::scope mHy] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add partHzE {
	::ttk::entry $parent.partHzE \
	    -textvariable [::itcl::scope mHz] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add partHUnitsL {
	::ttk::label $parent.partHUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add partR_vL {
	::ttk::label $parent.partR_vL \
	    -text "r_v:" \
	    -anchor e
    } {}
    itk_component add partR_vE {
	::ttk::entry $parent.partR_vE \
	    -textvariable [::itcl::scope mR_v] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add partR_vUnitsL {
	::ttk::label $parent.partR_vUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add partR_hL {
	::ttk::label $parent.partR_hL \
	    -text "r_h:" \
	    -anchor e
    } {}
    itk_component add partR_hE {
	::ttk::entry $parent.partR_hE \
	    -textvariable [::itcl::scope mR_h] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add partR_hUnitsL {
	::ttk::label $parent.partR_hUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}

    set row 0
    grid $itk_component(partType) \
	-row $row \
	-column 0 \
	-sticky nsew
    grid $itk_component(partName) \
	-row $row \
	-column 1 \
	-columnspan 3 \
	-sticky nsew
    incr row
    grid x $itk_component(partXL) \
	$itk_component(partYL) \
	$itk_component(partZL)
    incr row
    grid $itk_component(partVL) \
	$itk_component(partVxE) \
	$itk_component(partVyE) \
	$itk_component(partVzE) \
	$itk_component(partVUnitsL) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(partHL) \
	$itk_component(partHxE) \
	$itk_component(partHyE) \
	$itk_component(partHzE) \
	$itk_component(partHUnitsL) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(partR_vL) $itk_component(partR_vE) \
	-row $row \
	-sticky nsew
    grid $itk_component(partR_vUnitsL) \
	-row $row \
	-column 4 \
	-sticky nsew
    incr row
    grid $itk_component(partR_hL) $itk_component(partR_hE) \
	-row $row \
	-sticky nsew
    grid $itk_component(partR_hUnitsL) \
	-row $row \
	-column 4 \
	-sticky nsew
    grid columnconfigure $parent 1 -weight 1
    grid columnconfigure $parent 2 -weight 1
    grid columnconfigure $parent 3 -weight 1
    pack $parent -expand yes -fill x -anchor n

    # Set up bindings
    bind $itk_component(partVxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(partVyE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(partVzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(partHxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(partHyE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(partHzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(partR_vE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(partR_hE) <Return> [::itcl::code $this updateGeometryIfMod]
}

::itcl::body PartEditFrame::buildLowerPanel {} {
    set parent [$this childsite lower]

    foreach attribute {H r_v r_h} {
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

::itcl::body PartEditFrame::updateGeometryIfMod {} {
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
    set _R_v [bu_get_value_by_keyword r_v $gdata]
    set _R_h [bu_get_value_by_keyword r_h $gdata]

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
	$mR_v == ""  ||
	$mR_v == "-" ||
	$mR_h == ""  ||
	$mR_h == "-"} {
	# Not valid
	return
    }

    if {$_Vx != $mVx ||
	$_Vy != $mVy ||
	$_Vz != $mVz ||
	$_Hx != $mHx ||
	$_Hy != $mHy ||
	$_Hz != $mHz ||
	$_R_v != $mR_v ||
	$_R_h != $mR_h} {
	updateGeometry
    }
}

::itcl::body PartEditFrame::initEditState {} {
    set mEditCommand pscale
    set mEditClass $EDIT_CLASS_SCALE
    set mEditPCommand [::itcl::code $this p]
    configure -valueUnits "mm"

    switch -- $mEditMode \
	$setH {
	    set mEditParam1 H
	} \
	$setr_v {
	    set mEditParam1 v
	} \
	$setr_h {
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
