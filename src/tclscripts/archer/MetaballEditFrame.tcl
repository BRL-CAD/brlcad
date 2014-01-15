#                M E T A B A L L E D I T F R A M E . T C L
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
#    The class for editing metaballs within Archer.
#

::itcl::class MetaballEditFrame {
    inherit GeometryEditFrame

    constructor {args} {}
    destructor {}

    public {
	common SELECT_COL 0
	common FS_COL 1
	common S_COL 2
	common X_COL 3
	common Y_COL 4
	common Z_COL 5

	common selectPoint 1
	common movePoint 2
	common addPoint 3
	common deletePoint 4
	common setMethod 5
	common setThreshold 6
	common setFieldStrength 7
	common setSmooth 8

	common mDetailHeadings {{} Strength Smooth X Y Z}
	common mEditLabels {
	    {Select Point}
	    {Move Point}
	    {Add Point}
	    {Delete Point}
	    {Set Render Method}
	    {Set Threshold}
	    {Scale Pnt Strength}
	    {Scale Pnt Smooth}
	}

	# Override what's in GeometryEditFrame
	method clearAllTables {}
	method initGeometry {gdata}
	method initTranslate {}
	method updateGeometry {}
	method createGeometry {obj}
	method moveElement {_dm _obj _vx _vy _ocenter}
	method p {obj args}
    }

    protected {
	variable mDetail
	variable mMethod ""
	variable mThreshold ""
	variable mCurrentPoint 0

	variable mCurrentGridRow 0

	# Methods used by the constructor
	# override methods in GeometryEditFrame
	method buildUpperPanel
	method buildLowerPanel

	# Override what's in GeometryEditFrame
	method updateGeometryIfMod {}
	method initEditState {}

	method endMetaballPointMove {_dm _obj _mx _my}
	method handleEnter {_row _col}
	method highlightCurrentPoint {}
	method metaballPointAddCallback {}
	method metaballPointDeleteCallback {_pindex}
	method metaballPointMoveCallback {_pindex}
	method metaballPointSelectCallback {_pindex}

	method singleSelectCallback {_pindex}
	method validateDetailEntry {_row _col _newval _clientdata}

	method watchVar {_name1 _name2 _op}
    }

    private {}
}


# ------------------------------------------------------------
#                      CONSTRUCTOR
# ------------------------------------------------------------

::itcl::body MetaballEditFrame::constructor {args} {
    trace add variable [::itcl::scope mMethod] write [::itcl::code $this watchVar]
    trace add variable [::itcl::scope mThreshold] write [::itcl::code $this watchVar]

    eval itk_initialize $args
}

# ------------------------------------------------------------
#                        OPTIONS
# ------------------------------------------------------------


# ------------------------------------------------------------
#                      PUBLIC METHODS
# ------------------------------------------------------------


::itcl::body MetaballEditFrame::clearAllTables {} {
    $itk_option(-mged) data_axes points {}
    $itk_option(-mged) data_lines points {}

    set mCurrentPoint 0
    $itk_component(detailTab) unselectAllRows
}


## - initGeometry
#
# Initialize the variables containing the object's specification.
#
::itcl::body MetaballEditFrame::initGeometry {gdata} {
    set mInitGeometry 1
    set mMethod [lindex $gdata 0]
    set mThreshold [format "%.6f" [lindex $gdata 1]]

    unset mDetail
    set mDetail(active) ""

    set col 0
    foreach heading $mDetailHeadings {
	set mDetail(0,$col) $heading
	incr col
    }

    set row 1
    foreach item [lindex $gdata 2] {
	set mDetail($row,$SELECT_COL) ""

	set mDetail($row,$X_COL) [format "%.6f" [lindex $item 0]]
	set mDetail($row,$Y_COL) [format "%.6f" [lindex $item 1]]
	set mDetail($row,$Z_COL) [format "%.6f" [lindex $item 2]]
	set mDetail($row,$FS_COL) [format "%.6f" [lindex $item 3]]
	set mDetail($row,$S_COL) [format "%.6f" [lindex $item 4]]

	incr row
    }

    GeometryEditFrame::initGeometry $gdata
    set mInitGeometry 0

    metaballPointSelectCallback [expr {$mCurrentPoint - 1}]
}


::itcl::body MetaballEditFrame::initTranslate {} {
    switch -- $mEditMode \
	$movePoint {
	    $::ArcherCore::application initFindMetaballPoint $itk_option(-geometryObjectPath) 1 [::itcl::code $this metaballPointMoveCallback]
	} \
	default {
	    $::ArcherCore::application putString "MetaballEditFrame::initTranslate: bad mode - $mEditMode, only \"Move Point\" allowed!"
	}
}


::itcl::body MetaballEditFrame::updateGeometry {} {
    if {$itk_option(-mged) == "" ||
	$itk_option(-geometryObject) == ""} {
	return
    }

    set pdata {}
    set row 1
    while {[info exists mDetail($row,0)]} {
	# Build point data from row data
	lappend pdata [list $mDetail($row,$X_COL) $mDetail($row,$Y_COL) $mDetail($row,$Z_COL) $mDetail($row,$FS_COL) $mDetail($row,$S_COL)]
	incr row
    }

    $itk_option(-mged) adjust $itk_option(-geometryObject) $mMethod $mThreshold $pdata

    GeometryEditFrame::updateGeometry
}


::itcl::body MetaballEditFrame::createGeometry {obj} {
    if {![GeometryEditFrame::createGeometry $obj]} {
	return
    }

    $itk_option(-mged) put $obj metaball 1 1 {{-1 0 0 1 1} {1 0 0 1 1}}
}


::itcl::body MetaballEditFrame::moveElement {_dm _obj _vx _vy _ocenter} {
    set mb_i [expr {$mCurrentPoint - 1}]
    set pdata [lindex [$itk_option(-mged) get $itk_option(-geometryObjectPath)] 3]
    set pt [lrange [lindex $pdata $mb_i] 0 2]

    set vpt [$itk_option(-mged) pane_m2v_point $_dm $pt]
    set vz [lindex $vpt 2]
    set mpt [$itk_option(-mged) pane_v2m_point $_dm [list $_vx $_vy $vz]]
    $itk_option(-mged) move_metaballpt $_obj $mb_i $mpt
}


::itcl::body MetaballEditFrame::p {obj args} {
    if {[llength $args] != 1 || ![string is double $args]} {
	return "Usage: p sf"
    }

    set mb_i [expr {$mCurrentPoint - 1}]

    switch -- $mEditMode \
	$setFieldStrength {
	    $::ArcherCore::application p_pscale $obj f$mb_i $args
	} \
	$setSmooth {
	    $::ArcherCore::application p_pscale $obj s$mb_i $args
	} \
	$setMethod {
	    $::ArcherCore::application p_pset $obj m $args
	} \
	$setThreshold {
	    $::ArcherCore::application p_pset $obj t $args
	}

    highlightCurrentPoint

    return ""
}


# ------------------------------------------------------------
#                      PROTECTED METHODS
# ------------------------------------------------------------

::itcl::body MetaballEditFrame::buildUpperPanel {} {
    set parent [$this childsite]

    GeometryEditFrame::buildComboBox $parent \
	method \
	method \
	[::itcl::scope mMethod] \
	"Method:" \
	{0 1 2}

    itk_component add thresholdL {
	::ttk::label $parent.thresholdL \
	    -text "Threshold:" \
	    -anchor e
    } {}
    itk_component add thresholdE {
	::ttk::entry $parent.thresholdE \
	    -textvariable [::itcl::scope mThreshold] \
	    -validate key \
	    -validatecommand {::cadwidgets::Ged::validateDouble %P}
    } {}

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
	    -entercommand [::itcl::code $this handleEnter] \
	    -singleSelectCallback [::itcl::code $this singleSelectCallback]
    } {}

    # Set width of column 0
    $itk_component(detailTab) width 0 3

    set row 0
    grid $itk_component(methodL) -row $row -column 0 -sticky ne
    grid $itk_component(methodF) -row $row -column 1 -sticky nsew
    incr row
    grid $itk_component(thresholdL) -row $row -column 0 -sticky ne
    grid $itk_component(thresholdE) -row $row -column 1 -sticky nsew
    incr row
    grid $itk_component(detailTab) -row $row -column 0 -columnspan 2 -sticky nsew

    grid rowconfigure $parent $row -weight 1
    grid columnconfigure $parent 1 -weight 1
}

::itcl::body MetaballEditFrame::buildLowerPanel {} {
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
	    -command [::itcl::code $this highlightCurrentPoint] \
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

::itcl::body MetaballEditFrame::updateGeometryIfMod {} {
    if {$itk_option(-mged) == "" ||
	$itk_option(-geometryObject) == ""} {
	return
    }

    updateGeometry
}

::itcl::body MetaballEditFrame::initEditState {} {
    set mEditPCommand [::itcl::code $this p]
    set mb_i [expr {$mCurrentPoint - 1}]
    highlightCurrentPoint

    switch -- $mEditMode \
	$selectPoint {
	    set mEditCommand ""
	    set mEditClass ""
	    set mEditParam1 ""
	    $::ArcherCore::application initFindMetaballPoint $itk_option(-geometryObjectPath) 1 [::itcl::code $this metaballPointSelectCallback] 1
	} \
	$movePoint {
	    set mEditCommand move_metaballpt
	    set mEditClass $EDIT_CLASS_TRANS
	    set mEditLastTransMode $::ArcherCore::OBJECT_TRANSLATE_MODE
	    set mEditParam1 ""
	} \
	$addPoint {
	    set mEditCommand ""
	    set mEditClass ""
	    set mEditParam1 ""
	    $::ArcherCore::application initAddMetaballPoint $itk_option(-geometryObjectPath) 1 [::itcl::code $this metaballPointAddCallback]
	    set pdata [lindex [$itk_option(-mged) get $itk_option(-geometryObject)] 3]
	    metaballPointSelectCallback [expr {[llength $pdata] - 1}]
	} \
	$deletePoint {
	    set mEditCommand ""
	    set mEditClass ""
	    set mEditParam1 ""
	    $::ArcherCore::application initFindMetaballPoint $itk_option(-geometryObjectPath) 1 [::itcl::code $this metaballPointDeleteCallback] 1
	} \
	$setMethod {
	    set mEditCommand pset
	    set mEditClass $EDIT_CLASS_SET
	    set mEditParam1 m
	} \
	$setThreshold {
	    set mEditCommand pset
	    set mEditClass $EDIT_CLASS_SET
	    set mEditParam1 t
	} \
	$setFieldStrength {
	    set mEditCommand pscale
	    set mEditClass $EDIT_CLASS_SCALE
	    set mEditParam1 "f$mb_i"
	} \
	$setSmooth {
	    set mEditCommand pscale
	    set mEditClass $EDIT_CLASS_SCALE
	    set mEditParam1 "s$mb_i"
	} \
	default {
	    set mEditCommand ""
	    set mEditPCommand ""
	    set mEditParam1 ""
	}

    GeometryEditFrame::initEditState
}


::itcl::body MetaballEditFrame::endMetaballPointMove {_dm _obj _mx _my} {
    $::ArcherCore::application endObjTranslate $_dm $_obj $_mx $_my
}


::itcl::body MetaballEditFrame::handleEnter {_row _col} {
    if {$itk_option(-mged) == "" ||
	$itk_option(-geometryObject) == "" ||
	$_row < 1 ||
	$_col < 1 ||
	$_col > $Z_COL} {
	return
    }

    updateGeometryIfMod
}


::itcl::body MetaballEditFrame::highlightCurrentPoint {} {
    if {$itk_option(-mged) == "" ||
	[$itk_option(-mged) how $itk_option(-geometryObjectPath)] < 0 ||
	$mCurrentPoint < 1} {
	return
    }

    $itk_option(-mged) refresh_off
    set hlcolor [$::ArcherCore::application getRgbColor [$itk_option(-mged) cget -primitiveLabelColor]]
    $itk_option(-mged) data_axes draw $mHighlightPoints
    $itk_option(-mged) data_axes size $mHighlightPointSize
    eval $itk_option(-mged) data_axes color $hlcolor
    $itk_option(-mged) refresh_on

    set mb_i [expr {$mCurrentPoint - 1}]
    set pdata [lindex [$itk_option(-mged) get $itk_option(-geometryObjectPath)] 3]
    set pt [lrange [lindex $pdata $mb_i] 0 2]

    $itk_option(-mged) data_axes points [list $pt]
}


::itcl::body MetaballEditFrame::metaballPointAddCallback {} {
    set odata [lrange [$itk_option(-mged) get $itk_option(-geometryObject)] 1 end]
    initGeometry $odata
    $::ArcherCore::application setSave

    set pdata [lindex $odata 2]
    metaballPointSelectCallback [expr {[llength $pdata] - 1}]
}


::itcl::body MetaballEditFrame::metaballPointDeleteCallback {_pindex} {
    if {$itk_option(-mged) == ""} {
	return
    }

    set odata [lrange [$itk_option(-mged) get $itk_option(-geometryObject)] 1 end]

    eval $itk_option(-mged) delete_metaballpt $itk_option(-geometryObject) $_pindex
    eval $itk_option(-mged) redraw $itk_option(-geometryObjectPath)
    set odata [lrange [$itk_option(-mged) get $itk_option(-geometryObject)] 1 end]

    set pdata [lindex $odata 2]
    set plen [llength $pdata]

    if {$plen && ($mCurrentPoint < 1 || $mCurrentPoint > $plen)} {
	set mCurrentPoint $plen
    }

    initGeometry $odata
    $::ArcherCore::application setSave
}


::itcl::body MetaballEditFrame::metaballPointMoveCallback {_pindex} {
    if {$itk_option(-mged) == ""} {
	return
    }

    metaballPointSelectCallback $_pindex

    foreach dname {ul ur ll lr} {
	set win [$itk_option(-mged) component $dname]
	bind $win <ButtonRelease-1> "[::itcl::code $this endMetaballPointMove $dname $itk_option(-geometryObject) %x %y]; break"
    }

    set last_mouse [$itk_option(-mged) get_prev_ged_mouse]
    set pt_i [expr {$mCurrentPoint - 1}]
    eval $itk_option(-mged) move_metaballpt_mode $itk_option(-geometryObject) $pt_i $last_mouse
}


::itcl::body MetaballEditFrame::metaballPointSelectCallback {_pindex} {
    if {$_pindex < 0} {
	set mCurrentPoint 0
	return
    }

    set mEditParam1 $_pindex
    incr _pindex
    set mCurrentPoint $_pindex
    $itk_component(detailTab) selectSingleRow $_pindex
    highlightCurrentPoint
}


::itcl::body MetaballEditFrame::singleSelectCallback {_pindex} {
    set mCurrentPoint $_pindex
    initEditState
}


::itcl::body MetaballEditFrame::validateDetailEntry {_row _col _newval _clientdata} {
    if {![info exists mDetail($_row,0)]} {
	return 0
    }

    return [::cadwidgets::Ged::validateDouble $_newval]
}


::itcl::body MetaballEditFrame::watchVar {_name1 _name2 _op} {
    if {$mInitGeometry} {
	return
    }

    set len [llength $_name1]
    if {$len == 3} {
	# strip off everything but the variable name
	set name1 [regsub {::.*::} [lindex $_name1 2] ""]
    } else {
	set name1 $_name1
    }

    switch -- $name1 {
	mMethod -
	mThreshold {
	    updateGeometryIfMod
	}
    }
}


# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
