##
# Portions Copyright (c) 2002 SURVICE Engineering Company. All Rights Reserved.
# This file contains Original Code and/or Modifications of Original Code as
# defined in and that are subject to the SURVICE Public Source License
# (Version 1.3, dated March 12, 2002).
#
# TYPE: tcltk
##############################################################
#
# CombEditFrame.tcl
#
##############################################################
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
	variable mTree ""

	# Override what's in GeometryEditFrame
	method updateGeometryIfMod {}
    }

    private {}
}


# ------------------------------------------------------------
#                      CONSTRUCTOR
# ------------------------------------------------------------

::itcl::body CombEditFrame::constructor {args} {
    set parent [$this childsite]
    itk_component add combType {
	::label $parent.combtype \
	    -text "Combination:" \
	    -anchor e
    } {
	rename -font -boldLabelFont boldLabelFont Font
    }
    itk_component add combName {
	::label $parent.combname \
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
	::label $parent.combidL \
	    -text "Id:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add combIdE {
	::entry $parent.combidE \
	    -textvariable [::itcl::scope mId] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDigit %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add combAirL {
	::label $parent.combairL \
	    -text "Air:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add combAirE {
	::entry $parent.combairE \
	    -textvariable [::itcl::scope mAir] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDigit %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add combLosL {
	::label $parent.comblosL \
	    -text "Los:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add combLosE {
	::entry $parent.comblosE \
	    -textvariable [::itcl::scope mLos] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDigitMax100 %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add combGiftL {
	::label $parent.combgiftL \
	    -text "GIFTmater:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add combGiftE {
	::entry $parent.combgiftE \
	    -textvariable [::itcl::scope mGift] \
	    -validate key \
	    -validatecommand {GeometryEditFrame::validateDigit %P}
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add combRgbL {
	::label $parent.combrgbL \
	    -text "Rgb:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add combRgbE {
	::entry $parent.combrgbE \
	    -textvariable [::itcl::scope mRgb]
    } {
	rename -font -entryFont entryFont Font
    }
    itk_component add combShaderL {
	::label $parent.combshaderL \
	    -text "Shader:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add combShaderE {
	::entry $parent.combshaderE \
	    -textvariable [::itcl::scope mShader]
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
    itk_component add combTreeL {
	::label $parent.combtreeL \
	    -text "Tree:" \
	    -anchor e
    } {
	rename -font -labelFont labelFont Font
    }
    itk_component add combTreeE {
	::entry $parent.combtreeE \
	    -textvariable [::itcl::scope mTree]
    } {
	rename -font -entryFont entryFont Font
    }

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
    incr row
    grid $itk_component(combShaderL) $itk_component(combShaderE) \
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
#    grid $itk_component(combInheritL) $itk_component(combInheritE) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(combTreeL) $itk_component(combTreeE) \
	-row $row \
	-sticky nsew
    grid columnconfigure $parent 1 -weight 1
    pack $parent -expand yes -fill x -anchor n
#    grid columnconfigure [namespace tail $this] 1 -weight 1


    $itk_component(combRegionCB) configure \
	-selectioncommand [::itcl::code $this updateGeometryIfMod]
    $itk_component(combInheritCB) configure \
	-selectioncommand [::itcl::code $this updateGeometryIfMod]

    # Set up bindings
#    bind $itk_component(combRegionCB) <Enter> {focus %W}
#    bind $itk_component(combRegionCB) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(combIdE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(combAirE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(combLosE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(combGiftE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(combRgbE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(combShaderE) <Return> [::itcl::code $this updateGeometryIfMod]
#    bind $itk_component(combMaterialE) <Return> [::itcl::code $this updateGeometryIfMod]
#    bind $itk_component(combInheritE) <Return> [::itcl::code $this updateGeometryIfMod]
    bind $itk_component(combTreeE) <Return> [::itcl::code $this updateGeometryIfMod]

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
    set mRegion [bu_get_value_by_keyword region $gdata]

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
	set mTree $_tree
    } else {
	set mTree ""
    }

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

    lappend _attrs tree $mTree

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

    if {![catch {bu_get_value_by_keyword tree $gdata} _tree]} {
	set _mTree $_tree
    } else {
	set _mTree ""
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
	$_mTree != $mTree} {
	updateGeometry
    }

    if {$mInherit == ""} {
	set mInherit "no"
    }
}
