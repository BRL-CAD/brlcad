#            E X T R U D E E D I T F R A M E . T C L
# BRL-CAD
#
# Copyright (c) 2002-2007 United States Government as represented by
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
	variable mBx ""
	variable mBy ""
	variable mBz ""
	variable mS ""
	variable mK ""

	# Override what's in GeometryEditFrame
	method updateGeometryIfMod {}
    }

    private {}
}


# ------------------------------------------------------------
#                      CONSTRUCTOR
# ------------------------------------------------------------

::itcl::body ExtrudeEditFrame::constructor {args} {
    set parent [$this childsite]
    itk_component add extrudeType {
	::label $parent.extrudetype \
	    -text "Extrude:" \
	    -anchor e
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }
    itk_component add extrudeName {
	::label $parent.extrudename \
	    -textvariable [::itcl::scope itk_option(-geometryObject)] \
	    -anchor w
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }

    # Create header labels
    itk_component add extrudeXL {
	::label $parent.extrudeXL \
	    -text "X"
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }
    itk_component add extrudeYL {
	::label $parent.extrudeYL \
	    -text "Y"
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }
    itk_component add extrudeZL {
	::label $parent.extrudeZL \
	    -text "Z"
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }

    # create widgets for vertices
    itk_component add extrudeVL {
	::label $parent.extrudeVL \
	    -text "V:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add extrudeVxE {
	::entry $parent.extrudeVxE \
	    -textvariable [::itcl::scope mVx] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add extrudeVyE {
	::entry $parent.extrudeVyE \
	    -textvariable [::itcl::scope mVy] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add extrudeVzE {
	::entry $parent.extrudeVzE \
	    -textvariable [::itcl::scope mVz] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add extrudeVUnitsL {
	::label $parent.extrudeVUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add extrudeHL {
	::label $parent.extrudeHL \
	    -text "H:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add extrudeHxE {
	::entry $parent.extrudeHxE \
	    -textvariable [::itcl::scope mHx] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add extrudeHyE {
	::entry $parent.extrudeHyE \
	    -textvariable [::itcl::scope mHy] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add extrudeHzE {
	::entry $parent.extrudeHzE \
	    -textvariable [::itcl::scope mHz] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add extrudeHUnitsL {
	::label $parent.extrudeHUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add extrudeAL {
	::label $parent.extrudeAL \
	    -text "A:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add extrudeAxE {
	::entry $parent.extrudeAxE \
	    -textvariable [::itcl::scope mAx] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add extrudeAyE {
	::entry $parent.extrudeAyE \
	    -textvariable [::itcl::scope mAy] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add extrudeAzE {
	::entry $parent.extrudeAzE \
	    -textvariable [::itcl::scope mAz] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add extrudeAUnitsL {
	::label $parent.extrudeAUnitsL \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add extrudeBL {
	::label $parent.extrudeBL \
	    -text "B:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add extrudeBxE {
	::entry $parent.extrudeBxE \
	    -textvariable [::itcl::scope mBx] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add extrudeByE {
	::entry $parent.extrudeByE \
	    -textvariable [::itcl::scope mBy] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add extrudeBzE {
	::entry $parent.extrudeBzE \
	    -textvariable [::itcl::scope mBz] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add extrudeBUnitsL {
	::label $parent.extrudeBUnitsL \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add extrudeSL {
	::label $parent.extrudeSL \
	    -text "S:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add extrudeSE {
	::entry $parent.extrudeSE \
	    -textvariable [::itcl::scope mS] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add extrudeSUnitsL {
	::label $parent.extrudeSUnitsL \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add extrudeKL {
	::label $parent.extrudeKL \
	    -text "K:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add extrudeKE {
	::entry $parent.extrudeKE \
	    -textvariable [::itcl::scope mK] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add extrudeKUnitsL {
	::label $parent.extrudeKUnitsL \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }

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
    grid $itk_component(extrudeKL) $itk_component(extrudeKE) \
	-row $row \
	-sticky nsew
    grid $itk_component(extrudeKUnitsL) \
	-row $row \
	-column 4 \
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
    bind $itk_component(extrudeKE) <Return> [::itcl::code $this updateGeometryIfMod]

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
    set mK [bu_get_value_by_keyword K $gdata]

    GeometryEditFrame::initGeometry $gdata
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
	S $mS \
	K $mK

    if {$itk_option(-geometryChangedCallback) != ""} {
	$itk_option(-geometryChangedCallback)
    }
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
	S "none" \
	K $mDelta
}


# ------------------------------------------------------------
#                      PROTECTED METHODS
# ------------------------------------------------------------

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
    set _K [bu_get_value_by_keyword K $gdata]

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
	$mK  == ""  ||
	$mK  == "-"} {
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
	$_S != $mS  ||
	$_K != $mK} {
	updateGeometry
    }
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
