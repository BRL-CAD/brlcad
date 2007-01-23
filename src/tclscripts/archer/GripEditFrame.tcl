#               G R I P E D I T F R A M E . T C L
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
#    The class for editing gripicles within Archer.
#
##############################################################

::itcl::class GripEditFrame {
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
	variable mNx ""
	variable mNy ""
	variable mNz ""
	variable mL ""

	# Override what's in GeometryEditFrame
	method updateGeometryIfMod {}
    }

    private {}
}


# ------------------------------------------------------------
#                      CONSTRUCTOR
# ------------------------------------------------------------

::itcl::body GripEditFrame::constructor {args} {
    set parent [$this childsite]
    itk_component add gripType {
	::label $parent.griptype \
	    -text "Grip:" \
	    -anchor e
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }
    itk_component add gripName {
	::label $parent.gripname \
	    -textvariable [::itcl::scope itk_option(-geometryObject)] \
	    -anchor w
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }

    # Create header labels
    itk_component add gripXL {
	::label $parent.gripXL \
	    -text "X"
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }
    itk_component add gripYL {
	::label $parent.gripYL \
	    -text "Y"
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }
    itk_component add gripZL {
	::label $parent.gripZL \
	    -text "Z"
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }

    # create widgets for vertices
    itk_component add gripVL {
	::label $parent.gripVL \
	    -text "V:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add gripVxE {
	::entry $parent.gripVxE \
	    -textvariable [::itcl::scope mVx] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add gripVyE {
	::entry $parent.gripVyE \
	    -textvariable [::itcl::scope mVy] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add gripVzE {
	::entry $parent.gripVzE \
	    -textvariable [::itcl::scope mVz] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add gripVUnitsL {
	::label $parent.gripVUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add gripNL {
	::label $parent.gripNL \
	    -text "N:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add gripNxE {
	::entry $parent.gripNxE \
	    -textvariable [::itcl::scope mNx] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add gripNyE {
	::entry $parent.gripNyE \
	    -textvariable [::itcl::scope mNy] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add gripNzE {
	::entry $parent.gripNzE \
	    -textvariable [::itcl::scope mNz] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add gripNUnitsL {
	::label $parent.gripNUnitsL \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add gripLL {
	::label $parent.gripLL \
	    -text "L:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add gripLE {
	::entry $parent.gripLE \
	    -textvariable [::itcl::scope mL] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add gripLUnitsL {
	::label $parent.gripLUnitsL \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }

    set row 0
    grid $itk_component(gripType) \
	-row $row \
	-column 0 \
	-sticky nsew
    grid $itk_component(gripName) \
	-row $row \
	-column 1 \
	-columnspan 3 \
	-sticky nsew
    incr row
    grid x $itk_component(gripXL) \
	$itk_component(gripYL) \
	$itk_component(gripZL)
    incr row
    grid $itk_component(gripVL) \
	$itk_component(gripVxE) \
	$itk_component(gripVyE) \
	$itk_component(gripVzE) \
	$itk_component(gripVUnitsL) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(gripNL) \
	$itk_component(gripNxE) \
	$itk_component(gripNyE) \
	$itk_component(gripNzE) \
	$itk_component(gripNUnitsL) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(gripLL) $itk_component(gripLE) \
	-row $row \
	-sticky nsew
    grid $itk_component(gripLUnitsL) \
	-row $row \
	-column 4 \
	-sticky nsew
    grid columnconfigure $parent 1 -weight 1
    grid columnconfigure $parent 2 -weight 1
    grid columnconfigure $parent 3 -weight 1
    pack $parent -expand yes -fill x -anchor n

    # Set up bindings
    bind $itk_component(gripVxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(gripVyE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(gripVzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(gripNxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(gripNyE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(gripNzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(gripLE) <Return> [::itcl::code $this updateGeometryIfMod]

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
::itcl::body GripEditFrame::initGeometry {gdata} {
    set _V [bu_get_value_by_keyword V $gdata]
    set mVx [lindex $_V 0]
    set mVy [lindex $_V 1]
    set mVz [lindex $_V 2]
    set _N [bu_get_value_by_keyword N $gdata]
    set mNx [lindex $_N 0]
    set mNy [lindex $_N 1]
    set mNz [lindex $_N 2]
    set mL [bu_get_value_by_keyword L $gdata]

    GeometryEditFrame::initGeometry $gdata
}

::itcl::body GripEditFrame::updateGeometry {} {
    if {$itk_option(-mged) == "" ||
	$itk_option(-geometryObject) == ""} {
	return
    }

    $itk_option(-mged) adjust $itk_option(-geometryObject) \
	V [list $mVx $mVy $mVz] \
	N [list $mNx $mNy $mNz] \
	L $mL

    if {$itk_option(-geometryChangedCallback) != ""} {
	$itk_option(-geometryChangedCallback)
    }
}

::itcl::body GripEditFrame::createGeometry {obj} {
    if {![GeometryEditFrame::createGeometry $obj]} {
	return
    }

    $itk_option(-mged) put $obj grip \
	V [list $mCenterX $mCenterY $mCenterZ] \
	N [list 1 0 0] \
	L [expr {$mDelta * 2.0}]
}


# ------------------------------------------------------------
#                      PROTECTED METHODS
# ------------------------------------------------------------

::itcl::body GripEditFrame::updateGeometryIfMod {} {
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
    set _N [bu_get_value_by_keyword N $gdata]
    set _Nx [lindex $_N 0]
    set _Ny [lindex $_N 1]
    set _Nz [lindex $_N 2]
    set _L [bu_get_value_by_keyword L $gdata]

    if {$mVx == ""  ||
	$mVx == "-" ||
	$mVy == ""  ||
	$mVy == "-" ||
	$mVz == ""  ||
	$mVz == "-" ||
	$mNx == ""  ||
	$mNx == "-" ||
	$mNy == ""  ||
	$mNy == "-" ||
	$mNz == ""  ||
	$mNz == "-" ||
	$mL == ""  ||
	$mL == "-"} {
	# Not valid
	return
    }

    if {$_Vx != $mVx ||
	$_Vy != $mVy ||
	$_Vz != $mVz ||
	$_Nx != $mNx ||
	$_Ny != $mNy ||
	$_Nz != $mNz ||
	$_L != $mL} {
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
