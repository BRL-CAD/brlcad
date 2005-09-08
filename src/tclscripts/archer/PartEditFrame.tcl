##
# Portions Copyright (c) 2002 SURVICE Engineering Company. All Rights Reserved.
# This file contains Original Code and/or Modifications of Original Code as
# defined in and that are subject to the SURVICE Public Source License
# (Version 1.3, dated March 12, 2002).
#
# TYPE: tcltk
##############################################################
#
# PartEditFrame.tcl
#
##############################################################
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
    }

    protected {
	variable mVx ""
	variable mVy ""
	variable mVz ""
	variable mHx ""
	variable mHy ""
	variable mHz ""
	variable mR_v ""
	variable mR_h ""

	# Override what's in GeometryEditFrame
	method updateGeometryIfMod {}
    }

    private {}
}


# ------------------------------------------------------------
#                      CONSTRUCTOR
# ------------------------------------------------------------

::itcl::body PartEditFrame::constructor {args} {
    set parent [$this childsite]
    itk_component add partType {
	::label $parent.parttype \
	    -text "Part:" \
	    -anchor e
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }
    itk_component add partName {
	::label $parent.partname \
	    -textvariable [::itcl::scope itk_option(-geometryObject)] \
	    -anchor w
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }

    # Create header labels
    itk_component add partXL {
	::label $parent.partXL \
	    -text "X"
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }
    itk_component add partYL {
	::label $parent.partYL \
	    -text "Y"
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }
    itk_component add partZL {
	::label $parent.partZL \
	    -text "Z"
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }

    # create widgets for vertices
    itk_component add partVL {
	::label $parent.partVL \
	    -text "V:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add partVxE {
	::entry $parent.partVxE \
	    -textvariable [::itcl::scope mVx] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add partVyE {
	::entry $parent.partVyE \
	    -textvariable [::itcl::scope mVy] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add partVzE {
	::entry $parent.partVzE \
	    -textvariable [::itcl::scope mVz] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add partVUnitsL {
	::label $parent.partVUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add partHL {
	::label $parent.partHL \
	    -text "H:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add partHxE {
	::entry $parent.partHxE \
	    -textvariable [::itcl::scope mHx] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add partHyE {
	::entry $parent.partHyE \
	    -textvariable [::itcl::scope mHy] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add partHzE {
	::entry $parent.partHzE \
	    -textvariable [::itcl::scope mHz] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add partHUnitsL {
	::label $parent.partHUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add partR_vL {
	::label $parent.partR_vL \
	    -text "r_v:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add partR_vE {
	::entry $parent.partR_vE \
	    -textvariable [::itcl::scope mR_v] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add partR_vUnitsL {
	::label $parent.partR_vUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add partR_hL {
	::label $parent.partR_hL \
	    -text "r_h:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add partR_hE {
	::entry $parent.partR_hE \
	    -textvariable [::itcl::scope mR_h] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add partR_hUnitsL {
	::label $parent.partR_hUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }

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

    if {$itk_option(-geometryChangedCallback) != ""} {
	$itk_option(-geometryChangedCallback)
    }
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


# ------------------------------------------------------------
#                      PROTECTED METHODS
# ------------------------------------------------------------

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
