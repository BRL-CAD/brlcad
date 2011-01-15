#                S U P E R E L L E D I T F R A M E . T C L
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
# Author:
#    Bob Parker
#
# Description:
#    The class for editing superells within Archer.
#

::itcl::class SuperellEditFrame {
    inherit EllEditFrame

    constructor {args} {}
    destructor {}

    public {
	# Override what's in EllEditFrame
	method initGeometry {gdata}
	method updateGeometry {}
	method createGeometry {obj}
    }

    protected {
	common setN 5
	common setE 6

	variable mN ""
	variable mE ""

	# Methods used by the constructor.
	# Override methods in EllEditFrame.
	method buildUpperPanel {}
    }

    private {
    }
}


# ------------------------------------------------------------
#                      CONSTRUCTOR
# ------------------------------------------------------------

::itcl::body SuperellEditFrame::constructor {args} {
    eval itk_initialize $args

    $itk_component(ellType) configure -text "Superell"
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
::itcl::body SuperellEditFrame::initGeometry {gdata} {
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
    set mN [bu_get_value_by_keyword n $gdata]
    set mE [bu_get_value_by_keyword e $gdata]

    GeometryEditFrame::initGeometry $gdata
}

::itcl::body SuperellEditFrame::updateGeometry {} {
    if {$itk_option(-mged) == "" ||
	$itk_option(-geometryObject) == ""} {
	return
    }

    $itk_option(-mged) adjust $itk_option(-geometryObject) \
	V [list $mVx $mVy $mVz] \
	A [list $mAx $mAy $mAz] \
	B [list $mBx $mBy $mBz] \
	C [list $mCx $mCy $mCz] \
	n $mN \
	e $mE

    if {$itk_option(-geometryChangedCallback) != ""} {
	$itk_option(-geometryChangedCallback)
    }
}

::itcl::body SuperellEditFrame::createGeometry {obj} {
    if {![GeometryEditFrame::createGeometry $obj]} {
	return
    }

    $itk_option(-mged) put $obj ell \
	V [list $mCenterX $mCenterY $mCenterZ] \
	A [list $mDelta 0 0] \
	B [list 0 $mDelta 0] \
	C [list 0 0 $mDelta] \
	n $mN \
	e $mE
}


# ------------------------------------------------------------
#                      PROTECTED METHODS
# ------------------------------------------------------------

::itcl::body SuperellEditFrame::buildUpperPanel {} {
    EllEditFrame::buildUpperPanel

    set parent [$this childsite]
    itk_component add superellNL {
	::ttk::label $parent.superellNL \
	    -state disabled \
	    -text "n:" \
	    -anchor e
    } {}
    itk_component add superellNE {
	::ttk::entry $parent.superellNE \
	    -textvariable [::itcl::scope mN] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add superellNUnitsL {
	::ttk::label $parent.superellNUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add superellEL {
	::ttk::label $parent.superellEL \
	    -state disabled \
	    -text "e:" \
	    -anchor e
    } {}
    itk_component add superellEE {
	::ttk::entry $parent.superellEE \
	    -textvariable [::itcl::scope mE] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add superellEUnitsL {
	::ttk::label $parent.superellEUnitsL \
	    -anchor e
    } {}

    incr mCurrentGridRow
    grid $itk_component(superellNL) $itk_component(superellNE) \
	-row $mCurrentGridRow \
	-sticky nsew
    grid $itk_component(superellNUnitsL) \
	-row $mCurrentGridRow \
	-column 4 \
	-sticky nsew
    incr mCurrentGridRow
    grid $itk_component(superellEL) $itk_component(superellEE) \
	-row $mCurrentGridRow \
	-sticky nsew
    grid $itk_component(superellEUnitsL) \
	-row $mCurrentGridRow \
	-column 4 \
	-sticky nsew

    bind $itk_component(superellNE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(superellEE) <Return> [::itcl::code $this updateGeometryIfMod]
}


# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
