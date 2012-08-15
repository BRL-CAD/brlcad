#                P I P E E D I T F R A M E . T C L
# BRL-CAD
#
# Copyright (c) 2002-2012 United States Government as represented by
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
# Author:
#    Bob Parker
#
# Description:
#    The class for editing bots within Archer.
#

::itcl::class BotEditFrame {
    inherit GeometryEditFrame

    constructor {args} {}
    destructor {}

    public {
	common SELECT_COL 0
	common X_COL 1
	common Y_COL 2
	common Z_COL 3

	common selectPoint 1
	common movePoint 2

	common mDetailHeadings {{} X Y Z}
	common mEditLabels {
	    {Select Point}
	    {Move Point}
	}

	# Override what's in GeometryEditFrame
	method initGeometry {gdata}
	method updateGeometry {}
	method createGeometry {_name}
	method p {obj args}
	method moveBotPtMode {_dname _obj _x _y}
	method moveBotPt {_dname _obj _x _y}
    }

    protected {
	variable mDetail
	variable mCurrentBotPoint 1

	# Methods used by the constructor
	# override methods in GeometryEditFrame
	method buildUpperPanel
	method buildLowerPanel

	# Override what's in GeometryEditFrame
	method updateGeometryIfMod {}
	method initEditState {}

	method applyData {}
	method detailBrowseCommand {_row _col}
	method handleDetailPopup {_index _X _Y}
	method handleEnter {_row _col}
	method botPointSelectCallback {_pindex}
	method singleSelectCallback {_pindex}
	method validateDetailEntry {_row _col _newval _clientdata}
    }

    private {}
}


# ------------------------------------------------------------
#                      CONSTRUCTOR
# ------------------------------------------------------------

::itcl::body BotEditFrame::constructor {args} {
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
::itcl::body BotEditFrame::initGeometry {gdata} {
    unset mDetail
    set mDetail(active) ""

    set col 0
    foreach heading $mDetailHeadings {
	set mDetail(0,$col) $heading
	incr col
    }

    foreach {attr val} $gdata {
	switch -- $attr {
	    "mode" {
	    }
	    "orient" {
	    }
	    "flags" {
	    }
	    "V" {
		set index 1
		foreach item $val {
		    set mDetail($index,$SELECT_COL) ""
		    set mDetail($index,$X_COL) [lindex $item 0]
		    set mDetail($index,$Y_COL) [lindex $item 1]
		    set mDetail($index,$Z_COL) [lindex $item 2]
		    incr index
		}
	    }
	    "F" {
	    }
	    default {
		# Shouldn't get here
		puts "Encountered bad one - $attr"
	    }
	}
    }

    GeometryEditFrame::initGeometry $gdata

    if {$itk_option(-geometryObject) != $mPrevGeometryObject} {
	set mCurrentBotPoint 1
	set mPrevGeometryObject $itk_option(-geometryObject)
    }
    botPointSelectCallback [expr {$mCurrentBotPoint - 1}]
}

::itcl::body BotEditFrame::updateGeometry {} {
    # Not ready
    return

    if {$itk_option(-mged) == "" ||
	$itk_option(-geometryObject) == ""} {
	return
    }

    set pipe_spec {}
    set pt {}

    foreach aname [lsort [array names mDetail]] {
	set alist [split $aname ","]
	if {[llength $alist] != 2} {
	    continue
	}

	set row [lindex $alist 0]
	if {$row == 0} {
	    continue
	}

	set index [expr {$row - 1}]
	set col [lindex $alist 1]
	if {$col == 0} {
	    continue
	}

	switch -- $col \
	    $OD_COL {
		lappend pipe_spec O$index $mDetail($row,$col)
	    } \
	    $ID_COL {
		lappend pipe_spec I$index $mDetail($row,$col)
	    } \
	    $RADIUS_COL {
		lappend pipe_spec R$index $mDetail($row,$col)
	    } \
	    $PX_COL - \
	    $PY_COL {
		lappend pt $mDetail($row,$col)
	    } \
	    $PZ_COL {
		lappend pt $mDetail($row,$col)
		lappend pipe_spec V$index $pt
		set pt {}
	    } \
	    default {
		puts "Encountered bad one - $mDetail($row,$col)"
	    }
    }

    eval $itk_option(-mged) adjust $itk_option(-geometryObject) $pipe_spec
    GeometryEditFrame::updateGeometry
}

::itcl::body BotEditFrame::createGeometry {obj} {
    #Not ready yet
    return

    if {![GeometryEditFrame::createGeometry $obj]} {
	return
    }

    set od [expr {$mDelta * 0.2}]
    set id [expr {$od * 0.5}]
    set br $od

    $itk_option(-mged) put $obj pipe \
	V0 [list $mCenterX $mCenterY $mZmin] \
	O0 $od \
	I0 $id \
	R0 $br \
	V1 [list $mCenterX $mCenterY $mZmax] \
	O1 $od \
	I1 $id \
	R1 $br
}

::itcl::body BotEditFrame::p {obj args} {
    if {[llength $args] != 1 || ![string is double $args]} {
	return "Usage: p sf"
    }
#XXX Need to update this method
    return

    switch -- $mEditMode \
	$setA {
	    $::ArcherCore::application p_pscale $obj a $args
	} \
	$setB {
	    $::ArcherCore::application p_pscale $obj b $args
	} \
	$setC {
	    $::ArcherCore::application p_pscale $obj c $args
	} \
	$setABC {
	    $::ArcherCore::application p_pscale $obj abc $args
	}

    return ""
}


::itcl::body BotEditFrame::moveBotPtMode {_dname _obj _x _y} {
    set mEditParam1 [$itk_option(-mged) pane_mouse_find_botpt $_dname $_obj $_x $_y]
    $itk_option(-mged) pane_move_botpt_mode $_dname $_obj $mEditParam1 $_x $_y
}


::itcl::body BotEditFrame::moveBotPt {_dname _obj _x _y} {
    $itk_option(-mged) pane_mouse_move_botpt $_dname $_obj $mEditParam1 $_x $_y
}


# ------------------------------------------------------------
#                      PROTECTED METHODS
# ------------------------------------------------------------

::itcl::body BotEditFrame::buildUpperPanel {} {
    set parent [$this childsite]

    itk_component add detailTab {
	::cadwidgets::TkTable $parent.detailTab \
	    [::itcl::scope mDetail] \
	    $mDetailHeadings \
	    -cursor arrow \
	    -height 0 \
	    -maxheight 2000 \
	    -width 0 \
	    -rows 100000 \
	    -colstretchmode unset \
	    -validate 1 \
	    -validatecommand [::itcl::code $this validateDetailEntry] \
	    -tablePopupHandler [::itcl::code $this handleDetailPopup] \
	    -entercommand [::itcl::code $this handleEnter] \
	    -singleSelectCallback [::itcl::code $this singleSelectCallback]
    } {}
#	    -browsecommand [::itcl::code $this detailBrowseCommand %r %c]
#	    -dataCallback [::itcl::code $this applyData]

    # Set width of column 0
    $itk_component(detailTab) width 0 3

    pack $itk_component(detailTab) -expand yes -fill both
    pack $parent -expand yes -fill both
}

::itcl::body BotEditFrame::buildLowerPanel {} {
    set parent [$this childsite lower]
    set i 1
    foreach label $mEditLabels {
	itk_component add editRB$i {
	    ::ttk::radiobutton $parent.editRB$i \
		-variable [::itcl::scope mEditMode] \
		-value $i \
		-text $label \
		-command [::itcl::code $this initEditState]
	} {}

	pack $itk_component(editRB$i) \
	    -anchor w \
	    -expand yes

	incr i
    }
}

::itcl::body BotEditFrame::updateGeometryIfMod {} {
    if {$itk_option(-mged) == "" ||
	$itk_option(-geometryObject) == ""} {
	return
    }

    set doUpdate 0
    set gdata [$itk_option(-mged) get $itk_option(-geometryObject)]
    set gdata [lrange $gdata 1 end]

    foreach {attr val} $gdata {
	if {![regexp {^[VOIRvoir]([0-9]+)$} $attr all index]} {
	    puts "Encountered bad one - $attr"
	    continue
	}

	incr index

	switch -regexp -- $attr {
	    {[Vv][0-9]+} {
		if {$mDetail($index,$PX_COL) != [lindex $val 0] ||
		    $mDetail($index,$PY_COL) != [lindex $val 1] ||
		    $mDetail($index,$PZ_COL) != [lindex $val 2]} {
		    set doUpdate 1
		}
	    }
	    {[Oo][0-9]+} {
		if {$mDetail($index,$OD_COL) != $val} {
		    set doUpdate 1
		}
	    }
	    {[Ii][0-9]+} {
		if {$mDetail($index,$ID_COL) != $val} {
		    set doUpdate 1
		}
	    }
	    {[Rr][0-9]+} {
		if {$mDetail($index,$RADIUS_COL) != $val} {
		    set doUpdate 1
		}
	    }
	    default {
		# Shouldn't get here
		puts "Encountered bad one - $attr"
	    }
	}

	if {$doUpdate} {
	    updateGeometry
	    return
	}
    }
}

::itcl::body BotEditFrame::initEditState {} {
#    set mEditCommand pscale
#    set mEditClass $EDIT_CLASS_SCALE
#    configure -valueUnits "mm"

    set mEditPCommand [::itcl::code $this p]
    set mEditParam1 [expr {$mCurrentBotPoint - 1}]

    switch -- $mEditMode \
	$selectPoint {
	    set mEditCommand ""
	    set mEditClass ""
	    $::ArcherCore::application initFindBotPoint $itk_option(-geometryObjectPath) 1 [::itcl::code $this botPointSelectCallback]
	} \
	$movePoint {
	    set mEditCommand move_botpt
	    set mEditClass $EDIT_CLASS_TRANS
	}

    GeometryEditFrame::initEditState
}

::itcl::body BotEditFrame::applyData {} {
}

::itcl::body BotEditFrame::detailBrowseCommand {_row _col} {
    if {![info exists mDetail($_row,0)]} {
	return 0
    }

    $itk_component(detailTab) see $_row,$_col
}

::itcl::body BotEditFrame::handleDetailPopup {_index _X _Y} {
}

::itcl::body BotEditFrame::handleEnter {_row _col} {
    if {$itk_option(-mged) == "" ||
	$itk_option(-geometryObject) == "" ||
	$_row < 1 ||
	$_col < 1 ||
	$_col > $PZ_COL} {
	return
    }

    updateGeometryIfMod
}

::itcl::body BotEditFrame::botPointSelectCallback {_pindex} {
    set mEditParam1 $_pindex
    incr _pindex
    set mCurrentBotPoint $_pindex
    $itk_component(detailTab) selectSingleRow $_pindex
}

::itcl::body BotEditFrame::singleSelectCallback {_pindex} {
    set mCurrentBotPoint $_pindex
    set mEditParam1 [expr {$mCurrentBotPoint - 1}]
    initEditState
}

::itcl::body BotEditFrame::validateDetailEntry {_row _col _newval _clientdata} {
    if {![info exists mDetail($_row,0)]} {
	return 0
    }

    return [::cadwidgets::Ged::validateDouble $_newval]
}


# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
