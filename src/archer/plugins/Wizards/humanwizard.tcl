#                H U M A N W I Z A R D . T C L
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
#	 This is an Archer plugin for building human geometry.
#

::itcl::class humanWizard {
    inherit Wizard

    constructor {_archer _wizardTop _wizardState _wizardOrigin _originUnits args} {}
    destructor {}

    public {
	# Override's for the Wizard class
	common wizardMajorType $Archer::pluginMajorTypeWizard
	common wizardMinorType $Archer::pluginMinorTypeMged
	common wizardName "human Wizard"
	common wizardVersion "0.1"
	common wizardClass humanWizard

	# Methods that override Wizard methods
	method setWizardState {_wizardState}

	method drawhuman {}
	method buildhuman {}
	method buildhumanXML {}
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

	variable AutoMode 1
	variable BoundingBoxes 0
	variable Height 68
	variable Locationx 0
	variable Locationy 0
	variable Locationz 0
	variable ManualMode 0
	variable NumberSoldiers 1
	variable outputString human.g
	variable percentile 50
	variable Stancenumber 0
	variable StanceString Standing

	method initWizardState {}
	method buildParameter {parent}
	method buildParameterView {parent}

	method addWizardAttrs {obj {onlyTop 1}}

	method setStance {}

	method setStanceString {}
    }

    private {
    }
}

## - constructor
#
#
#
::itcl::body humanWizard::constructor {_archer _wizardTop _wizardState _wizardOrigin _originUnits args} {
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
    set wizardAction buildhuman
    set wizardXmlAction buildhumanXML
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

::itcl::body humanWizard::destructor {} {
    # nothing for now
}

::itcl::body humanWizard::setWizardState {_wizardState} {
    set wizardState $_wizardState
    initWizardState
}

::itcl::body humanWizard::initWizardState {} {
    foreach {vname val} $wizardState {
	if {[info exists $vname]} {
	    set $vname $val
	}
    }

    setStanceString
    setTreadTypeString
}

::itcl::body humanWizard::buildParameter {parent} {
    buildParameterView $parent

    grid rowconfigure $parent 0 -weight 1
    grid columnconfigure $parent 0 -weight 1
}

::itcl::body humanWizard::buildParameterView {parent} {
    itk_component add paramScroll {
	iwidgets::scrolledframe $parent.paramScroll \
	    -hscrollmode dynamic \
	    -vscrollmode dynamic
    } {}
    $itk_component(paramScroll) configure -background $ArcherCore::LABEL_BACKGROUND_COLOR
    set newParent [$itk_component(paramScroll) childsite]

    # Create frame for stuff not in a toggle arrow
    itk_component add paramNonArrowF {
	::ttk::frame $newParent.nonArrowF
    } {}

    # Create name entry field
    itk_component add paramNameL {
	::ttk::label $itk_component(paramNonArrowF).nameL \
	    -text "Name:" \
	    -anchor e
    } {}
    itk_component add paramNameE {
	::ttk::entry $itk_component(paramNonArrowF).nameE \
	    -textvariable [::itcl::scope wizardTop]
    } {}

    # Create rim diameter entry field
    itk_component add paramRimDiameterL {
	::ttk::label $itk_component(paramNonArrowF).rimDiameterL \
	    -text "Rim Diameter (in):" \
	    -anchor e
    } {}
    itk_component add paramRimDiameterE {
	::ttk::entry $itk_component(paramNonArrowF).rimDiameterE \
	    -textvariable [::itcl::scope rimDiameter]
    } {}

    # Create rim width entry field
    itk_component add paramRimWidthL {
	::ttk::label $itk_component(paramNonArrowF).rimWidthL \
	    -text "Rim Width (in):" \
	    -anchor e
    } {}
    itk_component add paramRimWidthE {
	::ttk::entry $itk_component(paramNonArrowF).rimWidthE \
	    -textvariable [::itcl::scope rimWidth]
    } {}

    # Create human aspect entry field
    itk_component add paramhumanAspectL {
	::ttk::label $itk_component(paramNonArrowF).humanAspectL \
	    -text "human Aspect (%):" \
	    -anchor e
    } {}
    itk_component add paramhumanAspectE {
	::ttk::entry $itk_component(paramNonArrowF).humanAspectE \
	    -textvariable [::itcl::scope humanAspect]
    } {}

    # Create human thickness entry field
    itk_component add paramhumanThicknessL {
	::ttk::label $itk_component(paramNonArrowF).humanThicknessL \
	    -text "human Thickness (mm):" \
	    -anchor e
    } {}
    itk_component add paramhumanThicknessE {
	::ttk::entry $itk_component(paramNonArrowF).humanThicknessE \
	    -textvariable [::itcl::scope humanThickness]
    } {}

    # Create human width entry field
    itk_component add paramhumanWidthL {
	::ttk::label $itk_component(paramNonArrowF).humanWidthL \
	    -text "human Width (mm):" \
	    -anchor e
    } {}
    itk_component add paramhumanWidthE {
	::ttk::entry $itk_component(paramNonArrowF).humanWidthE \
	    -textvariable [::itcl::scope humanWidth]
    } {}

    # Create tread depth entry field
    itk_component add paramTreadDepthL {
	::ttk::label $itk_component(paramNonArrowF).treadDepthL \
	    -text "Tread Depth (mm):" \
	    -anchor e
    } {}
    itk_component add paramTreadDepthE {
	::ttk::entry $itk_component(paramNonArrowF).treadDepthE \
	    -textvariable [::itcl::scope treadDepth]
    } {}

    # Create tread count entry field
    itk_component add paramTreadCountL {
	::ttk::label $itk_component(paramNonArrowF).treadCountL \
	    -text "Tread Count:" \
	    -anchor e
    } {}
    itk_component add paramTreadCountE {
	::ttk::entry $itk_component(paramNonArrowF).treadCountE \
	    -textvariable [::itcl::scope treadPatternCount]
    } {}

    # Create tread pattern entry field
    itk_component add paramTreadPatternL {
	::ttk::label $itk_component(paramNonArrowF).treadPatternL \
	    -text "Tread Pattern:" \
	    -anchor e
    } {}
    itk_component add paramStanceCB {
	::ttk::combobox $itk_component(paramNonArrowF).StanceCB \
	    -textvariable [::itcl::scope StanceString] \
	    -state readonly \
	    -values {Standing Sitting Driving ArmsOut FancySit Custom}
    } {}

    # Create tread type radiobox
    itk_component add paramTreadTypeL {
	::ttk::label $itk_component(paramNonArrowF).treadTypeL \
	    -text "Tread Type:" \
	    -anchor e
    } {}
    itk_component add paramTreadTypeCB {
	::ttk::combobox $itk_component(paramNonArrowF).treadTypeCB \
	    -textvariable [::itcl::scope treadTypeString] \
	    -state readonly \
	    -values {Small Large}
    } {}

    # Create empty label
    itk_component add emptyL {
	::ttk::label $itk_component(paramNonArrowF).emptyL \
	    -text "" \
	    -anchor e
    } {}

    # Create "AutoMode" checkbutton
    itk_component add paramAutoModeCB {
	::ttk::checkbutton $itk_component(paramNonArrowF).AutoModeCB \
	    -text "Automode" \
	    -variable [::itcl::scope createWheel]
    } {}

    # Create "ManualMode" checkbutton
    itk_component add paramManualModeCB {
        ::ttk::checkbutton $itk_component(paramNonArrowF).ManualModeCB \
            -text "ManualMode" \
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
    grid $itk_component(paramhumanAspectL) $itk_component(paramhumanAspectE) \
	-row $row -stick nsew
    incr row
    grid $itk_component(paramhumanThicknessL) $itk_component(paramhumanThicknessE) \
	-row $row -stick nsew
    incr row
    grid $itk_component(paramhumanWidthL) $itk_component(paramhumanWidthE) \
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

    set row 0
    grid $itk_component(paramNonArrowF) \
	-row $row -column 0 -sticky nsew
    grid columnconfigure $newParent 0 -weight 1

    grid $itk_component(paramScroll) -row 0 -column 0 -sticky nsew
    grid rowconfigure $parent 0 -weight 1
    grid columnconfigure $parent 0 -weight 1
}

::itcl::body humanWizard::addWizardAttrs {obj {onlyTop 1}} {
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

::itcl::body humanWizard::setStance {} {
    switch -- $StanceString {
	Standing {
	    set Stance 0
	}
	Sitting {
	    set Stance 1
	}
	Driving {
	    set Stance 2
	}
	ArmsOut {
	    set Stance 3
	}
	FancySit {
	    set Stance 4
	}
	Captain {
	    set Stance 5
	}
	Custom {
	    set Stance 999
	}
    }
}

::itcl::body humanWizard::setStanceString {} {
    switch -- $treadPattern {
	0 {
	    set StanceString Standing
	}
	1 {
	    set StanceString Sitting
	}
	2 {
	    set StanceString Driving
	}
	3 {
	    set StanceString ArmsOut
	}
	4 {
	    set StanceString FancySit
	}
	5 {
	    set StanceString Captain
	}
	999 {
	    set StanceString Custom
	}
    }
}

::itcl::body humanWizard::drawhuman {} {
    $archersGed configure -autoViewEnable 0
    $archersGed draw $wizardTop
    $archersGed refresh_all
    $archersGed configure -autoViewEnable 1
}

::itcl::body humanWizard::buildhuman {} {
    SetWaitCursor $archer

    setStance

    $archer pluginUpdateStatusBar "Building human..."
    $archer pluginUpdateSaveMode 1
    $archer pluginUpdateProgressBar 0.2
    after 50
    $archer pluginUpdateProgressBar 0.4
    after 50

    set wizardState \
	[list \
	     AutoMode $AutoMode \
	     BoundingBox $BoundingBox \
	     Height $Height \
	     Locationx $Locationx \
	     Locationy $Locationy \
	     Locationz $Locationz \
	     ManualMode $ManualMode \
	     NumberSoldiers $NumberSoldiers \
	     Percentile $Percentile \
	     Stance $Stance]

    $archersGed human \
	-A $AutoMode \
	-b $BoundingBox \
	-H $Height \
	-l $Locationx $Locationy $Locationz \
	-m $ManualMode \
	-N $NumberSoldiers \
	-p $Percentile \
	-s $Stance \
	$wizardTop

    # Add wizard attributes
    addWizardAttrs $wizardTop 0

    drawhuman
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

::itcl::body humanWizard::buildhumanXML {} {
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
