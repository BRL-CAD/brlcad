#               H A L F E D I T F R A M E . T C L
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
# Author(s):
#    Bob Parker
#
# Description:
#    The class for editing halfs within Archer.
#
##############################################################

::itcl::class HalfEditFrame {
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
	variable mNx ""
	variable mNy ""
	variable mNz ""
	variable mD ""

	# Override what's in GeometryEditFrame
	method updateGeometryIfMod {}
    }

    private {}
}


# ------------------------------------------------------------
#                      CONSTRUCTOR
# ------------------------------------------------------------

::itcl::body HalfEditFrame::constructor {args} {
    set parent [$this childsite]
    itk_component add halfType {
	::ttk::label $parent.halftype \
	    -text "Half:" \
	    -anchor e
    } {}
    itk_component add halfName {
	::ttk::label $parent.halfname \
	    -textvariable [::itcl::scope itk_option(-geometryObject)] \
	    -anchor w
    } {}

    # Create header labels
    itk_component add halfXL {
	::ttk::label $parent.halfXL \
	    -text "X"
    } {}
    itk_component add halfYL {
	::ttk::label $parent.halfYL \
	    -text "Y"
    } {}
    itk_component add halfZL {
	::ttk::label $parent.halfZL \
	    -text "Z"
    } {}

    # create widgets for vertices
    itk_component add halfNL {
	::ttk::label $parent.halfNL \
	    -text "N:" \
	    -anchor e
    } {}
    itk_component add halfNxE {
	::ttk::entry $parent.halfNxE \
	    -textvariable [::itcl::scope mNx] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add halfNyE {
	::ttk::entry $parent.halfNyE \
	    -textvariable [::itcl::scope mNy] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add halfNzE {
	::ttk::entry $parent.halfNzE \
	    -textvariable [::itcl::scope mNz] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add halfNUnitsL {
	::ttk::label $parent.halfNUnitsL \
	    -anchor e
    } {}
    itk_component add halfDL {
	::ttk::label $parent.halfDL \
	    -text "d:" \
	    -anchor e
    } {}
    itk_component add halfDE {
	::ttk::entry $parent.halfDE \
	    -textvariable [::itcl::scope mD] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add halfDUnitsL {
	::ttk::label $parent.halfDUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}

    set row 0
    grid $itk_component(halfType) \
	-row $row \
	-column 0 \
	-sticky nsew
    grid $itk_component(halfName) \
	-row $row \
	-column 1 \
	-columnspan 3 \
	-sticky nsew
    incr row
    grid x $itk_component(halfXL) \
	$itk_component(halfYL) \
	$itk_component(halfZL)
    incr row
    grid $itk_component(halfNL) \
	$itk_component(halfNxE) \
	$itk_component(halfNyE) \
	$itk_component(halfNzE) \
	$itk_component(halfNUnitsL) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(halfDL) $itk_component(halfDE) \
	-row $row \
	-sticky nsew
    grid $itk_component(halfDUnitsL) \
	-row $row \
	-column 4 \
	-sticky nsew
    grid columnconfigure $parent 1 -weight 1
    grid columnconfigure $parent 2 -weight 1
    grid columnconfigure $parent 3 -weight 1
    pack $parent -expand yes -fill x -anchor n

    # Set up bindings
    bind $itk_component(halfNxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(halfNyE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(halfNzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(halfDE) <Return> [::itcl::code $this updateGeometryIfMod]

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
::itcl::body HalfEditFrame::initGeometry {gdata} {
    set _N [bu_get_value_by_keyword N $gdata]
    set mNx [lindex $_N 0]
    set mNy [lindex $_N 1]
    set mNz [lindex $_N 2]
    set mD [bu_get_value_by_keyword d $gdata]

    GeometryEditFrame::initGeometry $gdata
}

::itcl::body HalfEditFrame::updateGeometry {} {
    if {$itk_option(-mged) == "" ||
	$itk_option(-geometryObject) == ""} {
	return
    }

    $itk_option(-mged) adjust $itk_option(-geometryObject) \
	N [list $mNx $mNy $mNz] \
	d $mD

    GeometryEditFrame::updateGeometry
}

::itcl::body HalfEditFrame::createGeometry {obj} {
    if {![GeometryEditFrame::createGeometry $obj]} {
	return
    }

    $itk_option(-mged) put $obj half \
	N {0 0 1} \
	d $mCenterZ
}


# ------------------------------------------------------------
#                      PROTECTED METHODS
# ------------------------------------------------------------

::itcl::body HalfEditFrame::updateGeometryIfMod {} {
    if {$itk_option(-mged) == "" ||
	$itk_option(-geometryObject) == ""} {
	return
    }


    set gdata [$itk_option(-mged) get $itk_option(-geometryObject)]
    set gdata [lrange $gdata 1 end]

    set _N [bu_get_value_by_keyword N $gdata]
    set _Nx [lindex $_N 0]
    set _Ny [lindex $_N 1]
    set _Nz [lindex $_N 2]
    set _D [bu_get_value_by_keyword d $gdata]

    if {$mNx == ""  ||
	$mNx == "-" ||
	$mNy == ""  ||
	$mNy == "-" ||
	$mNz == ""  ||
	$mNz == "-" ||
	$mD == ""   ||
	$mD == "-"} {
	# Not valid
	return
    }

    if {$_Nx != $mNx ||
	$_Ny != $mNy ||
	$_Nz != $mNz ||
	$_D != $mD} {
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
