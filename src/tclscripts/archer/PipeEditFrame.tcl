#                P I P E E D I T F R A M E . T C L
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
# Author:
#    Bob Parker
#
# Description:
#    The class for editing pipes within Archer.
#

::itcl::class PipeEditFrame {
    inherit GeometryEditFrame

    constructor {args} {}
    destructor {}

    public {
	common SELECT_COL 0
	common OD_COL 1
	common ID_COL 2
	common RADIUS_COL 3
	common PX_COL 4
	common PY_COL 5
	common PZ_COL 6

	common selectPoint 1
	common movePoint 2
	common deletePoint 3
	common appendPoint 4
	common prependPoint 5
	common setPointOD 6
	common setPointID 7
	common setPointBend 8
	common setPipeOD 9
	common setPipeID 10
	common setPipeBend 11

	common mDetailHeadings {{} OD ID Radius pX pY pZ}
	common mEditLabels {
	    {Select Point}
	    {Move Point}
	    {Delete Point}
	    {Append Point}
	    {Prepend Point}
	    {Set Point OD}
	    {Set Point ID}
	    {Set Point Bend}
	    {Set Pipe OD}
	    {Set Pipe ID}
	    {Set Pipe Bend}
	}

	# Override what's in GeometryEditFrame
	method clearAllTables {}
	method initGeometry {gdata}
	method initTranslate {}
	method updateGeometry {}
	method createGeometry {_name}
	method moveElement {_dm _obj _vx _vy _ocenter}
	method p {obj args}
    }

    protected {
	variable mDetail
	variable mCurrentPipePoint 0
	variable mPrevPipeObject ""

	# Methods used by the constructor
	# override methods in GeometryEditFrame
	method buildUpperPanel
	method buildLowerPanel

	# Override what's in GeometryEditFrame
	method updateGeometryIfMod {}
	method initEditState {}

	method applyData {}
	method detailBrowseCommand {_row _col}
	method endPipePointMove {_dm _obj _mx _my}
	method handleDetailPopup {_index _X _Y}
	method handleEnter {_row _col}
	method highlightCurrentPipePoint {}
	method pipePointAppendCallback {}
	method pipePointDeleteCallback {_pindex}
	method pipePointMoveCallback {_pindex}
	method pipePointPrependCallback {}
	method pipePointSelectCallback {_pindex}
	method singleSelectCallback {_pindex}
	method validateDetailEntry {_row _col _newval _clientdata}
    }

    private {}
}


# ------------------------------------------------------------
#                      CONSTRUCTOR
# ------------------------------------------------------------

::itcl::body PipeEditFrame::constructor {args} {
    eval itk_initialize $args
}

# ------------------------------------------------------------
#                        OPTIONS
# ------------------------------------------------------------


# ------------------------------------------------------------
#                      PUBLIC METHODS
# ------------------------------------------------------------


::itcl::body PipeEditFrame::clearAllTables {} {
    $itk_option(-mged) data_axes points {}
    $itk_option(-mged) data_lines points {}

    set mCurrentPipePoint 0
    $itk_component(detailTab) unselectAllRows
}


## - initGeometry
#
# Initialize the variables containing the object's specification.
#
::itcl::body PipeEditFrame::initGeometry {gdata} {
    unset mDetail
    set mDetail(active) ""

    set col 0
    foreach heading $mDetailHeadings {
	set mDetail(0,$col) $heading
	incr col
    }

    foreach {attr val} $gdata {
	if {![regexp {^[VOIRvoir]([0-9]+)$} $attr all index]} {
	    puts "Encountered bad one - $attr"
	    continue
	}

	incr index
	set mDetail($index,$SELECT_COL) ""

	switch -regexp -- $attr {
	    {[Vv][0-9]+} {
		set val [format "%.6f %.6f %.6f" [lindex $val 0] [lindex $val 1] [lindex $val 2]]
		set mDetail($index,$PX_COL) [lindex $val 0]
		set mDetail($index,$PY_COL) [lindex $val 1]
		set mDetail($index,$PZ_COL) [lindex $val 2]
	    }
	    {[Oo][0-9]+} {
		set val [format "%.6f" $val]
		set mDetail($index,$OD_COL) $val
	    }
	    {[Ii][0-9]+} {
		set val [format "%.6f" $val]
		set mDetail($index,$ID_COL) $val
	    }
	    {[Rr][0-9]+} {
		set val [format "%.6f" $val]
		set mDetail($index,$RADIUS_COL) $val
	    }
	    default {
		# Shouldn't get here
		puts "Encountered bad one - $attr"
	    }
	}
    }

    GeometryEditFrame::initGeometry $gdata

    if {$itk_option(-geometryObject) != $itk_option(-prevGeometryObject)} {
	set mCurrentPipePoint 0
	set itk_option(-prevGeometryObject) $itk_option(-geometryObject)
    } elseif {$mCurrentPipePoint > 0} {
	pipePointSelectCallback [expr {$mCurrentPipePoint - 1}]
    }
}


::itcl::body PipeEditFrame::initTranslate {} {
    switch -- $mEditMode \
	$movePoint {
	    $::ArcherCore::application initFindPipePoint $itk_option(-geometryObjectPath) 1 [::itcl::code $this pipePointMoveCallback]
	} \
	default {
	    $::ArcherCore::application putString "PipeEditFrame::initTranslate: bad mode - $mEditMode, only movePoint allowed!"
	}
}


::itcl::body PipeEditFrame::updateGeometry {} {
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

::itcl::body PipeEditFrame::createGeometry {obj} {
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


::itcl::body PipeEditFrame::moveElement {_dm _obj _vx _vy _ocenter} {
    set seg_i [expr {$mCurrentPipePoint - 1}]
    set pt [$itk_option(-mged) get $_obj V$seg_i]
    set vpt [$itk_option(-mged) pane_m2v_point $_dm $pt]
    set vz [lindex $vpt 2]
    set mpt [$itk_option(-mged) pane_v2m_point $_dm [list $_vx $_vy $vz]]
    $itk_option(-mged) move_pipept $_obj $seg_i $mpt
}


::itcl::body PipeEditFrame::p {obj args} {
    if {[llength $args] != 1 || ![string is double $args]} {
	return "Usage: p sf"
    }

    set seg_i [expr {$mCurrentPipePoint - 1}]

    switch -- $mEditMode \
	$setPipeBend {
	    $::ArcherCore::application p_pscale $obj B $args
	} \
	$setPipeID {
	    $::ArcherCore::application p_pscale $obj I $args
	} \
	$setPipeOD {
	    $::ArcherCore::application p_pscale $obj O $args
	} \
	$setPointBend {
	    $::ArcherCore::application p_pscale $obj b$seg_i $args
	} \
	$setPointID {
	    $::ArcherCore::application p_pscale $obj i$seg_i $args
	} \
	$setPointOD {
	    $::ArcherCore::application p_pscale $obj o$seg_i $args
	}

    return ""
}


# ------------------------------------------------------------
#                      PROTECTED METHODS
# ------------------------------------------------------------

::itcl::body PipeEditFrame::buildUpperPanel {} {
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

::itcl::body PipeEditFrame::buildLowerPanel {} {
    set parent [$this childsite lower]
    set row 1
    foreach label $mEditLabels {
	itk_component add editRB$row {
	    ::ttk::radiobutton $parent.editRB$row \
		-variable [::itcl::scope mEditMode] \
		-value $row \
		-text $label \
		-command [::itcl::code $this initEditState]
	} {}

	grid $itk_component(editRB$row) -row $row -column 0 -sticky nsew
	incr row
    }

    itk_component add hlPointsCB {
	::ttk::checkbutton $parent.hlPointsCB \
	    -text "Highlight Points" \
	    -command [::itcl::code $this highlightCurrentPipePoint] \
	    -variable ::GeometryEditFrame::mHighlightPoints
    } {}

    itk_component add pointSizeL {
	::ttk::label $parent.pointSizeL \
	    -anchor e \
	    -text "Highlight Point Size"
    } {}
    itk_component add pointSizeE {
	::ttk::entry $parent.pointSizeE \
	    -width 12 \
	    -textvariable [::itcl::scope mHighlightPointSize] \
	    -validate key \
	    -validatecommand [::itcl::code $this validatePointSize %P]
    } {}

    bind $itk_component(pointSizeE) <Return> [::itcl::code $this updatePointSize]

    incr row
    grid rowconfigure $parent $row -weight 1
    incr row
    grid $itk_component(hlPointsCB) -row $row -column 0 -sticky w
    incr row
    grid $itk_component(pointSizeL) -column 0 -row $row -sticky e
    grid $itk_component(pointSizeE) -column 1 -row $row -sticky ew
    grid columnconfigure $parent 1 -weight 1
}

::itcl::body PipeEditFrame::updateGeometryIfMod {} {
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

::itcl::body PipeEditFrame::initEditState {} {
    set mEditPCommand [::itcl::code $this p]
    set seg_i [expr {$mCurrentPipePoint - 1}]
    highlightCurrentPipePoint

    switch -- $mEditMode \
	$selectPoint {
	    set mEditCommand ""
	    set mEditClass ""
	    set mEditParam1 ""
	    $::ArcherCore::application initFindPipePoint $itk_option(-geometryObjectPath) 1 [::itcl::code $this pipePointSelectCallback] 1
	} \
	$movePoint {
	    set mEditCommand move_pipept
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditLastTransMode $::ArcherCore::OBJECT_TRANSLATE_MODE
	    set mEditParam1 ""
	} \
	$deletePoint {
	    set mEditCommand ""
	    set mEditClass ""
	    set mEditParam1 ""
	    $::ArcherCore::application initFindPipePoint $itk_option(-geometryObjectPath) 1 [::itcl::code $this pipePointDeleteCallback] 1
	} \
	$appendPoint {
	    set mEditCommand ""
	    set mEditClass ""
	    set mEditParam1 ""
	    $::ArcherCore::application initAppendPipePoint $itk_option(-geometryObjectPath) 1 [::itcl::code $this pipePointAppendCallback]
	    set odata [lrange [$itk_option(-mged) get $itk_option(-geometryObject)] 1 end]
	    pipePointSelectCallback [expr {int([llength $odata] * 0.125) - 1}]
	} \
	$prependPoint {
	    set mEditCommand ""
	    set mEditClass ""
	    set mEditParam1 ""
	    $::ArcherCore::application initPrependPipePoint $itk_option(-geometryObjectPath) 1  [::itcl::code $this pipePointPrependCallback]
	    pipePointSelectCallback 0
	} \
	$setPointOD {
	    set mEditCommand pscale
	    set mEditClass $EDIT_CLASS_SCALE
	    set mEditParam1 "o$seg_i"
	} \
	$setPointID {
	    set mEditCommand pscale
	    set mEditClass $EDIT_CLASS_SCALE
	    set mEditParam1 "i$seg_i"
	} \
	$setPointBend {
	    set mEditCommand pscale
	    set mEditClass $EDIT_CLASS_SCALE
	    set mEditParam1 "b$seg_i"
	} \
	$setPipeOD {
	    set mEditCommand pscale
	    set mEditClass $EDIT_CLASS_SCALE
	    set mEditParam1 O
	} \
	$setPipeID {
	    set mEditCommand pscale
	    set mEditClass $EDIT_CLASS_SCALE
	    set mEditParam1 I
	} \
	$setPipeBend {
	    set mEditCommand pscale
	    set mEditClass $EDIT_CLASS_SCALE
	    set mEditParam1 B
	} \
	default {
	    set mEditCommand ""
	    set mEditPCommand ""
	    set mEditParam1 ""
	}

    GeometryEditFrame::initEditState
}

::itcl::body PipeEditFrame::applyData {} {
}

::itcl::body PipeEditFrame::detailBrowseCommand {_row _col} {
    if {![info exists mDetail($_row,0)]} {
	return 0
    }

    $itk_component(detailTab) see $_row,$_col
}


::itcl::body PipeEditFrame::endPipePointMove {_dm _obj _mx _my} {
    $::ArcherCore::application endObjTranslate $_dm $_obj $_mx $_my
}


::itcl::body PipeEditFrame::handleDetailPopup {_index _X _Y} {
}

::itcl::body PipeEditFrame::handleEnter {_row _col} {
    if {$itk_option(-mged) == "" ||
	$itk_option(-geometryObject) == "" ||
	$_row < 1 ||
	$_col < 1 ||
	$_col > $PZ_COL} {
	return
    }

    updateGeometryIfMod
}


::itcl::body PipeEditFrame::highlightCurrentPipePoint {} {
    if {$itk_option(-mged) == "" ||
	[$itk_option(-mged) how $itk_option(-geometryObjectPath)] < 0 ||
	$mCurrentPipePoint < 1} {
	return
    }

    $itk_option(-mged) refresh_off
    set hlcolor [$::ArcherCore::application getRgbColor [$itk_option(-mged) cget -primitiveLabelColor]]
    $itk_option(-mged) data_axes draw $mHighlightPoints
    $itk_option(-mged) data_axes size $mHighlightPointSize
    eval $itk_option(-mged) data_axes color $hlcolor
    $itk_option(-mged) refresh_on

    set seg_i [expr {$mCurrentPipePoint - 1}]
    set pt [$itk_option(-mged) get $itk_option(-geometryObjectPath) V$seg_i]

    $itk_option(-mged) data_axes points [list $pt]
}


::itcl::body PipeEditFrame::pipePointAppendCallback {} {
    if {$itk_option(-mged) == ""} {
	return
    }

    set odata [lrange [$itk_option(-mged) get $itk_option(-geometryObject)] 1 end]
    set mCurrentPipePoint [expr {int([llength $odata] * 0.125)}]
    initGeometry $odata
    $::ArcherCore::application setSave
}

::itcl::body PipeEditFrame::pipePointDeleteCallback {_pindex} {
    if {$itk_option(-mged) == ""} {
	return
    }

    eval $itk_option(-mged) delete_pipept $itk_option(-geometryObject) $_pindex
    eval $itk_option(-mged) redraw $itk_option(-geometryObjectPath)
    set odata [lrange [$itk_option(-mged) get $itk_option(-geometryObject)] 1 end]

    set seg_i [expr {$mCurrentPipePoint - 1}]
    if {[catch {$itk_option(-mged) get $itk_option(-geometryObject) I$seg_i}]} {
	set mCurrentPipePoint $seg_i
	incr seg_i -1
    }

    initGeometry $odata
    $::ArcherCore::application setSave
}


::itcl::body PipeEditFrame::pipePointMoveCallback {_pindex} {
    if {$itk_option(-mged) == ""} {
	return
    }

    pipePointSelectCallback $_pindex

    foreach dname {ul ur ll lr} {
	set win [$itk_option(-mged) component $dname]
	bind $win <ButtonRelease-1> "[::itcl::code $this endPipePointMove $dname $itk_option(-geometryObject) %x %y]; break"
    }

    set last_mouse [$itk_option(-mged) get_prev_ged_mouse]
    set seg_i [expr {$mCurrentPipePoint - 1}]
    eval $itk_option(-mged) move_pipept_mode $itk_option(-geometryObject) $seg_i $last_mouse
}


::itcl::body PipeEditFrame::pipePointPrependCallback {} {
    if {$itk_option(-mged) == ""} {
	return
    }

    set odata [lrange [$itk_option(-mged) get $itk_option(-geometryObject)] 1 end]
    set mCurrentPipePoint 1
    initGeometry $odata
    $::ArcherCore::application setSave
}

::itcl::body PipeEditFrame::pipePointSelectCallback {_pindex} {
    set mEditParam1 $_pindex
    incr _pindex
    set mCurrentPipePoint $_pindex
    $itk_component(detailTab) selectSingleRow $_pindex
    highlightCurrentPipePoint
}

::itcl::body PipeEditFrame::singleSelectCallback {_pindex} {
    set mCurrentPipePoint $_pindex
    initEditState
}

::itcl::body PipeEditFrame::validateDetailEntry {_row _col _newval _clientdata} {
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
