##
# Portions Copyright (c) 2002 SURVICE Engineering Company. All Rights Reserved.
# This file contains Original Code and/or Modifications of Original Code as
# defined in and that are subject to the SURVICE Public Source License
# (Version 1.3, dated March 12, 2002).
#
# TYPE: tcltk
##############################################################
#
# GeometryEditFrame.tcl
#
##############################################################
#
# Author(s):
#    Bob Parker
#
# Description:
#    The super class for editing geometry within Archer.
#
##############################################################

::itcl::class GeometryEditFrame {
    inherit ::itk::Widget

    itk_option define -mged mged Mged ""
    itk_option define -geometryObject geometryObject GeometryObject ""
    itk_option define -geometryChangedCallback geometryChangedCallback GeometryChangedCallback ""

    itk_option define -labelFont labelFont Font [list $::Archer::SystemWindowFont 12]
    itk_option define -boldLabelFont boldLabelFont Font [list $::Archer::SystemWindowFont 12 bold]
    itk_option define -entryFont entryFont Font [list $::Archer::SystemWindowFont 12]
    itk_option define -units units Units ""
    itk_option define -valueUnits valueUnits ValueUnits ""

    constructor {args} {}
    destructor {}

    public {
	proc validateDigit {d}
	proc validateDigitMax100 {d}
	proc validateDouble {d}
	proc validateColorComp {c}
	proc validateColor {color}

	method childsite {{site upper}}

	method initGeometry {gdata}
	method updateGeometry {}
	method createGeometry {obj}
    }

    protected {
	variable mEditMode 0
	variable mScaleFactor 0.25
	variable mSize -1
	variable mDelta -1
	variable mCenterX 0
	variable mCenterY 0
	variable mCenterZ 0
	variable mXmin 0
	variable mXmax 0
	variable mYmin 0
	variable mYmax 0
	variable mZmin 0
	variable mZmax 0

	method buildUpperPanel {}
	method buildLowerPanel {}
	method buildValuePanel {}

	method updateUpperPanel {normal disabled}
	method updateValuePanel {}
	method updateGeometryIfMod {}

	method initValuePanel {}

	method buildComboBox {parent name1 name2 varName text listOfChoices}
	method buildArrow {parent prefix text buildViewFunc}
	method toggleArrow {arrow view args}
	method getView {}
    }

    private {
    }
}


# ------------------------------------------------------------
#                      CONSTRUCTOR
# ------------------------------------------------------------

::itcl::body GeometryEditFrame::constructor {args} {
    itk_component add pane {
	::iwidgets::Panedwindow $itk_interior.pane \
	    -orient horizontal \
	    -thickness 5 \
	    -sashborderwidth 1 \
	    -sashcursor sb_v_double_arrow \
	    -showhandle 0
    } {}

    $itk_component(pane) add upper
    $itk_component(pane) add lower

    if {1} {
	set parent [$itk_component(pane) childsite upper]
	itk_component add upper {
	    ::frame $parent.upper
	} {}

	set parent [$itk_component(pane) childsite lower]
	itk_component add lower {
	    ::frame $parent.lower
	} {}
    } else {
	set parent [$itk_component(pane) childsite upper]
	itk_component add upper {
	    ::iwidgets::Scrolledframe $parent.upper
	} {
	    keep -vscrollmode
	    keep -hscrollmode
	}

	set parent [$itk_component(pane) childsite lower]
	itk_component add lower {
	    ::iwidgets::Scrolledframe $parent.lower
	} {
	    keep -vscrollmode
	    keep -hscrollmode
	}
    }

    itk_component add valueSeparator {
	::frame $itk_interior.valueSeparator \
	    -height 2 \
	    -relief raised \
	    -borderwidth 1
    } {}
    itk_component add value {
	::frame $itk_interior.value
    } {}
    itk_component add valueL {
	::label $itk_component(value).valueL \
	    -text "Enter a value:" \
	    -anchor e
    } {}
    itk_component add valueCS {
	::frame $itk_component(value).valueCS
    } {}
    itk_component add valueUnitsL {
	::label $itk_component(value).valueUnitsL \
	    -textvariable [::itcl::scope itk_option(-valueUnits)] \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }

    # These are no-ops unless overridden in a subclass
    buildUpperPanel
    buildLowerPanel
    buildValuePanel

    pack $itk_component(upper) -expand yes -fill both
    pack $itk_component(lower) -expand yes -fill both
    pack $itk_component(pane) -expand yes -fill both

    pack $itk_component(valueL) \
	-side left \
	-anchor e
    pack $itk_component(valueUnitsL) \
	-side right
    pack $itk_component(valueCS) \
	-side right \
	-expand yes \
	-fill x
    pack $itk_component(value) \
	-side bottom \
	-anchor w \
	-fill x \
	-padx 2 \
	-pady 2
    pack $itk_component(valueSeparator) \
	-side bottom \
	-anchor w \
	-fill x

    eval itk_initialize $args
}


# ------------------------------------------------------------
#                        OPTIONS
# ------------------------------------------------------------

::itcl::configbody GeometryEditFrame::mged {
    # Nothing for now
}

::itcl::configbody GeometryEditFrame::geometryObject {
    # Nothing for now
}

::itcl::configbody GeometryEditFrame::geometryChangedCallback {
    # Nothing for now
}

::itcl::configbody GeometryEditFrame::labelFont {
    # Nothing for now
}

::itcl::configbody GeometryEditFrame::boldLabelFont {
    # Nothing for now
}

::itcl::configbody GeometryEditFrame::entryFont {
    # Nothing for now
}


# ------------------------------------------------------------
#                      PUBLIC CLASS METHODS
# ------------------------------------------------------------

::itcl::body GeometryEditFrame::validateDigit {d} {
    if {[string is digit $d]} {
	return 1
    }

    return 0
}

::itcl::body GeometryEditFrame::validateDigitMax100 {d} {
    if {![GeometryEditFrame::validateDigit $d]} {
	return 0
    }

    if {$d <= 100} {
	return 1
    }

    return 0
}

::itcl::body GeometryEditFrame::validateDouble {d} {
    if {$d == "-" || [string is double $d]} {
	return 1
    }

    return 0
}

::itcl::body GeometryEditFrame::validateColorComp {c} {
    if {$c == ""} {
	return 1
    }

    if {[string is digit $c]} {
	if {$c <= 255} {
	    return 1
	}

	return 0
    } else {
	return 0
    }
}

::itcl::body GeometryEditFrame::validateColor {color} {
    if {[llength $color] != 3} {
	return 0
    }

    foreach c $color {
	if {![GeometryEditFrame::validateColorComp $c]} {
	    return 0
	}
    }

    return 1
}


# ------------------------------------------------------------
#                      PUBLIC METHODS
# ------------------------------------------------------------

::itcl::body GeometryEditFrame::childsite {{site upper}} {
    switch -- $site {
	"value" {
	    return $itk_component(valueCS)
	}
	"lower" {
	    if {1} {
		return $itk_component(lower)
	    } else {
		return [$itk_component(lower) childsite]
	    }
	}
	default -
	"upper" {
	    if {1} {
		return $itk_component(upper)
	    } else {
		return [$itk_component(upper) childsite]
	    }
	}
    }
}

## - initGeometry
#
# After initializing the variables containing the object's
# specification, the child classes should call this method
# to insure that the scrollbars get updated.
#
::itcl::body GeometryEditFrame::initGeometry {gdata} {
    # The scrollmode options are needed so that the
    # scrollbars dynamically appear/disappear. Sheesh!
    update
    #after idle $this configure \
	-vscrollmode dynamic \
	-hscrollmode none

    updateValuePanel
}

::itcl::body GeometryEditFrame::updateGeometry {} {
    catch {eval $itk_option(-geometryChangedCallback)}
}

::itcl::body GeometryEditFrame::createGeometry {obj} {
    GeometryEditFrame::getView

    if {$mSize <= 0 ||
	$mDelta <= 0} {
	return 0
    }

    return 1
}


# ------------------------------------------------------------
#                      PROTECTED METHODS
# ------------------------------------------------------------

::itcl::body GeometryEditFrame::buildUpperPanel {} {
}

::itcl::body GeometryEditFrame::buildLowerPanel {} {
}

::itcl::body GeometryEditFrame::buildValuePanel {} {
}

::itcl::body GeometryEditFrame::updateUpperPanel {normal disabled} {
}

::itcl::body GeometryEditFrame::updateValuePanel {} {
}

::itcl::body GeometryEditFrame::updateGeometryIfMod {} {
}

::itcl::body GeometryEditFrame::initValuePanel {} {
}

::itcl::body GeometryEditFrame::buildComboBox {parent name1 name2 var text listOfChoices} {
    itk_component add $name1\L {
	::label $parent.$name2\L \
	    -text $text
    } {}

    set hbc [$itk_component($name1\L) cget -background]

    itk_component add $name1\F {
	::frame $parent.$name2\F \
	    -relief sunken \
	    -bd 2
    } {}

    set listHeight [expr [llength $listOfChoices] * 19]
    itk_component add $name1\CB {
	::iwidgets::combobox $itk_component($name1\F).$name2\CB \
	    -editable false \
	    -textvariable $var \
	    -listheight $listHeight \
	    -background $::Archer::SystemWindow \
	    -textbackground $::Archer::SystemWindow \
	    -relief flat
    } {}
    $itk_component($name1\CB) component entry configure \
	-disabledbackground $::Archer::SystemWindow \
	-disabledforeground $::Archer::SystemWindowText
    eval $itk_component($name1\CB) insert list end $listOfChoices

    $itk_component($name1\CB) component arrowBtn configure \
	-background $hbc \
	-highlightbackground $hbc
    pack $itk_component($name1\CB) -expand yes -fill both
}

::itcl::body GeometryEditFrame::buildArrow {parent prefix text buildViewFunc} {
    itk_component add $prefix {
	frame $parent.$prefix
    } {
	usual
    }
    itk_component add $prefix\Arrow {
	::swidgets::togglearrow $itk_component($prefix).arrow
    } {
#	usual
    }
    itk_component add $prefix\Label {
	label $itk_component($prefix).label -text $text \
		-anchor w
    } {
	usual
    }
    itk_component add $prefix\View {
	frame $itk_component($prefix).$prefix\View
    } {
	usual
    }
    $buildViewFunc $itk_component($prefix\View)
    grid $itk_component($prefix\Arrow) -row 0 -column 0 -sticky e
    grid $itk_component($prefix\Label) -row 0 -column 1 -sticky w
    grid columnconfigure $itk_component($prefix) 1 -weight 1
    $itk_component($prefix\Arrow) configure -command [itcl::code $this toggleArrow \
	    $itk_component($prefix\Arrow) $itk_component($prefix\View) -row 1 \
	    -column 1 -sticky nsew]
}

itcl::body GeometryEditFrame::toggleArrow {arrow view args} {
    set state [$arrow cget -togglestate]
    switch -- $state {
	closed {
	    grid forget $view
	}
	open {
	    eval grid $view $args
	}
    }
}

::itcl::body GeometryEditFrame::getView {} {
    if {$itk_option(-mged) == ""} {
	set mSize -1
	set mDelta -1

	return
    }

    set mSize [$itk_option(-mged) size]
    set local2base [$itk_option(-mged) local2base]

    set _center [$itk_option(-mged) center]
    set mCenterX [expr {[lindex $_center 0] * $local2base}]
    set mCenterY [expr {[lindex $_center 1] * $local2base}]
    set mCenterZ [expr {[lindex $_center 2] * $local2base}]

    set mDelta [expr {$mSize * $mScaleFactor * $local2base}]

    set mXmin [expr {$mCenterX - $mDelta}]
    set mXmax [expr {$mCenterX + $mDelta}]
    set mYmin [expr {$mCenterY - $mDelta}]
    set mYmax [expr {$mCenterY + $mDelta}]
    set mZmin [expr {$mCenterZ - $mDelta}]
    set mZmax [expr {$mCenterZ + $mDelta}]
}
