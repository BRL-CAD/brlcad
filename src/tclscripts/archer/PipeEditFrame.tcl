#                P I P E E D I T F R A M E . T C L
# BRL-CAD
#
# Copyright (c) 2002-2011 United States Government as represented by
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

	common ATTR_STRINGS {{} O I R V V V}

	# Override what's in GeometryEditFrame
	method initGeometry {gdata}
	method updateGeometry {}
	method createGeometry {_name}
	method p {obj args}
    }

    protected {
	variable mDetail
	variable mDetailHeadings {{} OD ID Radius pX pY pZ}
	variable mEditLabels {
	    {Select Point}
	    {Next Point}
	    {Previous Point}
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

	common setA 1
	common setB 2
	common setC 3
	common setABC 4

	variable mVx ""
	variable mVy ""
	variable mVz ""
	variable mAx ""
	variable mAy ""
	variable mAz ""
	variable mBx ""
	variable mBy ""
	variable mBz ""
	variable mCx ""
	variable mCy ""
	variable mCz ""

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
		set mDetail($index,$PX_COL) [lindex $val 0]
		set mDetail($index,$PY_COL) [lindex $val 1]
		set mDetail($index,$PZ_COL) [lindex $val 2]
	    }
	    {[Oo][0-9]+} {
		set mDetail($index,$OD_COL) $val
	    }
	    {[Ii][0-9]+} {
		set mDetail($index,$ID_COL) $val
	    }
	    {[Rr][0-9]+} {
		set mDetail($index,$RADIUS_COL) $val
	    }
	    default {
		# Shouldn't get here
		puts "Encountered bad one - $attr"
	    }
	}
    }

    GeometryEditFrame::initGeometry $gdata
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

::itcl::body PipeEditFrame::p {obj args} {
    if {[llength $args] != 1 || ![string is double $args]} {
	return "Usage: p sf"
    }

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
	    -entercommand [::itcl::code $this handleEnter]
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
    set mEditCommand pscale
    set mEditClass $EDIT_CLASS_SCALE
    set mEditPCommand [::itcl::code $this p]
    configure -valueUnits "mm"

    switch -- $mEditMode \
	$setA {
	    set mEditParam1 a
	} \
	$setB {
	    set mEditParam1 b
	} \
	$setC {
	    set mEditParam1 c
	} \
	$setABC {
	    set mEditParam1 abc
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

::itcl::body PipeEditFrame::validateDetailEntry {_row _col _newval _clientdata} {
    if {![info exists mDetail($_row,0)]} {
	return 0
    }

    if {[string is double $_newval]} {
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
