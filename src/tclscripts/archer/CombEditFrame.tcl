#               C O M B E D I T F R A M E . T C L
# BRL-CAD
#
# Copyright (c) 2002-2010 United States Government as represented by
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

	method buildGeneralGUI {}
	method buildShaderGUI {}
	method buildTreeGUI {}

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
    bind $itk_component(combIdE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(combAirE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(combLosE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(combGiftE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(combRgbE) <Return> [::itcl::code $this updateGeometryIfMod]

    bind $itk_component(combIdE) <Tab> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(combAirE) <Tab> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(combLosE) <Tab> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(combGiftE) <Tab> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(combRgbE) <Tab> [::itcl::code $this updateGeometryIfMod]

    #    bind $itk_component(combMaterialE) <Return> [::itcl::code $this updateGeometryIfMod]
    #    bind $itk_component(combInheritE) <Return> [::itcl::code $this updateGeometryIfMod]

    $itk_component(tabs) add $itk_component(combGeneralF) -text "General"
}

::itcl::body CombEditFrame::buildShaderGUI {} {
    itk_component add combShader {
	::ShaderEdit $itk_component(tabs).shader \
	    -shaderChangedCallback [::itcl::code $this updateGeometryIfMod]
    } {}

    #    bind $itk_component(combShaderE) <Return> [::itcl::code $this updateGeometryIfMod]

    $itk_component(tabs) add $itk_component(combShader) -text "Shader"
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

    #    bind $itk_component(combTreeT) <Leave> [::itcl::code $this updateGeometryIfMod]
    #    bind $itk_component(combTreeT) <Return> [::itcl::code $this updateGeometryIfMod]
    #    bind $itk_component(combTreeT) <FocusOut> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(combTreeT) <KeyRelease> [::itcl::code $this updateGeometryIfMod]

    $itk_component(tabs) add $itk_component(combTreeF) -text "Tree"
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
