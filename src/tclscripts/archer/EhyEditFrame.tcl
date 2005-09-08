##
# Portions Copyright (c) 2002 SURVICE Engineering Company. All Rights Reserved.
# This file contains Original Code and/or Modifications of Original Code as
# defined in and that are subject to the SURVICE Public Source License
# (Version 1.3, dated March 12, 2002).
#
# TYPE: tcltk
##############################################################
#
# EhyEditFrame.tcl
#
##############################################################
#
# Author(s):
#    Bob Parker
#
# Description:
#    The class for editing ehys within Archer.
#
##############################################################

::itcl::class EhyEditFrame {
    inherit GeometryEditFrame

    constructor {args} {}
    destructor {}

    # Methods used by the constructor
    protected {
	# override methods in GeometryEditFrame
	method buildUpperPanel
	method buildLowerPanel
	method buildValuePanel
    }

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
	variable mAx ""
	variable mAy ""
	variable mAz ""
	variable mR_1 ""
	variable mR_2 ""
	variable mC ""

	# Override what's in GeometryEditFrame
	method updateGeometryIfMod {}
    }

    private {}
}


# ------------------------------------------------------------
#                      CONSTRUCTOR
# ------------------------------------------------------------

::itcl::body EhyEditFrame::constructor {args} {
    eval itk_initialize $args
}

::itcl::body EhyEditFrame::buildUpperPanel {} {
    set parent [$this childsite]
    itk_component add ehyType {
	::label $parent.ehytype \
	    -text "Ehy:" \
	    -anchor e
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }
    itk_component add ehyName {
	::label $parent.ehyname \
	    -textvariable [::itcl::scope itk_option(-geometryObject)] \
	    -anchor w
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }

    # Create header labels
    itk_component add ehyXL {
	::label $parent.ehyXL \
	    -text "X"
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }
    itk_component add ehyYL {
	::label $parent.ehyYL \
	    -text "Y"
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }
    itk_component add ehyZL {
	::label $parent.ehyZL \
	    -text "Z"
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }

    # create widgets for vertices
    itk_component add ehyVL {
	::label $parent.ehyVL \
	    -text "V:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add ehyVxE {
	::entry $parent.ehyVxE \
	    -textvariable [::itcl::scope mVx] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add ehyVyE {
	::entry $parent.ehyVyE \
	    -textvariable [::itcl::scope mVy] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add ehyVzE {
	::entry $parent.ehyVzE \
	    -textvariable [::itcl::scope mVz] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add ehyVUnitsL {
	::label $parent.ehyVUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add ehyHL {
	::label $parent.ehyHL \
	    -text "H:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add ehyHxE {
	::entry $parent.ehyHxE \
	    -textvariable [::itcl::scope mHx] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add ehyHyE {
	::entry $parent.ehyHyE \
	    -textvariable [::itcl::scope mHy] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add ehyHzE {
	::entry $parent.ehyHzE \
	    -textvariable [::itcl::scope mHz] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add ehyHUnitsL {
	::label $parent.ehyHUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add ehyAL {
	::label $parent.ehyAL \
	    -text "A:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add ehyAxE {
	::entry $parent.ehyAxE \
	    -textvariable [::itcl::scope mAx] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add ehyAyE {
	::entry $parent.ehyAyE \
	    -textvariable [::itcl::scope mAy] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add ehyAzE {
	::entry $parent.ehyAzE \
	    -textvariable [::itcl::scope mAz] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add ehyAUnitsL {
	::label $parent.ehyAUnitsL \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add ehyR_1L {
	::label $parent.ehyR_1L \
	    -text "r_1:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add ehyR_1E {
	::entry $parent.ehyR_1E \
	    -textvariable [::itcl::scope mR_1] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add ehyR_1UnitsL {
	::label $parent.ehyR_1UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add ehyR_2L {
	::label $parent.ehyR_2L \
	    -text "r_2:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add ehyR_2E {
	::entry $parent.ehyR_2E \
	    -textvariable [::itcl::scope mR_2] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add ehyR_2UnitsL {
	::label $parent.ehyR_2UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add ehyCL {
	::label $parent.ehyCL \
	    -text "c:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add ehyCE {
	::entry $parent.ehyCE \
	    -textvariable [::itcl::scope mC] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add ehyCUnitsL {
	::label $parent.ehyCUnitsL \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }

    set row 0
    grid $itk_component(ehyType) \
	-row $row \
	-column 0 \
	-sticky nsew
    grid $itk_component(ehyName) \
	-row $row \
	-column 1 \
	-columnspan 3 \
	-sticky nsew
    incr row
    grid x $itk_component(ehyXL) \
	$itk_component(ehyYL) \
	$itk_component(ehyZL)
    incr row
    grid $itk_component(ehyVL) \
	$itk_component(ehyVxE) \
	$itk_component(ehyVyE) \
	$itk_component(ehyVzE) \
	$itk_component(ehyVUnitsL) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(ehyHL) \
	$itk_component(ehyHxE) \
	$itk_component(ehyHyE) \
	$itk_component(ehyHzE) \
	$itk_component(ehyHUnitsL) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(ehyAL) \
	$itk_component(ehyAxE) \
	$itk_component(ehyAyE) \
	$itk_component(ehyAzE) \
	$itk_component(ehyAUnitsL) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(ehyR_1L) $itk_component(ehyR_1E) \
	-row $row \
	-sticky nsew
    grid $itk_component(ehyR_1UnitsL) \
	-row $row \
	-column 4 \
	-sticky nsew
    incr row
    grid $itk_component(ehyR_2L) $itk_component(ehyR_2E) \
	-row $row \
	-sticky nsew
    grid $itk_component(ehyR_2UnitsL) \
	-row $row \
	-column 4 \
	-sticky nsew
    incr row
    grid $itk_component(ehyCL) $itk_component(ehyCE) \
	-row $row \
	-sticky nsew
    grid $itk_component(ehyCUnitsL) \
	-row $row \
	-column 4 \
	-sticky nsew
    grid columnconfigure $parent 1 -weight 1
    grid columnconfigure $parent 2 -weight 1
    grid columnconfigure $parent 3 -weight 1
    pack $parent -expand yes -fill x -anchor n

    # Set up bindings
    bind $itk_component(ehyVxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(ehyVyE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(ehyVzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(ehyHxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(ehyHyE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(ehyHzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(ehyAxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(ehyAyE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(ehyAzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(ehyR_1E) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(ehyR_2E) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(ehyCE) <Return> [::itcl::code $this updateGeometryIfMod]
}

::itcl::body EhyEditFrame::buildLowerPanel {} {
}

::itcl::body EhyEditFrame::buildValuePanel {} {
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
::itcl::body EhyEditFrame::initGeometry {gdata} {
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
    set mR_2 [bu_get_value_by_keyword r_1 $gdata]
    set mC [bu_get_value_by_keyword c $gdata]

    GeometryEditFrame::initGeometry $gdata
}

::itcl::body EhyEditFrame::updateGeometry {} {
    if {$itk_option(-mged) == "" ||
	$itk_option(-geometryObject) == ""} {
	return
    }

    $itk_option(-mged) adjust $itk_option(-geometryObject) \
	V [list $mVx $mVy $mVz] \
	H [list $mHx $mHy $mHz] \
	A [list $mAx $mAy $mAz] \
	r_1 $mR_1 \
	r_2 $mR_2 \
	c $mC

    if {$itk_option(-geometryChangedCallback) != ""} {
	$itk_option(-geometryChangedCallback)
    }
}

::itcl::body EhyEditFrame::createGeometry {obj} {
    if {![GeometryEditFrame::createGeometry $obj]} {
	return
    }

    $itk_option(-mged) put $obj ehy \
	V [list $mCenterX $mCenterY $mCenterZ] \
	H [list 0 0 $mDelta] \
	A {1 0 0} \
	r_1 $mDelta \
	r_2 $mDelta \
	c $mDelta
}


# ------------------------------------------------------------
#                      PROTECTED METHODS
# ------------------------------------------------------------

::itcl::body EhyEditFrame::updateGeometryIfMod {} {
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
	$mAx == ""  ||
	$mAx == "-" ||
	$mAy == ""  ||
	$mAy == "-" ||
	$mAz == ""  ||
	$mAz == "-" ||
	$mR_1 == ""  ||
	$mR_1 == "-" ||
	$mR_2 == ""  ||
	$mR_2 == "-" ||
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
	$_R_1 != $mR_1 ||
	$_R_2 != $mR_2 ||
	$_C != $mC} {
	updateGeometry
    }
}
