#           G E O M E T R Y E D I T F R A M E . T C L
# BRL-CAD
#
# Copyright (c) 2002-2016 United States Government as represented by
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
#    The super class for editing geometry within Archer.
#
##############################################################

::itcl::class GeometryEditFrame {
    inherit ::itk::Widget

    itk_option define -mged mged Mged ""
    itk_option define -geometryObject geometryObject GeometryObject ""
    itk_option define -geometryObjectPath geometryObjectPath GeometryObjectPath ""
    itk_option define -geometryChangedCallback geometryChangedCallback GeometryChangedCallback ""
    itk_option define -prevGeometryObject prevGeometryObject PrevGeometryObject ""

    itk_option define -labelFont labelFont Font [list $::ArcherCore::SystemWindowFont 12]
    itk_option define -boldLabelFont boldLabelFont Font [list $::ArcherCore::SystemWindowFont 12 bold]
    itk_option define -entryFont entryFont Font [list $::ArcherCore::SystemWindowFont 12]
    itk_option define -units units Units ""
    itk_option define -valueUnits valueUnits ValueUnits ""

    constructor {args} {}
    destructor {}

    public {
	common EDIT_CLASS_NONE 0
	common EDIT_CLASS_ROT 1
	common EDIT_CLASS_SCALE 2
	common EDIT_CLASS_TRANS 3
	common EDIT_CLASS_SET 4

	common mEditMode 0
	common mPrevEditMode 0
	common mEditClass $EDIT_CLASS_NONE
	common mEditCommand ""
	common mEditParam1 0
	common mEditParam2 0
	common mEditLastTransMode $::ArcherCore::OBJECT_CENTER_MODE
	common mEditPCommand ""
	common mHighlightPoints 1
	common mHighlightPointSize 1.0

	proc validateColorComp {c}
	proc validateColor {color}

	method clearEditState {{_clearModeOnly 0} {_initFlag 0}}
	method childsite {{site upper}}

	method initGeometry {gdata}
	method checkpointGeometry {}
	method revertGeometry {}
	method updateGeometry {}
	method createGeometry {obj}

	method moveElement {_dm _obj _vx _vy _ocenter}
	method p {obj args}
    }

    protected {
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
	variable mInitGeometry 0

	method buildUpperPanel {}
	method buildLowerPanel {}

	method updateUpperPanel {normal disabled}
	method updateGeometryIfMod {}

	method clearAllTables {}
	method initEditState {}

	method buildComboBox {parent name1 name2 varName text listOfChoices}
	method buildArrow {parent prefix text buildViewFunc}
	method getView {}
	method toggleArrow {arrow view args}
	method updatePointSize {}
	method validatePointSize {_size}
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
	    -showhandle 0 \
	    -background $ArcherCore::LABEL_BACKGROUND_COLOR
    } {}

    $itk_component(pane) add upper
    $itk_component(pane) add lower

    set parent [$itk_component(pane) childsite upper]
    itk_component add upper {
	::ttk::frame $parent.upper
    } {}

    # Repack parent so its anchor is to the north
    pack $parent \
	-expand yes \
	-fill both \
	-anchor n

    set parent [$itk_component(pane) childsite lower]
    itk_component add lower {
	::ttk::frame $parent.lower
    } {}

    # Repack parent so its anchor is to the north
    pack $parent \
	-expand yes \
	-fill both \
	-anchor n

    # These are no-ops unless overridden in a subclass
    buildUpperPanel
    buildLowerPanel

    pack $itk_component(upper) -expand yes -fill both
    pack $itk_component(lower) -expand yes -fill both
    pack $itk_component(pane) -expand yes -fill both

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

::itcl::configbody GeometryEditFrame::geometryObjectPath {
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

::itcl::configbody GeometryEditFrame::units {
    # Nothing for now
}

::itcl::configbody GeometryEditFrame::valueUnits {
    # Nothing for now
}


# ------------------------------------------------------------
#                      PUBLIC CLASS METHODS
# ------------------------------------------------------------

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


::itcl::body GeometryEditFrame::clearEditState {{_clearModeOnly 0} {_initFlag 0}} {
    set mEditMode 0

    if {$_clearModeOnly} {
	return
    }

    clearAllTables
    set itk_option(-prevGeometryObject) ""

    if {$_initFlag} {
	initEditState
    }
}


::itcl::body GeometryEditFrame::childsite {{site upper}} {
    switch -- $site {
	"lower" {
	    return $itk_component(lower)
	}
	default -
	"upper" {
	    return $itk_component(upper)
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
#    update
    #after idle $this configure \
	-vscrollmode dynamic \
	-hscrollmode none
}

::itcl::body GeometryEditFrame::updateGeometry {} {
    if {$itk_option(-geometryChangedCallback) != ""} {
	catch {eval $itk_option(-geometryChangedCallback)}
    }
}

::itcl::body GeometryEditFrame::checkpointGeometry {} {
}

::itcl::body GeometryEditFrame::revertGeometry {} {
}

::itcl::body GeometryEditFrame::createGeometry {obj} {
    GeometryEditFrame::getView

    if {$mSize <= 0 ||
	$mDelta <= 0} {
	return 0
    }

    return 1
}


::itcl::body GeometryEditFrame::moveElement {_dm _obj _vx _vy _ocenter} {
    $itk_option(-mged) $mEditCommand $_obj $mEditParam1 $_ocenter
}


::itcl::body GeometryEditFrame::p {obj args} {
}

# ------------------------------------------------------------
#                      PROTECTED METHODS
# ------------------------------------------------------------

::itcl::body GeometryEditFrame::buildUpperPanel {} {
}

::itcl::body GeometryEditFrame::buildLowerPanel {} {
}

::itcl::body GeometryEditFrame::updateUpperPanel {normal disabled} {
}

::itcl::body GeometryEditFrame::updateGeometryIfMod {} {
}


::itcl::body GeometryEditFrame::clearAllTables {} {
}


::itcl::body GeometryEditFrame::initEditState {} {
    set itk_option(-prevGeometryObject) $itk_option(-geometryObject)

    if {$mEditClass == $EDIT_CLASS_ROT} {
	$::ArcherCore::application setDefaultBindingMode $::ArcherCore::OBJECT_ROTATE_MODE
    } elseif {$mEditClass == $EDIT_CLASS_SCALE} {
	$::ArcherCore::application setDefaultBindingMode $::ArcherCore::OBJECT_SCALE_MODE
    } elseif {$mEditClass == $EDIT_CLASS_TRANS} {
	$::ArcherCore::application setDefaultBindingMode $mEditLastTransMode
    }
}

::itcl::body GeometryEditFrame::buildComboBox {parent name1 name2 var text listOfChoices} {
    itk_component add $name1\L {
	::ttk::label $parent.$name2\L \
	    -text $text
    } {}

    itk_component add $name1\F {
	::ttk::frame $parent.$name2\F \
	    -relief sunken
    } {}

    itk_component add $name1\CB {
	::ttk::combobox $itk_component($name1\F).$name2\CB \
	    -state readonly \
	    -textvariable $var \
	    -values $listOfChoices
    } {}

    pack $itk_component($name1\CB) -expand yes -fill both
}

::itcl::body GeometryEditFrame::buildArrow {parent prefix text buildViewFunc} {
    itk_component add $prefix {
	::ttk::frame $parent.$prefix
    } {}
    itk_component add $prefix\Arrow {
	::swidgets::togglearrow $itk_component($prefix).arrow
    } {}
    itk_component add $prefix\Label {
	::ttk::label $itk_component($prefix).label -text $text \
	    -anchor w
    } {}
    itk_component add $prefix\View {
	::ttk::frame $itk_component($prefix).$prefix\View
    } {}
    $buildViewFunc $itk_component($prefix\View)
    grid $itk_component($prefix\Arrow) -row 0 -column 0 -sticky e
    grid $itk_component($prefix\Label) -row 0 -column 1 -sticky w
    grid columnconfigure $itk_component($prefix) 1 -weight 1
    $itk_component($prefix\Arrow) configure -command [itcl::code $this toggleArrow \
							  $itk_component($prefix\Arrow) $itk_component($prefix\View) -row 1 \
							  -column 1 -sticky nsew]
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


::itcl::body GeometryEditFrame::updatePointSize {} {
    if {$mHighlightPointSize != "" && $mHighlightPointSize != "."} {
	$itk_option(-mged) data_axes size $mHighlightPointSize
    } else {
	$itk_option(-mged) data_axes size 0
    }
}


::itcl::body GeometryEditFrame::validatePointSize {_size} {
    if {$_size == "."} {
	return 1
    }

    if {[string is double $_size]} {
	# Need to check is first
	if {$_size == ""} {
	    return 1
	}

	if {$_size < 0} {
	    return 0
	}

	# Everything else is OK
	return 1
    }

    return 0
}



# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
