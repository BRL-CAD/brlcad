#               A R B 5 E D I T F R A M E . T C L
# BRL-CAD
#
# Copyright (c) 2002-2009 United States Government as represented by
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
#    The class for editing arb5s within Archer.
#
##############################################################

::itcl::class Arb5EditFrame {
    inherit Arb8EditFrame

    constructor {args} {}
    destructor {}

    # Methods used by the constructor
    protected {
	method buildMoveEdgePanel {parent}
	method buildMoveFacePanel {parent}
	method buildRotateFacePanel {parent}

	# override methods in GeometryEditFrame
	method buildUpperPanel
    }

    public {
	# Override what's in GeometryEditFrame
	method updateGeometry {}
	method createGeometry {obj}

	method moveEdge {edge}
	method moveFace {face}
	method rotateFace {face}
    }

    protected {
	variable moveEdge12 1
	variable moveEdge23 2
	variable moveEdge34 3
	variable moveEdge14 4
	variable moveEdge15 5
	variable moveEdge25 6
	variable moveEdge35 7
	variable moveEdge45 8
	variable movePoint5 9
	variable moveFace1234 10
	variable moveFace125 11
	variable moveFace235 12
	variable moveFace345 13
	variable moveFace145 14
	variable rotateFace1234 15
	variable rotateFace125 16
	variable rotateFace235 17
	variable rotateFace345 18
	variable rotateFace145 19

	# Override what's in Arb8EditFrame
	method updateUpperPanel {normal disabled}
	method updateValuePanel {}

	method initValuePanel {}

	method editGeometry {}
    }

    private {}
}


# ------------------------------------------------------------
#                      CONSTRUCTOR
# ------------------------------------------------------------

::itcl::body Arb5EditFrame::constructor {args} {
    eval itk_initialize $args
}

::itcl::body Arb5EditFrame::buildUpperPanel {} {
    set parent [$this childsite upper]
    itk_component add arb5Type {
	::ttk::label $parent.arb5type \
	    -text "Arb5:" \
	    -anchor e
    } {}
    itk_component add arb5Name {
	::ttk::label $parent.arb5name \
	    -textvariable [::itcl::scope itk_option(-geometryObject)] \
	    -anchor w
    } {}

    # Create header labels
    itk_component add arb5XL {
	::ttk::label $parent.arb5XL \
	    -text "X"
    } {}
    itk_component add arb5YL {
	::ttk::label $parent.arb5YL \
	    -text "Y"
    } {}
    itk_component add arb5ZL {
	::ttk::label $parent.arb5ZL \
	    -text "Z"
    } {}

    # create widgets for vertices
    itk_component add arb5V1L {
	::ttk::label $parent.arb5V1L \
	    -text "V1:" \
	    -anchor e
    } {}
    itk_component add arb5V1xE {
	::ttk::entry $parent.arb5V1xE \
	    -textvariable [::itcl::scope mV1x] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb5V1yE {
	::ttk::entry $parent.arb5V1yE \
	    -textvariable [::itcl::scope mV1y] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb5V1zE {
	::ttk::entry $parent.arb5V1zE \
	    -textvariable [::itcl::scope mV1z] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb5V1UnitsL {
	::ttk::label $parent.arb5V1UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add arb5V2L {
	::ttk::label $parent.arb5V2L \
	    -text "V2:" \
	    -anchor e
    } {}
    itk_component add arb5V2xE {
	::ttk::entry $parent.arb5V2xE \
	    -textvariable [::itcl::scope mV2x] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb5V2yE {
	::ttk::entry $parent.arb5V2yE \
	    -textvariable [::itcl::scope mV2y] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb5V2zE {
	::ttk::entry $parent.arb5V2zE \
	    -textvariable [::itcl::scope mV2z] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb5V2UnitsL {
	::ttk::label $parent.arb5V2UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add arb5V3L {
	::ttk::label $parent.arb5V3L \
	    -text "V3:" \
	    -anchor e
    } {}
    itk_component add arb5V3xE {
	::ttk::entry $parent.arb5V3xE \
	    -textvariable [::itcl::scope mV3x] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb5V3yE {
	::ttk::entry $parent.arb5V3yE \
	    -textvariable [::itcl::scope mV3y] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb5V3zE {
	::ttk::entry $parent.arb5V3zE \
	    -textvariable [::itcl::scope mV3z] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb5V3UnitsL {
	::ttk::label $parent.arb5V3UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add arb5V4L {
	::ttk::label $parent.arb5V4L \
	    -text "V4:" \
	    -anchor e
    } {}
    itk_component add arb5V4xE {
	::ttk::entry $parent.arb5V4xE \
	    -textvariable [::itcl::scope mV4x] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb5V4yE {
	::ttk::entry $parent.arb5V4yE \
	    -textvariable [::itcl::scope mV4y] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb5V4zE {
	::ttk::entry $parent.arb5V4zE \
	    -textvariable [::itcl::scope mV4z] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb5V4UnitsL {
	::ttk::label $parent.arb5V4UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}
    itk_component add arb5V5L {
	::ttk::label $parent.arb5V5L \
	    -text "V5:" \
	    -anchor e
    } {}
    itk_component add arb5V5xE {
	::ttk::entry $parent.arb5V5xE \
	    -textvariable [::itcl::scope mV5x] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb5V5yE {
	::ttk::entry $parent.arb5V5yE \
	    -textvariable [::itcl::scope mV5y] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb5V5zE {
	::ttk::entry $parent.arb5V5zE \
	    -textvariable [::itcl::scope mV5z] \
	    -state disabled \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {}
    itk_component add arb5V5UnitsL {
	::ttk::label $parent.arb5V5UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}

    set row 0
    grid $itk_component(arb5Type) \
	-row $row \
	-column 0 \
	-sticky nsew
    grid $itk_component(arb5Name) \
	-row $row \
	-column 1 \
	-columnspan 3 \
	-sticky nsew
    incr row
    grid x $itk_component(arb5XL) \
	$itk_component(arb5YL) \
	$itk_component(arb5ZL)
    incr row
    set col 0
    grid $itk_component(arb5V1L) \
	-row $row \
	-column $col \
	-sticky ne
    incr col
    grid $itk_component(arb5V1xE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb5V1yE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb5V1zE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb5V1UnitsL) \
	-row $row \
	-column $col \
	-sticky nsew
    incr row
    set col 0
    grid $itk_component(arb5V2L) \
	-row $row \
	-column $col \
	-sticky ne
    incr col
    grid $itk_component(arb5V2xE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb5V2yE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb5V2zE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb5V2UnitsL) \
	-row $row \
	-column $col \
	-sticky nsew
    incr row
    set col 0
    grid $itk_component(arb5V3L) \
	-row $row \
	-column $col \
	-sticky ne
    incr col
    grid $itk_component(arb5V3xE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb5V3yE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb5V3zE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb5V3UnitsL) \
	-row $row \
	-column $col \
	-sticky nsew
    incr row
    set col 0
    grid $itk_component(arb5V4L) \
	-row $row \
	-column $col \
	-sticky ne
    incr col
    grid $itk_component(arb5V4xE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb5V4yE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb5V4zE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb5V4UnitsL) \
	-row $row \
	-column $col \
	-sticky nsew
    incr row
    set col 0
    grid $itk_component(arb5V5L) \
	-row $row \
	-column $col \
	-sticky ne
    incr col
    grid $itk_component(arb5V5xE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb5V5yE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb5V5zE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb5V5UnitsL) \
	-row $row \
	-column $col \
	-sticky nsew
    grid columnconfigure $parent 1 -weight 1
    grid columnconfigure $parent 2 -weight 1
    grid columnconfigure $parent 3 -weight 1
    pack $parent -expand yes -fill x -anchor n

    # Set up bindings
    bind $itk_component(arb5V1xE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb5V1yE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb5V1zE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb5V2xE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb5V2yE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb5V2zE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb5V3xE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb5V3yE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb5V3zE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb5V4xE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb5V4yE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb5V4zE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb5V5xE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb5V5yE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb5V5zE) <Return> [::itcl::code $this updateGeometryIfMod]
}

::itcl::body Arb5EditFrame::buildMoveEdgePanel {parent} {
    foreach edge {12 23 34 14 15 25 35 45} {
	itk_component add moveEdge$edge {
	    ::ttk::radiobutton $parent.me$edge \
		-variable [::itcl::scope mEditMode] \
		-value [subst $[subst moveEdge$edge]] \
		-text "Move edge $edge" \
		-command [::itcl::code $this initValuePanel]
	} {}

	pack $itk_component(moveEdge$edge) \
	    -anchor w \
	    -expand yes
    }

    foreach point {5} {
	itk_component add movePoint$point {
	    ::ttk::radiobutton $parent.me$point \
		-variable [::itcl::scope mEditMode] \
		-value [subst $[subst movePoint$point]] \
		-text "Move point $point" \
		-command [::itcl::code $this initValuePanel]
	} {}

	pack $itk_component(movePoint$point) \
	    -anchor w \
	    -expand yes
    }
}

::itcl::body Arb5EditFrame::buildMoveFacePanel {parent} {
    foreach face {1234 125 235 345 145} {
	itk_component add moveFace$face {
	    ::ttk::radiobutton $parent.mf$face \
		-variable [::itcl::scope mEditMode] \
		-value [subst $[subst moveFace$face]] \
		-text "Move face $face" \
		-command [::itcl::code $this initValuePanel]
	} {}

	pack $itk_component(moveFace$face) \
	    -anchor w \
	    -expand yes
    }
}

::itcl::body Arb5EditFrame::buildRotateFacePanel {parent} {
    foreach face {1234 125 235 345 145} {
	itk_component add rotateFace$face {
	    ::ttk::radiobutton $parent.rf$face \
		-variable [::itcl::scope mEditMode] \
		-value [subst $[subst rotateFace$face]] \
		-text "Rotate face $face" \
		-command [::itcl::code $this initValuePanel]
	} {}

	pack $itk_component(rotateFace$face) \
	    -anchor w \
	    -expand yes
    }
}


# ------------------------------------------------------------
#                        OPTIONS
# ------------------------------------------------------------


# ------------------------------------------------------------
#                      PUBLIC METHODS
# ------------------------------------------------------------

::itcl::body Arb5EditFrame::updateGeometry {} {
    if {$itk_option(-mged) == "" ||
	$itk_option(-geometryObject) == ""} {
	return
    }

    $itk_option(-mged) adjust $itk_option(-geometryObject) \
	V1 [list $mV1x $mV1y $mV1z] \
	V2 [list $mV2x $mV2y $mV2z] \
	V3 [list $mV3x $mV3y $mV3z] \
	V4 [list $mV4x $mV4y $mV4z] \
	V5 [list $mV5x $mV5y $mV5z] \
	V6 [list $mV5x $mV5y $mV5z] \
	V7 [list $mV5x $mV5y $mV5z] \
	V8 [list $mV5x $mV5y $mV5z]

    # update unseen (by the user via the upper panel) variables
    set mV6x $mV5x
    set mV6y $mV5y
    set mV6z $mV5z
    set mV7x $mV5x
    set mV7y $mV5y
    set mV7z $mV5z
    set mV8x $mV5x
    set mV8y $mV5y
    set mV8z $mV5z

    if {$itk_option(-geometryChangedCallback) != ""} {
	$itk_option(-geometryChangedCallback)
    }
}

::itcl::body Arb5EditFrame::createGeometry {obj} {
    if {![GeometryEditFrame::createGeometry $obj]} {
	return
    }

    set _x [expr {($mXmin + $mXmax) * 0.5}]
    set _z [expr {($mZmin + $mZmax) * 0.5}]
    $itk_option(-mged) put $obj arb8 \
	V1 [list $mXmin $mYmin $mZmin] \
	V2 [list $mXmax $mYmin $mZmin] \
	V3 [list $mXmax $mYmin $mZmax] \
	V4 [list $mXmin $mYmin $mZmax] \
	V5 [list $_x $mYmax $_z] \
	V6 [list $_x $mYmax $_z] \
	V7 [list $_x $mYmax $_z] \
	V8 [list $_x $mYmax $_z]
}

::itcl::body Arb5EditFrame::moveEdge {edge} {
    switch -- $edge {
	"12" {
	    set edgeIndex 1
	}
	"23" {
	    set edgeIndex 2
	}
	"34" {
	    set edgeIndex 3
	}
	"14" {
	    set edgeIndex 4
	}
	"15" {
	    set edgeIndex 5
	}
	"25" {
	    set edgeIndex 6
	}
	"35" {
	    set edgeIndex 7
	}
	"45" {
	    set edgeIndex 8
	}
	"5" {
	    set edgeIndex 9
	}
    }

    $itk_option(-mged) move_arb_edge \
	$itk_option(-geometryObjectPath) \
	$edgeIndex \
	[list $mValueX $mValueY $mValueZ]

    initGeometry [lrange [$itk_option(-mged) get $itk_option(-geometryObject)] 1 end]

    if {$itk_option(-geometryChangedCallback) != ""} {
	$itk_option(-geometryChangedCallback)
    }
}

::itcl::body Arb5EditFrame::moveFace {face} {
    switch -- $face {
	"1234" {
	    set faceIndex 1
	}
	"125" {
	    set faceIndex 2
	}
	"235" {
	    set faceIndex 3
	}
	"345" {
	    set faceIndex 4
	}
	"145" {
	    set faceIndex 5
	}
    }

    $itk_option(-mged) move_arb_face \
	$itk_option(-geometryObject) \
	$faceIndex \
	[list $mValueX $mValueY $mValueZ]

    initGeometry [lrange [$itk_option(-mged) get $itk_option(-geometryObject)] 1 end]

    if {$itk_option(-geometryChangedCallback) != ""} {
	$itk_option(-geometryChangedCallback)
    }
}

::itcl::body Arb5EditFrame::rotateFace {face} {
    switch -- $face {
	"1234" {
	    set faceIndex 1
	}
	"125" {
	    set faceIndex 2
	}
	"235" {
	    set faceIndex 3
	}
	"345" {
	    set faceIndex 4
	}
	"145" {
	    set faceIndex 5
	}
    }

    $itk_option(-mged) rotate_arb_face \
	$itk_option(-geometryObject) \
	$faceIndex \
	$mEditParam2 \
	[list $mValueX $mValueY $mValueZ]

    initGeometry [lrange [$itk_option(-mged) get $itk_option(-geometryObject)] 1 end]

    if {$itk_option(-geometryChangedCallback) != ""} {
	$itk_option(-geometryChangedCallback)
    }
}


# ------------------------------------------------------------
#                      PROTECTED METHODS
# ------------------------------------------------------------

::itcl::body Arb5EditFrame::updateUpperPanel {normal disabled} {
    foreach rb $normal {
	$itk_component(arb5V$rb\L) configure -state normal
    }

    foreach rb $disabled {
	$itk_component(arb5V$rb\L) configure -state disabled
    }
}

::itcl::body Arb5EditFrame::updateValuePanel {} {
    switch -- $mEditMode \
	$moveEdge12 { \
			  set mValueX $mV1x; \
			  set mValueY $mV1y; \
			  set mValueZ $mV1z \
		      } \
	$moveEdge23 { \
			  set mValueX $mV2x; \
			  set mValueY $mV2y; \
			  set mValueZ $mV2z \
		      } \
	$moveEdge34 { \
			  set mValueX $mV3x; \
			  set mValueY $mV3y; \
			  set mValueZ $mV3z \
		      } \
	$moveEdge14 { \
			  set mValueX $mV1x; \
			  set mValueY $mV1y; \
			  set mValueZ $mV1z \
		      } \
	$moveEdge15 { \
			  set mValueX $mV1x; \
			  set mValueY $mV1y; \
			  set mValueZ $mV1z \
		      } \
	$moveEdge25 { \
			  set mValueX $mV2x; \
			  set mValueY $mV2y; \
			  set mValueZ $mV2z \
		      } \
	$moveEdge35 { \
			  set mValueX $mV3x; \
			  set mValueY $mV3y; \
			  set mValueZ $mV3z \
		      } \
	$moveEdge45 { \
			  set mValueX $mV4x; \
			  set mValueY $mV4y; \
			  set mValueZ $mV4z \
		      } \
	$movePoint5 { \
			  set mValueX $mV5x; \
			  set mValueY $mV5y; \
			  set mValueZ $mV5z \
		      } \
	$moveFace1234 { \
			    set mValueX $mV1x; \
			    set mValueY $mV1y; \
			    set mValueZ $mV1z \
			} \
	$moveFace125 { \
			   set mValueX $mV1x; \
			   set mValueY $mV1y; \
			   set mValueZ $mV1z \
		       } \
	$moveFace235 { \
			   set mValueX $mV2x; \
			   set mValueY $mV2y; \
			   set mValueZ $mV2z \
		       } \
	$moveFace345 { \
			   set mValueX $mV3x; \
			   set mValueY $mV3y; \
			   set mValueZ $mV3z \
		       } \
	$moveFace145 { \
			   set mValueX $mV1x; \
			   set mValueY $mV1y; \
			   set mValueZ $mV1z \
		       } \
	$rotateFace1234 { \
			      set mValueX 0; \
			      set mValueY 0; \
			      set mValueZ 0 \
			  } \
	$rotateFace125 { \
			     set mValueX 0; \
			     set mValueY 0; \
			     set mValueZ 0 \
			 } \
	$rotateFace235 { \
			     set mValueX 0; \
			     set mValueY 0; \
			     set mValueZ 0 \
			 } \
	$rotateFace345 { \
			     set mValueX 0; \
			     set mValueY 0; \
			     set mValueZ 0 \
			 } \
	$rotateFace145 { \
			     set mValueX 0; \
			     set mValueY 0; \
			     set mValueZ 0 \
			 }
}

::itcl::body Arb5EditFrame::initValuePanel {} {
    switch -- $mEditMode \
	$moveEdge12 { \
			  set mEditCommand move_arb_edge; \
			  set mEditClass $EDIT_CLASS_TRANS; \
			  set mEditParam1 1; \
			  configure -valueUnits "mm"; \
			  updateUpperPanel {} {1 2 3 4 5} \
		      } \
	$moveEdge23 { \
			  set mEditCommand move_arb_edge; \
			  set mEditClass $EDIT_CLASS_TRANS; \
			  set mEditParam1 2; \
			  configure -valueUnits "mm"; \
			  updateUpperPanel {} {1 2 3 4 5} \
		      } \
	$moveEdge34 { \
			  set mEditCommand move_arb_edge; \
			  set mEditClass $EDIT_CLASS_TRANS; \
			  set mEditParam1 3; \
			  configure -valueUnits "mm"; \
			  updateUpperPanel {} {1 2 3 4 5} \
		      } \
	$moveEdge14 { \
			  set mEditCommand move_arb_edge; \
			  set mEditClass $EDIT_CLASS_TRANS; \
			  set mEditParam1 4; \
			  configure -valueUnits "mm"; \
			  updateUpperPanel {} {1 2 3 4 5} \
		      } \
	$moveEdge15 { \
			  set mEditCommand move_arb_edge; \
			  set mEditClass $EDIT_CLASS_TRANS; \
			  set mEditParam1 5; \
			  configure -valueUnits "mm"; \
			  updateUpperPanel {} {1 2 3 4 5} \
		      } \
	$moveEdge25 { \
			  set mEditCommand move_arb_edge; \
			  set mEditClass $EDIT_CLASS_TRANS; \
			  set mEditParam1 6; \
			  configure -valueUnits "mm"; \
			  updateUpperPanel {} {1 2 3 4 5} \
		      } \
	$moveEdge35 { \
			  set mEditCommand move_arb_edge; \
			  set mEditClass $EDIT_CLASS_TRANS; \
			  set mEditParam1 7; \
			  configure -valueUnits "mm"; \
			  updateUpperPanel {} {1 2 3 4 5} \
		      } \
	$moveEdge45 { \
			  set mEditCommand move_arb_edge; \
			  set mEditClass $EDIT_CLASS_TRANS; \
			  set mEditParam1 8; \
			  configure -valueUnits "mm"; \
			  updateUpperPanel {} {1 2 3 4 5} \
		      } \
	$movePoint5 { \
			  set mEditCommand move_arb_edge; \
			  set mEditClass $EDIT_CLASS_TRANS; \
			  set mEditParam1 9; \
			  configure -valueUnits "mm"; \
			  updateUpperPanel {} {1 2 3 4 5} \
		      } \
	$moveFace1234 { \
			    set mEditCommand move_arb_face; \
			    set mEditClass $EDIT_CLASS_TRANS; \
			    set mEditParam1 1; \
			    configure -valueUnits "mm"; \
			    updateUpperPanel {} {1 2 3 4 5} \
			} \
	$moveFace125 { \
			   set mEditCommand move_arb_face; \
			   set mEditClass $EDIT_CLASS_TRANS; \
			   set mEditParam1 2; \
			   configure -valueUnits "mm"; \
			   updateUpperPanel {} {1 2 3 4 5} \
		       } \
	$moveFace235 { \
			   set mEditCommand move_arb_face; \
			   set mEditClass $EDIT_CLASS_TRANS; \
			   set mEditParam1 3; \
			   configure -valueUnits "mm"; \
			   updateUpperPanel {} {1 2 3 4 5} \
		       } \
	$moveFace345 { \
			   set mEditCommand move_arb_face; \
			   set mEditClass $EDIT_CLASS_TRANS; \
			   set mEditParam1 4; \
			   configure -valueUnits "mm"; \
			   updateUpperPanel {} {1 2 3 4 5} \
		       } \
	$moveFace145 { \
			   set mEditCommand move_arb_face; \
			   set mEditClass $EDIT_CLASS_TRANS; \
			   set mEditParam1 5; \
			   configure -valueUnits "mm"; \
			   updateUpperPanel {} {1 2 3 4 5} \
		       } \
	$rotateFace1234 { \
			      set mEditCommand rotate_arb_face; \
			      set mEditClass $EDIT_CLASS_ROT; \
			      set mEditParam1 1; \
			      set mEditParam2 1; \
			      configure -valueUnits "deg"; \
			      updateUpperPanel {1 2 3 4} {5} \
			  } \
	$rotateFace125 { \
			     set mEditCommand rotate_arb_face; \
			     set mEditClass $EDIT_CLASS_ROT; \
			     set mEditParam1 2; \
			     set mEditParam2 1; \
			     configure -valueUnits "deg"; \
			     updateUpperPanel {1 2 5} {3 4} \
			 } \
	$rotateFace235 { \
			     set mEditCommand rotate_arb_face; \
			     set mEditClass $EDIT_CLASS_ROT; \
			     set mEditParam1 3; \
			     set mEditParam2 2; \
			     configure -valueUnits "deg"; \
			     updateUpperPanel {2 3 5} {1 4} \
			 } \
	$rotateFace345 { \
			     set mEditCommand rotate_arb_face; \
			     set mEditClass $EDIT_CLASS_ROT; \
			     set mEditParam1 4; \
			     set mEditParam2 3; \
			     configure -valueUnits "deg"; \
			     updateUpperPanel {3 4 5} {1 2} \
			 } \
	$rotateFace145 { \
			     set mEditCommand rotate_arb_face; \
			     set mEditClass $EDIT_CLASS_ROT; \
			     set mEditParam1 5; \
			     set mEditParam2 1; \
			     configure -valueUnits "deg"; \
			     updateUpperPanel {1 4 5} {2 3} \
			 }

    GeometryEditFrame::initValuePanel
    updateValuePanel
}

::itcl::body Arb5EditFrame::editGeometry {} {
    switch -- $mEditMode \
	$moveEdge12 {moveEdge 12} \
	$moveEdge23 {moveEdge 23} \
	$moveEdge34 {moveEdge 34} \
	$moveEdge14 {moveEdge 14} \
	$moveEdge15 {moveEdge 15} \
	$moveEdge25 {moveEdge 25} \
	$moveEdge35 {moveEdge 35} \
	$moveEdge45 {moveEdge 45} \
	$movePoint5 {moveEdge 5} \
	$moveFace1234 {moveFace 1234} \
	$moveFace125 {moveFace 125} \
	$moveFace235 {moveFace 235} \
	$moveFace345 {moveFace 345} \
	$moveFace145 {moveFace 145} \
	$rotateFace1234 {rotateFace 1234} \
	$rotateFace125 {rotateFace 125} \
	$rotateFace235 {rotateFace 235} \
	$rotateFace345 {rotateFace 345} \
	$rotateFace145 {rotateFace 145}
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
