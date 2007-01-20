#                R P C E D I T F R A M E . T C L
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
#    The class for editing rpcs within Archer.
#
##############################################################

::itcl::class RpcEditFrame {
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

	# Override what's in GeometryEditFrame
	method updateGeometryIfMod {}
    }

    private {}
}


# ------------------------------------------------------------
#                      CONSTRUCTOR
# ------------------------------------------------------------

::itcl::body RpcEditFrame::constructor {args} {
    set parent [$this childsite]
    itk_component add rpcType {
	::label $parent.rpctype \
	    -text "Rpc:" \
	    -anchor e
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }
    itk_component add rpcName {
	::label $parent.rpcname \
	    -textvariable [::itcl::scope itk_option(-geometryObject)] \
	    -anchor w
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }

    # Create header labels
    itk_component add rpcXL {
	::label $parent.rpcXL \
	    -text "X"
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }
    itk_component add rpcYL {
	::label $parent.rpcYL \
	    -text "Y"
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }
    itk_component add rpcZL {
	::label $parent.rpcZL \
	    -text "Z"
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }

    # create widgets for vertices
    itk_component add rpcVL {
	::label $parent.rpcVL \
	    -text "V:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add rpcVxE {
	::entry $parent.rpcVxE \
	    -textvariable [::itcl::scope mVx] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add rpcVyE {
	::entry $parent.rpcVyE \
	    -textvariable [::itcl::scope mVy] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add rpcVzE {
	::entry $parent.rpcVzE \
	    -textvariable [::itcl::scope mVz] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add rpcVUnitsL {
	::label $parent.rpcVUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add rpcHL {
	::label $parent.rpcHL \
	    -text "H:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add rpcHxE {
	::entry $parent.rpcHxE \
	    -textvariable [::itcl::scope mHx] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add rpcHyE {
	::entry $parent.rpcHyE \
	    -textvariable [::itcl::scope mHy] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add rpcHzE {
	::entry $parent.rpcHzE \
	    -textvariable [::itcl::scope mHz] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add rpcHUnitsL {
	::label $parent.rpcHUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add rpcBL {
	::label $parent.rpcBL \
	    -text "B:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add rpcBxE {
	::entry $parent.rpcBxE \
	    -textvariable [::itcl::scope mBx] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add rpcByE {
	::entry $parent.rpcByE \
	    -textvariable [::itcl::scope mBy] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add rpcBzE {
	::entry $parent.rpcBzE \
	    -textvariable [::itcl::scope mBz] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add rpcBUnitsL {
	::label $parent.rpcBUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add rpcRL {
	::label $parent.rpcRL \
	    -text "r:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add rpcRE {
	::entry $parent.rpcRE \
	    -textvariable [::itcl::scope mR] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add rpcRUnitsL {
	::label $parent.rpcRUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }

    set row 0
    grid $itk_component(rpcType) \
	-row $row \
	-column 0 \
	-sticky nsew
    grid $itk_component(rpcName) \
	-row $row \
	-column 1 \
	-columnspan 3 \
	-sticky nsew
    incr row
    grid x $itk_component(rpcXL) \
	$itk_component(rpcYL) \
	$itk_component(rpcZL)
    incr row
    grid $itk_component(rpcVL) \
	$itk_component(rpcVxE) \
	$itk_component(rpcVyE) \
	$itk_component(rpcVzE) \
	$itk_component(rpcVUnitsL) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(rpcHL) \
	$itk_component(rpcHxE) \
	$itk_component(rpcHyE) \
	$itk_component(rpcHzE) \
	$itk_component(rpcHUnitsL) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(rpcBL) \
	$itk_component(rpcBxE) \
	$itk_component(rpcByE) \
	$itk_component(rpcBzE) \
	$itk_component(rpcBUnitsL) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(rpcRL) $itk_component(rpcRE) \
	-row $row \
	-sticky nsew
    grid $itk_component(rpcRUnitsL) \
	-row $row \
	-column 4 \
	-sticky nsew
    grid columnconfigure $parent 1 -weight 1
    grid columnconfigure $parent 2 -weight 1
    grid columnconfigure $parent 3 -weight 1
    pack $parent -expand yes -fill x -anchor n

    # Set up bindings
    bind $itk_component(rpcVxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(rpcVyE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(rpcVzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(rpcHxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(rpcHyE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(rpcHzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(rpcBxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(rpcByE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(rpcBzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(rpcRE) <Return> [::itcl::code $this updateGeometryIfMod]

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
::itcl::body RpcEditFrame::initGeometry {gdata} {
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

    GeometryEditFrame::initGeometry $gdata
}

::itcl::body RpcEditFrame::updateGeometry {} {
    if {$itk_option(-mged) == "" ||
	$itk_option(-geometryObject) == ""} {
	return
    }

    $itk_option(-mged) adjust $itk_option(-geometryObject) \
	V [list $mVx $mVy $mVz] \
	H [list $mHx $mHy $mHz] \
	B [list $mBx $mBy $mBz] \
	r $mR

    if {$itk_option(-geometryChangedCallback) != ""} {
	$itk_option(-geometryChangedCallback)
    }
}

::itcl::body RpcEditFrame::createGeometry {obj} {
    if {![GeometryEditFrame::createGeometry $obj]} {
	return
    }

    $itk_option(-mged) put $obj rpc \
	V [list $mCenterX $mCenterY $mCenterZ] \
	H [list 0 0 $mDelta] \
	B [list $mDelta 0 0] \
	r $mDelta
}


# ------------------------------------------------------------
#                      PROTECTED METHODS
# ------------------------------------------------------------

::itcl::body RpcEditFrame::updateGeometryIfMod {} {
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
	$mR == "-"} {
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
	$_R != $mR} {
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
