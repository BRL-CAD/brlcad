##
# Portions Copyright (c) 2002 SURVICE Engineering Company. All Rights Reserved.
# This file contains Original Code and/or Modifications of Original Code as
# defined in and that are subject to the SURVICE Public Source License
# (Version 1.3, dated March 12, 2002).
#
# TYPE: tcltk
##############################################################
#
# SphereEditFrame.tcl
#
##############################################################
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

    #Override's for EllEditFrame
    public {
	method createGeometry {obj}
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

