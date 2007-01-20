#                B O T E D I T F R A M E . T C L
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

    public {
	# Override what's in GeometryEditFrame
	method initGeometry {gdata}
	method updateGeometry {}
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
    itk_component add torVxL {
	::label $parent.torVxL \
	    -text "Vx:" \
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
    itk_component add torVyL {
	::label $parent.torVyL \
	    -text "Vy:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add torVyE {
	::entry $parent.torVyE \
	    -textvariable [::itcl::scope mVy] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add torVzL {
	::label $parent.torVzL \
	    -text "Vz:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add torVzE {
	::entry $parent.torVzE \
	    -textvariable [::itcl::scope mVz] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add torHxL {
	::label $parent.torHxL \
	    -text "Hx:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add torHxE {
	::entry $parent.torHxE \
	    -textvariable [::itcl::scope mHx] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add torHyL {
	::label $parent.torHyL \
	    -text "Hy:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add torHyE {
	::entry $parent.torHyE \
	    -textvariable [::itcl::scope mHy] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add torHzL {
	::label $parent.torHzL \
	    -text "Hz:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add torHzE {
	::entry $parent.torHzE \
	    -textvariable [::itcl::scope mHz] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
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

    set row 0
    grid $itk_component(torType) $itk_component(torName) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(torVxL) $itk_component(torVxE) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(torVyL) $itk_component(torVyE) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(torVzL) $itk_component(torVzE) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(torHxL) $itk_component(torHxE) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(torHyL) $itk_component(torHyE) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(torHzL) $itk_component(torHzE) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(torR_aL) $itk_component(torR_aE) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(torR_hL) $itk_component(torR_hE) \
	-row $row \
	-sticky nsew
    grid columnconfigure [namespace tail $this] 1 -weight 1

    # Set up bindings
    bind $itk_component(torVxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(torVyE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(torVzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(torHxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(torHyE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(torHzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(torR_aE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(torR_hE) <Return> [::itcl::code $this updateGeometryIfMod]

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
