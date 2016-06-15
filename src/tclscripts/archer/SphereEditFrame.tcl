#             S P H E R E E D I T F R A M E . T C L
# BRL-CAD
#
# Copyright (c) 2002-2014 United States Government as represented by
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
#    The class for editing spheres within Archer.
#
##############################################################

::itcl::class SphereEditFrame {
    inherit EllEditFrame

    constructor {args} {}
    destructor {}

    public {
	#Override's for EllEditFrame
	method createGeometry {obj}
	method p {_obj args}
    }

    protected {
	method buildLowerPanel {}
    }

    private {}
}


# ------------------------------------------------------------
#                      CONSTRUCTOR
# ------------------------------------------------------------

::itcl::body SphereEditFrame::constructor {args} {
    eval itk_initialize $args
    $itk_component(ellType) configure -text "Sphere:"
}


::itcl::body SphereEditFrame::createGeometry {obj} {
    if {![GeometryEditFrame::createGeometry $obj]} {
	return
    }

    $itk_option(-mged) put $obj sph \
	V [list $mCenterX $mCenterY $mCenterZ] \
	A [list $mDelta 0 0] \
	B [list 0 $mDelta 0] \
	C [list 0 0 $mDelta]
}


::itcl::body SphereEditFrame::p {_obj args} {
    if {[llength $args] != 1 || ![string is double $args]} {
	return "Usage: p sf"
    }

    switch -- $mEditMode \
	$setABC {
	    $::ArcherCore::application p_pscale $_obj abc $args
	}

    return ""
}


# ------------------------------------------------------------
#                      PROTECTED METHODS
# ------------------------------------------------------------

::itcl::body SphereEditFrame::buildLowerPanel {} {
    set parent [$this childsite lower]
    set row 0
    foreach attribute {ABC} {
	itk_component add set$attribute {
	    ::ttk::radiobutton $parent.set_$attribute \
		-variable [::itcl::scope mEditMode] \
		-value [subst $[subst set$attribute]] \
		-text "Set $attribute" \
		-command [::itcl::code $this initEditState]
	} {}

	grid $itk_component(set$attribute) -row $row -column 0 -sticky nsew
	incr row
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
