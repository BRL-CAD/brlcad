#                S K E T C H E D I T F R A M E . T C L
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
# Description:
#    The class for editing sketches within Archer.
#
##############################################################

::itcl::class SketchEditFrame {
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

	common moveArbitrary 1
	common moveSelected 2
	common selectPoints 3
	common selectSegments 4
	common createLine 5
	common createCircle 6
	common createArc 7
	common createBezier 8

	common mVertDetailHeadings {{} X Y Z}
	common mEdgeDetailHeadings {{} A B}
	common mFaceDetailHeadings {{} A B C}
	common mEditLabels {
	    {Move Arbitrary}
	    {Move Selected}
	    {Select Points}
	    {Select Segments}
	    {Create Line}
	    {Create Circle}
	    {Create Arc}
	    {Create Bezier}
	}

	common pi2 [expr {4.0 * asin( 1.0 )}]
	common rad2deg  [expr {360.0 / $pi2}]

	method do_scale {_sf}
	method get_scale {}
	method get_tobase {}
	method get_vlist {}
	method set_radius {_radius}

	method clearEditState {{_clearModeOnly 0}}
	method clearAllTables {}
	method selectSketchPts {_plist}

	# Override what's in GeometryEditFrame
	method initGeometry {_gdata}
	method updateGeometry {}
	method createGeometry {_name}
	method p {obj args}
    }

    protected {
	variable segments {}
	variable V
	variable A
	variable B
	variable VL {}
	variable SL {}

	variable mPickTol 0.3
	variable myscale 1.0
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
	variable save_entry
	variable angle
	variable bezier_indices ""
	variable selection_mode ""

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
	variable mHighlightPoints 1
	variable mHighlightPointSize 1.0
	variable mHighlightPointColor {255 255 255}

	# Methods used by the constructor
	# override methods in GeometryEditFrame
	method buildUpperPanel
	method buildLowerPanel

	# Override what's in GeometryEditFrame
	method updateGeometryIfMod {}
	method initEditState {}

	method applyData {}
	method createSegments {}
	method detailBrowseCommand {_row _col}
	method drawSegments {}
	method drawVertices {}
	method handleDetailPopup {_index _X _Y}
	method handleEnter {_row _col}
	method highlightCurrentSketchElements {}
	method initPointHighlight {}
	method initSketchData {_gdata}
	method loadTables {_gdata}
	method redrawSegments {}

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
	method end_arc_radius_adjust {_segment _mx _my}
	method fix_vertex_references {_unused_vindices}
	method item_pick_highlight {_sx _sy}
	method pick_arbitrary {_sx _sy}
	method pick_segment {_sx _sy}
	method pick_vertex {_sx _sy}
	method seg_delete {_sx _sy _vflag}
	method seg_pick_highlight {_sx _sy}
	method setup_move_arbitrary {}
	method setup_move_point {}
	method setup_move_segment {}
	method setup_move_selected {}
	method start_arc_radius_adjust {_segment _mx _my}
	method start_arc {_coord_type _x _y}
	method start_arc_pick {_x _y}
	method start_bezier {_coord_type _x _y}
	method start_bezier_pick {_x _y}
	method start_circle {_coord_type _x _y}
	method start_circle_pick {_x _y}
	method start_line {_coord_type _x _y}
	method start_line_guts {}
	method start_line_pick {_x _y}
	method start_move_arbitrary {_sx _sy _rflag}
	method start_move_point {_sx _sy}
	method start_move_segment {_sx _sy _rflag}
	method start_move_selected {_sx _sy}
	method start_seg_pick {}
	method start_vert_pick {}
	method tag_selected_verts {}
	method unhighlight_selected {}
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

    set tolocal [$::ArcherCore::application gedCmd base2local]
    set tobase [expr {1.0 / $tolocal}]
}

# ------------------------------------------------------------
#                        OPTIONS
# ------------------------------------------------------------


# ------------------------------------------------------------
#                      PUBLIC METHODS
# ------------------------------------------------------------



::itcl::body SketchEditFrame::do_scale {_sf} {
    set myscale [expr {$myscale * $_sf}]
    $itk_component(canvas) scale all 0 0 $_sf $_sf
    drawSegments
    $itk_component(canvas) configure -scrollregion [$itk_component(canvas) bbox all]
}


::itcl::body SketchEditFrame::get_scale {} {
    return $myscale
}


::itcl::body SketchEditFrame::get_tobase {} {
    return $tobase
}


::itcl::body SketchEditFrame::get_vlist {} {
    return $VL
}


::itcl::body SketchEditFrame::set_radius {_radius} {
    set radius $_radius
}


::itcl::body SketchEditFrame::clearEditState {{_clearModeOnly 0}} {
    set mEditMode 0

    if {$_clearModeOnly} {
	return
    }

    clearAllTables
    set itk_option(-prevGeometryObject) ""
}


::itcl::body SketchEditFrame::clearAllTables {} {
    $itk_option(-mged) data_axes points {}
    $itk_option(-mged) data_lines points {}

    set mCurrentSketchPoints ""
    set mCurrentSketchEdges ""
    set mCurrentSketchFaces ""
    $itk_component(vertTab) unselectAllRows
    $itk_component(edgeTab) unselectAllRows
    $itk_component(faceTab) unselectAllRows
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

    $::ArcherCore::application setCanvas $itk_component(canvas)
    set segments {}
    set VL {}
    set SL {}
    set needs_saving 0
    $itk_component(canvas) delete all
    initSketchData $_gdata
    createSegments
    drawSegments
    clear_canvas_bindings

    update idletasks
    set canv_height [winfo height $itk_component(canvas)]
    set canv_width [winfo width $itk_component(canvas)]
    set min_max [$itk_component(canvas) bbox all]
    set tmp_scale1 [expr double($canv_width) / ([lindex $min_max 2] - [lindex $min_max 0] + 2.0 * $vert_radius)]
    if { $tmp_scale1 < 0.0 } {set tmp_scale1 [expr -$tmp_scale1] }
    set tmp_scale2 [expr double($canv_height) / ([lindex $min_max 3] - [lindex $min_max 1] + 2.0 * $vert_radius)]
    if { $tmp_scale2 < 0.0 } {set tmp_scale2 [expr -$tmp_scale2] }
    if { $tmp_scale1 < $tmp_scale2 } {
	do_scale $tmp_scale1
    } else {
	do_scale $tmp_scale2
    }

    return

    loadTables $_gdata

    GeometryEditFrame::initGeometry $_gdata

    if {$itk_option(-geometryObject) != $itk_option(-prevGeometryObject)} {
	set mCurrentSketchPoints ""
	set mCurrentSketchEdges ""
	set mCurrentSketchFaces ""
	set itk_option(-prevGeometryObject) $itk_option(-geometryObject)

	$itk_component(edgeTab) unselectAllRows
	$itk_component(faceTab) unselectAllRows
    }

    selectCurrentSketchPoints
}


::itcl::body SketchEditFrame::updateGeometry {} {
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
		$::ArcherCore::application putString "Encountered bad one - $mVertDetail($row,$col)"
	    }
    }

    eval $itk_option(-mged) adjust $itk_option(-geometryObject) $pipe_spec
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

    itk_component add picktolL {
	::ttk::label $parent.picktolL \
	    -anchor e \
	    -text "Point Pick Tolerance"
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
    grid $itk_component(picktolL) -column 0 -row $row -sticky e
    grid $itk_component(picktolE) -column 1 -row $row -sticky ew
    grid columnconfigure $parent 1 -weight 1
}


::itcl::body SketchEditFrame::updateGeometryIfMod {} {
    if {$itk_option(-mged) == "" ||
	$itk_option(-geometryObject) == ""} {
	return
    }
}


::itcl::body SketchEditFrame::initEditState {} {
    if {$itk_option(-mged) == ""} {
	return
    }

    set mEditPCommand [::itcl::code $this p]
    set mEditParam1 ""
    set mEditCommand ""
    set mEditClass ""
    highlightCurrentSketchElements

    switch -- $mEditMode \
	$moveArbitrary {
	    setup_move_arbitrary
	} \
	$moveSelected {
	    setup_move_selected
	} \
	$selectPoints {
	    start_vert_pick
	} \
	$selectSegments {
	    start_seg_pick
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


::itcl::body SketchEditFrame::applyData {} {
}


::itcl::body SketchEditFrame::createSegments {} {
    foreach seg $SL {
	set type [lindex $seg 0]
	set seg [lrange $seg 1 end]
	switch $type {
	    line {
		lappend segments ::SketchEditFrame::[SketchLine \#auto $this $itk_component(canvas) $seg]
	    }
	    carc {
		set index [lsearch -exact $seg R]
		incr index
		set tmp_radius [lindex $seg $index]
		if { $tmp_radius > 0.0 } {
		    set tmp_radius [expr {$tolocal * $tmp_radius}]
		    set seg [lreplace $seg $index $index $tmp_radius]
		}
		lappend segments ::SketchEditFrame::[SketchCArc \#auto $this $itk_component(canvas) $seg]
	    }
	    bezier {
		lappend segments ::SketchEditFrame::[SketchBezier \#auto $this $itk_component(canvas) $seg]
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
    $itk_component(canvas) delete all
    drawVertices
    set first 1
    foreach seg $segments {
	if { $first } {
	    set first 0
	    $seg draw first_seg
	} else {
	    $seg draw ""
	}
    }
    $itk_component(canvas) configure -scrollregion [$itk_component(canvas) bbox all]
}


::itcl::body SketchEditFrame::drawVertices {} {
	set index 0
	$itk_component(canvas) delete verts
	foreach vert $VL {
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


::itcl::body SketchEditFrame::initPointHighlight {} {
    if {$itk_option(-mged) == ""} {
	return
    }

    $itk_option(-mged) data_axes draw $mHighlightPoints
}


::itcl::body SketchEditFrame::initSketchData {_gdata} {
    foreach {key value} $_gdata {
	switch $key {
	    V {
		for { set index 0 } { $index < 3 } { incr index } {
		    set V($index) [expr {$tolocal * [lindex $value $index]}]
		}
	    }
	    A {
		for { set index 0 } { $index < 3 } { incr index } {
		    set A($index) [expr {$tolocal * [lindex $value $index]}]
		}
	    }
	    B {
		for { set index 0 } { $index < 3 } { incr index } {
		    set B($index) [expr {$tolocal * [lindex $value $index]}]
		}
	    }
	    VL {
		foreach vert $value {
		    set x [expr {$tolocal * [lindex $vert 0]}]
		    set y [expr {$tolocal * [lindex $vert 1]}]
		    lappend VL "$x $y"
		}
	    }
	    SL {
		set SL $value
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
    bind $itk_component(canvas) <B1-Motion> {}
    bind $itk_component(canvas) <ButtonPress-1> {}
    bind $itk_component(canvas) <Shift-ButtonPress-1> {}
    bind $itk_component(canvas) <ButtonRelease-1> {}
    bind $itk_component(canvas) <Shift-ButtonRelease-1> {}
    bind $itk_component(canvas) <Control-Shift-ButtonPress-1> {}

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
	    set vert [lindex $VL $index1]
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
	    set index1 [llength $VL]
	    lappend VL "$ex $ey"
	    $_segment set_vars S $index1
	} else {
	    set VL [lreplace $VL $index1 $index1 "$ex $ey"]
	}
    }

    $itk_component(canvas) delete ::SketchEditFrame::$_segment
    $_segment draw ""
    set radius [$_segment get_radius]
    $itk_component(canvas) configure -scrollregion [$itk_component(canvas) bbox all]
    if {$_state} {
	create_circle
	drawVertices
	write_sketch_to_db
    }
}


::itcl::body SketchEditFrame::continue_circle_pick {_segment _mx _my} {
    set index [pick_vertex $_mx $_my]
    if {$index == -1} {
	set ex [expr {[$itk_component(canvas) canvasx $_mx] / $myscale}]
	set ey [expr {-[$itk_component(canvas) canvasy $_my] / $myscale}]

	if {$index1 == $index2} {
	    # need to create a new vertex
	    set index1 [llength $VL]
	    lappend VL "$ex $ey"
	    drawVertices
	}
    } else {
	if {$index != $index1} {
	    # At this point index1 refers to the last vertex in VL. Remove it.
	    set VL [lreplace $VL end end]
	    set index1 $index
	} else {
	    # Here we have to add a vertex
	    set vert [lindex $VL $index]
	    set index1 [llength $VL]
	    lappend VL $vert
	}
    }

    continue_circle $_segment 1 3 0 0
}


::itcl::body SketchEditFrame::continue_line {_segment _state _coord_type _mx _my} {
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
	    # use index numbers
	    $_segment set_vars E $index2

	    set vert [lindex $VL $index2]
	    set ex [lindex $vert 0]
	    set ey [lindex $vert 1]
	}
	default {
	    $::ArcherCore::application putString "continue_line: unrecognized coord type - $_coord_type"
	}
    }

    if {$_coord_type != 2} {
	set VL [lreplace $VL $index2 $index2 "$ex $ey"]
    }

    $itk_component(canvas) delete ::SketchEditFrame::$_segment
    $_segment draw ""
    $itk_component(canvas) configure -scrollregion [$itk_component(canvas) bbox all]

    if {$_state == 1} {
	create_line
	write_sketch_to_db
    } elseif {$_state == 3} {
	if {$_coord_type != 2} {
	    # The new segment is using a new vertex for its start point
	    set index1 [expr {[llength $VL] - 1}]
	} else {
	    # The new segment is using an existing vertex for its start point
	    set index1 $index2
	}

	set index2 [llength $VL]
	lappend VL "$ex $ey"

	start_line_guts
	write_sketch_to_db
    }

    drawVertices
}


::itcl::body SketchEditFrame::continue_line_pick {_segment _state _mx _my} {
    set index [pick_vertex $_mx $_my]
    if {$index == -1} return

    # The last item in VL is no longer needed
    set VL [lreplace $VL end end]

    set index2 $index
    continue_line $_segment $_state 2 0 0
}


::itcl::body SketchEditFrame::continue_move {_state _sx _sy} {
    if {$_state != 2} {
#	$this show_coords $_sx $_sy
	set x [$itk_component(canvas) canvasx $_sx]
	set y [$itk_component(canvas) canvasy $_sy]
	set dx [expr $x - $move_start_x]
	set dy [expr $y - $move_start_y]
    } else {
	set id [$itk_component(canvas) find withtag first_select]
	set tags [$itk_component(canvas) gettags $id]
	set index [string range [lindex $tags 0] 1 end]
	set old_coords [$itk_component(canvas) coords $id]
	set dx [expr { $x_coord * $myscale - ([lindex $old_coords 0] + [lindex $old_coords 2])/2.0}]
	set dy [expr { -$y_coord * $myscale - ([lindex $old_coords 1] + [lindex $old_coords 3])/2.0}]
    }
    $itk_component(canvas) move moving $dx $dy

    # actually move the vertices in the vertex list
    set ids [$itk_component(canvas) find withtag moving]
    foreach id $ids {
	set tags [$itk_component(canvas) gettags $id]
	set index [string range [lindex $tags 0] 1 end]
	set new_coords [$itk_component(canvas) coords $id]
	set new_x [expr ([lindex $new_coords 0] + [lindex $new_coords 2])/(2.0 * $myscale)]
	set new_y [expr -([lindex $new_coords 1] + [lindex $new_coords 3])/(2.0 * $myscale)]
	set VL [lreplace $VL $index $index [concat $new_x $new_y]]
    }
    redrawSegments
    if {$_state == 0} {
	set move_start_x $x
	set move_start_y $y
    } else {
	write_sketch_to_db
    }
}


::itcl::body SketchEditFrame::create_arc {} {
    clear_canvas_bindings
    bind $itk_component(canvas) <ButtonRelease-1> [code $this start_arc 1 %x %y]
    bind $itk_component(canvas) <ButtonRelease-3> [code $this start_arc_pick %x %y]
}


::itcl::body SketchEditFrame::create_bezier {} {
    clear_canvas_bindings
    bind $itk_component(canvas) <ButtonRelease-1> [code $this start_bezier 1 %x %y]
    bind $itk_component(canvas) <ButtonRelease-3> [code $this start_bezier_pick %x %y]
}


::itcl::body SketchEditFrame::create_circle {} {
    clear_canvas_bindings
    bind $itk_component(canvas) <ButtonPress-1> [code $this start_circle 1 %x %y]
}


::itcl::body SketchEditFrame::create_line {} {
    clear_canvas_bindings
    bind $itk_component(canvas) <ButtonRelease-1> [code $this start_line 1 %x %y]
    bind $itk_component(canvas) <ButtonRelease-3> [code $this start_line_pick %x %y]
}


::itcl::body SketchEditFrame::end_arc_radius_adjust {_segment _mx _my} {
    # screen coordinates
#    show_coords $_mx $_my
    set sx [expr {[$itk_component(canvas) canvasx $_mx] / $myscale}]
    set sy [expr {-[$itk_component(canvas) canvasy $_my] / $myscale}]
    set cx 0.0
    set cy 0.0

    set s_list [lindex $VL $index1]
    set s(0) [lindex $s_list 0]
    set s(1) [lindex $s_list 1]
    set e_list [lindex $VL $index2]
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

    drawSegments
    write_sketch_to_db
}


::itcl::body SketchEditFrame::fix_vertex_references {_unused_vindices} {
    foreach seg $segments {
	$seg fix_vertex_reference $_unused_vindices
    }
    drawVertices
}


::itcl::body SketchEditFrame::item_pick_highlight {_sx _sy} {
    set item [pick_arbitrary $_sx $_sy]
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

	    if {$item == $curr_seg} {
		set curr_seg ""
	    }
	}
    } else {
	# Strip off the first character
	set item [string range $item 1 end]

	set pid [$itk_component(canvas) find withtag p$item]

	if {[lsearch $ids $pid] == -1} {
	    $itk_component(canvas) itemconfigure p$item -fill red -outline red
	    $itk_component(canvas) addtag selected withtag p$item
	} else {
	    $itk_component(canvas) itemconfigure p$item -fill black -outline black
	    $itk_component(canvas) dtag p$item selected
	}
    }
}


::itcl::body SketchEditFrame::pick_arbitrary {_sx _sy} {
    set x [$itk_component(canvas) canvasx $_sx]
    set y [$itk_component(canvas) canvasy $_sy]
    set item [$itk_component(canvas) find closest $x $y]
    if {$item == ""} {
	return -1
    }

    return $item
}


::itcl::body SketchEditFrame::pick_segment {_sx _sy} {
    set x [$itk_component(canvas) canvasx $_sx]
    set y [$itk_component(canvas) canvasy $_sy]
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


::itcl::body SketchEditFrame::pick_vertex {_sx _sy} {
    set x [$itk_component(canvas) canvasx $_sx]
    set y [$itk_component(canvas) canvasy $_sy]
    set item [$itk_component(canvas) find closest $x $y 0 first_seg]
    if { $item == "" } {
	return -1
    }

    set tags [$itk_component(canvas) gettags $item]
    set index [lsearch -glob $tags p*]
    if { $index == -1 } {
	return -1
    }

    set index [string range [lindex $tags $index] 1 end]

    set sx [expr {$x / $myscale}]
    set sy [expr {-$y / $myscale}]

    # Check to see if the nearest vertex is within tolerance
    set coords [$itk_component(canvas) coords p$index]
    set cx [expr {([lindex $coords 0] + [lindex $coords 2]) / (2.0 * $myscale)}]
    set cy [expr {-([lindex $coords 1] + [lindex $coords 3]) / (2.0 * $myscale)}]
    set u [list $sx $sy 0]
    set v [list $cx $cy 0]
    set delta [::vsub2 $u $v]
    set mag [::vmagnitude $delta]

    if {$mag > $mPickTol} {
	return -1
    }

    return $index
}


::itcl::body SketchEditFrame::seg_delete {_sx _sy _vflag} {
    set item [pick_segment $_sx $_sy]
    if {$item == -1} return

    set index [lsearch $segments $item]
    set segments [lreplace $segments $index $index]
    $itk_component(canvas) delete $item

    if {$_vflag} {
	set unused_vindices {}
	foreach vindex [lsort -integer -decreasing [$item get_verts]] {
	    if {![vert_is_used $vindex]} {
		set VL [lreplace $VL $vindex $vindex]
		lappend unused_vindices $vindex
	    }
	}
	fix_vertex_references $unused_vindices
    }

    ::destroy $item
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

	if {$item == $curr_seg} {
	    set curr_seg ""
	}
    }
}


::itcl::body SketchEditFrame::setup_move_arbitrary {} {
    $itk_component(canvas) dtag moving moving
    unhighlight_selected

    clear_canvas_bindings
    bind $itk_component(canvas) <ButtonPress-1> [::itcl::code $this start_move_arbitrary %x %y 0]
    bind $itk_component(canvas) <Shift-ButtonPress-1> [::itcl::code $this start_move_segment %x %y 1]
}


::itcl::body SketchEditFrame::setup_move_point {} {
    clear_canvas_bindings
    bind $itk_component(canvas) <ButtonPress-1> [::itcl::code $this start_move_point %x %y]
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
    set s_list [lindex $VL $index1]
    set s(0) [lindex $s_list 0]
    set s(1) [lindex $s_list 1]
    set e_list [lindex $VL $index2]
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


::itcl::body SketchEditFrame::start_arc {_coord_type _mx _my} {
}


::itcl::body SketchEditFrame::start_arc_pick {_mx _my} {
}


::itcl::body SketchEditFrame::start_bezier {_coord_type _mx _my} {
}


::itcl::body SketchEditFrame::start_bezier_pick {_mx _my} {
}


::itcl::body SketchEditFrame::start_circle {_coord_type _mx _my} {
    set index [pick_vertex $_mx $_my]
    if {$index != -1} {
	set index1 $index
    } else {
	if {$_coord_type == 1} {
	    # screen coords
	    #show_coords $_mx $_my
	    set sx [expr {[$itk_component(canvas) canvasx $_mx] / $myscale}]
	    set sy [expr {-[$itk_component(canvas) canvasy $_my] / $myscale}]
	} elseif {$_coord_type == 0} {
	    # model coords
	    set sx $x_coord
	    set sy $y_coord
	}

	set index1 [llength $VL]
	lappend VL "$sx $sy"
    }

    set index2 $index1

    set radius 0.0
    set new_seg [Sketch_carc \#auto $this $itk_component(canvas) "S $index1 E $index2 R -1 L 0 O 0"]
    lappend segments $new_seg
    set needs_saving 1
    continue_circle $new_seg 0 3 $_mx $_my
    drawSegments

    clear_canvas_bindings
    bind $itk_component(canvas) <B1-Motion> [code $this continue_circle $new_seg 0 1 %x %y]
    bind $itk_component(canvas) <ButtonRelease-1> [code $this continue_circle_pick $new_seg %x %y]
#    bind $itk_component(coords).x <Return> [code $this continue_circle $new_seg 1 0 0 0]
#    bind $itk_component(coords).y <Return> [code $this continue_circle $new_seg 1 0 0 0]
#    bind $itk_component(coords).radius <Return> [code $this continue_circle $new_seg 1 2 0 0]
}


::itcl::body SketchEditFrame::start_circle_pick {_mx _my} {
}


::itcl::body SketchEditFrame::start_line {_coord_type _mx _my} {
    if {$_coord_type == 1} {
	# screen coords
	#show_coords $_mx $_my
	set sx [expr {[$itk_component(canvas) canvasx $_mx] / $myscale}]
	set sy [expr {-[$itk_component(canvas) canvasy $_my] / $myscale}]
    } elseif {$_coord_type == 0} {
	# model coords
	set sx $x_coord
	set sy $y_coord
    }

    if {$_coord_type != 2} {
	set index1 [llength $VL]
	lappend VL "$sx $sy"
	set index2 [llength $VL]
	lappend VL "$sx $sy"
    }

    start_line_guts
}


::itcl::body SketchEditFrame::start_line_guts {} {
    set new_seg [SketchLine \#auto $this $itk_component(canvas) "S $index1 E $index2"]
    lappend segments $new_seg
    set needs_saving 1
    drawVertices
    drawSegments
    clear_canvas_bindings
    bind $itk_component(canvas) <B1-Motion> [code $this continue_line $new_seg 0 1 %x %y]
    bind $itk_component(canvas) <ButtonPress-1> [code $this continue_line $new_seg 2 1 %x %y]
    bind $itk_component(canvas) <ButtonRelease-1> [code $this continue_line $new_seg 1 1 %x %y]
    bind $itk_component(canvas) <Shift-ButtonRelease-1> [code $this continue_line $new_seg 3 1 %x %y]
    bind $itk_component(canvas) <ButtonRelease-3> [code $this continue_line_pick $new_seg 1 %x %y]
    bind $itk_component(canvas) <Shift-ButtonRelease-3> [code $this continue_line_pick $new_seg 3 %x %y]
#    bind $itk_component(coords).x <Return> [code $this continue_line $new_seg 2 0 0 0]
#    bind $itk_component(coords).y <Return> [code $this continue_line $new_seg 2 0 0 0]
}


::itcl::body SketchEditFrame::start_line_pick {_mx _my} {
    set index [pick_vertex $_mx $_my]
    if {$index == -1} return

    set vertex [lindex $VL $index]
    set index1 $index
    set index2 [llength $VL]
    lappend VL $vertex
    start_line 2 0 0
}


::itcl::body SketchEditFrame::start_move_arbitrary {_sx _sy _rflag} {
    $itk_component(canvas) dtag moving moving
    unhighlight_selected

    item_pick_highlight $_sx $_sy
    $itk_component(canvas) addtag moving withtag selected

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


::itcl::body SketchEditFrame::start_move_point {_sx _sy} {
    $itk_component(canvas) dtag moving moving
    unhighlight_selected

    vert_pick_highlight $_sx $_sy
    $itk_component(canvas) addtag moving withtag selected

    start_move_selected $_sx $_sy
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


::itcl::body SketchEditFrame::start_seg_pick {} {
    $itk_component(canvas) dtag moving moving
    unhighlight_selected

    clear_canvas_bindings
    bind $itk_component(canvas) <ButtonPress-1> [code $this seg_pick_highlight %x %y]
    bind $itk_component(canvas) <Shift-ButtonPress-1> [code $this seg_delete %x %y 0]
    bind $itk_component(canvas) <Control-Shift-ButtonPress-1> [code $this seg_delete %x %y 1]
}


::itcl::body SketchEditFrame::start_vert_pick {} {
    $itk_component(canvas) dtag moving moving
    unhighlight_selected

    clear_canvas_bindings
    bind $itk_component(canvas) <ButtonPress-1> [::itcl::code $this vert_pick_highlight %x %y]
    bind $itk_component(canvas) <Shift-ButtonPress-1> [code $this vert_delete %x %y]
}


::itcl::body SketchEditFrame::tag_selected_verts {} {
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


::itcl::body SketchEditFrame::validatePickTol {_tol} {
    if {$_tol == "."} {
	set mPickTol $_tol

	return 1
    }

    if {[string is double $_tol]} {
	if {$_tol == "" || $_tol < 0} {
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

    if {[vert_is_used $index]} {
	$::ArcherCore::application putString \
	    "Cannot delete a vertex being used by a segment."
	$itk_component(canvas) dtag p$index selected
	$itk_component(canvas) itemconfigure p$index -outline black -fill black
    } else {
	set VL [lreplace $VL $index $index]
	fix_vertex_references $index
	write_sketch_to_db
    }
}


::itcl::body SketchEditFrame::vert_is_used {_vindex} {
    foreach seg $segments {
	if {[$seg is_vertex_used $_vindex]} {
	    return 1
	}
    }
    return 0
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
    set out "V { [expr {$tobase * $V(0)}] [expr {$tobase * $V(1)}] [expr {$tobase * $V(2)}] }"
    append out " A { [expr {$tobase * $A(0)}] [expr {$tobase * $A(1)}] [expr {$tobase * $A(2)}] }"
    append out " B { [expr {$tobase * $B(0)}] [expr {$tobase * $B(1)}] [expr {$tobase * $B(2)}] } VL {"
    foreach vert $VL {
	append out " { [expr {$tobase * [lindex $vert 0]}] [expr {$tobase * [lindex $vert 1]}] }"
    }
    append out " } SL {"
    foreach seg $segments {
	append out " [$seg serialize $tobase] "
    }
    append out " }"

    set command "adjust $itk_option(-geometryObject)"

    if {[catch "$::ArcherCore::application $command $out" ret]} {
	$::ArcherCore::application putString "ERROR Saving $itk_option(-geometryObject)!!!!, $ret"
    }
}


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

	$::ArcherCore::application putString

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

    method get_verts {} {
	return $index_list
    }

    method tag_verts { atag } {
	foreach index $index_list {
	    $canv addtag $atag withtag p$index
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
	set segments [expr $count * 4]
	set points ""
	for { set i 0 } { $i < $segments } { incr i } {
	    set t [expr {1.0 - [expr [expr $segments - $i - 1.0] / [expr $segments - 1.0]]}]
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

    method get_verts {} {
	return "$start_index $end_index"
    }

    method tag_verts { atag } {
	$canv addtag $atag withtag p$start_index
	$canv addtag $atag withtag p$end_index
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
