##
# Portions Copyright (c) 2002 SURVICE Engineering Company. All Rights Reserved.
# This file contains Original Code and/or Modifications of Original Code as
# defined in and that are subject to the SURVICE Public Source License
# (Version 1.3, dated March 12, 2002).
#
# TYPE: tcltk
##############################################################
#
# Arb6EditFrame.tcl
#
##############################################################
#
# Author(s):
#    Bob Parker
#
# Description:
#    The class for editing arb6s within Archer.
#
##############################################################

::itcl::class Arb6EditFrame {
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
	variable moveEdge36 7
	variable moveEdge46 8
	variable movePoint5 9
	variable movePoint6 10
	variable moveFace1234 11
	variable moveFace2365 12
	variable moveFace1564 13
	variable moveFace125 14
	variable moveFace346 15
	variable rotateFace1234 16
	variable rotateFace2365 17
	variable rotateFace1564 18
	variable rotateFace125 19
	variable rotateFace346 20

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

::itcl::body Arb6EditFrame::constructor {args} {
    eval itk_initialize $args
}

::itcl::body Arb6EditFrame::buildUpperPanel {} {
    set parent [$this childsite upper]
    itk_component add arb6Type {
	::label $parent.arb6type \
	    -text "Arb6:" \
	    -anchor e
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }
    itk_component add arb6Name {
	::label $parent.arb6name \
	    -textvariable [::itcl::scope itk_option(-geometryObject)] \
	    -anchor w
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }

    # Create header labels
    itk_component add arb6XL {
	::label $parent.arb6XL \
	    -text "X"
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }
    itk_component add arb6YL {
	::label $parent.arb6YL \
	    -text "Y"
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }
    itk_component add arb6ZL {
	::label $parent.arb6ZL \
	    -text "Z"
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }

    # create widgets for vertices
    itk_component add arb6V1L {
	::radiobutton $parent.arb6V1L \
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
    $itk_component(arb6V1L) configure \
	-disabledforeground [$itk_component(arb6V1L) cget -foreground] \
	-selectcolor  [$itk_component(arb6V1L) cget -background]
    itk_component add arb6V1xE {
	::entry $parent.arb6V1xE \
	    -textvariable [::itcl::scope mV1x] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb6V1yE {
	::entry $parent.arb6V1yE \
	    -textvariable [::itcl::scope mV1y] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb6V1zE {
	::entry $parent.arb6V1zE \
	    -textvariable [::itcl::scope mV1z] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb6V1UnitsL {
	::label $parent.arb6V1UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add arb6V2L {
	::radiobutton $parent.arb6V2L \
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
    $itk_component(arb6V2L) configure \
	-disabledforeground [$itk_component(arb6V2L) cget -foreground] \
	-selectcolor  [$itk_component(arb6V2L) cget -background]
    itk_component add arb6V2xE {
	::entry $parent.arb6V2xE \
	    -textvariable [::itcl::scope mV2x] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb6V2yE {
	::entry $parent.arb6V2yE \
	    -textvariable [::itcl::scope mV2y] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb6V2zE {
	::entry $parent.arb6V2zE \
	    -textvariable [::itcl::scope mV2z] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb6V2UnitsL {
	::label $parent.arb6V2UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add arb6V3L {
	::radiobutton $parent.arb6V3L \
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
    $itk_component(arb6V3L) configure \
	-disabledforeground [$itk_component(arb6V3L) cget -foreground] \
	-selectcolor  [$itk_component(arb6V3L) cget -background]
    itk_component add arb6V3xE {
	::entry $parent.arb6V3xE \
	    -textvariable [::itcl::scope mV3x] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb6V3yE {
	::entry $parent.arb6V3yE \
	    -textvariable [::itcl::scope mV3y] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb6V3zE {
	::entry $parent.arb6V3zE \
	    -textvariable [::itcl::scope mV3z] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb6V3UnitsL {
	::label $parent.arb6V3UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add arb6V4L {
	::radiobutton $parent.arb6V4L \
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
    $itk_component(arb6V4L) configure \
	-disabledforeground [$itk_component(arb6V4L) cget -foreground] \
	-selectcolor  [$itk_component(arb6V4L) cget -background]
    itk_component add arb6V4xE {
	::entry $parent.arb6V4xE \
	    -textvariable [::itcl::scope mV4x] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb6V4yE {
	::entry $parent.arb6V4yE \
	    -textvariable [::itcl::scope mV4y] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb6V4zE {
	::entry $parent.arb6V4zE \
	    -textvariable [::itcl::scope mV4z] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb6V4UnitsL {
	::label $parent.arb6V4UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add arb6V5L {
	::radiobutton $parent.arb6V5L \
	    -text "V5:" \
	    -indicatoron 0 \
	    -variable [::itcl::scope mVIndex] \
	    -value 5 \
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
    $itk_component(arb6V5L) configure \
	-disabledforeground [$itk_component(arb6V5L) cget -foreground] \
	-selectcolor  [$itk_component(arb6V5L) cget -background]
    itk_component add arb6V5xE {
	::entry $parent.arb6V5xE \
	    -textvariable [::itcl::scope mV5x] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb6V5yE {
	::entry $parent.arb6V5yE \
	    -textvariable [::itcl::scope mV5y] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb6V5zE {
	::entry $parent.arb6V5zE \
	    -textvariable [::itcl::scope mV5z] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb6V5UnitsL {
	::label $parent.arb6V5UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add arb6V6L {
	::radiobutton $parent.arb6V6L \
	    -text "V6:" \
	    -indicatoron 0 \
	    -variable [::itcl::scope mVIndex] \
	    -value 6 \
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
    $itk_component(arb6V6L) configure \
	-disabledforeground [$itk_component(arb6V6L) cget -foreground] \
	-selectcolor  [$itk_component(arb6V6L) cget -background]
    itk_component add arb6V6xE {
	::entry $parent.arb6V6xE \
	    -textvariable [::itcl::scope mV7x] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb6V6yE {
	::entry $parent.arb6V6yE \
	    -textvariable [::itcl::scope mV7y] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb6V6zE {
	::entry $parent.arb6V6zE \
	    -textvariable [::itcl::scope mV7z] \
	    -state disabled \
	    -disabledforeground black \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDouble %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add arb6V6UnitsL {
	::label $parent.arb6V6UnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }

    set row 0
    grid $itk_component(arb6Type) \
	-row $row \
	-column 0 \
	-sticky nsew
    grid $itk_component(arb6Name) \
	-row $row \
	-column 1 \
	-columnspan 3 \
	-sticky nsew
    incr row
    grid x $itk_component(arb6XL) \
	$itk_component(arb6YL) \
	$itk_component(arb6ZL)
    incr row
    set col 0
    grid $itk_component(arb6V1L) \
	-row $row \
	-column $col \
	-sticky ne
    incr col
    grid $itk_component(arb6V1xE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb6V1yE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb6V1zE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb6V1UnitsL) \
	-row $row \
	-column $col \
	-sticky nsew
    incr row
    set col 0
    grid $itk_component(arb6V2L) \
	-row $row \
	-column $col \
	-sticky ne
    incr col
    grid $itk_component(arb6V2xE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb6V2yE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb6V2zE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb6V2UnitsL) \
	-row $row \
	-column $col \
	-sticky nsew
    incr row
    set col 0
    grid $itk_component(arb6V3L) \
	-row $row \
	-column $col \
	-sticky ne
    incr col
    grid $itk_component(arb6V3xE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb6V3yE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb6V3zE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb6V3UnitsL) \
	-row $row \
	-column $col \
	-sticky nsew
    incr row
    set col 0
    grid $itk_component(arb6V4L) \
	-row $row \
	-column $col \
	-sticky ne
    incr col
    grid $itk_component(arb6V4xE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb6V4yE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb6V4zE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb6V4UnitsL) \
	-row $row \
	-column $col \
	-sticky nsew
    incr row
    set col 0
    grid $itk_component(arb6V5L) \
	-row $row \
	-column $col \
	-sticky ne
    incr col
    grid $itk_component(arb6V5xE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb6V5yE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb6V5zE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb6V5UnitsL) \
	-row $row \
	-column $col \
	-sticky nsew
    incr row
    set col 0
    grid $itk_component(arb6V6L) \
	-row $row \
	-column $col \
	-sticky ne
    incr col
    grid $itk_component(arb6V6xE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb6V6yE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb6V6zE) \
	-row $row \
	-column $col \
	-sticky nsew
    incr col
    grid $itk_component(arb6V6UnitsL) \
	-row $row \
	-column $col \
	-sticky nsew
    grid columnconfigure $parent 1 -weight 1
    grid columnconfigure $parent 2 -weight 1
    grid columnconfigure $parent 3 -weight 1
    pack $parent -expand yes -fill x -anchor n

    # Set up bindings
    bind $itk_component(arb6V1xE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb6V1yE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb6V1zE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb6V2xE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb6V2yE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb6V2zE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb6V3xE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb6V3yE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb6V3zE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb6V4xE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb6V4yE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb6V4zE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb6V5xE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb6V5yE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb6V5zE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb6V6xE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb6V6yE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(arb6V6zE) <Return> [::itcl::code $this updateGeometryIfMod]
}

::itcl::body Arb6EditFrame::buildMoveEdgePanel {parent} {
    foreach edge {12 23 34 14 15 25 36 46} {
	itk_component add moveEdge$edge {
	    ::radiobutton $parent.me$edge \
		-variable [::itcl::scope mEditMode] \
		-value [subst $[subst moveEdge$edge]] \
		-text "Move edge $edge" \
		-command [::itcl::code $this updateValuePanel]
	} {}

	pack $itk_component(moveEdge$edge) \
	    -anchor w \
	    -expand yes
    }

    foreach point {5 6} {
	itk_component add movePoint$point {
	    ::radiobutton $parent.me$point \
		-variable [::itcl::scope mEditMode] \
		-value [subst $[subst movePoint$point]] \
		-text "Move point $point" \
		-command [::itcl::code $this updateValuePanel]
	} {}

	pack $itk_component(movePoint$point) \
	    -anchor w \
	    -expand yes
    }
}

::itcl::body Arb6EditFrame::buildMoveFacePanel {parent} {
    foreach face {1234 2365 1564 125 346} {
	itk_component add moveFace$face {
	    ::radiobutton $parent.mf$face \
		-variable [::itcl::scope mEditMode] \
		-value [subst $[subst moveFace$face]] \
		-text "Move face $face" \
		-command [::itcl::code $this updateValuePanel]
	} {}

	pack $itk_component(moveFace$face) \
	    -anchor w \
	    -expand yes
    }
}

::itcl::body Arb6EditFrame::buildRotateFacePanel {parent} {
    foreach face {1234 2365 1564 125 346} {
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

::itcl::body Arb6EditFrame::updateGeometry {} {
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
	V7 [list $mV7x $mV7y $mV7z] \
	V8 [list $mV7x $mV7y $mV7z]

    # update unseen (by the user via the upper panel) variables
    set mV6x $mV5x
    set mV6y $mV5y
    set mV6z $mV5z
    set mV8x $mV7x
    set mV8y $mV7y
    set mV8z $mV7z

    if {$itk_option(-geometryChangedCallback) != ""} {
	$itk_option(-geometryChangedCallback)
    }
}

::itcl::body Arb6EditFrame::createGeometry {obj} {
    if {![GeometryEditFrame::createGeometry $obj]} {
	return
    }

    $itk_option(-mged) put $obj arb8 \
	V1 [list $mXmin $mYmin $mZmin] \
	V2 [list $mXmax $mYmin $mZmin] \
	V3 [list $mXmax $mYmin $mZmax] \
	V4 [list $mXmin $mYmin $mZmax] \
	V5 [list $mXmin $mYmax $mZmin] \
	V6 [list $mXmin $mYmax $mZmin] \
	V7 [list $mXmin $mYmax $mZmax] \
	V8 [list $mXmin $mYmax $mZmax]
}

::itcl::body Arb6EditFrame::moveEdge {edge} {
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
	"36" {
	    set edgeIndex 7
	}
	"46" {
	    set edgeIndex 8
	}
	"5" {
	    set edgeIndex 9
	}
	"6" {
	    set edgeIndex 10
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

::itcl::body Arb6EditFrame::moveFace {face} {
    switch -- $face {
	"1234" {
	    set faceIndex 1
	}
	"2365" {
	    set faceIndex 2
	}
	"1564" {
	    set faceIndex 3
	}
	"125" {
	    set faceIndex 4
	}
	"346" {
	    set faceIndex 5
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

::itcl::body Arb6EditFrame::rotateFace {face} {
    switch -- $face {
	"1234" {
	    set faceIndex 1
	}
	"2365" {
	    set faceIndex 2
	}
	"1564" {
	    set faceIndex 3
	}
	"125" {
	    set faceIndex 4
	}
	"346" {
	    set faceIndex 5
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

::itcl::body Arb6EditFrame::updateUpperPanel {normal disabled} {
    foreach rb $normal {
	$itk_component(arb6V$rb\L) configure -state normal
    }

    foreach rb $disabled {
	$itk_component(arb6V$rb\L) configure -state disabled
    }
}

::itcl::body Arb6EditFrame::updateValuePanel {} {
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
	$moveEdge36 { \
	    set mValueX $mV3x; \
	    set mValueY $mV3y; \
	    set mValueZ $mV3z \
	} \
	$moveEdge46 { \
	    set mValueX $mV4x; \
	    set mValueY $mV4y; \
	    set mValueZ $mV4z \
	} \
	$movePoint5 { \
	    set mValueX $mV5x; \
	    set mValueY $mV5y; \
	    set mValueZ $mV5z \
	} \
	$movePoint6 { \
	    set mValueX $mV7x; \
	    set mValueY $mV7y; \
	    set mValueZ $mV7z \
	} \
	$moveFace1234 { \
	    set mValueX $mV1x; \
	    set mValueY $mV1y; \
	    set mValueZ $mV1z \
	} \
	$moveFace2365 { \
	    set mValueX $mV2x; \
	    set mValueY $mV2y; \
	    set mValueZ $mV2z \
	} \
	$moveFace1564 { \
	    set mValueX $mV1x; \
	    set mValueY $mV1y; \
	    set mValueZ $mV1z \
	} \
	$moveFace125 { \
	    set mValueX $mV1x; \
	    set mValueY $mV1y; \
	    set mValueZ $mV1z \
	} \
	$moveFace346 { \
	    set mValueX $mV3x; \
	    set mValueY $mV3y; \
	    set mValueZ $mV3z \
	} \
	$rotateFace1234 { \
	    set mValueX 0; \
	    set mValueY 0; \
	    set mValueZ 0 \
	} \
	$rotateFace2365 { \
	    set mValueX 0; \
	    set mValueY 0; \
	    set mValueZ 0 \
	} \
	$rotateFace1564 { \
	    set mValueX 0; \
	    set mValueY 0; \
	    set mValueZ 0 \
	} \
	$rotateFace125 { \
	    set mValueX 0; \
	    set mValueY 0; \
	    set mValueZ 0 \
	} \
	$rotateFace346 { \
	    set mValueX 0; \
	    set mValueY 0; \
	    set mValueZ 0 \
	}
}

::itcl::body Arb6EditFrame::initValuePanel {} {
    switch -- $mEditMode \
	$moveEdge12 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6} \
	} \
	$moveEdge23 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6} \
	} \
	$moveEdge34 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6} \
	} \
	$moveEdge14 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6} \
	} \
	$moveEdge15 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6} \
	} \
	$moveEdge25 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6} \
	} \
	$moveEdge36 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6} \
	} \
	$moveEdge46 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6} \
	} \
	$movePoint5 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6} \
	} \
	$movePoint6 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6} \
	} \
	$moveFace1234 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6} \
	} \
	$moveFace2365 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6} \
	} \
	$moveFace1564 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6} \
	} \
	$moveFace125 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6} \
	} \
	$moveFace346 { \
	    set mVIndex 0; \
	    configure -valueUnits "mm"; \
	    updateUpperPanel {} {1 2 3 4 5 6} \
	} \
	$rotateFace1234 { \
	    set mVIndex 1; \
	    configure -valueUnits "deg"; \
	    updateUpperPanel {1 2 3 4} {5 6} \
	} \
	$rotateFace2365 { \
	    set mVIndex 2; \
	    configure -valueUnits "deg"; \
	    updateUpperPanel {2 3 6 5} {1 4} \
	} \
	$rotateFace1564 { \
	    set mVIndex 1; \
	    configure -valueUnits "deg"; \
	    updateUpperPanel {1 5 6 4} {2 3} \
	} \
	$rotateFace125 { \
	    set mVIndex 1; \
	    configure -valueUnits "deg"; \
	    updateUpperPanel {1 2 5} {3 4 6} \
	} \
	$rotateFace346 { \
	    set mVIndex 3; \
	    configure -valueUnits "deg"; \
	    updateUpperPanel {3 4 6} {1 2 5} \
	}

    updateValuePanel
}

::itcl::body Arb6EditFrame::editGeometry {} {
    switch -- $mEditMode \
	$moveEdge12 {moveEdge 12} \
	$moveEdge23 {moveEdge 23} \
	$moveEdge34 {moveEdge 34} \
	$moveEdge14 {moveEdge 14} \
	$moveEdge15 {moveEdge 15} \
	$moveEdge25 {moveEdge 25} \
	$moveEdge36 {moveEdge 36} \
	$moveEdge46 {moveEdge 46} \
	$movePoint5 {moveEdge 5} \
	$movePoint6 {moveEdge 6} \
	$moveFace1234 {moveFace 1234} \
	$moveFace2365 {moveFace 2365} \
	$moveFace1564 {moveFace 1564} \
	$moveFace125 {moveFace 125} \
	$moveFace346 {moveFace 346} \
	$rotateFace1234 {rotateFace 1234} \
	$rotateFace2365 {rotateFace 2365} \
	$rotateFace1564 {rotateFace 1564} \
	$rotateFace125 {rotateFace 125} \
	$rotateFace346 {rotateFace 346}
}
