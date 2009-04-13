#                T I R E W I Z A R D . T C L
# BRL-CAD
#
# Copyright (c) 2002-2009 United States Government as represented by
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
#	 This is an Archer plugin for building tire geometry.
#

::itcl::class TireWizard {
    inherit Wizard

    constructor {_archer _wizardTop _wizardState _wizardOrigin _originUnits args} {}
    destructor {}

    public {
	# Override's for the Wizard class
	common wizardMajorType $Archer::pluginMajorTypeWizard
	common wizardMinorType $Archer::pluginMinorTypeMged
	common wizardName "Tire Wizard"
	common wizardVersion "1.0"
	common wizardClass TireWizard

	# Methods that override Wizard methods
	method setWizardState {_wizardState}

	method drawTire {}
	method buildTire {}
	method buildTireXML {}
    }

    protected {
	variable local2base 1
	variable archer ""
	variable archersGed
	variable statusMsg ""

	variable closedIcon
	variable openIcon
	variable nodeIcon
	variable mkillIcon

	variable maxSideWallRadius 240
	variable rimDiameter 17
	variable rimWidth 8
	variable tireThickness 8
	variable tireWidth 215
	variable tireAspect 70
	variable treadPattern 1
	variable treadPatternString Small
	variable treadPatternCount 30
	variable treadType 1
	variable treadTypeString "Car"
	variable treadDepth 8
	variable createWheel 1

	method initWizardState {}
	method buildParameter {parent}
	method buildParameterView {parent}

	method addWizardAttrs {obj {onlyTop 1}}

	method setTreadPattern {_pattern _string}
	method setTreadType {_type _string}
    }

    private {
    }
}

## - constructor
#
#
#
::itcl::body TireWizard::constructor {_archer _wizardTop _wizardState _wizardOrigin _originUnits args} {
    global env

    itk_component add pane {
	iwidgets::panedwindow $itk_interior.pane \
	    -orient vertical
    } {}

    buildParameter $itk_interior

    grid rowconfigure $itk_interior 0 -weight 1
    grid columnconfigure $itk_interior 0 -weight 1

    set archer $_archer
    set archersGed [Archer::pluginGed $archer]

    # process options
    eval itk_initialize $args

    set wizardTop $_wizardTop
    setWizardState $_wizardState
    set wizardOrigin $_wizardOrigin
    set wizardAction buildTire
    set wizardXmlAction buildTireXML
    set wizardUnits in

    set savedUnits [$archersGed units -s]
    $archersGed units $_originUnits
    set sf1 [$archersGed local2base]
    $archersGed units $wizardUnits
    set sf2 [$archersGed base2local]
    set sf [expr {$sf1 * $sf2}]
    set wizardOrigin [vectorScale $wizardOrigin $sf]
    $archersGed units $savedUnits
}

::itcl::body TireWizard::destructor {} {
    # nothing for now
}

::itcl::body TireWizard::setWizardState {_wizardState} {
    set wizardState $_wizardState
    initWizardState
}

::itcl::body TireWizard::initWizardState {} {
    foreach {vname val} $wizardState {
	if {[info exists $vname]} {
	    set $vname $val
	}
    }
}

::itcl::body TireWizard::buildParameter {parent} {
    buildParameterView $parent

    grid rowconfigure $parent 0 -weight 1
    grid columnconfigure $parent 0 -weight 1
}

::itcl::body TireWizard::buildParameterView {parent} {
    itk_component add paramScroll {
	iwidgets::scrolledframe $parent.paramScroll \
	    -hscrollmode dynamic \
	    -vscrollmode dynamic
    } {}
    set newParent [$itk_component(paramScroll) childsite]

    # Create frame for stuff not in a toggle arrow
    itk_component add paramNonArrowF {
	::frame $newParent.nonArrowF
    } {}

    # Create name entry field
    itk_component add paramNameL {
	::label $itk_component(paramNonArrowF).nameL \
	    -text "Name:" \
	    -anchor e
    } {}
    itk_component add paramNameE {
	::entry $itk_component(paramNonArrowF).nameE \
	    -borderwidth 1 \
	    -textvariable [::itcl::scope wizardTop]
    } {}

    # Create rim diameter entry field
    itk_component add paramRimDiameterL {
	::label $itk_component(paramNonArrowF).rimDiameterL \
	    -text "Rim Diameter (in):" \
	    -anchor e
    } {}
    itk_component add paramRimDiameterE {
	::entry $itk_component(paramNonArrowF).rimDiameterE \
	    -borderwidth 1 \
	    -textvariable [::itcl::scope rimDiameter]
    } {}

    # Create rim width entry field
    itk_component add paramRimWidthL {
	::label $itk_component(paramNonArrowF).rimWidthL \
	    -text "Rim Width (in):" \
	    -anchor e
    } {}
    itk_component add paramRimWidthE {
	::entry $itk_component(paramNonArrowF).rimWidthE \
	    -borderwidth 1 \
	    -textvariable [::itcl::scope rimWidth]
    } {}

    # Create tire aspect entry field
    itk_component add paramTireAspectL {
	::label $itk_component(paramNonArrowF).tireAspectL \
	    -text "Tire Aspect (%):" \
	    -anchor e
    } {}
    itk_component add paramTireAspectE {
	::entry $itk_component(paramNonArrowF).tireAspectE \
	    -borderwidth 1 \
	    -textvariable [::itcl::scope tireAspect]
    } {}

    # Create tire thickness entry field
    itk_component add paramTireThicknessL {
	::label $itk_component(paramNonArrowF).tireThicknessL \
	    -text "Tire Thickness (mm):" \
	    -anchor e
    } {}
    itk_component add paramTireThicknessE {
	::entry $itk_component(paramNonArrowF).tireThicknessE \
	    -borderwidth 1 \
	    -textvariable [::itcl::scope tireThickness]
    } {}

    # Create tire width entry field
    itk_component add paramTireWidthL {
	::label $itk_component(paramNonArrowF).tireWidthL \
	    -text "Tire Width (mm):" \
	    -anchor e
    } {}
    itk_component add paramTireWidthE {
	::entry $itk_component(paramNonArrowF).tireWidthE \
	    -borderwidth 1 \
	    -textvariable [::itcl::scope tireWidth]
    } {}

    # Create tread depth entry field
    itk_component add paramTreadDepthL {
	::label $itk_component(paramNonArrowF).treadDepthL \
	    -text "Tread Depth (mm):" \
	    -anchor e
    } {}
    itk_component add paramTreadDepthE {
	::entry $itk_component(paramNonArrowF).treadDepthE \
	    -borderwidth 1 \
	    -textvariable [::itcl::scope treadDepth]
    } {}

    # Create tread count entry field
    itk_component add paramTreadCountL {
	::label $itk_component(paramNonArrowF).treadCountL \
	    -text "Tread Count:" \
	    -anchor e
    } {}
    itk_component add paramTreadCountE {
	::entry $itk_component(paramNonArrowF).treadCountE \
	    -borderwidth 1 \
	    -textvariable [::itcl::scope treadPatternCount]
    } {}

    # Create tread pattern entry field
    itk_component add paramTreadPatternL {
	::label $itk_component(paramNonArrowF).treadPatternL \
	    -text "Tread Pattern:" \
	    -anchor e
    } {}
    itk_component add paramTreadPatternCB {
	cadwidgets::ComboBox $itk_component(paramNonArrowF).treadPatternCB \
	    -entryvariable [::itcl::scope treadPatternString] \
	    -state disabled
    } {}
    $itk_component(paramTreadPatternCB) add command \
	-command "[::itcl::code $this setTreadPattern] 1 Small" \
	-label Small
    $itk_component(paramTreadPatternCB) add command \
	-command "[::itcl::code $this setTreadPattern] 2 Large" \
	-label Large
    $itk_component(paramTreadPatternCB) entryConfigure -disabledforeground #000000
    grid forget [$itk_component(paramTreadPatternCB) component label]
    [$itk_component(paramTreadPatternCB) component entry] configure -borderwidth 1
    [$itk_component(paramTreadPatternCB) component frame] configure -borderwidth 1
    [$itk_component(paramTreadPatternCB) component menubutton] configure -borderwidth 1

    # Create tread type radiobox
    itk_component add paramTreadTypeL {
	::label $itk_component(paramNonArrowF).treadTypeL \
	    -text "Tread Type:" \
	    -anchor e
    } {}
    itk_component add paramTreadTypeCB {
	cadwidgets::ComboBox $itk_component(paramNonArrowF).treadTypeCB \
	    -entryvariable [::itcl::scope treadTypeString] \
	    -state disabled
    } {}
    $itk_component(paramTreadTypeCB) add command \
	-command "[::itcl::code $this setTreadType] 1 Car" \
	-label Car
    $itk_component(paramTreadTypeCB) add command \
	-command "[::itcl::code $this setTreadType] 2 Truck" \
	-label Truck
    $itk_component(paramTreadTypeCB) entryConfigure -disabledforeground #000000
    grid forget [$itk_component(paramTreadTypeCB) component label]
    [$itk_component(paramTreadTypeCB) component entry] configure -borderwidth 1
    [$itk_component(paramTreadTypeCB) component frame] configure -borderwidth 1
    [$itk_component(paramTreadTypeCB) component menubutton] configure -borderwidth 1

    # Create empty label
    itk_component add emptyL {
	::label $itk_component(paramNonArrowF).emptyL \
	    -text "" \
	    -anchor e
    } {}

    # Create "Create Wheel" checkbutton
    itk_component add paramCreateWheelCB {
	::checkbutton $itk_component(paramNonArrowF).createWheelCB \
	    -text "Create Wheel" \
	    -variable [::itcl::scope createWheel]
    } {}

    set row 0
    grid $itk_component(paramNameL) $itk_component(paramNameE) \
	-row $row -stick nsew
    incr row
    grid $itk_component(paramRimDiameterL) $itk_component(paramRimDiameterE) \
	-row $row -stick nsew
    incr row
    grid $itk_component(paramRimWidthL) $itk_component(paramRimWidthE) \
	-row $row -stick nsew
    incr row
    grid $itk_component(paramTireAspectL) $itk_component(paramTireAspectE) \
	-row $row -stick nsew
    incr row
    grid $itk_component(paramTireThicknessL) $itk_component(paramTireThicknessE) \
	-row $row -stick nsew
    incr row
    grid $itk_component(paramTireWidthL) $itk_component(paramTireWidthE) \
	-row $row -stick nsew
    incr row
    grid $itk_component(paramTreadDepthL) $itk_component(paramTreadDepthE) \
	-row $row -stick nsew
    incr row
    grid $itk_component(paramTreadCountL) $itk_component(paramTreadCountE) \
	-row $row -stick nsew
    incr row
    grid $itk_component(paramTreadPatternL) $itk_component(paramTreadPatternCB) \
	-row $row -stick nsew
    incr row
    grid $itk_component(paramTreadTypeL) $itk_component(paramTreadTypeCB) \
	-row $row -stick nsew
    incr row
    grid $itk_component(emptyL) -columnspan 2 \
	-row $row -stick nsew
    incr row
    grid $itk_component(paramCreateWheelCB) -columnspan 2 \
	-row $row -stick nsew
    grid columnconfigure $itk_component(paramNonArrowF) 1 -weight 1

    itk_component add paramEmpty {
	frame $newParent.paramEmpty
    } {
	usual
    }

    set row 0
    grid $itk_component(paramNonArrowF) \
	-row $row -column 0 -sticky nsew
    incr row
    grid $itk_component(paramEmpty) \
	-row $row -column 0 -sticky nsew
    grid rowconfigure $newParent $row -weight 1
    grid columnconfigure $newParent 0 -weight 1

    grid $itk_component(paramScroll) -row 0 -column 0 -sticky nsew
    grid rowconfigure $parent 0 -weight 1
    grid columnconfigure $parent 0 -weight 1
}

::itcl::body TireWizard::addWizardAttrs {obj {onlyTop 1}} {
    if {$onlyTop} {
	$archersGed attr set $obj \
	    WizardTop $wizardTop
    } else {
	$archersGed attr set $obj \
	    WizardName $wizardName \
	    WizardClass $wizardClass \
	    WizardTop $wizardTop \
	    WizardState $wizardState \
	    WizardOrigin $wizardOrigin \
	    WizardUnits $wizardUnits \
	    WizardVersion $wizardVersion
    }
}

::itcl::body TireWizard::setTreadPattern {_pattern _string} {
    set treadPattern $_pattern
    set treadPatternString $_string
}

::itcl::body TireWizard::setTreadType {_type _string} {
    set treadType $_type
    set treadTypeString $_string
}

::itcl::body TireWizard::drawTire {} {
    $archersGed configure -autoViewEnable 0
    $archersGed draw $wizardTop
    $archersGed refresh_all
    $archersGed configure -autoViewEnable 1
}

::itcl::body TireWizard::buildTire {} {
    SetWaitCursor $archer

    $archer pluginUpdateStatusBar "Building Tire..."
    $archer pluginUpdateSaveMode 1
    $archer pluginUpdateProgressBar 0.2
    after 50
    $archer pluginUpdateProgressBar 0.4
    after 50

    set wizardState \
	[list \
	     maxSideWallRadius $maxSideWallRadius \
	     rimDiameter $rimDiameter \
	     rimWidth $rimWidth \
	     tireAspect $tireAspect \
	     tireThickness $tireThickness \
	     tireWidth $tireWidth \
	     treadDepth $treadDepth \
	     treadPattern $treadPattern \
	     treadPatternCount $treadPatternCount \
	     treadType $treadType \
	     createWheel $createWheel]

    $archersGed tire \
	-s $maxSideWallRadius \
	-D $rimDiameter \
	-j $rimWidth \
	-R $tireAspect \
	-u $tireThickness \
	-W $tireWidth \
	-g $treadDepth \
	-p $treadPattern \
	-c $treadPatternCount \
	-t $treadType \
	-w $createWheel \
	$wizardTop

    # Add wizard attributes
    addWizardAttrs $wizardTop 0

    drawTire
    $archer pluginUpdateProgressBar 0.6
    after 50
    $archer pluginUpdateProgressBar 0.8
    after 50
    $archer pluginUpdateProgressBar 1.0

    SetNormalCursor $archer

    $archer pluginUpdateProgressBar 0.0
    $archer pluginUpdateStatusBar ""
    return $wizardTop
}

::itcl::body TireWizard::buildTireXML {} {
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
