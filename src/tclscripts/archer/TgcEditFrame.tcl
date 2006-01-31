#                T G C E D I T F R A M E . T C L
# BRL-CAD
#
# Copyright (c) 2002-2006 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
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
	variable mBx ""
	variable mBy ""
	variable mBz ""
	variable mCx ""
	variable mCy ""
	variable mCz ""
	variable mDx ""
	variable mDy ""
	variable mDz ""

	# Override what's in GeometryEditFrame
	method updateGeometryIfMod {}
    }

    private {}
}


# ------------------------------------------------------------
#                      CONSTRUCTOR
# ------------------------------------------------------------

::itcl::body TgcEditFrame::constructor {args} {
    eval itk_initialize $args
}

::itcl::body TgcEditFrame::buildUpperPanel {} {
    set parent [$this childsite]
    itk_component add tgcType {
	::label $parent.tgctype \
	    -text "Tgc:" \
	    -anchor e
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }
    itk_component add tgcName {
	::label $parent.tgcname \
	    -textvariable [::itcl::scope itk_option(-geometryObject)] \
	    -anchor w
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }

    # Create header labels
    itk_component add tgcXL {
	::label $parent.tgcXL \
	    -text "X"
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }
    itk_component add tgcYL {
	::label $parent.tgcYL \
	    -text "Y"
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }
    itk_component add tgcZL {
	::label $parent.tgcZL \
	    -text "Z"
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }

    # create widgets for vertices
    itk_component add tgcVL {
	::label $parent.tgcVL \
	    -text "V:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add tgcVxE {
	::entry $parent.tgcVxE \
	    -textvariable [::itcl::scope mVx] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add tgcVyE {
	::entry $parent.tgcVyE \
	    -textvariable [::itcl::scope mVy] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add tgcVzE {
	::entry $parent.tgcVzE \
	    -textvariable [::itcl::scope mVz] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add tgcVUnitsL {
	::label $parent.tgcVUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add tgcHL {
	::label $parent.tgcHL \
	    -text "H:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add tgcHxE {
	::entry $parent.tgcHxE \
	    -textvariable [::itcl::scope mHx] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add tgcHyE {
	::entry $parent.tgcHyE \
	    -textvariable [::itcl::scope mHy] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add tgcHzE {
	::entry $parent.tgcHzE \
	    -textvariable [::itcl::scope mHz] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add tgcHUnitsL {
	::label $parent.tgcHUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add tgcAL {
	::label $parent.tgcAL \
	    -text "A:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add tgcAxE {
	::entry $parent.tgcAxE \
	    -textvariable [::itcl::scope mAx] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add tgcAyE {
	::entry $parent.tgcAyE \
	    -textvariable [::itcl::scope mAy] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add tgcAzE {
	::entry $parent.tgcAzE \
	    -textvariable [::itcl::scope mAz] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add tgcAUnitsL {
	::label $parent.tgcAUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add tgcBL {
	::label $parent.tgcBL \
	    -text "B:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add tgcBxE {
	::entry $parent.tgcBxE \
	    -textvariable [::itcl::scope mBx] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add tgcByE {
	::entry $parent.tgcByE \
	    -textvariable [::itcl::scope mBy] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add tgcBzE {
	::entry $parent.tgcBzE \
	    -textvariable [::itcl::scope mBz] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add tgcBUnitsL {
	::label $parent.tgcBUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add tgcCL {
	::label $parent.tgcCL \
	    -text "C:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add tgcCxE {
	::entry $parent.tgcCxE \
	    -textvariable [::itcl::scope mCx] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add tgcCyE {
	::entry $parent.tgcCyE \
	    -textvariable [::itcl::scope mCy] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add tgcCzE {
	::entry $parent.tgcCzE \
	    -textvariable [::itcl::scope mCz] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add tgcCUnitsL {
	::label $parent.tgcCUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add tgcDL {
	::label $parent.tgcDL \
	    -text "D:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add tgcDxE {
	::entry $parent.tgcDxE \
	    -textvariable [::itcl::scope mDx] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add tgcDyE {
	::entry $parent.tgcDyE \
	    -textvariable [::itcl::scope mDy] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add tgcDzE {
	::entry $parent.tgcDzE \
	    -textvariable [::itcl::scope mDz] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add tgcDUnitsL {
	::label $parent.tgcDUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }

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
}

::itcl::body TgcEditFrame::buildValuePanel {} {
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

    if {$itk_option(-geometryChangedCallback) != ""} {
	$itk_option(-geometryChangedCallback)
    }
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
	C {0.0 0.0 0.0} \
	D {0.0 0.0 0.0}
}


# ------------------------------------------------------------
#                      PROTECTED METHODS
# ------------------------------------------------------------

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

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
