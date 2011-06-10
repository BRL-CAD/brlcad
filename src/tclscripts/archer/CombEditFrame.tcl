#               C O M B E D I T F R A M E . T C L
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
# Author(s):
#    Bob Parker
#
# Description:
#    The class for editing combinations within Archer.
#
#
# Todo:
#   - Functionality needed for the relative edit tables
#      - create rows (prompt for operator and name)
#      - delete rows
#      - pulldown menu for operators
#      - pulldown menu for names
#      - easier ways to obtain keypoints
#
##############################################################

::itcl::class CombEditFrame {
    inherit GeometryEditFrame

    constructor {args} {}
    destructor {}

    public {
	# Override what's in GeometryEditFrame
	method initGeometry {gdata}
	method updateGeometry {}
	method createGeometry {obj}
    }

    protected {
	variable mRegion ""
	variable mId ""
	variable mAir ""
	variable mLos ""
	variable mGift ""
	variable mRgb ""
	variable mShader ""
	#	variable mMaterial ""
	variable mInherit ""
	variable mMemberData ""
	variable mMemberDataRotAet ""
	variable mMemberDataRotXyz ""
	variable mMemberDataRotArb ""
	variable mMemberDataTra ""
	variable mMemberDataSca ""
	variable mMemberHeadings {"" Op Name Az El Tw Tx Ty Tz Sa Sx Sy Sz Kx Ky Kz}
	variable mMemberHeadingsRotAet {"" Op Name Az El Tw Kx Ky Kz}
	variable mMemberHeadingsRotXyz {"" Op Name Rx Ry Rz Kx Ky Kz}
	variable mMemberHeadingsRotArb {"" Op Name Kx Ky Kz Dx Dy Dz Ang}
	variable mMemberHeadingsTra {"" Op Name Tx Ty Tz}
	variable mMemberHeadingsSca {"" Op Name Sa Sx Sy Sz Kx Ky Kz}
	variable mMemberDataOrder {RotAet RotXyz RotArb Tra Sca}
	variable mLastTabIndex -1
	variable mTableData {RotAet 9 "Rot (AET)" RotXyz 9 "Rot (XYZ)" RotArb 10 "Rot (Arbitrary)" Tra 6 Translation Sca 10 Scale}

	method buildGeneralGUI {}
	method buildShaderGUI {}
	method buildTreeGUI {}
	method buildMembersGUI {}
	method clearMemberData {}
	method clearMemberDataTable {_tname}
	method applyData {}
	method applyMemberData {}
	method getCurrentMemberDataType {}
	method initMemberData {}
	method initMemberDataTable {_tname _mdata}
	method packMemberDataIntoString {_tname}
	method resetData {}

	method handleTablePopup {_index _X _Y}
	method populateMembersMenu {_type _index _X _Y}
	method addMemberCreationMenuEntries {_type _row}
	method selectName  {args}
	method invertSelect {_tname}
	method appendRow  {_tname}
	method insertRow  {_type _row}
	method deleteRow  {_type _row}
	method setKeypoint  {args}
	method setKeypointVC  {_tname _row _col}
	method setKeypointVC_doit  {_tname _row _col}
	method syncColumn {_tname _row _col _val}
	method validateTableEntry {_row _col _newval _tname}

	# Override what's in GeometryEditFrame
	method updateGeometryIfMod {}
    }

    private {}
}


# ------------------------------------------------------------
#                      CONSTRUCTOR
# ------------------------------------------------------------

::itcl::body CombEditFrame::constructor {args} {
    set parent [childsite upper]

    itk_component add tabs {
	::ttk::notebook $parent.tabs
    } {}

    buildGeneralGUI
    buildShaderGUI
    buildTreeGUI
    buildMembersGUI

    pack $itk_component(tabs) -expand yes -fill both

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
::itcl::body CombEditFrame::initGeometry {gdata} {
    if {![catch {bu_get_value_by_keyword region $gdata} _region]} {
	set mRegion $_region
    } else {
	set mRegion ""
    }

    if {![catch {bu_get_value_by_keyword id $gdata} _id]} {
	set mId $_id
    } else {
	set mId ""
    }

    if {![catch {bu_get_value_by_keyword air $gdata} _air]} {
	set mAir $_air
    } else {
	set mAir ""
    }

    if {![catch {bu_get_value_by_keyword los $gdata} _los]} {
	set mLos $_los
    } else {
	set mLos ""
    }

    if {![catch {bu_get_value_by_keyword GIFTmater $gdata} _giftmater]} {
	set mGift $_giftmater
    } else {
	set mGift ""
    }

    if {![catch {bu_get_value_by_keyword rgb $gdata} _rgb]} {
	set mRgb $_rgb
    } else {
	set mRgb ""
    }

    if {![catch {bu_get_value_by_keyword shader $gdata} _shader]} {
	set mShader $_shader
    } else {
	set mShader ""
    }

    $itk_component(combShader) initShader $mShader

    #    if {![catch {bu_get_value_by_keyword material $gdata} _material]} {
    #	set mMaterial $_material
    #    } else {
    #	set mMaterial ""
    #    }

    if {![catch {bu_get_value_by_keyword inherit $gdata} _inherit]} {
	set mInherit $_inherit
    } else {
	set mInherit "no"
    }

    if {![catch {bu_get_value_by_keyword tree $gdata} _tree]} {
	set tree [ArcherCore::unpackTree $_tree]
    } else {
	set tree ""
    }
    $itk_component(combTreeT) delete 1.0 end
    $itk_component(combTreeT) insert end $tree

    GeometryEditFrame::initGeometry $gdata

    initMemberData
}

::itcl::body CombEditFrame::updateGeometry {} {
    if {$itk_option(-mged) == "" ||
	$itk_option(-geometryObject) == ""} {
	return
    }

    lappend _attrs region $mRegion
    if {$mRegion == "no"} {
	set mId ""
	set mAir ""
	set mLos ""
	set mGift ""
    } else {
	if {$mId != ""} {
	    lappend _attrs id $mId
	} else {
	    set mId 0
	    lappend _attrs id 0
	}
    }

    if {$mAir != ""} {
	lappend _attrs air $mAir
    } else {
	#	catch {$itk_option(-mged) attr rm $itk_option(-geometryObject) aircode}
    }

    if {$mLos != ""} {
	lappend _attrs los $mLos
    } else {
	#	catch {$itk_option(-mged) attr rm $itk_option(-geometryObject) los}
    }

    if {$mGift != ""} {
	lappend _attrs GIFTmater $mGift
    } else {
	#	catch {$itk_option(-mged) attr rm $itk_option(-geometryObject) material_id}
    }

    if {[GeometryEditFrame::validateColor $mRgb]} {
	lappend _attrs rgb $mRgb
    } else {
	set mRgb ""
	#	catch {$itk_option(-mged) attr rm $itk_option(-geometryObject) rgb}
    }

    lappend _attrs shader $mShader
    #    lappend _attrs material $mMaterial

    if {$mInherit != ""} {
	lappend _attrs inherit $mInherit
    } else {
	lappend _attrs inherit "no"
    }

    lappend _attrs tree [ArcherCore::packTree [$itk_component(combTreeT) get 1.0 end]]

    if {[catch {eval $itk_option(-mged) adjust $itk_option(-geometryObject) $_attrs}]} {
	return
    }

    if {$itk_option(-geometryChangedCallback) != ""} {
	$itk_option(-geometryChangedCallback)
    }
}

::itcl::body CombEditFrame::createGeometry {obj} {
    if {![GeometryEditFrame::createGeometry $obj]} {
	return
    }

    $itk_option(-mged) put $obj comb \
	region "no" \
	tree {}
}


# ------------------------------------------------------------
#                      PROTECTED METHODS
# ------------------------------------------------------------

::itcl::body CombEditFrame::buildGeneralGUI {} {
    itk_component add combGeneralF {
	::ttk::frame $itk_component(tabs).combgeneralf
    } {}

    set parent $itk_component(combGeneralF)
    itk_component add combType {
	::ttk::label $parent.combtype \
	    -text "Combination:" \
	    -anchor e
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }
    itk_component add combName {
	::ttk::label $parent.combname \
	    -textvariable [::itcl::scope itk_option(-geometryObject)] \
	    -anchor w
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }
    GeometryEditFrame::buildComboBox $parent \
	combRegion \
	combRegion \
	[::itcl::scope mRegion] \
	"Region:" \
	{yes no}
    itk_component add combIdL {
	::ttk::label $parent.combidL \
	    -text "Id:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add combIdE {
	::ttk::entry $parent.combidE \
	    -textvariable [::itcl::scope mId] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDigit %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add combAirL {
	::ttk::label $parent.combairL \
	    -text "Air:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add combAirE {
	::ttk::entry $parent.combairE \
	    -textvariable [::itcl::scope mAir] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDigit %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add combLosL {
	::ttk::label $parent.comblosL \
	    -text "Los:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add combLosE {
	::ttk::entry $parent.comblosE \
	    -textvariable [::itcl::scope mLos] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDigitMax100 %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add combGiftL {
	::ttk::label $parent.combgiftL \
	    -text "GIFTmater:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add combGiftE {
	::ttk::entry $parent.combgiftE \
	    -textvariable [::itcl::scope mGift] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDigit %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add combRgbL {
	::ttk::label $parent.combrgbL \
	    -text "Rgb:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add combRgbE {
	::ttk::entry $parent.combrgbE \
	    -textvariable [::itcl::scope mRgb]
    } {
	rename -font -entryFont entryFont Font
    }
    #    itk_component add combMaterialL {
    #	::label $parent.combmaterialL \
	#	    -text "Material:" \
	#	    -anchor e
    #    } {
    #	rename -font -labelFont labelFont Font
    #    }
    #    itk_component add combMaterialE {
    #	::entry $parent.combmaterialE \
	#	    -textvariable [::itcl::scope mMaterial]
    #    } {
    #	rename -font -entryFont entryFont Font
    #    }
    #    itk_component add combInheritL {
    #	::label $parent.combinheritL \
	#	    -text "Inherit:" \
	#	    -anchor e
    #    } {
    #	rename -font -labelFont labelFont Font
    #    }
    #    itk_component add combInheritE {
    #	::entry $parent.combinheritE \
	#	    -textvariable [::itcl::scope mInherit]
    #    } {
    #	rename -font -entryFont entryFont Font
    #    }
    GeometryEditFrame::buildComboBox $parent \
	combInherit \
	combInherit \
	[::itcl::scope mInherit] \
	"Inherit:" \
	{yes no}

    set row 0
    grid $itk_component(combType) $itk_component(combName) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(combRegionL) \
	-row $row \
	-column 0 \
	-sticky ne
    grid $itk_component(combRegionF) \
	-row $row \
	-column 1 \
	-sticky nsew
    incr row
    grid $itk_component(combIdL) $itk_component(combIdE) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(combAirL) $itk_component(combAirE) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(combLosL) $itk_component(combLosE) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(combGiftL) $itk_component(combGiftE) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(combRgbL) $itk_component(combRgbE) \
	-row $row \
	-sticky nsew
    #    incr row
    #    grid $itk_component(combMaterialL) $itk_component(combMaterialE) \
	#	-row $row \
	#	-sticky nsew
    incr row
    grid $itk_component(combInheritL) \
	-row $row \
	-column 0 \
	-sticky ne
    grid $itk_component(combInheritF) \
	-row $row \
	-column 1 \
	-sticky nsew
    grid columnconfigure $parent 1 -weight 1

    #    pack $itk_component(combGeneralF) -expand yes -fill both
    #    pack $parent -expand yes -fill x -anchor n
    #    grid columnconfigure [namespace tail $this] 1 -weight 1

    # Set up bindings
#    bind $itk_component(combIdE) <Return> [::itcl::code $this updateGeometryIfMod]
#    bind $itk_component(combAirE) <Return> [::itcl::code $this updateGeometryIfMod]
#    bind $itk_component(combLosE) <Return> [::itcl::code $this updateGeometryIfMod]
#    bind $itk_component(combGiftE) <Return> [::itcl::code $this updateGeometryIfMod]
#    bind $itk_component(combRgbE) <Return> [::itcl::code $this updateGeometryIfMod]

#    bind $itk_component(combIdE) <Tab> [::itcl::code $this updateGeometryIfMod]
#    bind $itk_component(combAirE) <Tab> [::itcl::code $this updateGeometryIfMod]
#    bind $itk_component(combLosE) <Tab> [::itcl::code $this updateGeometryIfMod]
#    bind $itk_component(combGiftE) <Tab> [::itcl::code $this updateGeometryIfMod]
#    bind $itk_component(combRgbE) <Tab> [::itcl::code $this updateGeometryIfMod]

    #    bind $itk_component(combMaterialE) <Return> [::itcl::code $this updateGeometryIfMod]
    #    bind $itk_component(combInheritE) <Return> [::itcl::code $this updateGeometryIfMod]

    $itk_component(tabs) add $itk_component(combGeneralF) -text "General"
    incr mLastTabIndex
}

::itcl::body CombEditFrame::buildShaderGUI {} {
    itk_component add combShader {
	::ShaderEdit $itk_component(tabs).shader
    } {}

#	    -shaderChangedCallback [::itcl::code $this updateGeometryIfMod]
    #    bind $itk_component(combShaderE) <Return> [::itcl::code $this updateGeometryIfMod]

    $itk_component(tabs) add $itk_component(combShader) -text "Shader"
    incr mLastTabIndex
}

::itcl::body CombEditFrame::buildTreeGUI {} {
    itk_component add combTreeF {
	::ttk::frame $itk_component(tabs).treef
    } {}

    set parent $itk_component(combTreeF)
    itk_component add combTreeT {
	::text $parent.treeT \
	    -background $::ArcherCore::LABEL_BACKGROUND_COLOR
    } {
	#usual
    }

    grid $itk_component(combTreeT) \
	-row 0 \
	-sticky nsew
    grid columnconfigure $parent 0 -weight 1
    grid rowconfigure $parent 0 -weight 1

    #    bind $itk_component(combTreeT) <Leave> [::itcl::code $this updateGeometryIfMod]
    #    bind $itk_component(combTreeT) <Return> [::itcl::code $this updateGeometryIfMod]
    #    bind $itk_component(combTreeT) <FocusOut> [::itcl::code $this updateGeometryIfMod]
    #bind $itk_component(combTreeT) <KeyRelease> [::itcl::code $this updateGeometryIfMod]

    $itk_component(tabs) add $itk_component(combTreeF) -text "Members"
    incr mLastTabIndex
}

::itcl::body CombEditFrame::buildMembersGUI {} {
    clearMemberData

    itk_component add combMembersTabs {
	::ttk::notebook $itk_component(tabs).memberstabs
    } {}

    itk_component add combMembersMenu {
	    ::menu $itk_component(tabs).membersmenu \
		-tearoff 0
    } {}

    itk_component add combMembersOpMenu {
	    ::menu $itk_component(combMembersMenu).membersopmenu \
		-tearoff 0
    } {}

    foreach {tname cols text} $mTableData {
	# Create table
	itk_component add combMembers$tname\F {
	    ::ttk::frame $itk_component(tabs).members$tname\F
	} {}
	itk_component add combMembers$tname {
	    ::cadwidgets::TkTable $itk_component(combMembers$tname\F).members$tname \
		[::itcl::scope mMemberData$tname] \
		[subst $[subst mMemberHeadings$tname]] \
		-cursor arrow \
		-height 0 \
		-maxheight 2000 \
		-width 0 \
		-rows 100000 \
		-colstretchmode unset \
		-validate 1 \
		-validatecommand [::itcl::code $this validateTableEntry] \
		-vclientdata $tname \
		-tablePopupHandler [::itcl::code $this handleTablePopup]
	} {}

	# Set width of columns 0, 1 and 2
	$itk_component(combMembers$tname) width 0 3
	$itk_component(combMembers$tname) width 1 4
	$itk_component(combMembers$tname) width 2 20

	grid $itk_component(combMembers$tname) -sticky nsew
	grid columnconfigure $itk_component(combMembers$tname\F) 0 -weight 1
	grid rowconfigure $itk_component(combMembers$tname\F) 0 -weight 1

	$itk_component(combMembersTabs) add $itk_component(combMembers$tname\F) -text $text
    }

    grid columnconfigure $itk_component(combMembersTabs) 0 -weight 1
    grid rowconfigure $itk_component(combMembersTabs) 0 -weight 1

    $itk_component(tabs) add $itk_component(combMembersTabs) -text "Relative Edit"
    incr mLastTabIndex

    set parent [childsite lower]
    itk_component add combApplyB {
	::ttk::button $parent.applyB \
	    -text "Apply" \
	    -command [::itcl::code $this applyData]
    } {}
    itk_component add combResetB {
	::ttk::button $parent.resetB \
	    -text "Reset" \
	    -command [::itcl::code $this resetData]
    } {}
    grid x $itk_component(combApplyB) $itk_component(combResetB) x -sticky nsew
    grid columnconfigure $parent 0 -weight 1
    grid columnconfigure $parent 3 -weight 1
    grid rowconfigure $parent 1 -weight 1
}

::itcl::body CombEditFrame::clearMemberData {} {
    unset mMemberDataRotAet
    unset mMemberDataRotXyz
    unset mMemberDataRotArb
    unset mMemberDataTra
    unset mMemberDataSca
#    unset mMemberData

    set col 0
    foreach heading $mMemberHeadingsRotAet {
	set mMemberDataRotAet(0,$col) $heading
	incr col
    }
    set col 0
    foreach heading $mMemberHeadingsRotXyz {
	set mMemberDataRotXyz(0,$col) $heading
	incr col
    }
    set col 0
    foreach heading $mMemberHeadingsRotArb {
	set mMemberDataRotArb(0,$col) $heading
	incr col
    }
    set col 0
    foreach heading $mMemberHeadingsTra {
	set mMemberDataTra(0,$col) $heading
	incr col
    }
    set col 0
    foreach heading $mMemberHeadingsSca {
	set mMemberDataSca(0,$col) $heading
	incr col
    }
#    set col 0
#    foreach heading $mMemberHeadings {
#	set mMemberData(0,$col) $heading
#	incr col
#    }
}

::itcl::body CombEditFrame::clearMemberDataTable {_tname} {
    unset mMemberData$_tname
    set col 0
    foreach heading [subst $[subst mMemberHeadings$_tname]] {
	set mMemberData$_tname\(0,$col\) $heading
	incr col
    }
}

::itcl::body CombEditFrame::applyData {} {
    set tindex [$itk_component(tabs) index current]
    if {$tindex != $mLastTabIndex} {
	updateGeometryIfMod
	return
    }

    applyMemberData
}

::itcl::body CombEditFrame::applyMemberData {} {
    set dtype [getCurrentMemberDataType]
    set tname [lindex $mMemberDataOrder $dtype]
    set mdata [packMemberDataIntoString $tname]

    # Save table row selection state
    set row 1
    set rsslist {}
    while {[info exists mMemberData$tname\($row,0\)]} {
	lappend rsslist [subst $[subst mMemberData$tname\($row,0\)]]
	incr row
    }

    incr dtype 2
    catch {eval $itk_option(-mged) combmem -r $dtype $itk_option(-geometryObject) [regsub -all {\n} $mdata " "]}
    GeometryEditFrame::updateGeometry

    # Restore table row selection state
    set row 1
    foreach item $rsslist {
	set mMemberData$tname\($row,0\) $item
	incr row
    }
}

::itcl::body CombEditFrame::getCurrentMemberDataType {} {
    set dtype [$itk_component(combMembersTabs) index current]
    return $dtype
}

::itcl::body CombEditFrame::initMemberData {} {
    if {$itk_option(-mged) == "" ||
	$itk_option(-geometryObject) == ""} {
	return
    }

    clearMemberData

    foreach {tname ttype} {RotAet 2 RotXyz 3 RotArb 4 Tra 5 Sca 6} {
	set mdata [$itk_option(-mged) combmem -i $ttype $itk_option(-geometryObject)]
	initMemberDataTable $tname $mdata
    }
}

::itcl::body CombEditFrame::initMemberDataTable {_tname _mdata} {
    set i 1
    foreach row [split $_mdata "\n"] {
	if {$row == ""} {
	    incr i
	    continue
	}

	set mMemberData$_tname\($i,0\) ""
	set j 1
	foreach col $row {
	    set mMemberData$_tname\($i,$j\) $col
	    incr j
	}

	incr i
    }
}

::itcl::body CombEditFrame::packMemberDataIntoString {_tname} {
    set row ""
    foreach index [lsort -dictionary [array names mMemberData$_tname]] {
	if {$index == "active"} {
	    continue
	}

	set ipair [regsub {,} $index " "]
	set col [lindex $ipair 1]
	if {$col == 0} {
	    continue
	}
	set new_row [lindex $ipair 0]

	if {$row != $new_row} {
	    set row $new_row
	}

	# Skip headings
	if {$row} {
	    lappend curr_mdata($row) [subst $[subst mMemberData$_tname\($index\)]]
	}
    }

    set mdata ""
    foreach index [lsort -dictionary [array names curr_mdata]] {
	append mdata $curr_mdata($index) "\n"
    }

    return $mdata
}

::itcl::body CombEditFrame::resetData {} {
    set gdata [lrange [$itk_option(-mged) get $itk_option(-geometryObject)] 1 end]
    initGeometry $gdata
}

::itcl::body CombEditFrame::handleTablePopup {_index _X _Y} {
    set type [getCurrentMemberDataType]
    populateMembersMenu $type $_index $_X $_Y
}

::itcl::body CombEditFrame::populateMembersMenu {_type _index _X _Y} {
    $itk_component(combMembersMenu) delete 0 end
    $itk_component(combMembersOpMenu) delete 0 end

    set ilist [split $_index ,]
    set row [lindex $ilist 0] 
    if {$row == 0} {
	# Ignore if in header
	return
    }

    set col [lindex $ilist 1]

    # Is the cell indicated by _index empty?
    set tname [lindex $mMemberDataOrder $_type]
    set anames [lsort -dictionary [array names mMemberData$tname]]
    set i [lsearch -sorted $anames $_index]
    if {$i == -1} {
	set cellEmpty 1
    } else {
	set cellEmpty 0
    }

    if {$cellEmpty} {
	$itk_component(combMembersMenu) add command \
	    -label "Append Row" \
	    -command [::itcl::code $this appendRow $_type]
    } else {

	if {$col == 0} {
	    if {[subst $[subst mMemberData$tname\($_index\)]] == "*"} {
		$itk_component(combMembersMenu) add command \
		    -label "Deselect" \
		    -command "$itk_component(combMembers$tname) setTableVal $_index \"\""
	    } else {
		$itk_component(combMembersMenu) add command \
		    -label "Select" \
		    -command "$itk_component(combMembers$tname) setTableVal $_index *"
	    }

	    $itk_component(combMembersMenu) add separator
	    $itk_component(combMembersMenu) add command \
		-label "Deselect All" \
		-command "$itk_component(combMembers$tname) setTableCol $col \"\""
	    $itk_component(combMembersMenu) add command \
		-label "Select All" \
		-command "$itk_component(combMembers$tname) setTableCol $col *"
	    $itk_component(combMembersMenu) add command \
		-label "Invert All" \
		-command [::itcl::code $this invertSelect $tname]
	    $itk_component(combMembersMenu) add separator
	} elseif {$col == 1} {
	    $itk_component(combMembersMenu) add cascade \
		-label "Select Op" \
		-menu $itk_component(combMembersOpMenu)
	    
	    $itk_component(combMembersOpMenu) add command \
		-label "Union" \
		-command "$itk_component(combMembers$tname) setTableVal $_index u"
	    $itk_component(combMembersOpMenu) add command \
		-label "Intersection" \
		-command "$itk_component(combMembers$tname) setTableVal $_index +"
	    $itk_component(combMembersOpMenu) add command \
		-label "Subtraction" \
		-command "$itk_component(combMembers$tname) setTableVal $_index -"
	} elseif {$col == 2} {
#	    $itk_component(combMembersMenu) add command \
		-label "Select Name" \
		-command [::itcl::code $this selectName $_type]
	} elseif {($_type == 0 && 6 <= $col && $col <= 8) ||
		  ($_type == 1 && 6 <= $col && $col <= 8)} {
	    $itk_component(combMembersMenu) add command \
		-label "Set Keypoint (View Center)" \
		-command [::itcl::code $this setKeypointVC $tname $row 6]
	} elseif {$_type == 2 && 3 <= $col && $col <= 5} {
	    $itk_component(combMembersMenu) add command \
		-label "Set Keypoint (View Center)" \
		-command [::itcl::code $this setKeypointVC $tname $row 3]
	} elseif {$_type == 4 && 7 <= $col && $col <= 9} {
	    $itk_component(combMembersMenu) add command \
		-label "Set Keypoint (View Center)" \
		-command [::itcl::code $this setKeypointVC $tname $row 7]
	}

	addMemberCreationMenuEntries $_type $row
    }

    tk_popup $itk_component(combMembersMenu) $_X $_Y
}

::itcl::body CombEditFrame::addMemberCreationMenuEntries {_type _row} {
    $itk_component(combMembersMenu) add command \
	-label "Append Row" \
	-command [::itcl::code $this appendRow $_type]
    $itk_component(combMembersMenu) add command \
	-label "Insert Row" \
	-command [::itcl::code $this insertRow $_type $_row]
    $itk_component(combMembersMenu) add command \
	-label "Delete Row" \
	-command [::itcl::code $this deleteRow $_type $_row]
}

::itcl::body CombEditFrame::selectName {args} {
    puts "CombEditFrame::selectName: implement me"
}

::itcl::body CombEditFrame::invertSelect {_tname} {
    set row 1
    while {[info exists mMemberData$_tname\($row,0\)]} {
	if {[subst $[subst mMemberData$_tname\($row,0\)]] == "*"} {
	    set mMemberData$_tname\($row,0\) ""
	} else {
	    set mMemberData$_tname\($row,0\) "*"
	}
	incr row
    }
}

::itcl::body CombEditFrame::appendRow {_type} {
    set tname [lindex $mMemberDataOrder $_type]
    set anames [lsort -dictionary [array names mMemberData$tname]]
    set lastIndex [lindex $anames end]

    if {$lastIndex == "active"} {
	# Use next-to-last
	set lastIndex [lindex $anames end-1]
    }

    set ilist [split $lastIndex ,]
    set lastRow [lindex $ilist 0]
    set lastCol [lindex $ilist 1]

    set type [expr {$_type + 2}]
    set mdata [$itk_option(-mged) combmem -i $type $itk_option(-geometryObject)]
    set dlist [split $mdata "\n"]
    set row [lindex $dlist 0]

    incr lastRow
    set mMemberData$tname\($lastRow,0\) ""
    set mMemberData$tname\($lastRow,1\) u
    set mMemberData$tname\($lastRow,2\) edit_me
    set j 3
    foreach col [lrange $row 2 end] {
	set mMemberData$tname\($lastRow,$j\) $col
	incr j
    }
}

::itcl::body CombEditFrame::insertRow {_type _row} {
    set tname [lindex $mMemberDataOrder $_type]
    set mdata [packMemberDataIntoString $tname]

    # Get a sample data row for this table type
    set type [expr {$_type + 2}]
    set tmpdata [$itk_option(-mged) combmem -i $type $itk_option(-geometryObject)]
    set tmplist [split $tmpdata "\n"]
    set rowData [lindex $tmplist 0]
    set rowData [lreplace $rowData 0 1 u edit_me]

    # Save table row selection state
    set row 1
    set rsslist {}
    while {[info exists mMemberData$tname\($row,0\)]} {
	# Append extra state for the new row
	if {$row == $_row} {
	    lappend rsslist ""
	}

	lappend rsslist [subst $[subst mMemberData$tname\($row,0\)]]
	incr row
    }

    incr _row -1
    set mlist [split $mdata "\n"]
    set mlist [linsert $mlist $_row $rowData]
    set mdata ""
    foreach item $mlist {
	append mdata $item "\n"
    }

    clearMemberDataTable $tname
    initMemberDataTable $tname $mdata

    # Restore table row selection state
    set row 1
    foreach item $rsslist {
	set mMemberData$tname\($row,0\) $item
	incr row
    }
}

::itcl::body CombEditFrame::deleteRow {_type _row} {
    set tname [lindex $mMemberDataOrder $_type]
    set mdata [packMemberDataIntoString $tname]

    # Save table row selection state
    set row 1
    set rsslist {}
    while {[info exists mMemberData$tname\($row,0\)]} {
	# Effectively skip/delete state for the deleted row
	if {$row != $_row} {
	    lappend rsslist [subst $[subst mMemberData$tname\($row,0\)]]
	}

	incr row
    }

    incr _row -1
    set mlist [split $mdata "\n"]
    set mlist [lreplace $mlist $_row $_row]
    set mdata ""
    foreach item $mlist {
	append mdata $item "\n"
    }

    clearMemberDataTable $tname
    initMemberDataTable $tname $mdata

    # Restore table row selection state
    set row 1
    foreach item $rsslist {
	set mMemberData$tname\($row,0\) $item
	incr row
    }
}

::itcl::body CombEditFrame::setKeypoint {args} {
    puts "CombEditFrame::setKeypoint: implement me"
}

::itcl::body CombEditFrame::setKeypointVC {_tname _row _col} {
    if {[subst $[subst mMemberData$_tname\($_row,0\)]] == "*"} {
	set anames [lsort -dictionary [array names mMemberData$_tname]]
	set rows {}
	foreach aindex [lsearch -all -regexp $anames "\[0-9\]+,0"] {
	    set aname [lindex $anames $aindex]
	    if {[subst $[subst mMemberData$_tname\($aname\)]] == "*"} {
		set alist [split $aname ,]
		lappend rows [lindex $alist 0]
	    }
	}
    } else {
	set rows $_row
    }

    foreach row $rows {
	setKeypointVC_doit $_tname $row $_col
    }
}

::itcl::body CombEditFrame::setKeypointVC_doit {_tname _row _col} {
    if {$itk_option(-mged) == ""} {
	return
    }

    set center [$itk_option(-mged) center]

    set j $_col
    foreach n $center {
	set mMemberData$_tname\($_row,$j\) $n
	incr j
    }
}

::itcl::body CombEditFrame::syncColumn {_tname _row _col _val} {
    set anames [lsort -dictionary [array names mMemberData$_tname]]
    foreach aindex [lsearch -all -regexp $anames "\[0-9\]+,$_col"] {
	set index [lindex $anames $aindex]
	set ilist [split $index ,]
	set row [lindex $ilist 0] 
	if {$row == 0 || $row == $_row} {
	    # Ignore if in header
	    continue
	}

	if {![info exists mMemberData$_tname\($row,0\)]} {
	    continue
	}

	if {[subst $[subst mMemberData$_tname\($row,0\)]] == "*"} {
	    set mMemberData$_tname\($row,$_col\) $_val
	}
    }

#    puts "CombEditFrame::syncColumn: set column $_col values to $_val"
#    puts "CombEditFrame::syncColumn: anames - $anames"
#    puts "CombEditFrame::syncColumn: colnames - $colnames"
}

::itcl::body CombEditFrame::validateTableEntry {_row _col _newval _tname} {
    if {$_col == 0 || $_col == 1 || ![info exists mMemberData$_tname\($_row,0\)]} {
	return 0
    }

    if {$_col == 1} {
	if {[regexp {[/\?\*\n]} $_newval]} {
	    return 0
	}

	return 1
    }

    if {[string is double $_newval]} {
	syncColumn $_tname $_row $_col $_newval
	return 1
    }

    return 0
}

::itcl::body CombEditFrame::updateGeometryIfMod {} {
    if {$itk_option(-mged) == "" ||
	$itk_option(-geometryObject) == ""} {
	return
    }

    set gdata [$itk_option(-mged) get $itk_option(-geometryObject)]
    set gdata [lrange $gdata 1 end]

    set _mRegion [bu_get_value_by_keyword region $gdata]

    if {![catch {bu_get_value_by_keyword id $gdata} _id]} {
	set _mId $_id
    } else {
	set _mId ""
    }

    if {![catch {bu_get_value_by_keyword air $gdata} _air]} {
	set _mAir $_air
    } else {
	set _mAir ""
    }

    if {![catch {bu_get_value_by_keyword los $gdata} _los]} {
	set _mLos $_los
    } else {
	set _mLos ""
    }

    if {![catch {bu_get_value_by_keyword GIFTmater $gdata} _giftmater]} {
	set _mGift $_giftmater
    } else {
	set _mGift ""
    }

    if {![catch {bu_get_value_by_keyword rgb $gdata} _rgb]} {
	set _mRgb $_rgb
    } else {
	set _mRgb ""
    }

    set mShader [$itk_component(combShader) getShaderSpec]
    if {![catch {bu_get_value_by_keyword shader $gdata} _shader]} {
	set _mShader $_shader
    } else {
	set _mShader ""
    }

    #    if {![catch {bu_get_value_by_keyword material $gdata} _material]} {
    #	set _mMaterial $_material
    #    } else {
    #	set _mMaterial ""
    #    }

    if {![catch {bu_get_value_by_keyword inherit $gdata} _inherit]} {
	set _mInherit $_inherit
    } else {
	set _mInherit ""
    }

    set tree [string trim [$itk_component(combTreeT) get 1.0 end]]
    if {![catch {bu_get_value_by_keyword tree $gdata} _tree]} {
	set _tree [string trim [ArcherCore::unpackTree $_tree]]
    } else {
	set _tree ""
    }

#    set mdata [$itk_option(-mged) combmem $itk_option(-geometryObject)]
#    set row ""
#    foreach index [lsort -dictionary [array names mMemberData]] {
#	if {$index == "active"} {
#	    continue
#	}
#
#	set ipair [regsub {,} $index " "]
#	set new_row [lindex $ipair 0]
#	set col [lindex $ipair 1]
#
#	if {$row != $new_row} {
#	    set row $new_row
#	}
#
#	# Skip headings
#	if {$row} {
#	    lappend curr_mdata($row) $mMemberData($index)
#	}
#    }
#
#    set _mdata ""
#    foreach index [lsort -dictionary [array names curr_mdata]] {
#	append _mdata $curr_mdata($index)
#	append _mdata "\n"
#    }
#
#    if {$_mdata != $mdata} {
#	set ret [catch {eval $itk_option(-mged) combmem $itk_option(-geometryObject) [regsub -all {\n} $_mdata " "]}]
#    }
#    set ret [catch {eval $itk_option(-mged) combmem -r $itk_option(-geometryObject) [regsub -all {\n} $_mdata " "]}]

    # Temporarily adjust mInherit
    if {$mInherit == "no"} {
	set mInherit ""
    }

    #	$_mMaterial != $mMaterial ||
    if {$_mRegion != $mRegion ||
	$_mId != $mId ||
	$_mAir != $mAir ||
	$_mLos != $mLos ||
	$_mGift != $mGift ||
	$_mRgb != $mRgb ||
	$_mShader != $mShader ||
	$_mInherit != $mInherit ||
	$_tree != $tree} {
	updateGeometry
    }

    if {$mInherit == ""} {
	set mInherit "no"
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
