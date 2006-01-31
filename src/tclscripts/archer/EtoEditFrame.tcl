#                E T O E D I T F R A M E . T C L
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
# Author:
#    Bob Parker
#
# Description:
#    The class for editing etos within Archer.
#

::itcl::class EtoEditFrame {
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
	variable mCx ""
	variable mCy ""
	variable mCz ""
	variable mR ""
	variable mR_d ""

	# Override what's in GeometryEditFrame
	method updateGeometryIfMod {}
    }

    private {}
}


# ------------------------------------------------------------
#                      CONSTRUCTOR
# ------------------------------------------------------------

::itcl::body EtoEditFrame::constructor {args} {
    set parent [$this childsite]
    itk_component add etoType {
	::label $parent.etotype \
	    -text "Eto:" \
	    -anchor e
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }
    itk_component add etoName {
	::label $parent.etoname \
	    -textvariable [::itcl::scope itk_option(-geometryObject)] \
	    -anchor w
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }

    # Create header labels
    itk_component add etoXL {
	::label $parent.etoXL \
	    -text "X"
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }
    itk_component add etoYL {
	::label $parent.etoYL \
	    -text "Y"
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }
    itk_component add etoZL {
	::label $parent.etoZL \
	    -text "Z"
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }

    # create widgets for vertices
    itk_component add etoVL {
	::label $parent.etoVL \
	    -text "V:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add etoVxE {
	::entry $parent.etoVxE \
	    -textvariable [::itcl::scope mVx] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add etoVyE {
	::entry $parent.etoVyE \
	    -textvariable [::itcl::scope mVy] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add etoVzE {
	::entry $parent.etoVzE \
	    -textvariable [::itcl::scope mVz] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add etoVUnitsL {
	::label $parent.etoVUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add etoNL {
	::label $parent.etoNL \
	    -text "N:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add etoNxE {
	::entry $parent.etoNxE \
	    -textvariable [::itcl::scope mNx] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add etoNyE {
	::entry $parent.etoNyE \
	    -textvariable [::itcl::scope mNy] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add etoNzE {
	::entry $parent.etoNzE \
	    -textvariable [::itcl::scope mNz] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add etoNUnitsL {
	::label $parent.etoNUnitsL \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add etoCL {
	::label $parent.etoCL \
	    -text "C:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add etoCxE {
	::entry $parent.etoCxE \
	    -textvariable [::itcl::scope mCx] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add etoCyE {
	::entry $parent.etoCyE \
	    -textvariable [::itcl::scope mCy] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add etoCzE {
	::entry $parent.etoCzE \
	    -textvariable [::itcl::scope mCz] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add etoCUnitsL {
	::label $parent.etoCUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add etoRL {
	::label $parent.etoRL \
	    -text "r:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add etoRE {
	::entry $parent.etoRE \
	    -textvariable [::itcl::scope mR] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add etoRUnitsL {
	::label $parent.etoRVUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add etoR_dL {
	::label $parent.etoR_dL \
	    -text "r_d:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add etoR_dE {
	::entry $parent.etoR_dE \
	    -textvariable [::itcl::scope mR_d] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add etoR_dUnitsL {
	::label $parent.etoR_dUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }

    set row 0
    grid $itk_component(etoType) \
	-row $row \
	-column 0 \
	-sticky nsew
    grid $itk_component(etoName) \
	-row $row \
	-column 1 \
	-columnspan 3 \
	-sticky nsew
    incr row
    grid x $itk_component(etoXL) \
	$itk_component(etoYL) \
	$itk_component(etoZL)
    incr row
    grid $itk_component(etoVL) \
	$itk_component(etoVxE) \
	$itk_component(etoVyE) \
	$itk_component(etoVzE) \
	$itk_component(etoVUnitsL) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(etoNL) \
	$itk_component(etoNxE) \
	$itk_component(etoNyE) \
	$itk_component(etoNzE) \
	$itk_component(etoNUnitsL) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(etoCL) \
	$itk_component(etoCxE) \
	$itk_component(etoCyE) \
	$itk_component(etoCzE) \
	$itk_component(etoCUnitsL) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(etoRL) $itk_component(etoRE) \
	-row $row \
	-sticky nsew
    grid $itk_component(etoRUnitsL) \
	-row $row \
	-column 4 \
	-sticky nsew
    incr row
    grid $itk_component(etoR_dL) $itk_component(etoR_dE) \
	-row $row \
	-sticky nsew
    grid $itk_component(etoR_dUnitsL) \
	-row $row \
	-column 4 \
	-sticky nsew
    grid columnconfigure $parent 1 -weight 1
    grid columnconfigure $parent 2 -weight 1
    grid columnconfigure $parent 3 -weight 1
    pack $parent -expand yes -fill x -anchor n

    # Set up bindings
    bind $itk_component(etoVxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(etoVyE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(etoVzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(etoNxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(etoNyE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(etoNzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(etoCxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(etoCyE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(etoCzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(etoRE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(etoR_dE) <Return> [::itcl::code $this updateGeometryIfMod]

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
::itcl::body EtoEditFrame::initGeometry {gdata} {
    set _V [bu_get_value_by_keyword V $gdata]
    set mVx [lindex $_V 0]
    set mVy [lindex $_V 1]
    set mVz [lindex $_V 2]
    set _N [bu_get_value_by_keyword N $gdata]
    set mNx [lindex $_N 0]
    set mNy [lindex $_N 1]
    set mNz [lindex $_N 2]
    set _C [bu_get_value_by_keyword C $gdata]
    set mCx [lindex $_C 0]
    set mCy [lindex $_C 1]
    set mCz [lindex $_C 2]
    set mR [bu_get_value_by_keyword r $gdata]
    set mR_d [bu_get_value_by_keyword r_d $gdata]

    GeometryEditFrame::initGeometry $gdata
}

::itcl::body EtoEditFrame::updateGeometry {} {
    if {$itk_option(-mged) == "" ||
	$itk_option(-geometryObject) == ""} {
	return
    }

    $itk_option(-mged) adjust $itk_option(-geometryObject) \
	V [list $mVx $mVy $mVz] \
	N [list $mNx $mNy $mNz] \
	C [list $mCx $mCy $mCz] \
	r $mR \
	r_d $mR_d

    if {$itk_option(-geometryChangedCallback) != ""} {
	$itk_option(-geometryChangedCallback)
    }
}

::itcl::body EtoEditFrame::createGeometry {obj} {
    if {![GeometryEditFrame::createGeometry $obj]} {
	return
    }

    $itk_option(-mged) put $obj eto \
	V [list $mCenterX $mCenterY $mCenterZ] \
	N [list $mDelta 0 0] \
	C [list $mDelta 0 0] \
	r $mDelta \
	r_d [expr {$mDelta * 0.1}]
}


# ------------------------------------------------------------
#                      PROTECTED METHODS
# ------------------------------------------------------------

::itcl::body EtoEditFrame::updateGeometryIfMod {} {
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
    set _C [bu_get_value_by_keyword C $gdata]
    set _Cx [lindex $_C 0]
    set _Cy [lindex $_C 1]
    set _Cz [lindex $_C 2]
    set _R [bu_get_value_by_keyword r $gdata]
    set _R_d [bu_get_value_by_keyword r_d $gdata]

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
	$mCx == ""  ||
	$mCx == "-" ||
	$mCy == ""  ||
	$mCy == "-" ||
	$mCz == ""  ||
	$mCz == "-" ||
	$mR == ""  ||
	$mR == "-" ||
	$mR_d == ""  ||
	$mR_d == "-"} {
	# Not valid
	return
    }

    if {$_Vx != $mVx ||
	$_Vy != $mVy ||
	$_Vz != $mVz ||
	$_Nx != $mNx ||
	$_Ny != $mNy ||
	$_Nz != $mNz ||
	$_Cx != $mCx ||
	$_Cy != $mCy ||
	$_Cz != $mCz ||
	$_R != $mR ||
	$_R_d != $mR_d} {
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
