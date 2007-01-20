#              T O R U S E D I T F R A M E . T C L
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
#    The class for editing tori within Archer.
#
##############################################################

::itcl::class TorusEditFrame {
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
	variable mR_a ""
	variable mR_h ""

	# Override what's in GeometryEditFrame
	method updateGeometryIfMod {}
    }

    private {}
}


# ------------------------------------------------------------
#                      CONSTRUCTOR
# ------------------------------------------------------------

::itcl::body TorusEditFrame::constructor {args} {
    eval itk_initialize $args
}

::itcl::body TorusEditFrame::buildUpperPanel {} {
    set parent [$this childsite]
    itk_component add torType {
	::label $parent.tortype \
	    -text "Torus:" \
	    -anchor e
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }
    itk_component add torName {
	::label $parent.torname \
	    -textvariable [::itcl::scope itk_option(-geometryObject)] \
	    -anchor w
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }

    # Create header labels
    itk_component add torXL {
	::label $parent.torXL \
	    -text "X"
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }
    itk_component add torYL {
	::label $parent.torYL \
	    -text "Y"
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }
    itk_component add torZL {
	::label $parent.torZL \
	    -text "Z"
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }

    # create widgets for vertices, vectors etc.
    itk_component add torVL {
	::label $parent.torVL \
	    -text "V:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add torVxE {
	::entry $parent.torVxE \
	    -textvariable [::itcl::scope mVx] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add torVyE {
	::entry $parent.torVyE \
	    -textvariable [::itcl::scope mVy] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add torVzE {
	::entry $parent.torVzE \
	    -textvariable [::itcl::scope mVz] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add torVUnitsL {
	::label $parent.torVUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add torHL {
	::label $parent.torHL \
	    -text "H:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add torHxE {
	::entry $parent.torHxE \
	    -textvariable [::itcl::scope mHx] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add torHyE {
	::entry $parent.torHyE \
	    -textvariable [::itcl::scope mHy] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add torHzE {
	::entry $parent.torHzE \
	    -textvariable [::itcl::scope mHz] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add torHUnitsL {
	::label $parent.torHUnitsL \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add torR_aL {
	::label $parent.torR_aL \
	    -text "r_a:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add torR_aE {
	::entry $parent.torR_aE \
	    -textvariable [::itcl::scope mR_a] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add torR_aUnitsL {
	::label $parent.torR_aUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add torR_hL {
	::label $parent.torR_hL \
	    -text "r_h:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add torR_hE {
	::entry $parent.torR_hE \
	    -textvariable [::itcl::scope mR_h] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add torR_hUnitsL {
	::label $parent.torR_hUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }

    set row 0
    grid $itk_component(torType) \
	-row $row \
	-column 0 \
	-sticky nsew
    grid $itk_component(torName) \
	-row $row \
	-column 1 \
	-columnspan 3 \
	-sticky nsew
    incr row
    grid x $itk_component(torXL) \
	$itk_component(torYL) \
	$itk_component(torZL)
    incr row
    grid $itk_component(torVL) \
	$itk_component(torVxE) \
	$itk_component(torVyE) \
	$itk_component(torVzE) \
	$itk_component(torVUnitsL) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(torHL) \
	$itk_component(torHxE) \
	$itk_component(torHyE) \
	$itk_component(torHzE) \
	$itk_component(torHUnitsL) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(torR_aL) \
	$itk_component(torR_aE) x x \
	$itk_component(torR_aUnitsL) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(torR_hL) \
	$itk_component(torR_hE) x x \
	$itk_component(torR_hUnitsL) \
	-row $row \
	-sticky nsew
    grid columnconfigure $parent 1 -weight 1
    grid columnconfigure $parent 2 -weight 1
    grid columnconfigure $parent 3 -weight 1
    pack $parent -expand yes -fill x -anchor n

    # Set up bindings
    bind $itk_component(torVxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(torVyE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(torVzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(torHxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(torHyE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(torHzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(torR_aE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(torR_hE) <Return> [::itcl::code $this updateGeometryIfMod]
}


::itcl::body TorusEditFrame::buildLowerPanel {} {
}

::itcl::body TorusEditFrame::buildValuePanel {} {
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
::itcl::body TorusEditFrame::initGeometry {gdata} {
    set _V [bu_get_value_by_keyword V $gdata]
    set mVx [lindex $_V 0]
    set mVy [lindex $_V 1]
    set mVz [lindex $_V 2]
    set _H [bu_get_value_by_keyword H $gdata]
    set mHx [lindex $_H 0]
    set mHy [lindex $_H 1]
    set mHz [lindex $_H 2]
    set mR_a [bu_get_value_by_keyword r_a $gdata]
    set mR_h [bu_get_value_by_keyword r_h $gdata]

    GeometryEditFrame::initGeometry $gdata
}

::itcl::body TorusEditFrame::updateGeometry {} {
    if {$itk_option(-mged) == "" ||
	$itk_option(-geometryObject) == ""} {
	return
    }

    $itk_option(-mged) adjust $itk_option(-geometryObject) \
	V [list $mVx $mVy $mVz] \
	H [list $mHx $mHy $mHz] \
	r_a $mR_a \
	r_h $mR_h

    if {$itk_option(-geometryChangedCallback) != ""} {
	$itk_option(-geometryChangedCallback)
    }
}

::itcl::body TorusEditFrame::createGeometry {obj} {
    if {![GeometryEditFrame::createGeometry $obj]} {
	return
    }

    $itk_option(-mged) put $obj tor \
	V [list $mCenterX $mCenterY $mCenterZ] \
	H [list $mDelta 0 0] \
	r_a $mDelta \
	r_h [expr {$mDelta * 0.1}]
}


# ------------------------------------------------------------
#                      PROTECTED METHODS
# ------------------------------------------------------------

::itcl::body TorusEditFrame::updateGeometryIfMod {} {
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
    set _R_a [bu_get_value_by_keyword r_a $gdata]
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
	$mR_a == ""  ||
	$mR_a == "-" ||
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
	$_R_a != $mR_a ||
	$_R_h != $mR_h} {
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
