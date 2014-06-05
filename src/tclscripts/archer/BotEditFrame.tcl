#                B O T E D I T F R A M E . T C L
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
#    The class for editing bots within Archer.
#

##
# TODO
#
# -) Add constrained translation of bot elements (i.e. points, edges, faces). Generalize for use with other primitives.
# -) Delete points, faces and edges
# -) Operations to create/extend surface mode bots
#    - Three pts are specified to create a face.
#    - An existing edge is selected followed by a new pt to create a face.
#    - Two connected edges on the boundary are selected to create a face by completing the loop.

::itcl::class BotEditFrame {
    inherit GeometryEditFrame

    constructor {args} {}
    destructor {}

    public {
	common SELECT_COL 0
	common A_COL 1
	common B_COL 2
	common C_COL 3
	common X_COL 1
	common Y_COL 2
	common Z_COL 3

	common movePoints 1
	common moveEdge 2
	common moveFace 3
	common selectPoints 4
	common selectEdges 5
	common selectFaces 6
	common splitEdge 7
	common splitFace 8

	common mVertDetailHeadings {{} X Y Z}
	common mEdgeDetailHeadings {{} A B}
	common mFaceDetailHeadings {{} A B C}
	common mEditLabels {
	    {Move Points}
	    {Move Edge}
	    {Move Face}
	    {Select Points}
	    {Select Edges}
	    {Select Faces}
	    {Split Edge}
	    {Split Face}
	}

	method selectBotPts {_plist}
	method setMoveMode {{_tflag 0}}

	# Override what's in GeometryEditFrame
	method initGeometry {_gdata}
	method updateGeometry {}
	method createGeometry {_name}
	method moveElement {_dm _obj _vx _vy _ocenter}
	method p {obj args}

	method moveBotEdgeMode {_dname _obj _x _y}
	method moveBotFaceMode {_dname _obj _x _y}
	method moveBotPt {_dname _obj _vx _vy}
	method moveBotPtMode {_dname _obj _viewz _x _y}
	method moveBotPts {_dname _obj _x _y _plist}
	method moveBotPtsMode {_dname _obj _x _y}
    }

    protected {
	variable mVertDetail
	variable mEdgeDetail
	variable mFaceDetail
	variable mPointList {}
	variable mEdgeList {}
	variable mFaceList {}
	variable mCurrentBotPoints ""
	variable mCurrentBotEdges ""
	variable mCurrentBotFaces ""
	variable mFrontPointsOnly 1
	variable mMaxVertThreshold 30
	variable mIgnoreMaxVertThreshold 0
	variable mShowTables 1

	# Methods used by the constructor
	# override methods in GeometryEditFrame
	method buildUpperPanel
	method buildLowerPanel

	# Override what's in GeometryEditFrame
	method clearAllTables {}
	method initEditState {}
	method updateGeometryIfMod {}

	method applyData {}
	method botEdgesSelectCallback {_edge {_sync 1}}
	method botEdgeSplitCallback {_edge}
	method botFacesSelectCallback {_face}
	method botFaceSplitCallback {_face}
	method botPointSelectCallback {_pindex}
	method botPointsSelectCallback {_pindex}
	method detailBrowseCommand {_row _col}
	method handleDetailPopup {_index _X _Y}
	method handleEnter {_row _col}
	method highlightCurrentBotElements {}
	method initPointHighlight {}
	method loadTables {_gdata {_lflag 1}}
	method manageTables {}
	method multiEdgeSelectCallback {}
	method multiFaceSelectCallback {}
	method multiPointSelectCallback {}
	method reloadTables {}
	method selectCurrentBotPoints {{_findEdges 1}}
	method syncTablesWrtPoints {{_findEdges 1}}
	method syncTablesWrtEdges {}
	method syncTablesWrtFaces {}
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



::itcl::body BotEditFrame::selectBotPts {_plist} {
    foreach item $_plist {
	incr item

	if {[lsearch $mCurrentBotPoints $item] == -1} {
	    lappend mCurrentBotPoints $item
	}
    }

    selectCurrentBotPoints
}


## - setMoveMode
#
#
::itcl::body BotEditFrame::setMoveMode {{_tflag 0}} {
    set len [llength $mCurrentBotPoints]

    if {$len} {
	set mEditPCommand [::itcl::code $this p]
	set mEditParam1 ""
	set mEditClass $EDIT_CLASS_TRANS
	set mEditLastTransMode $::ArcherCore::OBJECT_TRANSLATE_MODE

	if {$mEditMode != $movePoints && $mEditMode != $moveEdge && $mEditMode != $moveFace} {
	    set mEditMode $movePoints
	    set mEditCommand moveBotPtsMode
	}
    } elseif {$_tflag && ($mEditMode == $movePoints || $mEditMode == $moveEdge || $mEditMode == $moveFace)} {
	set len 1
    }

    return $len
}


## - initGeometry
#
# Initialize the variables containing the object's specification.
#
::itcl::body BotEditFrame::initGeometry {_gdata} {
    if {$itk_option(-mged) == "" ||
	$itk_option(-geometryObject) == ""} {
	return
    }

    GeometryEditFrame::initGeometry $_gdata

    if {$itk_option(-geometryObject) != $itk_option(-prevGeometryObject)} {
	set mCurrentBotPoints ""
	set mCurrentBotEdges ""
	set mCurrentBotFaces ""
	set itk_option(-prevGeometryObject) $itk_option(-geometryObject)
	set lflag 1
    } else {
	set lflag 0
    }

    loadTables $_gdata $lflag

    $itk_component(edgeTab) unselectAllRows
    $itk_component(faceTab) unselectAllRows

    selectCurrentBotPoints
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

    foreach aname [lsort [array names mVertDetail]] {
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
		lappend pipe_spec O$index $mVertDetail($row,$col)
	    } \
	    $ID_COL {
		lappend pipe_spec I$index $mVertDetail($row,$col)
	    } \
	    $RADIUS_COL {
		lappend pipe_spec R$index $mVertDetail($row,$col)
	    } \
	    $PX_COL - \
	    $PY_COL {
		lappend pt $mVertDetail($row,$col)
	    } \
	    $PZ_COL {
		lappend pt $mVertDetail($row,$col)
		lappend pipe_spec V$index $pt
		set pt {}
	    } \
	    default {
		puts "Encountered bad one - $mVertDetail($row,$col)"
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


::itcl::body BotEditFrame::moveElement {_dname _obj _vx _vy _ocenter} {
    switch -- $mEditMode \
	$movePoints {
	    moveBotPt $_dname $_obj $_vx $_vy
	} \
	$moveEdge {
	    $::ArcherCore::application putString "This mode is not ready for edges."
	} \
	$moveFace {
	    $::ArcherCore::application putString "This mode is not ready for faces."
	}
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


::itcl::body BotEditFrame::moveBotEdgeMode {_dname _obj _x _y} {
    $itk_option(-mged) clear_bot_callbacks

    if {$mFrontPointsOnly} {
	set zlist [ $::ArcherCore::application getZClipState]
	set viewz [lindex $zlist 0]
    } else {
	set viewz -1.0
    }

    set edge [$itk_option(-mged) pane_mouse_find_bot_edge $_dname $_obj $viewz $_x $_y]

    set mCurrentBotPoints {}
    set mCurrentBotEdges {}
    $itk_component(vertTab) unselectAllRows
    $itk_component(edgeTab) unselectAllRows
    botEdgesSelectCallback $edge

    set plist [$itk_component(vertTab) getSelectedRows]
    moveBotPts $_dname $_obj $_x $_y $plist
}


::itcl::body BotEditFrame::moveBotFaceMode {_dname _obj _x _y} {
    $itk_option(-mged) clear_bot_callbacks
    set zlist [ $::ArcherCore::application getZClipState]
    set viewz [lindex $zlist 0]

    set face [$itk_option(-mged) pane_mouse_find_bot_face $_dname $_obj $viewz $_x $_y]
    if {$face == ""} {
	return
    }

#    set mCurrentBotPoints {}
#    $itk_component(vertTab) unselectAllRows
    set mCurrentBotFaces {}
    botFacesSelectCallback $face

    set plist [$itk_component(vertTab) getSelectedRows]
    moveBotPts $_dname $_obj $_x $_y $plist
}


::itcl::body BotEditFrame::moveBotPt {_dname _obj _vx _vy} {
    set len [llength $mCurrentBotPoints]
    switch -- $len {
	0 {
	    $::ArcherCore::application putString "No points have been selected."
	}
	1 {
	    set i [expr {$mCurrentBotPoints - 1}]
	    set botpt [lindex $mPointList $i]
	    set vpt [$itk_option(-mged) pane_m2v_point $_dname $botpt]
	    set vz [lindex $vpt 2]
	    set mpt [$itk_option(-mged) pane_v2m_point $_dname [list $_vx $_vy $vz]]
	    $itk_option(-mged) move_botpt $_obj $i $mpt
	}
	default {
	    $::ArcherCore::application putString "More than one point has been selected."
	}
    }
}


::itcl::body BotEditFrame::moveBotPtMode {_dname _obj _viewz _x _y} {
    $itk_option(-mged) clear_bot_callbacks

    set pindex [$itk_option(-mged) pane_mouse_find_botpt $_dname $_obj $_viewz $_x $_y]
    $itk_option(-mged) pane_move_botpt_mode $_dname $_obj $pindex $_x $_y

    botPointSelectCallback $pindex
}


::itcl::body BotEditFrame::moveBotPts {_dname _obj _x _y _plist} {
    set plist2 {}
    foreach item $_plist {
	incr item -1
	lappend plist2 $item
    }
    eval $itk_option(-mged) pane_move_botpts_mode $_dname $_x $_y $_obj $plist2
}


::itcl::body BotEditFrame::moveBotPtsMode {_dname _obj _x _y} {
    $itk_option(-mged) clear_bot_callbacks

    set plist [$itk_component(vertTab) getSelectedRows]
    if {[llength $plist] < 2} {
	if {$mFrontPointsOnly} {
	    set zlist [ $::ArcherCore::application getZClipState]
	    set viewz [lindex $zlist 0]
	} else {
	    set viewz -1.0
	}

	moveBotPtMode $_dname $_obj $viewz $_x $_y
    } else {
	moveBotPts $_dname $_obj $_x $_y $plist
    }
}



# ------------------------------------------------------------
#                      PROTECTED METHODS
# ------------------------------------------------------------

::itcl::body BotEditFrame::buildUpperPanel {} {
    set parent [$this childsite]

    itk_component add botType {
	::ttk::label $parent.bottype \
	    -text "Bot:" \
	    -anchor e
    } {}
    itk_component add botName {
	::ttk::label $parent.botname \
	    -textvariable [::itcl::scope itk_option(-geometryObject)] \
	    -anchor w
    } {}

    itk_component add vertTabLF {
	::ttk::labelframe $parent.vertTabLF \
	    -text "Vertices" \
	    -labelanchor n
    } {}

    itk_component add vertTab {
	::cadwidgets::TkTable $itk_component(vertTabLF).vertTab \
	    [::itcl::scope mVertDetail] \
	    $mVertDetailHeadings \
	    -cursor arrow \
	    -height 0 \
	    -maxheight 2000 \
	    -width 0 \
	    -rows 100000 \
	    -colstretchmode unset \
	    -validate 1 \
	    -validatecommand [::itcl::code $this validateDetailEntry] \
	    -tablePopupHandler [::itcl::code $this handleDetailPopup] \
	    -multiSelectCallback [::itcl::code $this multiPointSelectCallback]
    } {}

    itk_component add edgeTabLF {
	::ttk::labelframe $parent.edgeTabLF \
	    -text "Edges" \
	    -labelanchor n
    } {}

    itk_component add edgeTab {
	::cadwidgets::TkTable $itk_component(edgeTabLF).edgeTab \
	    [::itcl::scope mEdgeDetail] \
	    $mEdgeDetailHeadings \
	    -cursor arrow \
	    -height 0 \
	    -maxheight 2000 \
	    -width 0 \
	    -rows 100000 \
	    -colstretchmode unset \
	    -validate 1 \
	    -validatecommand [::itcl::code $this validateDetailEntry] \
	    -tablePopupHandler [::itcl::code $this handleDetailPopup] \
	    -multiSelectCallback [::itcl::code $this multiEdgeSelectCallback]
    } {}

    itk_component add faceTabLF {
	::ttk::labelframe $parent.faceTabLF \
	    -text "Faces" \
	    -labelanchor n
    } {}

    itk_component add faceTab {
	::cadwidgets::TkTable $itk_component(faceTabLF).faceTab \
	    [::itcl::scope mFaceDetail] \
	    $mFaceDetailHeadings \
	    -cursor arrow \
	    -height 0 \
	    -maxheight 2000 \
	    -width 0 \
	    -rows 100000 \
	    -colstretchmode unset \
	    -validate 1 \
	    -validatecommand [::itcl::code $this validateDetailEntry] \
	    -tablePopupHandler [::itcl::code $this handleDetailPopup] \
	    -multiSelectCallback [::itcl::code $this multiFaceSelectCallback]
    } {}

    # Set width of column 0
    $itk_component(vertTab) width 0 3
    $itk_component(edgeTab) width 0 3
    $itk_component(faceTab) width 0 3

    pack $itk_component(vertTab) -expand yes -fill both
    pack $itk_component(edgeTab) -expand yes -fill both
    pack $itk_component(faceTab) -expand yes -fill both
}


::itcl::body BotEditFrame::buildLowerPanel {} {
    set parent [$this childsite lower]
    set i 1
    set row -1
    foreach label $mEditLabels {
	itk_component add editRB$i {
	    ::ttk::radiobutton $parent.editRB$i \
		-variable [::itcl::scope mEditMode] \
		-value $i \
		-text $label \
		-command [::itcl::code $this initEditState]
	} {}

	incr row
	grid $itk_component(editRB$i) -row $row -column 0 -sticky w

	incr i
    }

    itk_component add frontPointOnlyCB {
	::ttk::checkbutton $parent.frontPointOnlyCB \
	    -text "Front Points Only" \
	    -command [::itcl::code $this initEditState] \
	    -variable [::itcl::scope mFrontPointsOnly]
    } {}
    itk_component add ignoreMaxVertThreshCB {
	::ttk::checkbutton $parent.ignoreMaxVertThreshCB \
	    -text "Always Allow Edit" \
	    -command [::itcl::code $this reloadTables] \
	    -variable [::itcl::scope mIgnoreMaxVertThreshold]
    } {}
    itk_component add hlPointsCB {
	::ttk::checkbutton $parent.hlPointsCB \
	    -text "Highlight Points" \
	    -command [::itcl::code $this highlightCurrentBotElements] \
	    -variable ::GeometryEditFrame::mHighlightPoints
    } {}

    itk_component add maxVertThreshL {
	::ttk::label $parent.maxVertThreshL \
	    -anchor e \
	    -text "Max Vert Threshold"
    } {}
    itk_component add maxVertThreshE {
	::ttk::entry $parent.maxVertThreshE \
	    -width 12 \
	    -textvariable [::itcl::scope mMaxVertThreshold] \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDigit %P}
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

    bind $itk_component(maxVertThreshE) <Return> [::itcl::code $this reloadTables]
    bind $itk_component(pointSizeE) <Return> [::itcl::code $this updatePointSize]

    incr row
    grid rowconfigure $parent $row -weight 1
    incr row
    grid $itk_component(ignoreMaxVertThreshCB) -row $row -column 0 -sticky w
    incr row
    grid $itk_component(frontPointOnlyCB) -row $row -column 0 -sticky w
    incr row
    grid $itk_component(hlPointsCB) -row $row -column 0 -sticky w
    incr row
    grid $itk_component(maxVertThreshL) -column 0 -row $row -sticky e
    grid $itk_component(maxVertThreshE) -column 1 -row $row -sticky ew
    incr row
    grid $itk_component(pointSizeL) -column 0 -row $row -sticky e
    grid $itk_component(pointSizeE) -column 1 -row $row -sticky ew
    grid columnconfigure $parent 1 -weight 1
}


::itcl::body BotEditFrame::clearAllTables {} {
    $itk_option(-mged) data_axes points {}
    $itk_option(-mged) data_lines points {}

    set mCurrentBotPoints ""
    set mCurrentBotEdges ""
    set mCurrentBotFaces ""
    $itk_component(vertTab) unselectAllRows
    $itk_component(edgeTab) unselectAllRows
    $itk_component(faceTab) unselectAllRows
}


::itcl::body BotEditFrame::initEditState {} {
    if {$itk_option(-mged) == ""} {
	return
    }

    set mEditPCommand [::itcl::code $this p]
    set mEditParam1 ""
    highlightCurrentBotElements

    switch -- $mEditMode \
	$movePoints {
	    set mEditCommand moveBotPtsMode
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditLastTransMode $::ArcherCore::OBJECT_TRANSLATE_MODE
	} \
	$moveEdge {
	    set mEditCommand moveBotEdgeMode
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditLastTransMode $::ArcherCore::OBJECT_TRANSLATE_MODE
	} \
	$moveFace {
	    set mEditCommand moveBotFaceMode
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditLastTransMode $::ArcherCore::OBJECT_TRANSLATE_MODE
	} \
	$selectPoints {
	    set mEditCommand ""
	    set mEditClass ""
	    if {$mFrontPointsOnly} {
		set zlist [ $::ArcherCore::application getZClipState]
		set viewz [lindex $zlist 0]
	    } else {
		set viewz -1.0
	    }
	    $::ArcherCore::application initFindBotPoint $itk_option(-geometryObjectPath) 1 $viewz [::itcl::code $this botPointsSelectCallback]
	} \
	$selectEdges {
	    set mEditCommand ""
	    set mEditClass ""
	    if {$mFrontPointsOnly} {
		set zlist [ $::ArcherCore::application getZClipState]
		set viewz [lindex $zlist 0]
	    } else {
		set viewz -1.0
	    }
	    $::ArcherCore::application initFindBotEdge $itk_option(-geometryObjectPath) 1 $viewz [::itcl::code $this botEdgesSelectCallback]
	} \
	$selectFaces {
	    set mEditCommand ""
	    set mEditClass ""
	    $::ArcherCore::application initFindBotFace $itk_option(-geometryObjectPath) 1 [::itcl::code $this botFacesSelectCallback]
	} \
	$splitEdge {
	    set mEditCommand ""
	    set mEditClass ""
	    if {$mFrontPointsOnly} {
		set zlist [ $::ArcherCore::application getZClipState]
		set viewz [lindex $zlist 0]
	    } else {
		set viewz -1.0
	    }
	    $::ArcherCore::application initFindBotEdge $itk_option(-geometryObjectPath) 1 $viewz [::itcl::code $this botEdgeSplitCallback]
	} \
	$splitFace {
	    set mEditCommand ""
	    set mEditClass ""
	    $::ArcherCore::application initFindBotFace $itk_option(-geometryObjectPath) 1 [::itcl::code $this botFaceSplitCallback]
	} \
	default {
	    set mEditCommand ""
	    set mEditPCommand ""
	    set mEditParam1 ""
	}

    GeometryEditFrame::initEditState
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
		if {$mVertDetail($index,$PX_COL) != [lindex $val 0] ||
		    $mVertDetail($index,$PY_COL) != [lindex $val 1] ||
		    $mVertDetail($index,$PZ_COL) != [lindex $val 2]} {
		    set doUpdate 1
		}
	    }
	    {[Oo][0-9]+} {
		if {$mVertDetail($index,$OD_COL) != $val} {
		    set doUpdate 1
		}
	    }
	    {[Ii][0-9]+} {
		if {$mVertDetail($index,$ID_COL) != $val} {
		    set doUpdate 1
		}
	    }
	    {[Rr][0-9]+} {
		if {$mVertDetail($index,$RADIUS_COL) != $val} {
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


::itcl::body BotEditFrame::applyData {} {
}


::itcl::body BotEditFrame::botEdgesSelectCallback {_edge {_sync 1}} {
    set p0 [lindex $_edge 0]
    set p1 [lindex $_edge 1]

    if {$p0 > $p1} {
	set _edge [list $p1 $p0]
    }

    set ei [lsearch $mEdgeList $_edge]
    if {$ei < 0} {
	return
    }

    incr ei
    set eii [lsearch $mCurrentBotEdges $ei]
    if {$eii < 0} {
	lappend mCurrentBotEdges $ei
	$itk_component(edgeTab) selectRow $ei
	$itk_component(edgeTab) see "$ei,0"
    } else {
	set mCurrentBotEdges [lreplace $mCurrentBotEdges $eii $eii]
	$itk_component(edgeTab) unselectRow $ei
	$itk_component(edgeTab) see "$ei,0"
    }

    if {$_sync} {
	syncTablesWrtEdges
    }
}


::itcl::body BotEditFrame::botEdgeSplitCallback {_edge} {
    $itk_option(-mged) bot_edge_split $itk_option(-geometryObjectPath) $_edge
    $::ArcherCore::application setSave

    set gdata [lrange [$itk_option(-mged) get $itk_option(-geometryObject)] 1 end]
    loadTables $gdata

    # Select one of the newly created edges (i.e. one with the largest vertex index)
    set selist [lsort -integer -index 1 $mEdgeList]
    set edge [lindex $selist end]
    set ei [lsearch $mEdgeList $edge]

    if {1} {
	clearAllTables
    } else {
	incr ei
	set mCurrentBotEdges $ei
	$itk_component(edgeTab) unselectAllRows
	$itk_component(edgeTab) selectRow $ei
	$itk_component(edgeTab) see "$ei,0"

	syncTablesWrtEdges
    }
}


::itcl::body BotEditFrame::botFacesSelectCallback {_face} {
    # Increment _face to match its position in faceTab
    incr _face

    set findex [lsearch $mCurrentBotFaces $_face]

    if {$findex < 0} {
	lappend mCurrentBotFaces $_face
	$itk_component(faceTab) selectRow $_face
	$itk_component(faceTab) see "$_face,0"
    } else {
	set mCurrentBotFaces [lreplace $mCurrentBotFaces $findex $findex]
	$itk_component(faceTab) unselectRow $_face
	$itk_component(faceTab) see "$_face,0"
    }

    syncTablesWrtFaces
}


::itcl::body BotEditFrame::botFaceSplitCallback {_face} {
    $itk_option(-mged) bot_face_split $itk_option(-geometryObjectPath) $_face
    $::ArcherCore::application setSave

    set gdata [lrange [$itk_option(-mged) get $itk_option(-geometryObject)] 1 end]
    loadTables $gdata

    if {1} {
	clearAllTables
    } else {
	incr _face
	set mCurrentBotFaces $_face
	$itk_component(faceTab) unselectAllRows
	$itk_component(faceTab) selectRow $_face
	$itk_component(faceTab) see "$_face,0"

	syncTablesWrtFaces
    }
}


::itcl::body BotEditFrame::botPointSelectCallback {_pindex} {
    incr _pindex
    set mCurrentBotPoints $_pindex
    $itk_component(vertTab) selectSingleRow $_pindex
    $itk_component(vertTab) see "$_pindex,0"

    highlightCurrentBotElements
}


::itcl::body BotEditFrame::botPointsSelectCallback {_pindex} {
    incr _pindex

    set i [lsearch $mCurrentBotPoints $_pindex]
    if {$i < 0} {
	lappend mCurrentBotPoints $_pindex
	$itk_component(vertTab) selectRow $_pindex
	$itk_component(vertTab) see "$_pindex,0"
    } else {
	set mCurrentBotPoints [lreplace $mCurrentBotPoints $i $i]
	$itk_component(vertTab) unselectRow $_pindex
	$itk_component(vertTab) see "$_pindex,0"
    }

    syncTablesWrtPoints
}


::itcl::body BotEditFrame::detailBrowseCommand {_row _col} {
    if {![info exists mVertDetail($_row,0)]} {
	return 0
    }

    $itk_component(vertTab) see $_row,$_col
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


::itcl::body BotEditFrame::highlightCurrentBotElements {} {
    if {$itk_option(-mged) == "" ||
	[$itk_option(-mged) how $itk_option(-geometryObjectPath)] < 0} {
	return
    }

    set hpoints {}
    foreach index $mCurrentBotPoints {
	incr index -1
	lappend hpoints [lindex $mPointList $index]
    }

    set lsegPoints {}
    foreach edge $mCurrentBotEdges {
	set indexA $mEdgeDetail($edge,$A_COL)
	set indexB $mEdgeDetail($edge,$B_COL)
	lappend lsegPoints [lindex $mPointList $indexA]
	lappend lsegPoints [lindex $mPointList $indexB]
    }

    $itk_option(-mged) refresh_off
    set hlcolor [$::ArcherCore::application getRgbColor [$itk_option(-mged) cget -primitiveLabelColor]]

    $itk_option(-mged) data_axes draw $mHighlightPoints
    $itk_option(-mged) data_axes size $mHighlightPointSize
    eval $itk_option(-mged) data_axes color $hlcolor

    $itk_option(-mged) data_lines draw $mHighlightPoints
    eval $itk_option(-mged) data_lines color $hlcolor
    $itk_option(-mged) data_lines points $lsegPoints

    $itk_option(-mged) refresh_on

    $itk_option(-mged) data_axes points $hpoints
}


::itcl::body BotEditFrame::initPointHighlight {} {
    if {$itk_option(-mged) == ""} {
	return
    }

    $itk_option(-mged) data_axes draw $mHighlightPoints
}


::itcl::body BotEditFrame::loadTables {_gdata {_lflag 1}} {
    set vl [$itk_option(-mged) get $itk_option(-geometryObject) V]
    set vlen [llength $vl]
    if {$mIgnoreMaxVertThreshold || $vlen <= $mMaxVertThreshold} {
	set mShowTables 1
    } else {
	set mShowTables 0
    }

    if {![manageTables]} {
	return
    }

    if {!$_lflag} {
	set mPointList {}
	foreach {attr val} $_gdata {
	    switch -- $attr {
		"V" {
		    set index 1
		    foreach item $val {
			set mVertDetail($index,$SELECT_COL) ""
			set mVertDetail($index,$X_COL) [lindex $item 0]
			set mVertDetail($index,$Y_COL) [lindex $item 1]
			set mVertDetail($index,$Z_COL) [lindex $item 2]
			incr index

			lappend mPointList $item
		    }
		}
	    }
	}

	return
    }

    SetWaitCursor $::ArcherCore::application
    unset mVertDetail
    unset mEdgeDetail
    unset mFaceDetail

    set mVertDetail(active) ""
    set mEdgeDetail(active) ""
    set mFaceDetail(active) ""

    set col 0
    foreach heading $mVertDetailHeadings {
	set mVertDetail(0,$col) $heading
	incr col
    }

    set col 0
    foreach heading $mEdgeDetailHeadings {
	set mEdgeDetail(0,$col) $heading
	incr col
    }

    set col 0
    foreach heading $mFaceDetailHeadings {
	set mFaceDetail(0,$col) $heading
	incr col
    }

    set mPointList {}
    set mEdgeList {}
    set mFaceList {}
    foreach {attr val} $_gdata {
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
		    set mVertDetail($index,$SELECT_COL) ""
		    set mVertDetail($index,$X_COL) [lindex $item 0]
		    set mVertDetail($index,$Y_COL) [lindex $item 1]
		    set mVertDetail($index,$Z_COL) [lindex $item 2]
		    incr index

		    lappend mPointList $item
		}
	    }
	    "F" {
		set index 1
		foreach item $val {
		    set mFaceDetail($index,$SELECT_COL) ""
		    set mFaceDetail($index,$A_COL) [lindex $item 0]
		    set mFaceDetail($index,$B_COL) [lindex $item 1]
		    set mFaceDetail($index,$C_COL) [lindex $item 2]
		    incr index

		    lappend mFaceList [lsort -integer $item]
		}
	    }
	    default {
		# Shouldn't get here
		puts "Encountered bad one - $attr"
	    }
	}
    }

    set tmpEdgeList [$itk_option(-mged) get_bot_edges $itk_option(-geometryObject)]
    set index 1
    foreach item $tmpEdgeList {
	set mEdgeDetail($index,$SELECT_COL) ""
	set p0 [lindex $item 0]
	set p1 [lindex $item 1]

	if {$p0 > $p1} {
	    set mEdgeDetail($index,$A_COL) $p1
	    set mEdgeDetail($index,$B_COL) $p0
	    lappend mEdgeList [list $p1 $p0]
	} else {
	    set mEdgeDetail($index,$A_COL) $p0
	    set mEdgeDetail($index,$B_COL) $p1
	    lappend mEdgeList [list $p0 $p1]
	}
	incr index
    }

    SetNormalCursor $::ArcherCore::application
}


::itcl::body BotEditFrame::manageTables {} {
    if {$mShowTables} {
	set parent [$this childsite]

	set row 0
	grid $itk_component(botType) \
	    -row $row \
	    -column 0 \
	    -sticky nsew
	grid $itk_component(botName) \
	    -row $row \
	    -column 1 \
	    -columnspan 3 \
	    -sticky nsew
	incr row
	grid $itk_component(vertTabLF) -row $row -sticky nsew -columnspan 2
	grid rowconfigure $parent $row -weight 1
	incr row
	grid $itk_component(edgeTabLF) -row $row -sticky nsew -columnspan 2
	grid rowconfigure $parent $row -weight 1
	incr row
	grid $itk_component(faceTabLF) -row $row -sticky nsew -columnspan 2
	grid rowconfigure $parent $row -weight 1

	grid columnconfigure $parent 1 -weight 1

	set i 1
	foreach label $mEditLabels {
	    $itk_component(editRB$i) configure -state normal
	    incr i
	}

	return 1
    } else {
	clearEditState

	grid forget $itk_component(vertTabLF)
	grid forget $itk_component(edgeTabLF)
	grid forget $itk_component(faceTabLF)

	set i 1
	foreach label $mEditLabels {
	    $itk_component(editRB$i) configure -state disabled
	    incr i
	}

	return 0
    }
}


::itcl::body BotEditFrame::multiEdgeSelectCallback {} {
    set mCurrentBotEdges [$itk_component(edgeTab) getSelectedRows]
    syncTablesWrtEdges
}


::itcl::body BotEditFrame::multiFaceSelectCallback {} {
    set mCurrentBotFaces [$itk_component(faceTab) getSelectedRows]
    syncTablesWrtFaces
}


::itcl::body BotEditFrame::multiPointSelectCallback {} {
    set mCurrentBotPoints [$itk_component(vertTab) getSelectedRows]
    syncTablesWrtPoints
}


::itcl::body BotEditFrame::reloadTables {} {
    set gdata [lrange [$itk_option(-mged) get $itk_option(-geometryObject)] 1 end]
    loadTables $gdata
}


::itcl::body BotEditFrame::selectCurrentBotPoints {{_findEdges 1}} {
    $itk_component(vertTab) unselectAllRows

    set item ""
    foreach item $mCurrentBotPoints {
	$itk_component(vertTab) selectRow $item
    }

    if {$item != ""} {
	$itk_component(vertTab) see "$item,0"
    }

    syncTablesWrtPoints $_findEdges
}


::itcl::body BotEditFrame::syncTablesWrtPoints {{_findEdges 1}} {
    if {$_findEdges} {
	$itk_component(edgeTab) unselectAllRows
	set mCurrentBotEdges ""
    }

    $itk_component(faceTab) unselectAllRows
    set mCurrentBotFaces ""

    set plist {}
    set len [llength $mCurrentBotPoints]
    if {$len < 2} {
	highlightCurrentBotElements
	return
    }

    # Sorting the points insures the correct point order
    # within an edge for searching the edge list below.
    set sortedPoints {}
    foreach item [lsort -integer $mCurrentBotPoints] {
	incr item -1
	lappend sortedPoints $item
    }

    if {$_findEdges} {
	set i 1
	foreach ptA [lrange $sortedPoints 0 end-1] {
	    foreach ptB [lrange $sortedPoints $i end] {
		# Look for edge ($ptA $ptB)

		set edge_index [lsearch $mEdgeList [list $ptA $ptB]]
		if {$edge_index < 0} {
		    continue
		}

		incr edge_index
		lappend mCurrentBotEdges $edge_index
		$itk_component(edgeTab) selectRow $edge_index
		$itk_component(edgeTab) see "$edge_index,0"
	    }

	    incr i
	}
    }

    set len [llength $mCurrentBotEdges]
    if {$len < 2} {
	highlightCurrentBotElements
	return
    }

    set i 1
    foreach edgeA [lrange $mCurrentBotEdges 0 end-2] {
	set j [expr {$i + 1}]
	foreach edgeB [lrange $mCurrentBotEdges $i end-1] {
	    foreach edgeC [lrange $mCurrentBotEdges $j end] {
		set flist [list $mEdgeDetail($edgeA,$A_COL) $mEdgeDetail($edgeA,$B_COL)]
		lappend flist $mEdgeDetail($edgeB,$A_COL) $mEdgeDetail($edgeB,$B_COL)
		lappend flist $mEdgeDetail($edgeC,$A_COL) $mEdgeDetail($edgeC,$B_COL)

		set flist [lsort -integer -unique $flist]
		if {[llength $flist] != 3} {
		    continue
		}

		set face_index [lsearch $mFaceList $flist]
		if {$face_index < 0} {
		    continue
		}

		incr face_index
		lappend mCurrentBotFaces $face_index
		$itk_component(faceTab) selectRow $face_index
		$itk_component(faceTab) see "$face_index,0"
	    }

	    incr j
	}

	incr i
    }

    highlightCurrentBotElements
}


::itcl::body BotEditFrame::syncTablesWrtEdges {} {
    set mCurrentBotPoints {}
    foreach edge $mCurrentBotEdges {
	set indexA $mEdgeDetail($edge,$A_COL)
	set indexB $mEdgeDetail($edge,$B_COL)
	incr indexA
	incr indexB
	lappend mCurrentBotPoints $indexA $indexB
    }
    set mCurrentBotPoints [lsort -integer -unique $mCurrentBotPoints]

    selectCurrentBotPoints 0
}


::itcl::body BotEditFrame::syncTablesWrtFaces {} {
    # Clear the list of points and edges
    $itk_component(vertTab) unselectAllRows
    $itk_component(edgeTab) unselectAllRows
    set mCurrentBotPoints {}
    set mCurrentBotEdges {}

    # Compute the list of points and edges
    foreach face $mCurrentBotFaces {
	set indexA $mFaceDetail($face,$A_COL)
	set indexB $mFaceDetail($face,$B_COL)
	set indexC $mFaceDetail($face,$C_COL)

	# Process edge AB
	if {$indexA < $indexB} {
	    set edgeAB [list $indexA $indexB]
	} else {
	    set edgeAB [list $indexB $indexA]
	}
	set ei [lsearch $mEdgeList $edgeAB]
	incr ei
	lappend mCurrentBotEdges $ei
	$itk_component(edgeTab) selectRow $ei
	$itk_component(edgeTab) see "$ei,0"

	# Process edge BC
	if {$indexB < $indexC} {
	    set edgeBC [list $indexB $indexC]
	} else {
	    set edgeBC [list $indexC $indexB]
	}
	set ei [lsearch $mEdgeList $edgeBC]
	incr ei
	lappend mCurrentBotEdges $ei
	$itk_component(edgeTab) selectRow $ei
	$itk_component(edgeTab) see "$ei,0"

 	# Process edge AC
	if {$indexA < $indexC} {
	    set edgeAC [list $indexA $indexC]
	} else {
	    set edgeAC [list $indexC $indexA]
	}
	set ei [lsearch $mEdgeList $edgeAC]
	incr ei
	lappend mCurrentBotEdges $ei
	$itk_component(edgeTab) selectRow $ei
	$itk_component(edgeTab) see "$ei,0"

	incr indexA
	incr indexB
	incr indexC
	lappend mCurrentBotPoints $indexA $indexB $indexC
	$itk_component(vertTab) selectRow $indexC
	$itk_component(vertTab) selectRow $indexB
	$itk_component(vertTab) selectRow $indexA
	$itk_component(vertTab) see "$indexA,0"
    }
    set mCurrentBotEdges [lsort -integer -unique $mCurrentBotEdges]
    set mCurrentBotPoints [lsort -integer -unique $mCurrentBotPoints]

    highlightCurrentBotElements
}


::itcl::body BotEditFrame::validateDetailEntry {_row _col _newval _clientdata} {
    return 0

    if {![info exists mVertDetail($_row,0)]} {
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
