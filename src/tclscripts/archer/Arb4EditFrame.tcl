#               A R B 4 E D I T F R A M E . T C L
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
#    The class for editing arb4s within Archer.
#
##############################################################

::itcl::class Arb4EditFrame {
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
	variable movePoint1 1
	variable movePoint2 2
	variable movePoint3 3
	variable movePoint4 4
	variable moveFace123 5
	variable moveFace124 6
	variable moveFace234 7
	variable moveFace134 8
	variable rotateFace123 9
	variable rotateFace124 10
	variable rotateFace234 11
	variable rotateFace134 12

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

::itcl::body Arb4EditFrame::constructor {args} {
    eval itk_initialize $args
}

::itcl::body Arb4EditFrame::buildUpperPanel {} {
    set parent [$this childsite upper]
    itk_component add arb4Type {
	::label $parent.arb4type \
	    -text "Arb4:" \
	    -anchor e
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }
    itk_component add arb4Name {
	::label $parent.arb4name \
	    -textvariable [::itcl::scope itk_option(-geometryObject)] \
	    -anchor w
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }

    # Create header labels
    itk_component add arb4XL {
	::label $parent.arb4XL \
	    -text "X"
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }
    itk_component add arb4YL {
	::label $parent.arb4YL \
	    -text "Y"
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }
    itk_component add arb4ZL {
	::label $parent.arb4ZL \
	    -text "Z"
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }

    # create widgets for vertices
    itk_component add arb4V1L {
	::radiobutton $parent.arb4V1L \
	    -text "V1:" \
	    -indicatoron 0 \
	    -variable [::itcl::scope mVIndex] \
	    -value 1 \
	    -relief flat \
	    -overrelief raised \
	    -offrelief flat \
	    -borderwidth 1 \
	    -padx 0 \
	    -highlightthickness 0 \
	    -state disabled \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    $itk_component(arb4V1L) configure \
	-disabledforeground [$itk_component(arb4V1L) cget -foreground] \
	-selectcolor  [$itk_component(arb4V1L) cget -background]
    itk_component add arb4V1xE {
	::entry $parent.arb4V1xE \
	    -textvariable [::itcl::scope mV1x] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb4V1yE {
	::entry $parent.arb4V1yE \
	    -textvariable [::itcl::scope mV1y] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb4V1zE {
	::entry $parent.arb4V1zE \
	    -textvariable [::itcl::scope mV1z] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb4V1UnitsL {
	::label $parent.arb4V1UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add arb4V2L {
	::radiobutton $parent.arb4V2L \
	    -text "V2:" \
	    -indicatoron 0 \
	    -variable [::itcl::scope mVIndex] \
	    -value 2 \
	    -relief flat \
	    -overrelief raised \
	    -offrelief flat \
	    -borderwidth 1 \
	    -padx 0 \
	    -highlightthickness 0 \
	    -state disabled \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    $itk_component(arb4V2L) configure \
	-disabledforeground [$itk_component(arb4V2L) cget -foreground] \
	-selectcolor  [$itk_component(arb4V2L) cget -background]
    itk_component add arb4V2xE {
	::entry $parent.arb4V2xE \
	    -textvariable [::itcl::scope mV2x] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb4V2yE {
	::entry $parent.arb4V2yE \
	    -textvariable [::itcl::scope mV2y] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb4V2zE {
	::entry $parent.arb4V2zE \
	    -textvariable [::itcl::scope mV2z] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb4V2UnitsL {
	::label $parent.arb4V2UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add arb4V3L {
	::radiobutton $parent.arb4V3L \
	    -text "V3:" \
	    -indicatoron 0 \
	    -variable [::itcl::scope mVIndex] \
	    -value 3 \
	    -relief flat \
	    -overrelief raised \
	    -offrelief flat \
	    -borderwidth 1 \
	    -padx 0 \
	    -highlightthickness 0 \
	    -state disabled \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    $itk_component(arb4V3L) configure \
	-disabledforeground [$itk_component(arb4V3L) cget -foreground] \
	-selectcolor  [$itk_component(arb4V3L) cget -background]
    itk_component add arb4V3xE {
	::entry $parent.arb4V3xE \
	    -textvariable [::itcl::scope mV3x] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb4V3yE {
	::entry $parent.arb4V3yE \
	    -textvariable [::itcl::scope mV3y] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb4V3zE {
	::entry $parent.arb4V3zE \
	    -textvariable [::itcl::scope mV3z] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb4V3UnitsL {
	::label $parent.arb4V3UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add arb4V4L {
	::radiobutton $parent.arb4V4L \
	    -text "V4:" \
	    -indicatoron 0 \
	    -variable [::itcl::scope mVIndex] \
	    -value 4 \
	    -relief flat \
	    -overrelief raised \
	    -offrelief flat \
	    -borderwidth 1 \
	    -padx 0 \
	    -highlightthickness 0 \
	    -state disabled \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    $itk_component(arb4V4L) configure \
	-disabledforeground [$itk_component(arb4V4L) cget -foreground] \
	-selectcolor  [$itk_component(arb4V4L) cget -background]
    itk_component add arb4V4xE {
	::entry $parent.arb4V4xE \
	    -textvariable [::itcl::scope mV5x] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb4V4yE {
	::entry $parent.arb4V4yE \
	    -textvariable [::itcl::scope mV5y] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb4V4zE {
	::entry $parent.arb4V4zE \
	    -textvariable [::itcl::scope mV5z] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb4V4UnitsL {
	::label $parent.arb4V4UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }

    set row 0
    grid $itk_component(arb4Type) \
	-row $row \
	-column 0 \
	-sticky nsew
    grid $itk_component(arb4Name) \
	-row $row \
	-column 1 \
	-columnspan 3 \
	-sticky nsew
    incr row
    grid x $itk_component(arb4XL) \
	$itk_component(arb4YL) \
	$itk_component(arb4ZL)
    incr row
    set col 0
    grid $itk_component(arb4V1L) \
	-row $row \
	-column $col \
	-sticky ne
    incr col
    grid $itk_component(arb4V1xE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb4V1yE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb4V1zE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb4V1UnitsL) \
	-row $row \
	-column $col \
	-sticky nsew
    incr row
    set col 0
    grid $itk_component(arb4V2L) \
	-row $row \
	-column $col \
	-sticky ne
    incr col
    grid $itk_component(arb4V2xE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb4V2yE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb4V2zE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb4V2UnitsL) \
	-row $row \
	-column $col \
	-sticky nsew
    incr row
    set col 0
    grid $itk_component(arb4V3L) \
	-row $row \
	-column $col \
	-sticky ne
    incr col
    grid $itk_component(arb4V3xE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb4V3yE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb4V3zE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb4V3UnitsL) \
	-row $row \
	-column $col \
	-sticky nsew
    incr row
    set col 0
    grid $itk_component(arb4V4L) \
	-row $row \
	-column $col \
	-sticky ne
    incr col
    grid $itk_component(arb4V4xE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb4V4yE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb4V4zE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb4V4UnitsL) \
	-row $row \
	-column $col \
	-sticky nsew
    grid columnconfigure $parent 1 -weight 1
    grid columnconfigure $parent 2 -weight 1
    grid columnconfigure $parent 3 -weight 1
    pack $parent -expand yes -fill x -anchor n

    # Set up bindings
    bind $itk_component(arb4V1xE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb4V1yE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb4V1zE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb4V2xE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb4V2yE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb4V2zE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb4V3xE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb4V3yE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb4V3zE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb4V4xE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb4V4yE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb4V4zE) <Return> [::itcl::code $this updateGeometryIfMod]
}

::itcl::body Arb4EditFrame::buildMoveEdgePanel {parent} {
    foreach point {1 2 3 4} {
	itk_component add movePoint$point {
	    ::radiobutton $parent.me$point \
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

::itcl::body Arb4EditFrame::buildMoveFacePanel {parent} {
    foreach face {123 124 234 134} {
	itk_component add moveFace$face {
	    ::radiobutton $parent.mf$face \
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

::itcl::body Arb4EditFrame::buildRotateFacePanel {parent} {
    foreach face {123 124 234 134} {
	itk_component add rotateFace$face {
	    ::radiobutton $parent.rf$face \
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

::itcl::body Arb4EditFrame::updateGeometry {} {
    if {$itk_option(-mged) == "" ||
	$itk_option(-geometryObject) == ""} {
	return
    }

    $itk_option(-mged) adjust $itk_option(-geometryObject) \
	V1 [list $mV1x $mV1y $mV1z] \
	V2 [list $mV2x $mV2y $mV2z] \
	V3 [list $mV3x $mV3y $mV3z] \
	V4 [list $mV1x $mV1y $mV1z] \
	V5 [list $mV5x $mV5y $mV5z] \
	V6 [list $mV5x $mV5y $mV5z] \
	V7 [list $mV5x $mV5y $mV5z] \
	V8 [list $mV5x $mV5y $mV5z]

    # update unseen (by the user via the upper panel) variables
    set mV4x $mV1x
    set mV4y $mV1y
    set mV4z $mV1z
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

::itcl::body Arb4EditFrame::createGeometry {obj} {
    if {![GeometryEditFrame::createGeometry $obj]} {
	return
    }

#    $itk_option(-mged) put $obj arb8 \
	V1 [list $mXmin $mYmin $mZmin] \
	V2 [list $mXmax $mYmin $mZmin] \
	V3 [list $mXmax $mYmin $mZmax] \
	V4 [list $mXmin $mYmin $mZmin] \
	V5 [list $mXmax $mYmax $mZmin] \
	V6 [list $mXmax $mYmax $mZmin] \
	V7 [list $mXmax $mYmax $mZmin] \
	V8 [list $mXmax $mYmax $mZmin]
    $itk_option(-mged) put $obj arb8 \
	V1 [list $mXmin $mYmin $mZmin] \
	V2 [list $mXmax $mYmin $mZmin] \
	V3 [list $mXmax $mYmin $mZmax] \
	V4 [list $mXmin $mYmin $mZmin] \
	V5 [list $mXmax $mYmax $mZmin] \
	V6 [list $mXmax $mYmax $mZmin] \
	V7 [list $mXmax $mYmax $mZmin] \
	V8 [list $mXmax $mYmax $mZmin]
}

::itcl::body Arb4EditFrame::moveEdge {edge} {
    switch -- $edge {
	"1" {
	    set edgeIndex 1
	}
	"2" {
	    set edgeIndex 2
	}
	"3" {
	    set edgeIndex 3
	}
	"4" {
	    set edgeIndex 4
	}
    }

    set gdata [$itk_option(-mged) move_arb_edge \
		   $itk_option(-geometryObject) \
		   $edgeIndex \
		   [list $mValueX $mValueY $mValueZ]]

    eval $itk_option(-mged) adjust \
	$itk_option(-geometryObject) \
	$gdata

    initGeometry $gdata

    if {$itk_option(-geometryChangedCallback) != ""} {
	$itk_option(-geometryChangedCallback)
    }
}

::itcl::body Arb4EditFrame::moveFace {face} {
    switch -- $face {
	"123" {
	    set faceIndex 1
	}
	"124" {
	    set faceIndex 2
	}
	"234" {
	    set faceIndex 3
	}
	"134" {
	    set faceIndex 4
	}
    }

    set gdata [$itk_option(-mged) move_arb_face \
		   $itk_option(-geometryObject) \
		   $faceIndex \
		   [list $mValueX $mValueY $mValueZ]]

    eval $itk_option(-mged) adjust \
	$itk_option(-geometryObject) \
	$gdata

    initGeometry $gdata

    if {$itk_option(-geometryChangedCallback) != ""} {
	$itk_option(-geometryChangedCallback)
    }
}

::itcl::body Arb4EditFrame::rotateFace {face} {
    switch -- $face {
	"123" {
	    set faceIndex 1
	}
	"124" {
	    set faceIndex 2
	}
	"234" {
	    set faceIndex 3
	}
	"134" {
	    set faceIndex 4
	}
    }

    set gdata [$itk_option(-mged) rotate_arb_face \
		   $itk_option(-geometryObject) \
		   $faceIndex \
		   $mVIndex \
		   [list $mValueX $mValueY $mValueZ]]

    eval $itk_option(-mged) adjust \
	$itk_option(-geometryObject) \
	$gdata

    initGeometry $gdata

    if {$itk_option(-geometryChangedCallback) != ""} {
	$itk_option(-geometryChangedCallback)
    }
}


# ------------------------------------------------------------
#                      PROTECTED METHODS
# ------------------------------------------------------------

::itcl::body Arb4EditFrame::updateUpperPanel {normal disabled} {
    foreach rb $normal {
	$itk_component(arb4V$rb\L) configure -state normal
    }

    foreach rb $disabled {
	$itk_component(arb4V$rb\L) configure -state disabled
    }
}

::itcl::body Arb4EditFrame::updateValuePanel {} {
    switch -- $mEditMode \
	$movePoint1 { \
	    set mValueX $mV1x; \
	    set mValueY $mV1y; \
	    set mValueZ $mV1z \
	} \
	$movePoint2 { \
	    set mValueX $mV2x; \
	    set mValueY $mV2y; \
	    set mValueZ $mV2z \
	} \
	$movePoint3 { \
	    set mValueX $mV3x; \
	    set mValueY $mV3y; \
	    set mValueZ $mV3z \
	} \
	$movePoint4 { \
	    set mValueX $mV5x; \
	    set mValueY $mV5y; \
	    set mValueZ $mV5z \
	} \
	$moveFace123 { \
	    set mValueX $mV1x; \
	    set mValueY $mV1y; \
	    set mValueZ $mV1z \
	} \
	$moveFace124 { \
	    set mValueX $mV1x; \
	    set mValueY $mV1y; \
	    set mValueZ $mV1z \
	} \
	$moveFace234 { \
	    set mValueX $mV2x; \
	    set mValueY $mV2y; \
	    set mValueZ $mV2z \
	} \
	$moveFace134 { \
	    set mValueX $mV1x; \
	    set mValueY $mV1y; \
	    set mValueZ $mV1z \
	} \
	$rotateFace123 { \
	    set mValueX 0; \
	    set mValueY 0; \
	    set mValueZ 0 \
	} \
	$rotateFace124 { \
	    set mValueX 0; \
	    set mValueY 0; \
	    set mValueZ 0 \
	} \
	$rotateFace234 { \
	    set mValueX 0; \
	    set mValueY 0; \
	    set mValueZ 0 \
	} \
	$rotateFace134 { \
	    set mValueX 0; \
	    set mValueY 0; \
	    set mValueZ 0 \
	}
}

::itcl::body Arb4EditFrame::initValuePanel {} {
    switch -- $mEditMode \
	$movePoint1 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4} \
	} \
	$movePoint2 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4} \
	} \
	$movePoint3 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4} \
	} \
	$movePoint4 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4} \
	} \
	$moveFace123 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4} \
	} \
	$moveFace124 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4} \
	} \
	$moveFace234 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4} \
	} \
	$moveFace134 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4} \
	} \
	$rotateFace123 { \
	    set mVIndex 1; \
	    configure -valueUnits "deg"; \
	    updateUpperPanel {1 2 3} {4} \
	} \
	$rotateFace124 { \
	    set mVIndex 1; \
	    configure -valueUnits "deg"; \
	    updateUpperPanel {1 2 4} {3} \
	} \
	$rotateFace234 { \
	    set mVIndex 2; \
	    configure -valueUnits "deg"; \
	    updateUpperPanel {2 3 4} {1} \
	} \
	$rotateFace134 { \
	    set mVIndex 1; \
	    configure -valueUnits "deg"; \
	    updateUpperPanel {1 3 4} {2} \
	}

    updateValuePanel
}

::itcl::body Arb4EditFrame::editGeometry {} {
    switch -- $mEditMode \
	$movePoint1 {moveEdge 1} \
	$movePoint2 {moveEdge 2} \
	$movePoint3 {moveEdge 3} \
	$movePoint4 {moveEdge 4} \
	$moveFace123 {moveFace 123} \
	$moveFace124 {moveFace 124} \
	$moveFace234 {moveFace 234} \
	$moveFace134 {moveFace 134} \
	$rotateFace123 {rotateFace 123} \
	$rotateFace124 {rotateFace 124} \
	$rotateFace234 {rotateFace 234} \
	$rotateFace134 {rotateFace 134}
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
