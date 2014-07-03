#                S K E T C H E D I T F R A M E . T C L
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
# Description:
#    The class for editing sketches within Archer.
#
##############################################################

::itcl::class SketchEditFrame {
    inherit GeometryEditFrame

    itk_option define -units units Units ""

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

	common moveArbitrary 1
	common createLine 2
	common createCircle 3
	common createArc 4
	common createBezier 5

	common EPSILON 0.00000000000000001

	common mVertDetailHeadings {{} X Y Z}
	common mEdgeDetailHeadings {{} A B}
	common mFaceDetailHeadings {{} A B C}
	common mEditLabels {
	    "Move\t\tm"
	    "Create Line\tl"
	    "Create Circle\tc"
	    "Create Arc\t\ta"
	    "Create Bezier\tb"
	}

	common pi2 [expr {4.0 * asin( 1.0 )}]
	common rad2deg  [expr {360.0 / $pi2}]

	method get_scale {}
	method get_tobase {}
	method get_vlist {}
	method set_radius {_radius}

	method selectSketchPts {_plist}

	# Override what's in GeometryEditFrame
	method initGeometry {_gdata}
	method updateGeometry {}
	method createGeometry {_name}
	method p {obj args}
    }

    protected {
	variable mSegments {}
	variable mVL {}
	variable mSL {}

	variable mVx ""
	variable mVy ""
	variable mVz ""
	variable mAx ""
	variable mAy ""
	variable mAz ""
	variable mBx ""
	variable mBy ""
	variable mBz ""

	variable mAnchorX 0
	variable mAnchorXLabel "Anchor X (mm)"
	variable mAnchorY 0
	variable mAnchorYLabel "Anchor Y (mm)"
	variable mDrawGrid 1
	variable mSnapGrid 1
	variable mMajorGridSpacing 5
	variable mMinorGridSpacing 0.5
	variable mMinorGridSpacingLabel "Minor Grid Spacing (mm)"
	variable mPrevMouseX 0
	variable mPrevMouseY 0
	variable mCanvasCenterX 1
	variable mCanvasCenterY 1
	variable mCanvasHeight 1
	variable mCanvasInvWidth 1
	variable mCanvasWidth 1
	variable mDetailMode 0
	variable mPickTol 11
	variable mLastIndex -1
	variable mEscapeCreate 1
	variable mCallingFromEndBezier 0
	variable myscale 1.0
	variable mScrollCenterX 0
	variable mScrollCenterY 0
	variable vert_radius 3
	variable tobase 1.0
	variable tolocal 1.0
	variable x_coord 0.0
	variable y_coord 0.0
	variable radius 0.0
	variable index1 -1
	variable index2 -1
	variable needs_saving 0
	variable move_start_x
	variable move_start_y
	variable curr_seg ""
	variable curr_vertex ""
	variable save_entry
	variable angle
	variable bezier_indices ""
	variable selection_mode ""

	variable mIgnoreMotion 0
	variable mVertDetail
	variable mSegmentDetail
	variable mFaceDetail
	variable mPointList {}
	variable mEdgeList {}
	variable mFaceList {}
	variable mCurrentSketchPoints ""
	variable mCurrentSketchEdges ""
	variable mCurrentSketchFaces ""
	variable mFrontPointsOnly 1

	# Methods used by the constructor
	# override methods in GeometryEditFrame
	method buildUpperPanel
	method buildLowerPanel

	# Override what's in GeometryEditFrame
	method clearAllTables {}
	method initEditState {}
	method updateGeometryIfMod {}

	method applyData {}
	method bboxVerts {}
	method createSegments {}
	method detailBrowseCommand {_row _col}
	method drawSegments {}
	method drawVertices {}
	method handleDetailPopup {_index _X _Y}
	method handleEnter {_row _col}
	method highlightCurrentSketchElements {}
	method initCanvas {_gdata}
	method initPointHighlight {}
	method initScrollRegion {}
	method initSketchData {_gdata}
	method loadTables {_gdata}
	method redrawSegments {}

	method build_grid {_x1 _y1 _x2 _y2 _final_sizing {_adjust_spacing 1}}
	method circle_3pt {_x1 _y1 _x2 _y2 _x3 _y3 _cx_out _cy_out}
	method clear_canvas_bindings {}
	method continue_circle {_segment _state _coord_type _mx _my}
	method continue_circle_pick {_segment _mx _my}
	method continue_line {_segment _state _coord_type _mx _my}
	method continue_line_pick {_segment _state _mx _my}
	method continue_move {_state _sx _sy}
	method create_arc {}
	method create_bezier {}
	method create_circle {}
	method create_line {}
	method do_scale {_sf _gflag _final {_adjust_spacing 1} {_epsilon 0.000001}}
	method do_snap_sketch {_cx _cy}
	method do_translate {_dx _dy _gflag _final}
	method delete_selected {}
	method end_arc {_mx _my}
	method end_arc_radius_adjust {_segment _mx _my}
	method end_bezier {_segment _cflag}
	method end_scale {}
	method fix_vertex_references {_unused_vindices}
	method handle_configure {}
	method handle_escape {}
	method handle_scale {_mx _my _final}
	method handle_translate {_mx _my _final}
	method item_pick_highlight {_mx _my}
	method mouse_to_sketch {_mx _my}
	method next_bezier {_segment _mx _my}
	method pick_arbitrary {_mx _my}
	method pick_segment {_mx _my}
	method pick_vertex {_mx _my {_tag ""}}
	method seg_delete {_sx _sy _vflag}
	method seg_pick_highlight {_sx _sy}
	method set_canvas {}
	method setup_move_arbitrary {}
	method setup_move_segment {}
	method setup_move_selected {}
	method start_arc_radius_adjust {_segment _mx _my}
	method start_arc {_x _y}
	method start_bezier {_x _y}
	method start_circle {_coord_type _x _y}
	method start_line {_x _y}
	method start_line_guts {{_mx ""} {_my ""}}
	method start_move_arbitrary {_sx _sy _rflag}
	method start_move_segment {_sx _sy _rflag}
	method start_move_selected {_sx _sy}
	method start_move_selected2 {_sx _sy}
	method start_scale {_mx _my}
	method start_seg_pick {}
	method start_translate {_mx _my}
	method start_vert_pick {}
	method tag_selected_verts {}
	method unhighlight_selected {}
	method updateGrid {}
	method validateMajorGridSpacing {_spacing}
	method validateMinorGridSpacing {_spacing}
	method validatePickTol {_tol}
	method vert_delete {_sx _sy}
	method vert_is_used {_vindex}
	method vert_pick_highlight {_sx _sy}
	method write_sketch_to_db {}
    }

    private {}
}


# ------------------------------------------------------------
#                      CONSTRUCTOR
# ------------------------------------------------------------

::itcl::body SketchEditFrame::constructor {args} {
    eval itk_initialize $args

    set parent [$::ArcherCore::application getCanvasArea]

    itk_component add canvas {
	::canvas $parent.sketchcanvas
    }

    bind $itk_component(canvas) <Enter> {::focus %W}
    bind $itk_component(canvas) <Escape> [::itcl::code $this handle_escape]
    bind $itk_component(canvas) <Configure> [::itcl::code $this handle_configure]
}

# ------------------------------------------------------------
#                        OPTIONS
# ------------------------------------------------------------

::itcl::configbody SketchEditFrame::units {
    set mAnchorXLabel "Anchor X ($itk_option(-units))"
    set mAnchorYLabel "Anchor Y ($itk_option(-units))"
    set mMinorGridSpacingLabel "Minor Grid Spacing ($itk_option(-units))"

    set tolocal [$::ArcherCore::application gedCmd base2local]
    set tobase [$::ArcherCore::application gedCmd local2base]

    if {$itk_option(-geometryObject) != ""} {
	set_canvas
    }
}


# ------------------------------------------------------------
#                      PUBLIC METHODS
# ------------------------------------------------------------



::itcl::body SketchEditFrame::get_scale {} {
    return $myscale
}


::itcl::body SketchEditFrame::get_tobase {} {
    return $tobase
}


::itcl::body SketchEditFrame::get_vlist {} {
    return $mVL
}


::itcl::body SketchEditFrame::set_radius {_radius} {
    set radius $_radius
}


::itcl::body SketchEditFrame::selectSketchPts {_plist} {
    foreach item $_plist {
	incr item

	if {[lsearch $mCurrentSketchPoints $item] == -1} {
	    lappend mCurrentSketchPoints $item
	}
    }

    selectCurrentSketchPoints
}


## - initGeometry
#
# Initialize the variables containing the object's specification.
#
::itcl::body SketchEditFrame::initGeometry {_gdata} {
    if {$itk_option(-mged) == "" ||
	$itk_option(-geometryObject) == ""} {
	return
    }

    set_canvas
}


::itcl::body SketchEditFrame::updateGeometry {} {
    if {$itk_option(-mged) == "" ||
	$itk_option(-geometryObject) == ""} {
	return
    }

#    $itk_option(-mged) adjust $itk_option(-geometryObject) \
	V [list $mVx $mVy $mVz] \
	A [list $mAx $mAy $mAz] \
	B [list $mBx $mBy $mBz]
    write_sketch_to_db

    GeometryEditFrame::updateGeometry
}


::itcl::body SketchEditFrame::createGeometry {obj} {
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


::itcl::body SketchEditFrame::p {obj args} {
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



# ------------------------------------------------------------
#                      PROTECTED METHODS
# ------------------------------------------------------------

::itcl::body SketchEditFrame::buildUpperPanel {} {
    set parent [$this childsite]

    itk_component add sketchType {
	::ttk::label $parent.sketchtype \
	    -text "Sketch:" \
	    -anchor e
    } {}
    itk_component add sketchName {
	::ttk::label $parent.sketchname \
	    -textvariable [::itcl::scope itk_option(-geometryObject)] \
	    -anchor w
    } {}

    itk_component add sketchDMode {
	::ttk::checkbutton $parent.sketchdmode \
	    -command [::itcl::code $this set_canvas] \
	    -text "Detail" \
	    -variable [::itcl::scope mDetailMode]
    } {}

    # Create header labels
    itk_component add sketchXL {
	::ttk::label $parent.sketchXL \
	    -text "X"
    } {}
    itk_component add sketchYL {
	::ttk::label $parent.sketchYL \
	    -text "Y"
    } {}
    itk_component add sketchZL {
	::ttk::label $parent.sketchZL \
	    -text "Z"
    } {}

    # create widgets for vertices
    itk_component add sketchVL {
	::ttk::label $parent.sketchVL \
	    -text "V:" \
	    -anchor e
    } {}
    itk_component add sketchVxE {
	::ttk::entry $parent.sketchVxE \
	    -textvariable [::itcl::scope mVx] \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add sketchVyE {
	::ttk::entry $parent.sketchVyE \
	    -textvariable [::itcl::scope mVy] \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add sketchVzE {
	::ttk::entry $parent.sketchVzE \
	    -textvariable [::itcl::scope mVz] \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add sketchVUnitsL {
	::ttk::label $parent.sketchVUnitsL \
	    -textvariable [::itcl::scope itk_option(-units)] \
	    -anchor e
    } {}

    itk_component add sketchAL {
	::ttk::label $parent.sketchAL \
	    -text "A:" \
	    -anchor e
    } {}
    itk_component add sketchAxE {
	::ttk::entry $parent.sketchAxE \
	    -textvariable [::itcl::scope mAx] \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add sketchAyE {
	::ttk::entry $parent.sketchAyE \
	    -textvariable [::itcl::scope mAy] \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add sketchAzE {
	::ttk::entry $parent.sketchAzE \
	    -textvariable [::itcl::scope mAz] \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add sketchBL {
	::ttk::label $parent.sketchBL \
	    -text "B:" \
	    -anchor e
    } {}
    itk_component add sketchBxE {
	::ttk::entry $parent.sketchBxE \
	    -textvariable [::itcl::scope mBx] \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add sketchByE {
	::ttk::entry $parent.sketchByE \
	    -textvariable [::itcl::scope mBy] \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}
    itk_component add sketchBzE {
	::ttk::entry $parent.sketchBzE \
	    -textvariable [::itcl::scope mBz] \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}

    itk_component add sketchHintsKeyL {
	::ttk::label $parent.sketchHintsKeyL \
	    -text "
Key and Button Hints:
a\tArc mode
b\tBezier mode
c\tCircle mode
l\tLine mode
m\tMove mode

Escape\tNew contour
BSpace\tDelete selected
d\tDelete selected
Delete\tDelete selected" \
	    -anchor w
    } {}

    itk_component add sketchHintsBtnL {
	::ttk::label $parent.sketchHintsBtnL \
	    -text "
Button-1
\tUsed to select,
\tmove and create
\tsketch objects
Shift-Button-1
\tMulti-select while
\tin move mode
Lock-Ctrl-Shift-Button-1
\tScale view
Lock-Shift-Button-1
\tTranslate view" \
	    -anchor w
    } {}

    set row 0
    grid $itk_component(sketchType) \
	-row $row \
	-column 0 \
	-sticky nsew
    grid $itk_component(sketchName) \
	-row $row \
	-column 1 \
	-columnspan 3 \
	-sticky nsew
    grid $itk_component(sketchDMode) \
	-row $row \
	-column 3 \
	-columnspan 2 \
	-sticky nse
    incr row
    grid x $itk_component(sketchXL) \
	$itk_component(sketchYL) \
	$itk_component(sketchZL) \
	-row $row
    incr row
    grid $itk_component(sketchVL) \
	$itk_component(sketchVxE) \
	$itk_component(sketchVyE) \
	$itk_component(sketchVzE) \
	$itk_component(sketchVUnitsL) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(sketchAL) \
	$itk_component(sketchAxE) \
	$itk_component(sketchAyE) \
	$itk_component(sketchAzE) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(sketchBL) \
	$itk_component(sketchBxE) \
	$itk_component(sketchByE) \
	$itk_component(sketchBzE) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(sketchHintsKeyL) \
	-columnspan 5 \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(sketchHintsBtnL) \
	-columnspan 5 \
	-row $row \
	-sticky nsew

    grid columnconfigure $parent 1 -weight 1
    grid columnconfigure $parent 2 -weight 1
    grid columnconfigure $parent 3 -weight 1
#    grid columnconfigure $parent 4 -weight 1

    # Set up bindings
    bind $itk_component(sketchVxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(sketchVyE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(sketchVzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(sketchAxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(sketchAyE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(sketchAzE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(sketchBxE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(sketchByE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(sketchBzE) <Return> [::itcl::code $this updateGeometryIfMod]
}


::itcl::body SketchEditFrame::buildLowerPanel {} {
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

    itk_component add drawgridCB {
	::ttk::checkbutton $parent.drawgrid \
	    -text "Draw Grid" \
	    -variable [::itcl::scope mDrawGrid] \
	    -command [::itcl::code $this updateGrid]
    } {}

    itk_component add snapgridCB {
	::ttk::checkbutton $parent.snapgrid \
	    -text "Snap Grid" \
	    -variable [::itcl::scope mSnapGrid]
    } {}

    itk_component add anchorXL {
	::ttk::label $parent.anchorxL \
	    -anchor e \
	    -textvariable [::itcl::scope mAnchorXLabel]
    } {}
    itk_component add anchorXE {
	::ttk::entry $parent.anchorxE \
	    -width 12 \
	    -textvariable [::itcl::scope mAnchorX] \
	    -validate key \
	    -validatecommand  {::cadwidgets::Ged::validateDouble %P}
    } {}

    itk_component add anchorYL {
	::ttk::label $parent.anchoryL \
	    -anchor e \
	    -textvariable [::itcl::scope mAnchorYLabel]
    } {}
    itk_component add anchorYE {
	::ttk::entry $parent.anchoryE \
	    -width 12 \
	    -textvariable [::itcl::scope mAnchorY] \
	    -validate key \
	    -validatecommand  {::cadwidgets::Ged::validateDouble %P}
    } {}

    itk_component add majorgridL {
	::ttk::label $parent.majorgridL \
	    -anchor e \
	    -text "Major Grid Spacing (ticks)"
    } {}
    itk_component add majorgridE {
	::ttk::entry $parent.majorgridE \
	    -width 12 \
	    -textvariable [::itcl::scope mMajorGridSpacing] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateMajorGridSpacing %P]
    } {}

    itk_component add minorgridL {
	::ttk::label $parent.minorgridL \
	    -anchor e \
	    -textvariable [::itcl::scope mMinorGridSpacingLabel]
    } {}
    itk_component add minorgridE {
	::ttk::entry $parent.minorgridE \
	    -width 12 \
	    -textvariable [::itcl::scope mMinorGridSpacing] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateMinorGridSpacing %P]
    } {}

    itk_component add picktolL {
	::ttk::label $parent.picktolL \
	    -anchor e \
	    -text "Point Pick Tol (pixels)"
    } {}
    itk_component add picktolE {
	::ttk::entry $parent.picktolE \
	    -width 12 \
	    -textvariable [::itcl::scope mPickTol] \
	    -validate key \
	    -validatecommand [::itcl::code $this validatePickTol %P]
    } {}

    incr row
    grid rowconfigure $parent $row -weight 1
    incr row
    grid $itk_component(drawgridCB) -column 0 -row $row -sticky w
    incr row
    grid $itk_component(snapgridCB) -column 0 -row $row -sticky w
    incr row
    grid $itk_component(anchorXL) -column 0 -row $row -sticky w
    grid $itk_component(anchorXE) -column 1 -row $row -sticky ew
    incr row
    grid $itk_component(anchorYL) -column 0 -row $row -sticky w
    grid $itk_component(anchorYE) -column 1 -row $row -sticky ew
    incr row
    grid $itk_component(majorgridL) -column 0 -row $row -sticky w
    grid $itk_component(majorgridE) -column 1 -row $row -sticky ew
    incr row
    grid $itk_component(minorgridL) -column 0 -row $row -sticky w
    grid $itk_component(minorgridE) -column 1 -row $row -sticky ew
    incr row
    grid $itk_component(picktolL) -column 0 -row $row -sticky w
    grid $itk_component(picktolE) -column 1 -row $row -sticky ew
    grid columnconfigure $parent 1 -weight 1

    bind $itk_component(anchorXE) <Return> [::itcl::code $this updateGrid]
    bind $itk_component(anchorYE) <Return> [::itcl::code $this updateGrid]
    bind $itk_component(majorgridE) <Return> [::itcl::code $this updateGrid]
    bind $itk_component(minorgridE) <Return> [::itcl::code $this updateGrid]
}


::itcl::body SketchEditFrame::clearAllTables {} {
    $itk_option(-mged) data_axes points {}
    $itk_option(-mged) data_lines points {}

    set mCurrentSketchPoints ""
    set mCurrentSketchEdges ""
    set mCurrentSketchFaces ""
}


::itcl::body SketchEditFrame::initEditState {} {
    if {$itk_option(-mged) == ""} {
	return
    }

#    set mEditPCommand [::itcl::code $this p]
    set mEditPCommand ""
    set mEditParam1 ""
    set mEditCommand ""
    set mEditClass ""
    highlightCurrentSketchElements

    switch -- $mEditMode \
	$moveArbitrary {
	    setup_move_arbitrary
	} \
	$createLine {
	    create_line
	} \
	$createCircle {
	    create_circle
	} \
	$createArc {
	    create_arc
	} \
	$createBezier {
	    create_bezier
	}

    GeometryEditFrame::initEditState
}


::itcl::body SketchEditFrame::updateGeometryIfMod {} {
    if {$itk_option(-mged) == "" ||
	$itk_option(-geometryObject) == ""} {
	return
    }

    set gdata [$itk_option(-mged) get $itk_option(-geometryObject)]
    set gdata [lrange $gdata 1 end]

    set V [bu_get_value_by_keyword V $gdata]
    set Vx [lindex $V 0]
    set Vy [lindex $V 1]
    set Vz [lindex $V 2]
    set A [bu_get_value_by_keyword A $gdata]
    set Ax [lindex $A 0]
    set Ay [lindex $A 1]
    set Az [lindex $A 2]
    set B [bu_get_value_by_keyword B $gdata]
    set Bx [lindex $B 0]
    set By [lindex $B 1]
    set Bz [lindex $B 2]

    if {$mVx == ""  ||
	$mVx == "-" ||
	$mVy == ""  ||
	$mVy == "-" ||
	$mVz == ""  ||
	$mVz == "-" ||
	$mAx == ""  ||
	$mAx == "-" ||
	$mAy == ""  ||
	$mAy == "-" ||
	$mAz == ""  ||
	$mAz == "-" ||
	$mBx == ""  ||
	$mBx == "-" ||
	$mBy == ""  ||
	$mBy == "-" ||
	$mBz == ""  ||
	$mBz == "-"} {
	# Not valid
	return
    }

    if {$Vx != $mVx ||
	$Vy != $mVy ||
	$Vz != $mVz ||
	$Ax != $mAx ||
	$Ay != $mAy ||
	$Az != $mAz ||
	$Bx != $mBx ||
	$By != $mBy ||
	$Bz != $mBz} {
	updateGeometry
    }
}


::itcl::body SketchEditFrame::applyData {} {
}


::itcl::body SketchEditFrame::bboxVerts {} {
    set minX [expr {pow(2,32) - 1}]
    set minY $minX
    set maxX [expr {-$minX - 1}]
    set maxY $maxX

    foreach vert $mVL {
	set xc [lindex $vert 0]
	set yc [lindex $vert 1]
	set x [expr {$myscale * $xc}]
	set y [expr {-$myscale * $yc}]

	if {$x < $minX} {
	    set minX $x
	} elseif {$x > $maxX} {
	    set maxX $x
	}

	if {$y < $minY} {
	    set minY $y
	} elseif {$y > $maxY} {
	    set maxY $y
	}
    }

    return [list $minX $minY $maxX $maxY]
}


::itcl::body SketchEditFrame::createSegments {} {
    foreach seg $mSL {
	set type [lindex $seg 0]
	set seg [lrange $seg 1 end]
	switch $type {
	    line {
		lappend mSegments ::SketchEditFrame::[SketchLine \#auto $this $itk_component(canvas) $seg]
	    }
	    carc {
		set index [lsearch -exact $seg R]
		incr index
		set tmp_radius [lindex $seg $index]
		if { $tmp_radius > 0.0 } {
		    set tmp_radius [expr {$tolocal * $tmp_radius}]
		    set seg [lreplace $seg $index $index $tmp_radius]
		}
		lappend mSegments ::SketchEditFrame::[SketchCArc \#auto $this $itk_component(canvas) $seg]
	    }
	    bezier {
		lappend mSegments ::SketchEditFrame::[SketchBezier \#auto $this $itk_component(canvas) $seg]
	    }
	    default {
		$::ArcherCore::application putString "Curve segments of type '$type' are not yet handled"
	    }
	}
    }
}


::itcl::body SketchEditFrame::detailBrowseCommand {_row _col} {
    if {![info exists mVertDetail($_row,0)]} {
	return 0
    }

    $itk_component(vertTab) see $_row,$_col
}


::itcl::body SketchEditFrame::drawSegments {} {
    $itk_component(canvas) delete segs verts
    drawVertices
    set first 1
    foreach seg $mSegments {
	if { $first } {
	    set first 0
	    $seg draw first_seg
	} else {
	    $seg draw ""
	}
    }
#    $itk_component(canvas) configure -scrollregion [$itk_component(canvas) bbox all]
}


::itcl::body SketchEditFrame::drawVertices {} {
    set index 0
    $itk_component(canvas) delete verts
    foreach vert $mVL {
	set xc [lindex $vert 0]
	set yc [lindex $vert 1]
	set x1 [expr {$myscale * $xc - $vert_radius}]
	set y1 [expr {-$myscale * $yc - $vert_radius}]
	set x2 [expr {$myscale * $xc + $vert_radius}]
	set y2 [expr {-$myscale * $yc + $vert_radius}]
	set last [$itk_component(canvas) create oval $x1 $y1 $x2 $y2 -fill black -tags "p$index verts"]
	incr index
    }
}


::itcl::body SketchEditFrame::handleDetailPopup {_index _X _Y} {
}


::itcl::body SketchEditFrame::handleEnter {_row _col} {
    if {$itk_option(-mged) == "" ||
	$itk_option(-geometryObject) == "" ||
	$_row < 1 ||
	$_col < 1 ||
	$_col > $PZ_COL} {
	return
    }

    updateGeometryIfMod
}


::itcl::body SketchEditFrame::highlightCurrentSketchElements {} {
    if {$itk_option(-mged) == ""} {
	return
    }


    set hpoints {}
    foreach index $mCurrentSketchPoints {
	incr index -1
	lappend hpoints [lindex $mPointList $index]
    }

    set lsegPoints {}
    foreach edge $mCurrentSketchEdges {
	set indexA $mSegmentDetail($edge,$A_COL)
	set indexB $mSegmentDetail($edge,$B_COL)
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


::itcl::body SketchEditFrame::initCanvas {_gdata} {
    $::ArcherCore::application setCanvas $itk_component(canvas)
    ::update idletasks

    set myscale 1.0
    set mSegments {}
    set mVL {}
    set mSL {}
    set needs_saving 0
    initSketchData $_gdata
    createSegments
    drawSegments
    clear_canvas_bindings
    set mEscapeCreate 1
    set mEditMode 0
    set mPrevEditMode 0
    set mLastIndex -1

    set min_max [bboxVerts]
    if {[llength $min_max] != 4} {
	set min_max {-1 -1 1 1}
    }

    set tmp_scale1 [expr {double($mCanvasWidth) / ([lindex $min_max 2] - [lindex $min_max 0]) * 0.5}]
    if {$tmp_scale1 < 0.0} {
	set tmp_scale1 [expr -$tmp_scale1]
    }

    set tmp_scale2 [expr {double($mCanvasHeight) / ([lindex $min_max 3] - [lindex $min_max 1]) * 0.5}]
    if {$tmp_scale2 < 0.0} {
	set tmp_scale2 [expr -$tmp_scale2]
    }

    if {$tmp_scale1 < $tmp_scale2} {
	set scale $tmp_scale1
    } else {
	set scale $tmp_scale2
    }

    do_scale $scale 0 0
    initScrollRegion
    do_scale 1.0 1 1
}


::itcl::body SketchEditFrame::initPointHighlight {} {
    if {$itk_option(-mged) == ""} {
	return
    }

    $itk_option(-mged) data_axes draw $mHighlightPoints
}


::itcl::body SketchEditFrame::initScrollRegion {} {
    set bbox [$itk_component(canvas) bbox segs verts]
    if {[llength $bbox] != 4} {
	set bbox {-1 -1 1 1}
    }
    set x1 [lindex $bbox 0]
    set y1 [lindex $bbox 1]
    set x2 [lindex $bbox 2]
    set y2 [lindex $bbox 3]
    set dx [expr {abs($x2 - $x1)}]
    set dy [expr {abs($y2 - $y1)}]
    if {$dx <= $mCanvasWidth} {
	set extra [expr {$mCanvasWidth - $dx}]
	set leftover [expr {int($extra * 0.5)}]
	set x1 [expr {$x1 - $leftover}]
	if {[expr {$extra%2}]} {
	    set x2 [expr {$x2 + $leftover + 1}]
	} else {
	    set x2 [expr {$x2 + $leftover}]
	}
    } else {
	set short [expr {$dx - $mCanvasWidth}]
	set required [expr {int($short * 0.5)}]
	set x1 [expr {$x1 + $required}]
	if {[expr {$short%2}]} {
	    set x2 [expr {$x2 - $required - 1}]
	} else {
	    set x2 [expr {$x2 - $required}]
	}
    }

    if {$dy <= $mCanvasHeight} {
	set extra [expr {$mCanvasHeight - $dy}]
	set leftover [expr {int($extra * 0.5)}]
	set y1 [expr {$y1 - $leftover}]
	if {[expr {$extra%2}]} {
	    set y2 [expr {$y2 + $leftover + 1}]
	} else {
	    set y2 [expr {$y2 + $leftover}]
	}
    } else {
	set short [expr {$dy - $mCanvasHeight}]
	set required [expr {int($short * 0.5)}]
	set y1 [expr {$y1 + $required}]
	if {[expr {$short%2}]} {
	    set y2 [expr {$y2 - $required - 1}]
	} else {
	    set y2 [expr {$y2 - $required}]
	}
    }

    $itk_component(canvas) configure -scrollregion [list $x1 $y1 $x2 $y2]
    set mScrollCenterX [expr {$x2 - int(($x2 - $x1) * 0.5)}]
    set mScrollCenterY [expr {$y2 - int(($y2 - $y1) * 0.5)}]
}


::itcl::body SketchEditFrame::initSketchData {_gdata} {
    foreach {key value} $_gdata {
	switch $key {
	    V {
		if {[llength $value] == 3} {
		    set mVx [expr {$tolocal * [lindex $value 0]}]
		    set mVy [expr {$tolocal * [lindex $value 1]}]
		    set mVz [expr {$tolocal * [lindex $value 2]}]
		    #set mVx [format "%.8f" [expr {$tolocal * [lindex $value 0]}]]
		    #set mVy [format "%.8f" [expr {$tolocal * [lindex $value 1]}]]
		    #set mVz [format "%.8f" [expr {$tolocal * [lindex $value 2]}]]
		}
	    }
	    A {
		if {[llength $value] == 3} {
		    set mAx [expr {$tolocal * [lindex $value 0]}]
		    set mAy [expr {$tolocal * [lindex $value 1]}]
		    set mAz [expr {$tolocal * [lindex $value 2]}]
		    #set mAx [format "%.8f" [expr {$tolocal * [lindex $value 0]}]]
		    #set mAy [format "%.8f" [expr {$tolocal * [lindex $value 1]}]]
		    #set mAz [format "%.8f" [expr {$tolocal * [lindex $value 2]}]]
		}
	    }
	    B {
		if {[llength $value] == 3} {
		    set mBx [expr {$tolocal * [lindex $value 0]}]
		    set mBy [expr {$tolocal * [lindex $value 1]}]
		    set mBz [expr {$tolocal * [lindex $value 2]}]
		    #set mBx [format "%.8f" [expr {$tolocal * [lindex $value 0]}]]
		    #set mBy [format "%.8f" [expr {$tolocal * [lindex $value 1]}]]
		    #set mBz [format "%.8f" [expr {$tolocal * [lindex $value 2]}]]
		}
	    }
	    VL {
		foreach vert $value {
		    set x [expr {$tolocal * [lindex $vert 0]}]
		    set y [expr {$tolocal * [lindex $vert 1]}]
		    lappend mVL "$x $y"
		}
	    }
	    SL {
		set mSL $value
	    }
	}
    }
}


::itcl::body SketchEditFrame::loadTables {_gdata} {
    #XXX not ready
}


::itcl::body SketchEditFrame::redrawSegments {} {
    set ids [$itk_component(canvas) find withtag segs]
    foreach id $ids {
	set tags [$itk_component(canvas) gettags $id]
	set seg [lindex $tags 0]
	$itk_component(canvas) delete $id
	$seg draw [lrange $tags 2 end]
    }
}


::itcl::body SketchEditFrame::build_grid {_x1 _y1 _x2 _y2 _final {_adjust_spacing 1}} {
    # delete previous grid
    $itk_component(canvas) delete grid

    if {!$mDrawGrid} {
	return
    }

    set spacing [expr {$mMinorGridSpacing * $myscale}]

    if {$_adjust_spacing} {
	if {$spacing < 10} {
	    while {$spacing < 10} {
		set mMinorGridSpacing [format "%.6f" [expr {$mMinorGridSpacing * 10}]]
		set spacing [expr {$mMinorGridSpacing * $myscale}]
	    }
	} elseif {$spacing > 100} {
	    while {$spacing > 100} {
		set mMinorGridSpacing [format "%.6f" [expr {$mMinorGridSpacing * 0.1}]]
		set spacing [expr {$mMinorGridSpacing * $myscale}]
	    }
	}
    }

    set major_spacing [expr {$spacing * $mMajorGridSpacing}]

    set snap_1 [do_snap_sketch [expr {$_x1 / $myscale}] [expr {$_y1 / $myscale}]]
    set snap_1_x [lindex $snap_1 0]
    set snap_1_y [lindex $snap_1 1]

    set xsteps [expr {abs(round(($snap_1_x - $mAnchorX) / double($mMinorGridSpacing)))}]
    set ysteps [expr {abs(round(($snap_1_y - $mAnchorY) / double($mMinorGridSpacing)))}]

    set xsteps [expr {$xsteps%$mMajorGridSpacing}]
    if {$xsteps} {
	set xsteps [expr {($mMajorGridSpacing - $xsteps)}]
	set start_x [expr {($snap_1_x * $myscale) - ($xsteps * $mMinorGridSpacing * $myscale)}]
    } else {
	set start_x [expr {$snap_1_x * $myscale}]
    }

    set ysteps [expr {$ysteps%$mMajorGridSpacing}]
    if {$ysteps} {
	set ysteps [expr {$mMajorGridSpacing - $ysteps}]
	set start_y [expr {($snap_1_y * $myscale) - ($ysteps * $mMinorGridSpacing * $myscale)}]
    } else {
	set start_y [expr {$snap_1_y * $myscale}]
    }

    set snap_2 [do_snap_sketch [expr {$_x2 / $myscale}] [expr {$_y2 / $myscale}]]
    set end_x [expr {([lindex $snap_2 0] + 1) * $myscale}]
    set end_y [expr {([lindex $snap_2 1] + 1) * $myscale}]

    set ruler_x [$itk_component(canvas) canvasx 10]
    set ruler_y [$itk_component(canvas) canvasy 15]
    set ruler_start_x [$itk_component(canvas) canvasx 50]
    set ruler_start_y [$itk_component(canvas) canvasy 50]

    if {$_final} {
	# Draw the rows
	for {set y $start_y} {$y <= $end_y} {set y [expr {$y + $major_spacing}]} {
	    for {set x $start_x} {$x <= $end_x} {set x [expr {$x + $spacing}]} {
		set x1 [expr {$x - 1}]
		set y1 [expr {$y - 1}]
		set x2 [expr {$x + 1}]
		set y2 [expr {$y + 1}]
		$itk_component(canvas) create oval $x $y $x $y -fill black -tags grid
	    }

	    if {$y > $ruler_start_y} {
		set t [format "%.4f" [expr {$y / $myscale}]]
		$itk_component(canvas) create text $ruler_x $y -text $t -anchor sw -fill black -tags "grid ruler"
	    }
	}

	# Draw the columns
	for {set x $start_x} {$x <= $end_x} {set x [expr {$x + $major_spacing}]} {
	    set row 0
	    for {set y $start_y} {$y <= $end_y} {set y [expr {$y + $spacing}]} {
		if {[expr {$row%$mMajorGridSpacing}]} {
		    $itk_component(canvas) create oval $x $y $x $y -fill black -tags grid
		}
		incr row
	    }

	    if {$x > $ruler_start_x} {
		set t [format "%.4f" [expr {$x / $myscale}]]
		$itk_component(canvas) create text $x $ruler_y -text $t -anchor center -fill black -tags "grid ruler"
	    }
	}
    } else {
	# Draw the rows
	for {set y $start_y} {$y <= $end_y} {set y [expr {$y + $major_spacing}]} {
	    $itk_component(canvas) create line $start_x $y $end_x $y -fill black -tags grid
	}

	# Draw the columns
	for {set x $start_x} {$x <= $end_x} {set x [expr {$x + $major_spacing}]} {
	    $itk_component(canvas) create line $x $start_y $x $end_y -fill black -tags grid
	}
    }
}


::itcl::body SketchEditFrame::circle_3pt {_x1 _y1 _x2 _y2 _x3 _y3 _cx_out _cy_out} {
    # find the center of a circle that passes through three points
    # return the center in "cx_out cy_out"
    # returns 0 on success
    # returns 1 if no such circle exists

    upvar $_cx_out cx
    upvar $_cy_out cy

    # first find the midpoints of two lines connecting the three points
    set mid1(0) [expr { ($_x1 + $_x2)/2.0 }]
    set mid1(1) [expr { ($_y1 + $_y2)/2.0 }]
    set mid2(0) [expr { ($_x3 + $_x2)/2.0 }]
    set mid2(1) [expr { ($_y3 + $_y2)/2.0 }]


    # next find the slopes of the perpendicular bisectors
    set dir1(0) [expr { $_y1 - $_y2 }]
    set dir1(1) [expr { $_x2 - $_x1 }]
    set dir2(0) [expr { $_y2 - $_y3 }]
    set dir2(1) [expr { $_x3 - $_x2 }]

    # find difference between the bisector start points
    set diffs(0) 0.0
    set diffs(1) 0.0
    diff diffs mid2 mid1

    # center point is the intersection of the two perpendicular bisectors
    set cross1 [cross2d dir2 dir1]
    set cross2 [cross2d dir1 diffs]

    if {[expr {abs($cross1) < $EPSILON}]} {
	return 1
    }

    # if cross1 is to small, this will catch the error
    if { [catch { expr {$cross2 / $cross1} } beta] } {
	# return an error flag
	return 1
    }

    set cx [expr { $mid2(0) + $beta * $dir2(0)}]
    set cy [expr { $mid2(1) + $beta * $dir2(1)}]

    return 0
}


::itcl::body SketchEditFrame::clear_canvas_bindings {} {
    bind $itk_component(canvas) <a> [::itcl::code $this create_arc]
    bind $itk_component(canvas) <A> [::itcl::code $this create_arc]
    bind $itk_component(canvas) <b> [::itcl::code $this create_bezier]
    bind $itk_component(canvas) <B> [::itcl::code $this create_bezier]
    bind $itk_component(canvas) <c> [::itcl::code $this create_circle]
    bind $itk_component(canvas) <C> [::itcl::code $this create_circle]
    bind $itk_component(canvas) <l> [::itcl::code $this create_line]
    bind $itk_component(canvas) <L> [::itcl::code $this create_line]
    bind $itk_component(canvas) <m> [::itcl::code $this setup_move_arbitrary]
    bind $itk_component(canvas) <M> [::itcl::code $this setup_move_arbitrary]

    bind $itk_component(canvas) <BackSpace> [::itcl::code $this delete_selected]
    bind $itk_component(canvas) <d> [::itcl::code $this delete_selected]
    bind $itk_component(canvas) <D> [::itcl::code $this delete_selected]
    bind $itk_component(canvas) <Delete> [::itcl::code $this delete_selected]

    bind $itk_component(canvas) <Lock-Control-Shift-ButtonPress-1> [::itcl::code $this start_scale %x %y]
    bind $itk_component(canvas) <Lock-Shift-ButtonPress-1> [::itcl::code $this start_translate %x %y]
    bind $itk_component(canvas) <ButtonRelease-1> [::itcl::code $this end_scale]
    bind $itk_component(canvas) <Control-Shift-B1-Motion> {}
    bind $itk_component(canvas) <B1-Motion> {}
    bind $itk_component(canvas) <ButtonPress-1> {}
    bind $itk_component(canvas) <Shift-ButtonPress-1> {}
    bind $itk_component(canvas) <Control-Alt-ButtonPress-1> {}

    bind $itk_component(canvas) <ButtonRelease-2> {}

    bind $itk_component(canvas) <ButtonRelease-3> {}
    bind $itk_component(canvas) <Shift-ButtonRelease-3> {}
}


::itcl::body SketchEditFrame::continue_circle {_segment _state _coord_type _mx _my} {
    switch -- $_coord_type {
	0 {
	    # model coords
	    set ex $x_coord
	    set ey $y_coord
	}
	1 {
	    # screen coords
	    #show_coords $_mx $_my
	    set ex [expr {[$itk_component(canvas) canvasx $_mx] / $myscale}]
	    set ey [expr {-[$itk_component(canvas) canvasy $_my] / $myscale}]
	}
	2 {
	    # use radius entry widget
	    set vert [lindex $mVL $index1]
	    set ex [expr {[lindex $vert 0] + $radius}]
	    set ey [lindex $vert 1]
	}
	3 {
	    # use index numbers
	    $_segment set_vars S $index1
	}
	default {
	    $::ArcherCore::application putString "continue_circle: unrecognized coord type - $_coord_type"
	}
    }

    if {$_coord_type != 3} {
	if {$index1 == $index2} {
	    # need to create a new vertex
	    set index1 [llength $mVL]
	    lappend mVL "$ex $ey"
	    $_segment set_vars S $index1
	} else {
	    set mVL [lreplace $mVL $index1 $index1 "$ex $ey"]
	}
    }

    $itk_component(canvas) delete ::SketchEditFrame::$_segment
    $_segment draw ""
    set radius [$_segment get_radius]
#    $itk_component(canvas) configure -scrollregion [$itk_component(canvas) bbox all]
    if {$_state} {
	$itk_component(canvas) configure -cursor crosshair
	create_circle
	drawVertices
	write_sketch_to_db

	bind $itk_component(canvas) <B1-Motion> {}
	bind $itk_component(canvas) <ButtonRelease-1> [::itcl::code $this end_scale]
    }
}


::itcl::body SketchEditFrame::continue_circle_pick {_segment _mx _my} {
    set index [pick_vertex $_mx $_my]
    if {$index == -1} {
	set elist [mouse_to_sketch $_mx $_my]
	set ex [lindex $elist 0]
	set ey [lindex $elist 1]

	if {$index1 == $index2} {
	    # need to create a new vertex
	    set index1 [llength $mVL]
	    lappend mVL "$ex $ey"
	} else {
	    set mVL [lreplace $mVL $index1 $index1 "$ex $ey"]
	}
    } else {
	if {$index != $index1} {
	    # At this point index1 refers to the last vertex in VL. Remove it.
	    set mVL [lreplace $mVL end end]
	    set index1 $index
	} else {
	    # Here we have to add a vertex
	    set vert [lindex $mVL $index]
	    set index1 [llength $mVL]
	    lappend mVL $vert
	}
    }

    continue_circle $_segment 1 3 0 0
}


::itcl::body SketchEditFrame::continue_line {_segment _state _coord_type _mx _my} {
    if {$_state == 0 && $mIgnoreMotion} {
	return
    }

    switch -- $_coord_type {
	0 {
	    # model coords
	    set ex $x_coord
	    set ey $y_coord
	}
	1 {
	    # screen coords
	    #show_coords $_mx $_my
	    set ex [expr {[$itk_component(canvas) canvasx $_mx] / $myscale}]
	    set ey [expr {-[$itk_component(canvas) canvasy $_my] / $myscale}]

	    if {$_state == 1} {
		# need to create a new vertex
		set index2 [llength $mVL]
		lappend mVL "$ex $ey"
	    }
	}
	2 {
	    # use index numbers
	    $_segment set_vars E $index2
	}
	default {
	    $::ArcherCore::application putString "continue_line: unrecognized coord type - $_coord_type"
	}
    }

    if {$_coord_type != 2} {
	set mVL [lreplace $mVL $index2 $index2 "$ex $ey"]
    }

    $itk_component(canvas) delete ::SketchEditFrame::$_segment
    $_segment draw ""

    if {$_state == 2} {
	$itk_component(canvas) configure -cursor crosshair
	create_line
	write_sketch_to_db

	bind $itk_component(canvas) <B1-Motion> {}
	bind $itk_component(canvas) <ButtonRelease-1> [::itcl::code $this end_scale]
    }

    drawVertices
}


::itcl::body SketchEditFrame::continue_line_pick {_segment _state _mx _my} {
    set index [pick_vertex $_mx $_my p$index2]
    if {$index == -1} {
	if {$_state == 1} {
	    $itk_component(canvas) configure -cursor {}
	}

	# If it's a button press
	if {$_state == 1} {
	    set ex [expr {[$itk_component(canvas) canvasx $_mx] / $myscale}]
	    set ey [expr {-[$itk_component(canvas) canvasy $_my] / $myscale}]
	} else {
	    set slist [mouse_to_sketch $_mx $_my]
	    set ex [lindex $slist 0]
	    set ey [lindex $slist 1]
	}

	if {$index1 != $index2} {
	    if {!$mIgnoreMotion} {
		# Update the vertex
		set mVL [lreplace $mVL $index2 $index2 "$ex $ey"]
	    }
	} else {
	    # Add a vertex
	    set index2 [llength $mVL]
	    lappend mVL "$ex $ey"
	}
    } else {
	if {$index != $index2} {
	    set mIgnoreMotion 1
	    set prevIndex2 $index2
	    set index2 $index

	    # If not a button press
	    if {$_state != 1} {
		set lastv [expr {[llength $mVL] - 1}]

		if {$lastv == $prevIndex2} {
		    set mVL [lreplace $mVL end end]
		}
	    }
	} elseif {$_state == 2} {
	    set slist [mouse_to_sketch $_mx $_my]
	    set ex [lindex $slist 0]
	    set ey [lindex $slist 1]

	    if {!$mIgnoreMotion} {
		# Update the vertex
		set mVL [lreplace $mVL $index2 $index2 "$ex $ey"]
	    }
	}
    }

    continue_line $_segment $_state 2 0 0
}


::itcl::body SketchEditFrame::continue_move {_state _mx _my} {
    if {$curr_vertex != ""} {
	set index [pick_vertex $_mx $_my p$curr_vertex]
	if {$index == $curr_vertex} {
	    set index -1
	}
    } else {
	set index -1
    }

    set slist [vert_is_used $curr_vertex]
    set slen [llength $slist]

    if {$_state == 0 || $index == -1 || $slen != 1} {
	set x [$itk_component(canvas) canvasx $_mx]
	#	$this show_coords $_mx $_my
	set y [$itk_component(canvas) canvasy $_my]
	set dx [expr $x - $move_start_x]
	set dy [expr $y - $move_start_y]
    } else {
	set item $slist
	set type [$item get_type]
	set vlist [$item get_verts]
	set vindex [lsearch $vlist $curr_vertex]

	switch -- $type {
	    "SketchCArc" -
	    "SketchLine" {
		if {$vindex == 0} {
		    $item set_vars S $index
		} else {
		    $item set_vars E $index
		}
	    }
	    "SketchBezier" {
		set vlist [lreplace $vlist $vindex $vindex $index]
		$item set_vars P $vlist
	    }
	}

	set mVL [lreplace $mVL $curr_vertex $curr_vertex]
	fix_vertex_references $curr_vertex

	set mEscapeCreate 1
	set mLastIndex -1

	return
    }
    $itk_component(canvas) move moving $dx $dy

    set ids [$itk_component(canvas) find withtag moving]
    set len [llength $ids]
    if {$len == 0} {
	# This should not happen
	return
    }

    # actually move the vertices in the vertex list
    if {$_state != 0 && $len == 1} {
	set tags [$itk_component(canvas) gettags $ids]
	set index [string range [lindex $tags 0] 1 end]
	set new_coords [$itk_component(canvas) coords $ids]
	set new_x [expr ([lindex $new_coords 0] + [lindex $new_coords 2])/(2.0 * $myscale)]
	set new_y [expr -([lindex $new_coords 1] + [lindex $new_coords 3])/(2.0 * $myscale)]

	if {$mSnapGrid} {
	    set mVL [lreplace $mVL $index $index [do_snap_sketch $new_x $new_y]]
	} else {
	    set mVL [lreplace $mVL $index $index "$new_x $new_y"]
	}

	drawVertices
    } else {
	foreach id $ids {
	    set tags [$itk_component(canvas) gettags $id]
	    set index [string range [lindex $tags 0] 1 end]
	    set new_coords [$itk_component(canvas) coords $id]
	    set new_x [expr ([lindex $new_coords 0] + [lindex $new_coords 2])/(2.0 * $myscale)]
	    set new_y [expr -([lindex $new_coords 1] + [lindex $new_coords 3])/(2.0 * $myscale)]
	    set mVL [lreplace $mVL $index $index "$new_x $new_y"]
	}
    }

    redrawSegments

    if {$_state == 0} {
	set move_start_x $x
	set move_start_y $y
    } else {
	$itk_component(canvas) configure -cursor crosshair
	write_sketch_to_db

	bind $itk_component(canvas) <B1-Motion> {}
	bind $itk_component(canvas) <ButtonRelease-1> [::itcl::code $this end_scale]
    }
}


::itcl::body SketchEditFrame::create_arc {} {
    set mEditMode $createArc
    $itk_component(canvas) configure -cursor crosshair

    if {$mPrevEditMode == $createBezier} {
	end_bezier $curr_seg 0
	set curr_seg ""
    }

    clear_canvas_bindings

    if {$mEscapeCreate} {
	bind $itk_component(canvas) <ButtonRelease-1> [::itcl::code $this start_arc %x %y]
    } else {
	set mLastIndex $index2
	bind $itk_component(canvas) <ButtonPress-1> [::itcl::code $this start_arc %x %y]
    }

    set mPrevEditMode $mEditMode
}


::itcl::body SketchEditFrame::create_bezier {} {
    set mEditMode $createBezier
    $itk_component(canvas) configure -cursor crosshair

    if {$mPrevEditMode == $createBezier && !$mCallingFromEndBezier} {
	end_bezier $curr_seg 0
	set curr_seg ""
    }

    set bezier_indices ""
    clear_canvas_bindings
    bind $itk_component(canvas) <ButtonPress-1> [::itcl::code $this start_bezier %x %y]

    set mPrevEditMode $mEditMode
}


::itcl::body SketchEditFrame::create_circle {} {
    set mEditMode $createCircle
    $itk_component(canvas) configure -cursor crosshair

    if {$mPrevEditMode == $createBezier} {
	end_bezier $curr_seg 0
	set curr_seg ""
    }

    clear_canvas_bindings
    bind $itk_component(canvas) <ButtonPress-1> [::itcl::code $this start_circle 1 %x %y]

    set mPrevEditMode $mEditMode
    set mLastIndex -1
    set mEscapeCreate 1
}


::itcl::body SketchEditFrame::create_line {} {
    set mEditMode $createLine
    $itk_component(canvas) configure -cursor crosshair

    if {$mPrevEditMode == $createBezier} {
	end_bezier $curr_seg 0
	set curr_seg ""
	set index2 $mLastIndex
    }

    clear_canvas_bindings

    if {$mEscapeCreate} {
	bind $itk_component(canvas) <ButtonRelease-1> [::itcl::code $this start_line %x %y]
    } else {
	set mLastIndex $index2
	bind $itk_component(canvas) <ButtonPress-1> [::itcl::code $this start_line %x %y]
    }

    set mPrevEditMode $mEditMode
}


::itcl::body SketchEditFrame::do_scale {_sf _gflag _final {_adjust_spacing 1} {_epsilon 0.000001}} {
    if {$_sf < $_epsilon} {
	return
    }

    set myscale [expr {$myscale * $_sf}]
    drawSegments

    set mScrollCenterX [expr {$mScrollCenterX * $_sf}]
    set mScrollCenterY [expr {$mScrollCenterY * $_sf}]

    set x1 [expr {$mScrollCenterX - $mCanvasCenterX}]
    if {[expr {$mCanvasWidth%2}]} {
	set x2 [expr {$mScrollCenterX + $mCanvasCenterX + 1}]
    } else {
	set x2 [expr {$mScrollCenterX + $mCanvasCenterX}]
    }

    set y1 [expr {$mScrollCenterY - $mCanvasCenterY}]
    if {[expr {$mCanvasHeight%2}]} {
	set y2 [expr {$mScrollCenterY + $mCanvasCenterY + 1}]
    } else {
	set y2 [expr {$mScrollCenterY + $mCanvasCenterY}]
    }

    $itk_component(canvas) configure -scrollregion [list $x1 $y1 $x2 $y2]

    if {$_gflag} {
	build_grid $x1 $y1 $x2 $y2 $_final $_adjust_spacing
    }
}


::itcl::body SketchEditFrame::do_snap_sketch {_x _y} {
    set snap_x [expr {round(($_x - $mAnchorX) / double($mMinorGridSpacing)) * $mMinorGridSpacing + $mAnchorX}]
    set snap_y [expr {round(($_y - $mAnchorY) / double($mMinorGridSpacing)) * $mMinorGridSpacing + $mAnchorY}]

    return [format "%g %g" $snap_x $snap_y]
}


::itcl::body SketchEditFrame::do_translate {_dx _dy _gflag _final} {
    set mScrollCenterX [expr {$mScrollCenterX + $_dx}]
    set mScrollCenterY [expr {$mScrollCenterY + $_dy}]

    set x1 [expr {$mScrollCenterX - $mCanvasCenterX}]
    if {[expr {$mCanvasWidth%2}]} {
	set x2 [expr {$mScrollCenterX + $mCanvasCenterX + 1}]
    } else {
	set x2 [expr {$mScrollCenterX + $mCanvasCenterX}]
    }

    set y1 [expr {$mScrollCenterY - $mCanvasCenterY}]
    if {[expr {$mCanvasHeight%2}]} {
	set y2 [expr {$mScrollCenterY + $mCanvasCenterY + 1}]
    } else {
	set y2 [expr {$mScrollCenterY + $mCanvasCenterY}]
    }

    $itk_component(canvas) configure -scrollregion [list $x1 $y1 $x2 $y2]

    if {$_gflag} {
	build_grid $x1 $y1 $x2 $y2 $_final
    }
}


::itcl::body SketchEditFrame::delete_selected {} {
    set selected [$itk_component(canvas) find withtag selected]
    set slist {}
    set vlist {}
    foreach id $selected {
	set tags [$itk_component(canvas) gettags $id]
	set item [lindex $tags 0]
	set type [lindex $tags 1]
	switch $type {
	    segs   {
		lappend slist $item
	    }
	    verts  {
		lappend vlist [string range $item 1 end]
	    }
	}
    }

    set svlist {}
    foreach item $slist {
	set index [lsearch $mSegments $item]
	set mSegments [lreplace $mSegments $index $index]
	$itk_component(canvas) delete $item

	eval lappend svlist [$item get_verts]

	::itcl::delete object $item
    }

    set alist [lsort -integer -decreasing -unique [eval lappend alist $vlist $svlist]]

    set unused_vindices {}
    foreach vindex $alist {
	if {[vert_is_used $vindex] == {}} {
	    set mVL [lreplace $mVL $vindex $vindex]
	    lappend unused_vindices $vindex
	}
    }
    fix_vertex_references $unused_vindices

    write_sketch_to_db
}


::itcl::body SketchEditFrame::end_arc {_mx _my} {
    set index [pick_vertex $_mx $_my]
    if {$index == -1} {
	set elist [mouse_to_sketch $_mx $_my]
	set ex [lindex $elist 0]
	set ey [lindex $elist 1]

	set index [llength $mVL]
	lappend mVL "$ex $ey"
	drawVertices
    }

    set index2 $index
    set mLastIndex $index2

    # calculate an initial radius
    set s [lindex $mVL $index1]
    set sx [lindex $s 0]
    set sy [lindex $s 1]
    set e [lindex $mVL $index2]
    set ex [lindex $e 0]
    set ey [lindex $e 1]
    set radius [::dist $sx $sy $ex $ey]
    set new_seg [SketchCArc \#auto $this $itk_component(canvas) "S $index1 E $index2 R $radius L 0 O 1"]
    lappend mSegments ::SketchEditFrame::$new_seg
    set needs_saving 1
    drawSegments

    $itk_component(canvas) configure -cursor {}
    $new_seg highlight
    start_arc_radius_adjust $new_seg $_mx $_my
    bind $itk_component(canvas) <B1-Motion> [::itcl::code $this start_arc_radius_adjust $new_seg %x %y]
    bind $itk_component(canvas) <ButtonRelease-1> "[::itcl::code $this end_arc_radius_adjust $new_seg %x %y]; [::itcl::code $this create_arc]"
#    bind $itk_component(coords).radius <Return> [::itcl::code $this set_arc_radius_end $new_seg 0 0 0]
}


::itcl::body SketchEditFrame::end_bezier {_segment _cflag} {
    clear_canvas_bindings

    set bi_len [llength $bezier_indices]

    if {$bi_len < 2} {
	return
    } elseif {$bi_len == 2 && [lindex $bezier_indices 0] == [lindex $bezier_indices 1]} {
	set index [lsearch $mSegments ::SketchEditFrame::$_segment]
	set mSegments [lreplace $mSegments $index $index]
	$itk_component(canvas) delete ::SketchEditFrame::$_segment

	set vindex [lindex $bezier_indices 0]
	if {[vert_is_used $vindex] == {}} {
	    set mVL [lreplace $mVL $vindex $vindex]
	    drawVertices
	}

	set bezier_indices ""
    } else {
	set mLastIndex [lindex $bezier_indices end]
    }

    set curr_seg ""
    write_sketch_to_db

    if {$_cflag} {
	set mCallingFromEndBezier 1
	create_bezier
	set mCallingFromEndBezier 0
    }
}


::itcl::body SketchEditFrame::end_arc_radius_adjust {_segment _mx _my} {
    # screen coordinates
    #show_coords $_mx $_my
    set slist [mouse_to_sketch $_mx $_my]
    set sx [lindex $slist 0]
    set sy [lindex $slist 1]
    set cx 0.0
    set cy 0.0

    set s_list [lindex $mVL $index1]
    set s(0) [lindex $s_list 0]
    set s(1) [lindex $s_list 1]
    set e_list [lindex $mVL $index2]
    set e(0) [lindex $e_list 0]
    set e(1) [lindex $e_list 1]
    if {[circle_3pt $s(0) $s(1) $sx $sy $e(0) $e(1) cx cy]} {
	return
    }

    set center(0) $cx
    set center(1) $cy
    diff diff1 e s
    diff diff2 center s
    if {[cross2d diff1 diff2] > 0.0} {
	set center_is_left 1
    } else {
	set center_is_left 0
    }

    set start_ang [expr { atan2( ($s(1) - $cy), ($s(0) - $cx) ) }]
    set mid_ang [expr { atan2( ($sy - $cy), ($sx - $cx) ) }]
    while { $mid_ang < $start_ang } {
	set mid_ang [expr $mid_ang + $pi2]
    }
    set end_ang [expr { atan2( ($e(1) - $cy), ($e(0) - $cx) ) }]
    while { $end_ang < $mid_ang } {
	set end_ang [expr $end_ang + $pi2]
    }

    if {[expr {$end_ang - $start_ang}] > $pi2} {
	set orient 1
    } else {
	set orient 0
    }
    set radius [::dist $s(0) $s(1) $cx $cy]
    $_segment set_vars R $radius L $center_is_left O $orient

    $itk_component(canvas) configure -cursor crosshair
    drawSegments
    write_sketch_to_db

    bind $itk_component(canvas) <B1-Motion> {}
    bind $itk_component(canvas) <ButtonRelease-1> [::itcl::code $this end_scale]
}


::itcl::body SketchEditFrame::end_scale {} {
    bind $itk_component(canvas) <Control-Shift-B1-Motion> {}
}


::itcl::body SketchEditFrame::fix_vertex_references {_unused_vindices} {
    foreach seg $mSegments {
	$seg fix_vertex_reference $_unused_vindices
    }
    drawVertices
}


::itcl::body SketchEditFrame::handle_configure {} {
    set mCanvasHeight [winfo height $itk_component(canvas)]
    set mCanvasWidth [winfo width $itk_component(canvas)]
    set mCanvasInvWidth [expr {1.0 / double($mCanvasWidth)}]
    set mCanvasCenterX [expr {int($mCanvasWidth * 0.5)}]
    set mCanvasCenterY [expr {int($mCanvasHeight * 0.5)}]

    do_scale 1.0 1 1
}


::itcl::body SketchEditFrame::handle_escape {} {
    switch -- $mEditMode \
	$moveArbitrary {
	    # nothing yet
	} \
	$createLine {
	    set mEscapeCreate 1
	    set mLastIndex -1
	    create_line
	} \
	$createCircle {
	    # nothing yet
	} \
	$createArc {
	    set mEscapeCreate 1
	    set mLastIndex -1
	    create_arc
	} \
	$createBezier {
	    set mEscapeCreate 1
	    end_bezier $curr_seg 1
	    set mLastIndex -1
	}
}


::itcl::body SketchEditFrame::handle_scale {_mx _my _final} {
    set dx [expr {$_mx - $mPrevMouseX}]
    set dy [expr {$mPrevMouseY - $_my}]

    set mPrevMouseX $_mx
    set mPrevMouseY $_my

    set sdx [expr {$mCanvasInvWidth * $dx * 2.0}]
    set sdy [expr {$mCanvasInvWidth * $dy * 2.0}]
    if {[expr {abs($sdx) > abs($sdy)}]} {
	set sf [expr {1.0 + $sdx}]
    } else {
	set sf [expr {1.0 + $sdy}]
    }

    do_scale $sf 1 $_final
}


::itcl::body SketchEditFrame::handle_translate {_mx _my _final} {
    set dx [expr {$mPrevMouseX - $_mx}]
    set dy [expr {$mPrevMouseY - $_my}]

    set mPrevMouseX $_mx
    set mPrevMouseY $_my

    do_translate $dx $dy 1 $_final
}


::itcl::body SketchEditFrame::item_pick_highlight {_mx _my} {
    set item [pick_arbitrary $_mx $_my]
    if {$item == -1} return

    set tags [$itk_component(canvas) gettags $item]
    set item [lindex $tags 0]
    set type [lindex $tags 1]
    set ids [$itk_component(canvas) find withtag selected]

    if {$type == "segs"} {
	set pid [$itk_component(canvas) find withtag $item]

	if {[lsearch $ids $pid] == -1} {
	    $item highlight
	    set curr_seg $item
	} else {
	    $item unhighlight
	    $item untag_verts moving

	    if {$item == $curr_seg} {
		set curr_seg ""
	    }
	}

	set curr_vertex ""
    } else {
	set pid [$itk_component(canvas) find withtag $item]

	if {[lsearch $ids $pid] == -1} {
	    $itk_component(canvas) itemconfigure $item -fill red -outline red
	    $itk_component(canvas) addtag selected withtag $item
	    set curr_vertex [string range $item 1 end]
	} else {
	    $itk_component(canvas) itemconfigure $item -fill black -outline black
	    $itk_component(canvas) dtag $item selected
	    set curr_vertex ""
	}

	set curr_seg ""
    }

    tag_selected_verts
}


::itcl::body SketchEditFrame::mouse_to_sketch {_mx _my} {
    set x [expr {[$itk_component(canvas) canvasx $_mx] / $myscale}]
    set y [expr {-[$itk_component(canvas) canvasy $_my] / $myscale}]

    if {$mSnapGrid} {
	return [do_snap_sketch $x $y]
    }

    return "$x $y"
}


::itcl::body SketchEditFrame::next_bezier {_segment _mx _my} {
    set index [pick_vertex $_mx $_my]
    if {$index != -1} {
	if {[llength $bezier_indices] == 2 && [lindex $bezier_indices 0] == [lindex $bezier_indices 1]} {
	    set bezier_indices [lindex $bezier_indices 0]
	    set needs_saving 1
	}
	lappend bezier_indices $index
    } else {
	# screen coords
	#show_coords $_mx $_my
	set slist [mouse_to_sketch $_mx $_my]
	set sx [lindex $slist 0]
	set sy [lindex $slist 1]

	if {[llength $bezier_indices] == 2 && [lindex $bezier_indices 0] == [lindex $bezier_indices 1]} {
	    set bezier_indices [lindex $bezier_indices 0]
	    set needs_saving 1
	}
	lappend bezier_indices [llength $mVL]
	lappend mVL "$sx $sy"
	drawVertices
    }

    $itk_component(canvas) delete ::SketchEditFrame::$_segment
    $_segment set_vars D [expr [llength $bezier_indices] - 1] P $bezier_indices
    $_segment draw ""
}


::itcl::body SketchEditFrame::pick_arbitrary {_mx _my} {
    set item [pick_vertex $_mx $_my]
    if {$item == -1} {
	return [pick_segment $_mx $_my]
    }

    return "p$item"
}


::itcl::body SketchEditFrame::pick_segment {_mx _my} {
    set x [$itk_component(canvas) canvasx $_mx]
    set y [$itk_component(canvas) canvasy $_my]
    set item [$itk_component(canvas) find closest $x $y]
    if {$item == ""} {
	return -1
    }

    set tags [$itk_component(canvas) gettags $item]
    if {[lsearch -exact $tags segs] == -1} {
	return -1
    }

    return [lindex $tags 0]
}


::itcl::body SketchEditFrame::pick_vertex {_mx _my {_tag ""}} {
    set x [$itk_component(canvas) canvasx $_mx]
    set y [$itk_component(canvas) canvasy $_my]

    if {$_tag == ""} {
	set item [$itk_component(canvas) find closest $x $y 0 first_seg]
    } else {
	set item [$itk_component(canvas) find closest $x $y 0 $_tag]
    }
    if { $item == "" } {
	return -1
    }

    set tags [$itk_component(canvas) gettags $item]
    set index [lsearch -glob $tags p*]
    if { $index == -1 } {
	return -1
    }

    set index [string range [lindex $tags $index] 1 end]

    # Check to see if the nearest vertex is within tolerance (pixels)
    set coords [$itk_component(canvas) coords p$index]
    set cx [expr {([lindex $coords 0] + [lindex $coords 2]) * 0.5}]
    set cy [expr {([lindex $coords 1] + [lindex $coords 3]) * 0.5}]
    set mag [::dist $x $y $cx $cy]

    if {$mag > $mPickTol} {
	return -1
    }

    return $index
}


::itcl::body SketchEditFrame::seg_delete {_mx _my _vflag} {
    set item [pick_segment $_mx $_my]
    if {$item == -1} return

    set index [lsearch $mSegments $item]
    set mSegments [lreplace $mSegments $index $index]
    $itk_component(canvas) delete $item

    if {$_vflag} {
	set unused_vindices {}
	foreach vindex [lsort -integer -decreasing [$item get_verts]] {
	    if {[vert_is_used $vindex] == {}} {
		set mVL [lreplace $mVL $vindex $vindex]
		lappend unused_vindices $vindex
	    }
	}
	fix_vertex_references $unused_vindices
    }

    ::itcl::delete object $item
    write_sketch_to_db
}


::itcl::body SketchEditFrame::seg_pick_highlight {_sx _sy} {
    set item [pick_segment $_sx $_sy]
    if {$item == -1} return

    set ids [$itk_component(canvas) find withtag selected]
    set pid [$itk_component(canvas) find withtag $item]

    if {[lsearch $ids $pid] == -1} {
	$item highlight
	set curr_seg $item
    } else {
	$item unhighlight
	$item untag_verts moving

	if {$item == $curr_seg} {
	    set curr_seg ""
	}
    }

    tag_selected_verts
}


::itcl::body SketchEditFrame::set_canvas {} {
    set gdata [lrange [$itk_option(-mged) get $itk_option(-geometryObject)] 1 end]

    if {$mDetailMode} {
	initCanvas $gdata

	set i 1
	foreach label $mEditLabels {
	    $itk_component(editRB$i) configure -state normal
	    incr i
	}
    } else {
	$::ArcherCore::application restoreCanvas
	initSketchData $gdata
	createSegments

	set i 1
	set mEditMode 0
	foreach label $mEditLabels {
	    $itk_component(editRB$i) configure -state disabled
	    incr i
	}
    }
}


::itcl::body SketchEditFrame::setup_move_arbitrary {} {
    handle_escape
    set mEditMode $moveArbitrary
    $itk_component(canvas) configure -cursor crosshair

    if {$mPrevEditMode == $createBezier} {
	end_bezier $curr_seg 0
	set curr_seg ""
    }

    $itk_component(canvas) dtag moving moving
    unhighlight_selected

    clear_canvas_bindings
    bind $itk_component(canvas) <ButtonPress-1> [::itcl::code $this start_move_arbitrary %x %y 0]
    bind $itk_component(canvas) <Control-ButtonPress-1> [::itcl::code $this start_move_selected %x %y]
    bind $itk_component(canvas) <Shift-ButtonPress-1> [::itcl::code $this start_move_selected2 %x %y]
    bind $itk_component(canvas) <Control-Alt-ButtonPress-1> [::itcl::code $this start_move_segment %x %y 1]

    set mPrevEditMode $mEditMode
}


::itcl::body SketchEditFrame::setup_move_segment {} {
    clear_canvas_bindings
    bind $itk_component(canvas) <ButtonPress-1> [::itcl::code $this start_move_segment %x %y 0]
    bind $itk_component(canvas) <Shift-ButtonPress-1> [::itcl::code $this start_move_segment %x %y 1]
}


::itcl::body SketchEditFrame::setup_move_selected {} {
    clear_canvas_bindings

    tag_selected_verts
    bind $itk_component(canvas) <ButtonPress-1> [::itcl::code $this start_move_selected %x %y]
}


::itcl::body SketchEditFrame::start_arc_radius_adjust {_segment _mx _my} {
#    show_coords $_mx $_my
    set sx [expr {[$itk_component(canvas) canvasx $_mx] / $myscale}]
    set sy [expr {-[$itk_component(canvas) canvasy $_my] / $myscale}]
    set cx 0.0
    set cy 0.0
    set s_list [lindex $mVL $index1]
    set s(0) [lindex $s_list 0]
    set s(1) [lindex $s_list 1]
    set e_list [lindex $mVL $index2]
    set e(0) [lindex $e_list 0]
    set e(1) [lindex $e_list 1]
    if {[circle_3pt $s(0) $s(1) $sx $sy $e(0) $e(1) cx cy]} {
	return
    }

    set center(0) $cx
    set center(1) $cy
    diff diff1 e s
    diff diff2 center s

    if {[cross2d diff1 diff2] > 0.0} {
	set center_is_left 1
    } else {
	set center_is_left 0
    }

    set start_ang [expr { atan2( ($s(1) - $cy), ($s(0) - $cx) ) }]
    set mid_ang [expr { atan2( ($sy - $cy), ($sx - $cx) ) }]
    while {$mid_ang < $start_ang} {
	set mid_ang [expr $mid_ang + $pi2]
    }
    set end_ang [expr { atan2( ($e(1) - $cy), ($e(0) - $cx) ) }]
    while {$end_ang < $mid_ang} {
	set end_ang [expr $end_ang + $pi2]
    }

    if { [expr {$end_ang - $start_ang}] > $pi2 } {
	set orient 1
    } else {
	set orient 0
    }

    set radius [::dist $s(0) $s(1) $cx $cy]
    $_segment set_vars R $radius L $center_is_left O $orient
    redrawSegments
}


::itcl::body SketchEditFrame::start_arc {_mx _my} {
    set mEscapeCreate 0

    if {$mLastIndex != -1} {
	set index1 $mLastIndex
	end_arc $_mx $_my
    } else {
	set index [pick_vertex $_mx $_my]
	if {$index != -1} {
	    set index1 $index
	} else {
	    # screen coords
	    #show_coords $_mx $_my
	    set slist [mouse_to_sketch $_mx $_my]
	    set sx [lindex $slist 0]
	    set sy [lindex $slist 1]

	    set index1 [llength $mVL]
	    lappend mVL "$sx $sy"
	    drawVertices
	}
    }

    bind $itk_component(canvas) <ButtonPress-1> [::itcl::code $this end_arc %x %y]
#    bind $itk_component(coords).x <Return> [::itcl::code $this end_arc 0 0 0]
#    bind $itk_component(coords).y <Return> [::itcl::code $this end_arc 0 0 0]
}


::itcl::body SketchEditFrame::start_bezier {_mx _my} {
    set mEscapeCreate 0

    set index [pick_vertex $_mx $_my]
    if {$index != -1} {
	if {$mLastIndex != -1} {
	    set bezier_indices [list $mLastIndex $index]
	} else {
	    set bezier_indices $index
	}
    } else {
	# screen coords
	#show_coords $_mx $_my
	set slist [mouse_to_sketch $_mx $_my]
	set sx [lindex $slist 0]
	set sy [lindex $slist 1]


	if {$mLastIndex != -1} {
	    set bezier_indices [list $mLastIndex [llength $mVL]]
	} else {
	    set bezier_indices [llength $mVL]
	    lappend bezier_indices $bezier_indices
	}

	lappend mVL "$sx $sy"
	drawVertices
    }

    set curr_seg [SketchBezier \#auto $this $itk_component(canvas) \
		     "D [expr [llength $bezier_indices] - 1] P [list $bezier_indices]"]
    lappend mSegments ::SketchEditFrame::$curr_seg
    $curr_seg draw ""

    # setup to pick next bezier point
    bind $itk_component(canvas) <ButtonPress-1> [::itcl::code $this next_bezier $curr_seg %x %y]

#    bind $itk_component(coords).x <Return> [::itcl::code $this next_bezier $curr_seg 0 0 0]
#    bind $itk_component(coords).y <Return> [::itcl::code $this next_bezier $curr_seg 0 0 0]
}


::itcl::body SketchEditFrame::start_circle {_coord_type _mx _my} {
    set index [pick_vertex $_mx $_my]
    if {$index != -1} {
	set index1 $index
    } else {
	if {$_coord_type == 1} {
	    # screen coords
	    #show_coords $_mx $_my
	    set slist [mouse_to_sketch $_mx $_my]
	    set sx [lindex $slist 0]
	    set sy [lindex $slist 1]
	} elseif {$_coord_type == 0} {
	    # model coords
	    set sx $x_coord
	    set sy $y_coord
	}

	set index1 [llength $mVL]
	lappend mVL "$sx $sy"
    }

    set index2 $index1

    set radius 0.0
    set new_seg [SketchCArc \#auto $this $itk_component(canvas) "S $index1 E $index2 R -1 L 0 O 0"]
    lappend mSegments ::SketchEditFrame::$new_seg
    set needs_saving 1
    continue_circle $new_seg 0 3 $_mx $_my
    drawSegments
    $itk_component(canvas) configure -cursor {}

    clear_canvas_bindings
    bind $itk_component(canvas) <B1-Motion> [::itcl::code $this continue_circle $new_seg 0 1 %x %y]
    bind $itk_component(canvas) <ButtonRelease-1> [::itcl::code $this continue_circle_pick $new_seg %x %y]
#    bind $itk_component(coords).x <Return> [::itcl::code $this continue_circle $new_seg 1 0 0 0]
#    bind $itk_component(coords).y <Return> [::itcl::code $this continue_circle $new_seg 1 0 0 0]
#    bind $itk_component(coords).radius <Return> [::itcl::code $this continue_circle $new_seg 1 2 0 0]
}


::itcl::body SketchEditFrame::start_line {_mx _my} {
    set mEscapeCreate 0

    if {$mLastIndex != -1} {
	set index [pick_vertex $_mx $_my]
	if {$index == $mLastIndex} {
	    return
	}

	set index1 $mLastIndex
	set index2 $index1
	set mLastIndex -1
	start_line_guts $_mx $_my
    } else {
	set index [pick_vertex $_mx $_my]
	if {$index != -1} {
	    set index1 $index
	} else {
	    # screen coords
	    #show_coords $_mx $_my

	    set index1 [llength $mVL]
	    lappend mVL [mouse_to_sketch $_mx $_my]
	}

	set index2 $index1
	start_line_guts
    }
}


::itcl::body SketchEditFrame::start_line_guts {{_mx ""} {_my ""}} {
    $itk_component(canvas) configure -cursor crosshair
    set new_seg [SketchLine \#auto $this $itk_component(canvas) "S $index1 E $index2"]
    lappend mSegments ::SketchEditFrame::$new_seg
    set needs_saving 1
    drawSegments
    clear_canvas_bindings
    set mIgnoreMotion 0

    bind $itk_component(canvas) <B1-Motion> [::itcl::code $this continue_line $new_seg 0 1 %x %y]
    bind $itk_component(canvas) <ButtonPress-1> [::itcl::code $this continue_line_pick $new_seg 1 %x %y]
    bind $itk_component(canvas) <ButtonRelease-1> [::itcl::code $this continue_line_pick $new_seg 2 %x %y]
#    bind $itk_component(coords).x <Return> [::itcl::code $this continue_line $new_seg 2 0 0 0]
#    bind $itk_component(coords).y <Return> [::itcl::code $this continue_line $new_seg 2 0 0 0]

    if {$_mx != "" && $_my != ""} {
	continue_line_pick $new_seg 1 $_mx $_my
    }
}


::itcl::body SketchEditFrame::start_move_arbitrary {_sx _sy _rflag} {
    $itk_component(canvas) dtag moving moving
    unhighlight_selected

    item_pick_highlight $_sx $_sy
    $itk_component(canvas) addtag moving withtag selected
    $itk_component(canvas) configure -cursor {}

    if {$curr_seg != ""} {
	if {$_rflag && [$itk_component(canvas) type $curr_seg] == "arc" } {
	    set indices [$curr_seg get_verts]
	    set index1 [lindex $indices 0]
	    set index2 [lindex $indices 1]
	    start_arc_radius_adjust $curr_seg $_sx $_sy
	    bind $itk_component(canvas) <B1-Motion> [::itcl::code $this start_arc_radius_adjust $curr_seg %x %y]
	    bind $itk_component(canvas) <ButtonRelease-1> [::itcl::code $this end_arc_radius_adjust $curr_seg %x %y]
	} else {
	    tag_selected_verts
	    start_move_selected $_sx $_sy
	}
    } else {
	start_move_selected $_sx $_sy
    }
}


::itcl::body SketchEditFrame::start_move_segment {_sx _sy _rflag} {
    $itk_component(canvas) dtag moving moving
    unhighlight_selected

    seg_pick_highlight $_sx $_sy

    if {$_rflag && [$itk_component(canvas) type $curr_seg] == "arc" } {
	set indices [$curr_seg get_verts]
	set index1 [lindex $indices 0]
	set index2 [lindex $indices 1]
	start_arc_radius_adjust $curr_seg $_sx $_sy
	bind $itk_component(canvas) <B1-Motion> [::itcl::code $this start_arc_radius_adjust $curr_seg %x %y]
	bind $itk_component(canvas) <ButtonRelease-1> [::itcl::code $this end_arc_radius_adjust $curr_seg %x %y]
    } else {
	tag_selected_verts
	start_move_selected $_sx $_sy
    }
}


::itcl::body SketchEditFrame::start_move_selected {_sx _sy} {
    set move_start_x [$itk_component(canvas) canvasx $_sx]
    set move_start_y [$itk_component(canvas) canvasy $_sy]
    bind $itk_component(canvas) <B1-Motion> [::itcl::code $this continue_move 0 %x %y]
    bind $itk_component(canvas) <ButtonRelease-1> [::itcl::code $this continue_move 1 %x %y]
    set needs_saving 1
}


::itcl::body SketchEditFrame::start_move_selected2 {_sx _sy} {
    item_pick_highlight  $_sx $_sy
#    seg_pick_highlight  $_sx $_sy
    start_move_selected $_sx $_sy
}


::itcl::body SketchEditFrame::start_scale {_mx _my} {
    set mPrevMouseX $_mx
    set mPrevMouseY $_my
    bind $itk_component(canvas) <Lock-Control-Shift-B1-Motion> [::itcl::code $this handle_scale %x %y 0]
    bind $itk_component(canvas) <Lock-Control-Shift-ButtonRelease-1> [::itcl::code $this handle_scale %x %y 1]
}


::itcl::body SketchEditFrame::start_translate {_mx _my} {
    set mPrevMouseX $_mx
    set mPrevMouseY $_my
    bind $itk_component(canvas) <Lock-Shift-B1-Motion> [::itcl::code $this handle_translate %x %y 0]
    bind $itk_component(canvas) <Lock-Shift-ButtonRelease-1> [::itcl::code $this handle_translate %x %y 1]
}


::itcl::body SketchEditFrame::start_seg_pick {} {
    $itk_component(canvas) dtag moving moving
    unhighlight_selected

    clear_canvas_bindings
    bind $itk_component(canvas) <ButtonPress-1> [::itcl::code $this seg_pick_highlight %x %y]
#    bind $itk_component(canvas) <Shift-ButtonPress-1> [::itcl::code $this seg_delete %x %y 0]
#    bind $itk_component(canvas) <Control-Shift-ButtonPress-1> [::itcl::code $this seg_delete %x %y 1]
}


::itcl::body SketchEditFrame::start_vert_pick {} {
    $itk_component(canvas) dtag moving moving
    unhighlight_selected

    clear_canvas_bindings
    bind $itk_component(canvas) <ButtonPress-1> [::itcl::code $this vert_pick_highlight %x %y]
    bind $itk_component(canvas) <Shift-ButtonPress-1> [::itcl::code $this vert_delete %x %y]
}


::itcl::body SketchEditFrame::tag_selected_verts {} {
    $itk_component(canvas) dtag moving moving

    $itk_component(canvas) addtag moving withtag selected
    set ids [$itk_component(canvas) find withtag selected]
    foreach id $ids {
	set tags [$itk_component(canvas) gettags $id]
	set type [lindex $tags 1]
	if {$type == "segs"} {
	    $itk_component(canvas) dtag $id moving
	    set item [lindex $tags 0]
	    $item tag_verts moving
	}
    }
}


::itcl::body SketchEditFrame::unhighlight_selected {} {
    set curr_selection [$itk_component(canvas) find withtag selected]
    foreach id $curr_selection {
	set tags [$itk_component(canvas) gettags $id]
	set item [lindex $tags 0]
	set type [lindex $tags 1]
	switch $type {
	    segs   {
		$item unhighlight
	    }
	    verts  {
		$itk_component(canvas) itemconfigure $id -outline black -fill black
	    }
	}
    }
    $itk_component(canvas) dtag selected selected
    $itk_component(canvas) dtag first_select first_select
}


::itcl::body SketchEditFrame::updateGrid {} {
    if {$mAnchorX != "" &&
	$mAnchorY != "" &&
	$mMajorGridSpacing != "" &&
	$mMajorGridSpacing != "0" &&
	$mMinorGridSpacing != "" &&
	$mMinorGridSpacing != "0"} {
	do_scale 1.0 1 1 0
    }
}


::itcl::body SketchEditFrame::validateMajorGridSpacing {_spacing} {
    if {![::cadwidgets::Ged::validateDigit $_spacing]} {
	return 0
    }

    if {$_spacing == ""} {
	return 1
    }

    if {$_spacing == 0} {
	return 0
    }

    return 1
}


::itcl::body SketchEditFrame::validateMinorGridSpacing {_spacing} {
    if {![::cadwidgets::Ged::validateDouble $_spacing]} {
	return 0
    }

    if {$_spacing == "" || $_spacing == "."} {
	return 1
    }

    if {$_spacing == "-" ||
	$_spacing < 0} {
	return 0
    }

    return 1
}


::itcl::body SketchEditFrame::validatePickTol {_tol} {
    if {$_tol == "."} {
	set mPickTol $_tol

	return 1
    }

    if {$_tol == ""} {
	return 1
    }

    if {[string is double $_tol]} {
	if {$_tol < 0} {
	    set t 0
	} else {
	    set t $_tol
	}

	set mPickTol $t

	return 1
    }

    return 0
}


::itcl::body SketchEditFrame::vert_delete {_sx _sy} {
    set index [pick_vertex $_sx $_sy]
    if {$index == -1} return

    if {[vert_is_used $index] != {}} {
	$::ArcherCore::application putString \
	    "Cannot delete a vertex being used by a segment."
	$itk_component(canvas) dtag p$index selected
	$itk_component(canvas) itemconfigure p$index -outline black -fill black
    } else {
	set mVL [lreplace $mVL $index $index]
	fix_vertex_references $index
	write_sketch_to_db
    }
}


::itcl::body SketchEditFrame::vert_is_used {_vindex} {
    set slist {}
    foreach seg $mSegments {
	if {[$seg is_vertex_used $_vindex]} {
	    #$seg describe
	    lappend slist $seg
	}
    }

    return $slist
}


::itcl::body SketchEditFrame::vert_pick_highlight {_sx _sy} {
    set item [pick_vertex $_sx $_sy]
    if {$item == -1} return

    set ids [$itk_component(canvas) find withtag selected]
    set pid [$itk_component(canvas) find withtag p$item]

    if {[lsearch $ids $pid] == -1} {
	$itk_component(canvas) itemconfigure p$item -fill red -outline red
	$itk_component(canvas) addtag selected withtag p$item
    } else {
	$itk_component(canvas) itemconfigure p$item -fill black -outline black
	$itk_component(canvas) dtag p$item selected
    }
}


::itcl::body SketchEditFrame::write_sketch_to_db {} {
    set out "V { [expr {$tobase * $mVx}] [expr {$tobase * $mVy}] [expr {$tobase * $mVz}] }"
    append out " A { [expr {$tobase * $mAx}] [expr {$tobase * $mAy}] [expr {$tobase * $mAz}] }"
    append out " B { [expr {$tobase * $mBx}] [expr {$tobase * $mBy}] [expr {$tobase * $mBz}] } VL {"
    foreach vert $mVL {
	append out " { [expr {$tobase * [lindex $vert 0]}] [expr {$tobase * [lindex $vert 1]}] }"
    }

    append out " } SL {"
    foreach seg $mSegments {
	append out " [$seg serialize $tobase] "
    }
    append out " }"

    set command "adjust $itk_option(-geometryObject)"

    if {[catch "$::ArcherCore::application $command $out" ret]} {
	$::ArcherCore::application putString "ERROR Saving $itk_option(-geometryObject)!!!!, $ret"
    }
}


# Fixme: This needs to inherit from a common base class
class SketchCArc {
    private variable canv
    private variable editor
    private variable start_index -1
    private variable end_index -1
    private variable radius -1
    private variable center_is_left -1
    private variable orientation -1

    constructor { sketch_editor canvas seg } {
	set editor $sketch_editor
	set canv $canvas
	eval "set_vars $seg"
    }

    method set_vars { args } {
	foreach {key value} $args {
	    switch $key {
		S { set start_index $value }
		E { set end_index $value }
		R { set radius $value }
		L { set center_is_left $value }
		O { set orientation $value }
	    }
	}
    }

    method reverse_orientation {} {
	set orientation [expr { !$orientation }]
    }

    method tag_verts { atag } {
	$canv addtag $atag withtag p$start_index
	$canv addtag $atag withtag p$end_index
    }

    method untag_verts { atag } {
	$canv dtag p$start_index $atag
	$canv dtag p$end_index $atag
    }

    method fix_vertex_reference { deleted_verts } {
	foreach index $deleted_verts {
	    if { $start_index > $index } {incr start_index -1}
	    if { $end_index > $index } {incr end_index -1}
	}
    }

    method is_vertex_used { index } {
	if { $start_index == $index } {return 1}
	if { $end_index == $index } {return 1}
	return 0
    }

    method get_type {} {
	return SketchCArc
    }

    method get_verts {} {
	return "$start_index $end_index"
    }

    method get_tangent_at_vertex { index } {
	if { $index != $start_index && $index != $end_index } {
	    return "0 0"
	}
	set myscale [$editor get_scale]
	set vlist [$editor get_vlist]
	set start [lindex $vlist $start_index]
	set sx [expr {$myscale * [lindex $start 0]}]
	set sy [expr {-$myscale * [lindex $start 1]}]
	set end [lindex $vlist $end_index]
	set ex [expr {$myscale * [lindex $end 0]}]
	set ey [expr {-$myscale * [lindex $end 1]}]
	if { $radius < 0.0 } {
	    if { $vertex == $end_index } {
		return "0 0"
	    }
	    set normalx [expr $ex - $sx]
	    set normaly [expr $ey - $sy]
	    set len [::dist $sx $sy $ex $ey]
	    if { [catch {expr 1.0 / $len} one_over_len] } {
		return "0 0"
	    }
	    set tangentx [expr -$normaly * $one_over_len ]
	    set tangenty [expr $normalx * $one_over_len ]
	} else {
	    set tmp_radius [expr {$myscale * $radius}]
	    set center [find_arc_center $sx $sy $ex $ey $tmp_radius $center_is_left]
	    set cx [lindex $center 0]
	    set cy [lindex $center 1]
	    if { $vertex == $start_index } {
		set normalx [expr $sx - $cx]
		set normaly [expr $sy - $cy]
		set len [::dist $sx $sy $cx $cy]
		if { [catch {expr 1.0 / $len} one_over_len] } {
		    return "0 0"
		}
		if { $orientation } {
		    set tangentx [expr $normaly * $one_over_len ]
		    set tangenty [expr -$normalx * $one_over_len ]
		} else  {
		    set tangentx [expr -$normaly * $one_over_len ]
		    set tangenty [expr $normalx * $one_over_len ]
		}
	    } else {
		set normalx [expr $ex - $cx]
		set normaly [expr $ey - $cy]
		set len [::dist $ex $ey $cx $cy]
		if { [catch {expr 1.0 / $len} one_over_len] } {
		    return "0 0"
		}
		if { $orientation } {
		    set tangentx [expr -$normaly * $one_over_len ]
		    set tangenty [expr $normalx * $one_over_len ]
		} else  {
		    set tangentx [expr $normaly * $one_over_len ]
		    set tangenty [expr -$normalx * $one_over_len ]
		}
	    }
	}
	return "$tangentx $tangenty"
    }

    method describe {} {
	set myscale [$editor get_scale]
	set vlist [$editor get_vlist]
	set start [lindex $vlist $start_index]
	set end [lindex $vlist $end_index]

	if { $radius < 0.0 } {
	    $::ArcherCore::application putString "full circle centered at vertex #$end_index ($end) with vertex #$start_index ($start)"
	} else {
	    $::ArcherCore::application putString "circular arc (radius = $radius) from vertex #$start_index ($start) to #$end_index ($end)"
	}
	$::ArcherCore::application putString "	[$this serialize [$editor get_tobase]]"
    }

    method get_radius {} {
	set myscale [$editor get_scale]
	set vlist [$editor get_vlist]
	if { $radius >= 0.0 } {
	    return $radius
	} else {
	    set start [lindex $vlist $start_index]
	    set end [lindex $vlist $end_index]
	    set sx [expr {$myscale * [lindex $start 0]}]
	    set sy [expr {-$myscale * [lindex $start 1]}]
	    set ex [expr {$myscale * [lindex $end 0]}]
	    set ey [expr {-$myscale * [lindex $end 1]}]
	    set tmp_radius [::dist $sx $sy $ex $ey]
	    return $tmp_radius
	}
    }

    method serialize { tobase } {
	if { $radius < 0.0 } {
	    return "{ carc S $start_index E $end_index R $radius L $center_is_left O $orientation } "
	} else {
	    return "{ carc S $start_index E $end_index R [expr {$tobase * $radius}] L $center_is_left O $orientation }"
	}
    }

    method highlight {} {
	$canv itemconfigure $this -outline red
	$canv addtag selected withtag $this
    }

    method unhighlight {} {
	$canv itemconfigure $this -outline black
	$canv dtag $this selected
    }

    method is_full_circle {} {
	if { $radius < 0.0 } {
	    return 1
	} else {
	    return 0
	}
    }

    method draw { atag } {
	set myscale [$editor get_scale]
	set vlist [$editor get_vlist]
	if { $start_index > -1 } {
	    set start [lindex $vlist $start_index]
	    set sx [expr {$myscale * [lindex $start 0]}]
	    set sy [expr {-$myscale * [lindex $start 1]}]
	}
	if { $end_index > -1 } {
	    set end [lindex $vlist $end_index]
	    set ex [expr {$myscale * [lindex $end 0]}]
	    set ey [expr {-$myscale * [lindex $end 1]}]
	}

	if { $radius < 0.0 } {
	    # full circle
	    set tmp_radius [::dist $sx $sy $ex $ey]
	    $editor set_radius $tmp_radius
	    set x1 [expr {$ex - $tmp_radius}]
	    set y1 [expr {$ey - $tmp_radius}]
	    set x2 [expr {$ex + $tmp_radius}]
	    set y2 [expr {$ey + $tmp_radius}]
	    if {[lsearch -exact $atag selected] != -1} {
		$canv create oval $x1 $y1 $x2 $y2 -outline red -tags [concat $this segs $atag]
	    } else {
		$canv create oval $x1 $y1 $x2 $y2 -tags [concat $this segs $atag]
	    }
	} elseif { $radius > 0.0 } {
	    # arc
	    set tmp_radius [expr {$myscale * $radius}]
	    set center [find_arc_center $sx $sy $ex $ey $tmp_radius $center_is_left]
	    set cx [lindex $center 0]
	    set cy [lindex $center 1]
	    set min_radius [::dist $sx $sy $cx $cy]
	    if { $tmp_radius < $min_radius } {
		set tmp_radius $min_radius
		set radius [expr { $tmp_radius / $myscale }]
		# show the new radius in the window
		$editor set_radius $radius
	    }
	    set x1 [expr {$cx - $tmp_radius}]
	    set y1 [expr {$cy - $tmp_radius}]
	    set x2 [expr {$cx + $tmp_radius}]
	    set y2 [expr {$cy + $tmp_radius}]
	    set start_ang [expr {-$SketchEditFrame::rad2deg * atan2( ($sy - $cy), ($sx - $cx))}]
	    set end_ang [expr {-$SketchEditFrame::rad2deg * atan2( ($ey - $cy), ($ex - $cx))}]
	    if { $orientation == 0 } {
		while { $end_ang < $start_ang } {
		    set end_ang [expr {$end_ang + 360.0}]
		}
	    } else {
		while { $end_ang > $start_ang } {
		    set end_ang [expr {$end_ang - 360.0}]
		}
	    }
	    if {[lsearch -exact $atag selected] != -1} {
		$canv create arc $x1 $y1 $x2 $y2 \
		    -start $start_ang \
		    -extent [expr {$end_ang - $start_ang}] \
		    -outline red \
		    -style arc -tags [concat $this segs $atag]
	    } else {
		$canv create arc $x1 $y1 $x2 $y2 \
		    -start $start_ang \
		    -extent [expr {$end_ang - $start_ang}] \
		    -style arc -tags [concat $this segs $atag]
	    }
	}
    }
}


# Fixme: This needs to inherit from a common base class
class SketchBezier {
    private variable canv
    private variable editor
    private variable num_points
    private variable index_list

    constructor { sketch_editor canvas seg } {
	set editor $sketch_editor
	set canv $canvas
	eval "set_vars $seg"
    }

    method set_vars { args } {
	foreach { key value } $args {
	    switch $key {
		D { set num_points [expr $value + 1] }
		P { set index_list $value }
	    }
	}
    }

    method get_type {} {
	return SketchBezier
    }

    method get_verts {} {
	return $index_list
    }

    method tag_verts { atag } {
	foreach index $index_list {
	    $canv addtag $atag withtag p$index
	}
    }

    method untag_verts { atag } {
	foreach index $index_list {
	    $canv dtag p$index $atag
	}
    }

    method fix_vertex_reference { deleted_verts } {
	foreach del $deleted_verts {
	    for { set index 0 } { $index < $num_points } { incr index } {
		set vert [lindex $index_list $index]
		if { $vert > $del } {
		    set index_list [lreplace $index_list $index $index [expr $vert - 1]]
		}
	    }
	}
    }

    method is_vertex_used { index } {
	if { [lsearch -exact $index_list $index] != -1 } {
	    return 1
	} else {
	    return 0
	}
    }

    method get_tangent_at_vertex { index } {
	return "0 0"
    }

    method highlight {} {
	$canv itemconfigure $this -fill red
	$canv addtag selected withtag $this
    }

    method unhighlight {} {
	$canv itemconfigure $this -fill black
	$canv dtag $this selected
    }

    method draw { atag } {
	set myscale [$editor get_scale]
	set vlist [$editor get_vlist]
	set coords ""
	set count 0

	# scale the control point coordinates
	foreach index $index_list {
	    set pt [lindex $vlist $index]
	    lappend coords [expr {$myscale * [lindex $pt 0]}] [expr {-$myscale * [lindex $pt 1]}]
	    incr count
	}

	# calculate some segments
	set mSegments [expr $count * 4]
	set points ""
	for { set i 0 } { $i < $mSegments}  { incr i } {
	    set t [expr {1.0 - [expr [expr $mSegments - $i - 1.0] / [expr $mSegments - 1.0]]}]
	    lappend points [calc_bezier $count $coords $t]
	}
	set points [join $points]

	# draw line segments instead
	if {[lsearch -exact $atag selected ] != -1} {
	    $canv create line $points -fill red -tags [concat $this segs $atag]
	} else {
	    $canv create line $points -tags [concat $this segs $atag]
	}
    }

    method describe {} {
	set vlist [$editor get_vlist]
	set coords ""
	foreach index $index_list {
	    lappend coords [lindex $vlist $index]
	}
	$::ArcherCore::application putString "bezier with $num_points vertices: $coords"
    }

    method serialize { tobase } {
	return "{bezier D [expr $num_points - 1] P [list $index_list]}"
    }
}


# Fixme: This needs to inherit from a common base class
class SketchLine {
    private variable canv
    private variable editor
    private variable start_index -1
    private variable end_index -1

    constructor { sketch_editor canvas seg } {
	set editor $sketch_editor
	set canv $canvas
	eval "set_vars $seg"
    }


    method set_vars { args } {
	foreach {key value} $args {
	    switch $key {
		S { set start_index $value }
		E { set end_index $value }
	    }
	}
    }

    method get_type {} {
	return SketchLine
    }

    method get_verts {} {
	return "$start_index $end_index"
    }

    method tag_verts { atag } {
	$canv addtag $atag withtag p$start_index
	$canv addtag $atag withtag p$end_index
    }

    method untag_verts { atag } {
	$canv dtag p$start_index $atag
	$canv dtag p$end_index $atag
    }

    method fix_vertex_reference { deleted_verts } {
	foreach index $deleted_verts {
	    if { $start_index > $index } {incr start_index -1}
	    if { $end_index > $index } {incr end_index -1}
	}
    }

    method is_vertex_used { index } {
	if { $start_index == $index } {return 1}
	if { $end_index == $index } {return 1}
	return 0
    }

    method get_tangent_at_vertex { index } {
	if { $index != $start_index && $index != $end_index } {
	    return "0 0"
	}
	set vlist [$editor get_vlist]
	set start [lindex $vlist $start_index]
	set end [lindex $vlist $end_index]
	set dx [expr {[lindex $end 0] - [lindex $start 0]}]
	set dy [expr {[lindex $end 1] - [lindex $start 1]}]
	set len [expr {sqrt($dx * $dx + $dy * $dy)}]
	if { [catch {expr 1.0 / $len} one_over_len] } {
	    return "0 0"
	}
	if { $index == $end_index } {
	    set tangentx [expr $dx * $one_over_len ]
	    set tangenty [expr $dy * $one_over_len ]
	} else {
	    set tangentx [expr -$dx * $one_over_len ]
	    set tangenty [expr -$dy * $one_over_len ]
	}
	return "$tangentx $tangenty"
    }

    method highlight {} {
	$canv itemconfigure $this -fill red
	$canv addtag selected withtag $this
    }

    method unhighlight {} {
	$canv itemconfigure $this -fill black
	$canv dtag $this selected
    }

    method draw {atag} {
	set myscale [$editor get_scale]
	set vlist [$editor get_vlist]
	set start [lindex $vlist $start_index]
	set end [lindex $vlist $end_index]
	set sx [expr {$myscale * [lindex $start 0]}]
	set sy [expr {-$myscale * [lindex $start 1]}]
	set ex [expr {$myscale * [lindex $end 0]}]
	set ey [expr {-$myscale * [lindex $end 1]}]
	if {[lsearch -exact $atag selected ] != -1} {
	    $canv create line $sx $sy $ex $ey -fill red -tags [concat $this segs $atag]
	} else {
	    $canv create line $sx $sy $ex $ey -tags [concat $this segs $atag]
	}
    }

    method describe {} {
	set vlist [$editor get_vlist]
	set start [lindex $vlist $start_index]
	set end [lindex $vlist $end_index]
	$::ArcherCore::application putString "line from vertex #$start_index ($start) to #$end_index ($end)"
    }

    method serialize { tobase } {
	return "{ line S $start_index E $end_index }"
    }
}


proc bezdex { i j k num_pts } {
    return [expr $i * [expr $num_pts * 2] + $j * 2 + $k]
}


proc calc_bezier {num_pts coords t} {
    for { set j 0 } { $j < $num_pts } { incr j } {
	set vtemp([bezdex 0 $j 0 $num_pts]) [lindex $coords [expr $j * 2]]
	set vtemp([bezdex 0 $j 1 $num_pts]) [lindex $coords [expr $j * 2 + 1]]
    }

    set degree [expr $num_pts - 1]
    for { set i 1 } { $i < $num_pts } { incr i } {
	for { set j 0 } { $j <= [expr $degree - $i] } { incr j } {
	    set vtemp([bezdex $i $j 0 $num_pts]) \
		[ expr \
		      [expr [expr 1.0 - $t] * $vtemp([bezdex [expr $i - 1] $j 0 $num_pts])] + \
		      [expr $t * $vtemp([bezdex [expr $i - 1] [expr $j + 1] 0 $num_pts])]]

	    set vtemp([bezdex $i $j 1 $num_pts]) \
		[ expr \
		      [expr [expr 1.0 - $t] * $vtemp([bezdex [expr $i - 1] $j 1 $num_pts])] + \
		      [expr $t * $vtemp([bezdex [expr $i - 1] [expr $j + 1] 1 $num_pts])]]
	}
    }

    return "$vtemp([bezdex $degree 0 0 $num_pts]) $vtemp([bezdex $degree 0 1 $num_pts])"
}


proc dot { v1_in v2_in } {
    # args are names of arrays
    upvar $v1_in v1
    upvar $v2_in v2

    return [expr { $v1(0) * $v2(0) + $v1(1) * $v2(1) } ]
}


proc dist { x1 y1 x2 y2 } {
    return [expr { sqrt( ($x1 - $x2) * ($x1 - $x2) + ($y1 - $y2) * ($y1 - $y2))}]
}


proc cross2d { v1_in v2_in } {
    # args are names of arrays
    # only the value of the 'z' component is returned
    upvar $v1_in v1
    upvar $v2_in v2
    return [expr {$v1(0) * $v2(1) - $v1(1) * $v2(0)}]
}


proc diff { v_out v1_in v2_in } {
    # args are the names of arrays
    # returns difference array in v_out
    upvar $v1_in v1
    upvar $v2_in v2
    upvar $v_out v
    set v(0) [expr {$v1(0) - $v2(0)}]
    set v(1) [expr {$v1(1) - $v2(1)}]
}


proc find_arc_center { sx sy ex ey radius center_is_left } {
    # The center must lie on the perpendicular bisector of the line connecting the two points

    # vector from start point to end point
    set s2e(0) [expr {$ex - $sx}]
    set s2e(1) [expr {$ey - $sy}]

    # mid point
    set midx [expr {($sx + $ex) / 2.0}]
    set midy [expr {($sy + $ey) / 2.0}]

    # perpendicular direction
    set dirx [expr {-($midy - $sy)}]
    set diry [expr {$midx - $sx}]

    # unitize direction vector
    set len1sq [expr {$dirx * $dirx + $diry * $diry}]
    set dir_len [expr {sqrt( $len1sq ) }]
    set dirx [expr {$dirx / $dir_len}]
    set diry [expr {$diry / $dir_len}]

    # calculate distance from mid point to center
    set lensq [expr {$radius * $radius - $len1sq}]
    if { $lensq <= 0. } {
	set tmp_len 0.0
    } else {
	set tmp_len [expr {sqrt( $lensq )}]
    }

    # calculate center as distance from mid point along the bisector direction
    set cx [expr { $midx + $dirx * $tmp_len } ]
    set cy [expr { $midy + $diry * $tmp_len } ]

    # There are two possible centers, make sure we have the right one
    set s2c(0) [expr {$cx - $sx}]
    set s2c(1) [expr {$cy - $sy}]

    if { $center_is_left == 0 } {
	if { [cross2d s2e s2c] < 0.0 } {
	    # wrong center
	    set cx [expr { $midx - $dirx * $tmp_len } ]
	    set cy [expr { $midy - $diry * $tmp_len } ]
	}
    } else {
	if { [cross2d s2e s2c] > 0.0 } {
	    # wrong center
	    set cx [expr { $midx - $dirx * $tmp_len } ]
	    set cy [expr { $midy - $diry * $tmp_len } ]
	}
    }

    return "$cx $cy"
}


# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
