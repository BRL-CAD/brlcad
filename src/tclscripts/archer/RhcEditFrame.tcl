#                R H C E D I T F R A M E . T C L
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
# Author(s):
#    Bob Parker
#
# Description:
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
	method createGeometry {obj}
    }

    protected {
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

	# Override what's in GeometryEditFrame
	method updateGeometryIfMod {}
    }

    private {}
}


# ------------------------------------------------------------
#                      CONSTRUCTOR
# ------------------------------------------------------------

::itcl::body RhcEditFrame::constructor {args} {
    set parent [$this childsite]
    itk_component add rhcType {
	::label $parent.rhctype \
	    -text "Rhc:" \
	    -anchor e
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }
    itk_component add rhcName {
	::label $parent.rhcname \
	    -textvariable [::itcl::scope itk_option(-geometryObject)] \
	    -anchor w
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }

    # Create header labels
    itk_component add rhcXL {
	::label $parent.rhcXL \
	    -text "X"
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }
    itk_component add rhcYL {
	::label $parent.rhcYL \
	    -text "Y"
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }
    itk_component add rhcZL {
	::label $parent.rhcZL \
	    -text "Z"
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }

    # create widgets for vertices
    itk_component add rhcVL {
	::label $parent.rhcVL \
	    -text "V:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add rhcVxE {
	::entry $parent.rhcVxE \
	    -textvariable [::itcl::scope mVx] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add rhcVyE {
	::entry $parent.rhcVyE \
	    -textvariable [::itcl::scope mVy] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add rhcVzE {
	::entry $parent.rhcVzE \
	    -textvariable [::itcl::scope mVz] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add rhcVUnitsL {
	::label $parent.rhcVUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add rhcHL {
	::label $parent.rhcHL \
	    -text "H:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add rhcHxE {
	::entry $parent.rhcHxE \
	    -textvariable [::itcl::scope mHx] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add rhcHyE {
	::entry $parent.rhcHyE \
	    -textvariable [::itcl::scope mHy] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add rhcHzE {
	::entry $parent.rhcHzE \
	    -textvariable [::itcl::scope mHz] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add rhcHUnitsL {
	::label $parent.rhcHUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add rhcBL {
	::label $parent.rhcBL \
	    -text "B:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add rhcBxE {
	::entry $parent.rhcBxE \
	    -textvariable [::itcl::scope mBx] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add rhcByE {
	::entry $parent.rhcByE \
	    -textvariable [::itcl::scope mBy] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add rhcBzE {
	::entry $parent.rhcBzE \
	    -textvariable [::itcl::scope mBz] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add rhcBUnitsL {
	::label $parent.rhcBUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add rhcRL {
	::label $parent.rhcRL \
	    -text "r:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add rhcRE {
	::entry $parent.rhcRE \
	    -textvariable [::itcl::scope mR] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add rhcRUnitsL {
	::label $parent.rhcRUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add rhcCL {
	::label $parent.rhcCL \
	    -text "c:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add rhcCE {
	::entry $parent.rhcCE \
	    -textvariable [::itcl::scope mC] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add rhcCUnitsL {
	::label $parent.rhcCUnitsL \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }

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

    if {$itk_option(-geometryChangedCallback) != ""} {
	$itk_option(-geometryChangedCallback)
    }
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


# ------------------------------------------------------------
#                      PROTECTED METHODS
# ------------------------------------------------------------

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

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
