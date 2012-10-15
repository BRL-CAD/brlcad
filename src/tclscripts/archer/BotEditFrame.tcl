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
	common A_COL 1
	common B_COL 2
	common C_COL 3
	common X_COL 1
	common Y_COL 2
	common Z_COL 3

	common movePoint 1
	common moveEdge 2
	common moveFace 3
	common selectPoint 4
	common selectEdge 5
	common selectFace 6
	common splitEdge 7
	common splitFace 8

	common mVertDetailHeadings {{} X Y Z}
	common mEdgeDetailHeadings {{} A B}
	common mFaceDetailHeadings {{} A B C}
	common mEditLabels {
	    {Move Point}
	    {Move Edge}
	    {Move Face}
	    {Select Point}
	    {Select Edge}
	    {Select Face}
	    {Split Edge}
	    {Split Face}
	}

	# Override what's in GeometryEditFrame
	method initGeometry {gdata}
	method updateGeometry {}
	method createGeometry {_name}
	method p {obj args}
	method moveBotEdgeMode {_dname _obj _x _y}
	method moveBotElement {_dname _obj _x _y}
	method moveBotFaceMode {_dname _obj _x _y}
	method moveBotPtMode {_dname _obj _x _y}
	method moveBotPt {_dname _obj _x _y}
    }

    protected {
	variable mVertDetail
	variable mEdgeDetail
	variable mFaceDetail
	variable mEdgeList
	variable mCurrentBotEdge 1
	variable mCurrentBotPoint 1
	variable mCurrentBotFace 1

	# Methods used by the constructor
	# override methods in GeometryEditFrame
	method buildUpperPanel
	method buildLowerPanel

	# Override what's in GeometryEditFrame
	method updateGeometryIfMod {}
	method initEditState {}

	method applyData {}
	method botEdgeSelectCallback {_edge}
	method botEdgeSplitCallback {_elist}
	method botFaceMoveCallback {_face}
	method botFaceSelectCallback {_face}
	method botFaceSplitCallback {_face}
	method botPointSelectCallback {_pindex}
	method detailBrowseCommand {_row _col}
	method handleDetailPopup {_index _X _Y}
	method handleEnter {_row _col}
	method singleEdgeSelectCallback {_pindex}
	method singleFaceSelectCallback {_pindex}
	method singlePointSelectCallback {_pindex}
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
    if {$itk_option(-mged) == "" ||
	$itk_option(-geometryObject) == ""} {
	return
    }

    unset mVertDetail
    unset mFaceDetail

    set mVertDetail(active) ""
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
		    set mVertDetail($index,$SELECT_COL) ""
		    set mVertDetail($index,$X_COL) [lindex $item 0]
		    set mVertDetail($index,$Y_COL) [lindex $item 1]
		    set mVertDetail($index,$Z_COL) [lindex $item 2]
		    incr index
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
		}
	    }
	    default {
		# Shouldn't get here
		puts "Encountered bad one - $attr"
	    }
	}
    }

    set tmpEdgeList [$itk_option(-mged) get_bot_edges $itk_option(-geometryObject)]
    set mEdgeList {}
    set index 1
    foreach item $tmpEdgeList {
	set mEdgeDetail($index,$SELECT_COL) ""
	set e0 [lindex $item 0]
	set e1 [lindex $item 1]

	if {$e0 > $e1} {
	    set mEdgeDetail($index,$A_COL) $e1
	    set mEdgeDetail($index,$B_COL) $e0
	    lappend mEdgeList [list $e1 $e0]
	} else {
	    set mEdgeDetail($index,$A_COL) $e0
	    set mEdgeDetail($index,$B_COL) $e1
	    lappend mEdgeList [list $e0 $e1]
	}
	incr index
    }

    GeometryEditFrame::initGeometry $gdata

    if {$itk_option(-geometryObject) != $mPrevGeometryObject} {
	set mCurrentBotPoint 1
	set mPrevGeometryObject $itk_option(-geometryObject)
    }

#    botPointSelectCallback [expr {$mCurrentBotPoint - 1}]
    botFaceSelectCallback [expr {$mCurrentBotFace - 1}]
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
    set mCurrentBotEdge [$itk_option(-mged) pane_mouse_find_bot_edge $_dname $_obj $_x $_y]
    eval $itk_option(-mged) pane_move_botpts_mode $_dname $_x $_y $_obj $mCurrentBotEdge
}


::itcl::body BotEditFrame::moveBotElement {_dname _obj _x _y} {
    switch -- $mEditMode \
	$movePoint {
	    moveBotPt $_dname $_obj $_x $_y
	} \
	$moveEdge {
	    $::ArcherCore::application putString "This mode is not ready for edges."
	} \
	$moveFace {
	    $::ArcherCore::application putString "This mode is not ready for faces."
	}
}


::itcl::body BotEditFrame::moveBotFaceMode {_dname _obj _x _y} {
    $itk_option(-mged) clear_bot_callbacks
    set zlist [ $::ArcherCore::application getZClipState]
    set viewz [lindex $zlist 0]

    set face [$itk_option(-mged) pane_mouse_find_bot_face $_dname $_obj $viewz $_x $_y]
    if {$face == ""} {
	return
    }

    set flist [$itk_option(-mged) get $_obj F]
    eval $itk_option(-mged) pane_move_botpts_mode $_dname $_x $_y $_obj [lindex $flist $face]

    botFaceSelectCallback $face
}


::itcl::body BotEditFrame::moveBotPtMode {_dname _obj _x _y} {
    $itk_option(-mged) clear_bot_callbacks
    set pindex [$itk_option(-mged) pane_mouse_find_botpt $_dname $_obj $_x $_y]
    $itk_option(-mged) pane_move_botpt_mode $_dname $_obj $pindex $_x $_y

    botPointSelectCallback $pindex
}


::itcl::body BotEditFrame::moveBotPt {_dname _obj _x _y} {
    $itk_option(-mged) pane_mouse_move_botpt $_dname $_obj [expr {$mCurrentBotPoint - 1}] $_x $_y
}


# ------------------------------------------------------------
#                      PROTECTED METHODS
# ------------------------------------------------------------

::itcl::body BotEditFrame::buildUpperPanel {} {
    set parent [$this childsite]

    itk_component add vertTabLF {
	::ttk::labelframe $parent.vertTabLF \
	    -text "Bot Vertices" \
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
	    -singleSelectCallback [::itcl::code $this singlePointSelectCallback]
    } {}
#	    -entercommand [::itcl::code $this handleEnter]
#	    -browsecommand [::itcl::code $this detailBrowseCommand %r %c]
#	    -dataCallback [::itcl::code $this applyData]


    itk_component add edgeTabLF {
	::ttk::labelframe $parent.edgeTabLF \
	    -text "Bot Edges" \
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
	    -singleSelectCallback [::itcl::code $this singleEdgeSelectCallback]
    } {}

    itk_component add faceTabLF {
	::ttk::labelframe $parent.faceTabLF \
	    -text "Bot Faces" \
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
	    -singleSelectCallback [::itcl::code $this singleFaceSelectCallback]
    } {}

    # Set width of column 0
    $itk_component(vertTab) width 0 3
    $itk_component(edgeTab) width 0 3
    $itk_component(faceTab) width 0 3

    pack $itk_component(vertTab) -expand yes -fill both
    pack $itk_component(edgeTab) -expand yes -fill both
    pack $itk_component(faceTab) -expand yes -fill both

    set row 0
    grid $itk_component(vertTabLF) -row $row -sticky nsew
    grid rowconfigure $parent $row -weight 1
    incr row
    grid $itk_component(edgeTabLF) -row $row -sticky nsew
    grid rowconfigure $parent $row -weight 1
    incr row
    grid $itk_component(faceTabLF) -row $row -sticky nsew
    grid rowconfigure $parent $row -weight 1

    grid columnconfigure $parent 0 -weight 1
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

::itcl::body BotEditFrame::initEditState {} {
#    set mEditCommand pscale
#    set mEditClass $EDIT_CLASS_SCALE
#    configure -valueUnits "mm"

    set mEditPCommand [::itcl::code $this p]
    set mEditParam1 ""

    switch -- $mEditMode \
	$movePoint {
	    set mEditCommand moveBotPtMode
	    set mEditClass $EDIT_CLASS_TRANS
	} \
	$moveEdge {
	    set mEditCommand moveBotEdgeMode
	    set mEditClass $EDIT_CLASS_TRANS
	} \
	$moveFace {
	    set mEditCommand moveBotFaceMode
	    set mEditClass $EDIT_CLASS_TRANS
	} \
	$selectPoint {
	    set mEditCommand ""
	    set mEditClass ""
	    $::ArcherCore::application initFindBotPoint $itk_option(-geometryObjectPath) 1 [::itcl::code $this botPointSelectCallback]
	} \
	$selectEdge {
	    set mEditCommand ""
	    set mEditClass ""
	    $::ArcherCore::application initFindBotEdge $itk_option(-geometryObjectPath) 1 [::itcl::code $this botEdgeSelectCallback]
	} \
	$selectFace {
	    set mEditCommand ""
	    set mEditClass ""
	    $::ArcherCore::application initFindBotFace $itk_option(-geometryObjectPath) 1 [::itcl::code $this botFaceSelectCallback]
	} \
	$splitEdge {
	    set mEditCommand ""
	    set mEditClass ""
	    $::ArcherCore::application initFindBotEdge $itk_option(-geometryObjectPath) 1 [::itcl::code $this botEdgeSplitCallback]
	} \
	$splitFace {
	    set mEditCommand ""
	    set mEditClass ""
	    $::ArcherCore::application initFindBotFace $itk_option(-geometryObjectPath) 1 [::itcl::code $this botFaceSplitCallback]
	}

    GeometryEditFrame::initEditState
}


::itcl::body BotEditFrame::applyData {} {
}


::itcl::body BotEditFrame::botEdgeSelectCallback {_edge} {
    set e0 [lindex $_edge 0]
    set e1 [lindex $_edge 1]

    if {$e0 > $e1} {
	set _edge [list $e1 $e0]
    }

    set edge_index [lsearch $mEdgeList $_edge]
    if {$edge_index < 0} {
	return
    }

    incr edge_index
    set mCurrentBotEdge $edge_index
    $itk_component(edgeTab) selectSingleRow $edge_index
    $itk_component(edgeTab) see "$edge_index,0"

    set pointIndex $mEdgeDetail($edge_index,$A_COL)
    botPointSelectCallback $pointIndex
}


::itcl::body BotEditFrame::botEdgeSplitCallback {_elist} {
    set mCurrentBotEdge $_elist
    $itk_option(-mged) bot_edge_split $itk_option(-geometryObjectPath) $mCurrentBotEdge
    $::ArcherCore::application setSave
}


::itcl::body BotEditFrame::botFaceMoveCallback {_face} {
    incr _face
    set mCurrentBotFace $_face
    $itk_component(faceTab) selectSingleRow $_face
    $itk_component(faceTab) see "$_face,0"
}


::itcl::body BotEditFrame::botFaceSelectCallback {_face} {
    incr _face
    set mCurrentBotFace $_face
    $itk_component(faceTab) selectSingleRow $_face
    $itk_component(faceTab) see "$_face,0"

    botEdgeSelectCallback [list $mFaceDetail($_face,$A_COL) $mFaceDetail($_face,$B_COL)]
}


::itcl::body BotEditFrame::botFaceSplitCallback {_face} {
    incr _face
    set mCurrentBotFace $_face
    $itk_component(faceTab) selectSingleRow $_face
    $itk_component(faceTab) see "$_face,0"
    $itk_option(-mged) bot_face_split $itk_option(-geometryObjectPath) [expr {$mCurrentBotFace - 1}]
    $::ArcherCore::application setSave
}


::itcl::body BotEditFrame::botPointSelectCallback {_pindex} {
    incr _pindex
    set mCurrentBotPoint $_pindex
    $itk_component(vertTab) selectSingleRow $_pindex
    $itk_component(vertTab) see "$_pindex,0"
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

::itcl::body BotEditFrame::singleEdgeSelectCallback {_pindex} {
    set mCurrentBotEdge $_pindex
    initEditState
}

::itcl::body BotEditFrame::singleFaceSelectCallback {_pindex} {
    set mCurrentBotFace $_pindex
    initEditState
}

::itcl::body BotEditFrame::singlePointSelectCallback {_pindex} {
    set mCurrentBotPoint $_pindex
    initEditState
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
