#                E L L E D I T F R A M E . T C L
# BRL-CAD
#
# Copyright (c) 2002-2007 United States Government as represented by
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
# Author:
#    Bob Parker
#
# Description:
#    The class for editing ells within Archer.
#

::itcl::class EllEditFrame {
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
	variable mAx ""
	variable mAy ""
	variable mAz ""
	variable mBx ""
	variable mBy ""
	variable mBz ""
	variable mCx ""
	variable mCy ""
	variable mCz ""

	# Override what's in GeometryEditFrame
	method updateGeometryIfMod {}
    }

    private {}
}


# ------------------------------------------------------------
#                      CONSTRUCTOR
# ------------------------------------------------------------

::itcl::body EllEditFrame::constructor {args} {
    eval itk_initialize $args
}

::itcl::body EllEditFrame::buildUpperPanel {} {
    set parent [$this childsite]
    itk_component add ellType {
	::label $parent.elltype \
	    -text "Ellipsoid:" \
	    -anchor e
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }
    itk_component add ellName {
	::label $parent.ellname \
	    -textvariable [::itcl::scope itk_option(-geometryObject)] \
	    -anchor w
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }

    # Create header labels
    itk_component add ellXL {
	::label $parent.ellXL \
	    -text "X"
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }
    itk_component add ellYL {
	::label $parent.ellYL \
	    -text "Y"
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }
    itk_component add ellZL {
	::label $parent.ellZL \
	    -text "Z"
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }

    # create widgets for vertices and vectors
    itk_component add ellVL {
	::label $parent.ellVL \
	    -text "V:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add ellVxE {
	::entry $parent.ellVxE \
	    -textvariable [::itcl::scope mVx] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add ellVyE {
	::entry $parent.ellVyE \
	    -textvariable [::itcl::scope mVy] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add ellVzE {
	::entry $parent.ellVzE \
	    -textvariable [::itcl::scope mVz] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add ellVUnitsL {
	::label $parent.ellVUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add ellAL {
	::label $parent.ellAL \
	    -text "A:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add ellAxE {
	::entry $parent.ellAxE \
	    -textvariable [::itcl::scope mAx] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add ellAyE {
	::entry $parent.ellAyE \
	    -textvariable [::itcl::scope mAy] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add ellAzE {
	::entry $parent.ellAzE \
	    -textvariable [::itcl::scope mAz] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add ellAUnitsL {
	::label $parent.ellAUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add ellBL {
	::label $parent.ellBL \
	    -text "B:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add ellBxE {
	::entry $parent.ellBxE \
	    -textvariable [::itcl::scope mBx] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add ellByE {
	::entry $parent.ellByE \
	    -textvariable [::itcl::scope mBy] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add ellBzE {
	::entry $parent.ellBzE \
	    -textvariable [::itcl::scope mBz] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add ellBUnitsL {
	::label $parent.ellBUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add ellCL {
	::label $parent.ellCL \
	    -text "C:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add ellCxE {
	::entry $parent.ellCxE \
	    -textvariable [::itcl::scope mCx] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add ellCyE {
	::entry $parent.ellCyE \
	    -textvariable [::itcl::scope mCy] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add ellCzE {
	::entry $parent.ellCzE \
	    -textvariable [::itcl::scope mCz] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add ellCUnitsL {
	::label $parent.ellCUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }

    set row 0
    grid $itk_component(ellType) \
	-row $row \
	-column 0 \
	-sticky nsew
    grid $itk_component(ellName) \
	-row $row \
	-column 1 \
	-columnspan 3 \
	-sticky nsew
    incr row
    grid x $itk_component(ellXL) \
	$itk_component(ellYL) \
	$itk_component(ellZL)
    incr row
    grid $itk_component(ellVL) \
	$itk_component(ellVxE) \
	$itk_component(ellVyE) \
	$itk_component(ellVzE) \
	$itk_component(ellVUnitsL) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(ellAL) \
	$itk_component(ellAxE) \
	$itk_component(ellAyE) \
	$itk_component(ellAzE) \
	$itk_component(ellAUnitsL) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(ellBL) \
	$itk_component(ellBxE) \
	$itk_component(ellByE) \
	$itk_component(ellBzE) \
	$itk_component(ellBUnitsL) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(ellCL) \
	$itk_component(ellCxE) \
	$itk_component(ellCyE) \
	$itk_component(ellCzE) \
	$itk_component(ellCUnitsL) \
	-row $row \
	-sticky nsew
    grid columnconfigure $parent 1 -weight 1
    grid columnconfigure $parent 2 -weight 1
    grid columnconfigure $parent 3 -weight 1
    pack $parent -expand yes -fill x -anchor n

    # Set up bindings
    bind $itk_component(ellVxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(ellVyE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(ellVzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(ellAxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(ellAyE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(ellAzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(ellBxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(ellByE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(ellBzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(ellCxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(ellCyE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(ellCzE) <Return> [::itcl::code $this updateGeometryIfMod]
}

::itcl::body EllEditFrame::buildLowerPanel {} {
}

::itcl::body EllEditFrame::buildValuePanel {} {
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
::itcl::body EllEditFrame::initGeometry {gdata} {
    set _V [bu_get_value_by_keyword V $gdata]
    set mVx [lindex $_V 0]
    set mVy [lindex $_V 1]
    set mVz [lindex $_V 2]
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

    GeometryEditFrame::initGeometry $gdata
}

::itcl::body EllEditFrame::updateGeometry {} {
    if {$itk_option(-mged) == "" ||
	$itk_option(-geometryObject) == ""} {
	return
    }

    $itk_option(-mged) adjust $itk_option(-geometryObject) \
	V [list $mVx $mVy $mVz] \
	A [list $mAx $mAy $mAz] \
	B [list $mBx $mBy $mBz] \
	C [list $mCx $mCy $mCz]

    if {$itk_option(-geometryChangedCallback) != ""} {
	$itk_option(-geometryChangedCallback)
    }
}

::itcl::body EllEditFrame::createGeometry {obj} {
    if {![GeometryEditFrame::createGeometry $obj]} {
	return
    }

    $itk_option(-mged) put $obj ell \
	V [list $mCenterX $mCenterY $mCenterZ] \
	A [list $mDelta 0 0] \
	B [list 0 $mDelta 0] \
	C [list 0 0 $mDelta]
}


# ------------------------------------------------------------
#                      PROTECTED METHODS
# ------------------------------------------------------------

::itcl::body EllEditFrame::updateGeometryIfMod {} {
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

    if {$mVx == ""  ||
	$mVx == "-" ||
	$mVy == ""  ||
	$mVy == "-" ||
	$mVz == ""  ||
	$mVz == "-" ||
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
	$mCz == "-"} {
	# Not valid
	return
    }

    if {$_Vx != $mVx ||
	$_Vy != $mVy ||
	$_Vz != $mVz ||
	$_Ax != $mAx ||
	$_Ay != $mAy ||
	$_Az != $mAz ||
	$_Bx != $mBx ||
	$_By != $mBy ||
	$_Bz != $mBz ||
	$_Cx != $mCx ||
	$_Cy != $mCy ||
	$_Cz != $mCz} {
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
