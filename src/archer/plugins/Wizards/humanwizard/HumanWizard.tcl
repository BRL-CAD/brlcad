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

::itcl::class HumanWizard {
    inherit Wizard

    constructor {_archer _wizardTop _WizardState _wizardOrigin _originUnits args} {}
    destructor {}

    public {
	# Override's for the Wizard class
	common wizardMajorType $Archer::pluginMajorTypeWizard
	common wizardMinorType $Archer::pluginMinorTypeMged
	common wizardName "Human Wizard"
	common wizardVersion "0.2"
	common wizardClass HumanWizard

	# Methods that override Wizard methods
	method setWizardState {_WizardState}

	method drawHuman {}
	method buildHuman {}
	method buildHumanXML {}
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

# Variables for command line input
	variable AutoMode 1
	variable BoundingBoxes 0
	#68 Inches
	variable Height 68
	variable Locationx 0
	variable Locationy 0
	variable Locationz 0
	variable ManualMode 0
	variable NumberSoldiers 1
	variable output 0
	variable outputString "Fellow.g"
	variable percentile 50
	variable Stance 0
	variable StanceString Standing

#Variables for manual mode
	variable headSize 0
	variable neckLength 0
	variable neckWidth 0
	variable TopTorso 0
	variable LowTorso 0
	variable Shoulders 0
	variable Abs 0
	variable Pelvis 0
	variable TorsoLength 0
	variable UpperArmLength 0
	variable UpperArmWidth 0
	variable ElbowWidth 0
	variable LowerArmLength 0
	variable WristWidth 0
	variable HandLength 0
	variable HandWidth 0
	variable ArmLength 0
	variable ThighLength 0
	variable ThighWidth 0
	variable KneeWidth 0
	variable CalfLength 0
	variable AnkleWidth 0
	variable FootLength 0
	variable ToeWidth 0
	variable LegLength 0

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
::itcl::body HumanWizard::constructor {_archer _wizardTop _WizardState _wizardOrigin _originUnits args} {
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
    set WizardState $_WizardState
    set wizardOrigin $_wizardOrigin
    set wizardAction buildHuman
    set wizardXmlAction buildHumanXML
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

::itcl::body HumanWizard::destructor {} {
    # nothing for now
}

::itcl::body HumanWizard::setWizardState {_WizardState} {
    set WizardState $_WizardState
    initWizardState
}

::itcl::body HumanWizard::initWizardState {} {
    foreach {vname val} $WizardState {
	if {[info exists $vname]} {
	    set $vname $val
	}
    }

    setStanceString
}

::itcl::body HumanWizard::buildParameter {parent} {
    buildParameterView $parent

    grid rowconfigure $parent 0 -weight 1
    grid columnconfigure $parent 0 -weight 1
}

::itcl::body HumanWizard::buildParameterView {parent} {
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
	    -textvariable [::itcl::scope outputString]
    } {}

    # Create Height entry field
    itk_component add paramHeightL {
	::ttk::label $itk_component(paramNonArrowF).heightL \
	    -text "Height (in):" \
	    -anchor e
    } {}
    itk_component add paramHeightE {
	::ttk::entry $itk_component(paramNonArrowF).heightE \
	    -textvariable [::itcl::scope Height]
    } {}

    # Create Location X entry field
    itk_component add paramLocationxL {
	::ttk::label $itk_component(paramNonArrowF).locationxL \
	    -text "Centerpoint x (in):" \
	    -anchor e
    } {}
    itk_component add paramLocationxE {
	::ttk::entry $itk_component(paramNonArrowF).locationxE \
	    -textvariable [::itcl::scope Locationx]
    } {}

    # Create Location Y entry field
    itk_component add paramLocationyL {
	::ttk::label $itk_component(paramNonArrowF).locationyL \
	    -text "Centerpoint y (in):" \
	    -anchor e
    } {}
    itk_component add paramLocationyE {
	::ttk::entry $itk_component(paramNonArrowF).locationyE \
	    -textvariable [::itcl::scope Locationy]
    } {}

    # Create Location Z entry field
    itk_component add paramLocationzL {
	::ttk::label $itk_component(paramNonArrowF).locationzL \
	    -text "Centerpoint z (in):" \
	    -anchor e
    } {}
    itk_component add paramLocationzE {
	::ttk::entry $itk_component(paramNonArrowF).locationzE \
	    -textvariable [::itcl::scope Locationz]
    } {}

    # Create NumberSoldiers entry field
    itk_component add paramNumberSoldiersL {
	::ttk::label $itk_component(paramNonArrowF).numberSoldiersL \
	    -text "Number of soldiers (to be squard):" \
	    -anchor e
    } {}
    itk_component add paramNumberSoldiersE {
	::ttk::entry $itk_component(paramNonArrowF).numberSoldiersE \
	    -textvariable [::itcl::scope NumberSoldiers]
    } {}

    # Create percentile entry field
    itk_component add paramPercentileL {
	::ttk::label $itk_component(paramNonArrowF).percentileL \
	    -text "Enter percentile:" \
	    -anchor e
    } {}
    itk_component add paramPercentileE {
	::ttk::entry $itk_component(paramNonArrowF).percentileE \
	    -textvariable [::itcl::scope percentile]
    } {}
	
    # Create stance entry field
    itk_component add paramStanceL {
        ::ttk::label $itk_component(paramNonArrowF).stanceL \
            -text "Stance:" \
            -anchor e
    } {}

    itk_component add paramStanceCB {
        ::ttk::combobox $itk_component(paramNonArrowF).stanceCB \
            -textvariable [::itcl::scope StanceString] \
            -state readonly \
            -values {Standing Sitting Driving ArmsOut FancySit Custom}
    } {}

    # Create empty label
    itk_component add emptyL {
	::ttk::label $itk_component(paramNonArrowF).emptyL \
	    -text "" \
	    -anchor e
    } {}

    # Create "AutoMode" checkbutton
    itk_component add paramAutoModeCB {
	::ttk::checkbutton $itk_component(paramNonArrowF).autoModeCB \
	    -text "Automode" \
	    -variable [::itcl::scope AutoMode]
    } {}

    # Create "ManualMode" checkbutton
    itk_component add paramManualModeCB {
        ::ttk::checkbutton $itk_component(paramNonArrowF).manualModeCB \
            -text "ManualMode" \
            -variable [::itcl::scope ManualMode]
    } {}

    # Create "BoundingBoxes" checkbutton
    itk_component add paramBoundingBoxCB {
        ::ttk::checkbutton $itk_component(paramNonArrowF).boundingBoxCB \
            -text "BoundingBoxes" \
            -variable [::itcl::scope BoundingBoxes]
    } {}

    set row 0
    grid $itk_component(paramNameL) $itk_component(paramNameE) \
	-row $row -stick nsew
    incr row
    grid $itk_component(paramHeightL) $itk_component(paramHeightE) \
	-row $row -stick nsew
    incr row
    grid $itk_component(paramLocationxL) $itk_component(paramLocationxE) \
	-row $row -stick nsew
    incr row
    grid $itk_component(paramLocationyL) $itk_component(paramLocationyE) \
	-row $row -stick nsew
    incr row
    grid $itk_component(paramLocationzL) $itk_component(paramLocationzE) \
	-row $row -stick nsew
    incr row
    grid $itk_component(paramNumberSoldiersL) $itk_component(paramNumberSoldiersE) \
	-row $row -stick nsew
    incr row
    grid $itk_component(paramPercentileL) $itk_component(paramPercentileE) \
	-row $row -stick nsew
    incr row
    grid $itk_component(paramStanceL) $itk_component(paramStanceCB) \
	-row $row -stick nsew
    incr row
    grid $itk_component(emptyL) -columnspan 2 \
	-row $row -stick nsew
    incr row
    grid $itk_component(paramAutoModeCB) -columnspan 2 \
	-row $row -stick nsew
    grid columnconfigure $itk_component(paramNonArrowF) 1 -weight 1

    incr row
    grid $itk_component(paramManualModeCB) -columnspan 2 \
        -row $row -stick nsew
    grid columnconfigure $itk_component(paramNonArrowF) 1 -weight 1

    incr row
    grid $itk_component(paramBoundingBoxCB) -columnspan 2 \
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

::itcl::body HumanWizard::addWizardAttrs {obj {onlyTop 1}} {
    if {$onlyTop} {
	$archersGed attr set $obj \
	    WizardTop $wizardTop
    } else {
	$archersGed attr set $obj \
	    WizardName $wizardName \
	    WizardClass $wizardClass \
	    WizardTop $wizardTop \
	    WizardState $WizardState \
	    WizardOrigin $wizardOrigin \
	    WizardUnits $wizardUnits \
	    WizardVersion $wizardVersion
    }
}

::itcl::body HumanWizard::setStance {} {
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

::itcl::body HumanWizard::setStanceString {} {
    switch -- $Stance {
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

::itcl::body HumanWizard::drawHuman {} {
    $archersGed configure -autoViewEnable 0
    $archersGed draw $wizardTop
    $archersGed refresh_all
    $archersGed configure -autoViewEnable 1
}

::itcl::body HumanWizard::buildHuman {} {
    SetWaitCursor $archer

    setStance

    $archer pluginUpdateStatusBar "Building human..."
    $archer pluginUpdateSaveMode 1
    $archer pluginUpdateProgressBar 0.2
    after 50
    $archer pluginUpdateProgressBar 0.4
    after 50

    set WizardState \
	[list \
	     AutoMode $AutoMode \
	     BoundingBoxes $BoundingBoxes \
	     Height $Height \
	     Locationx $Locationx \
	     Locationy $Locationy \
	     Locationz $Locationz \
	     ManualMode $ManualMode \
	     NumberSoldiers $NumberSoldiers \
	     percentile $percentile \
	     Stance $Stance]

    $archersGed human \
	-A $AutoMode \
	-b $BoundingBoxes \
	-H $Height \
	-l $Locationx $Locationy $Locationz \
	-m $ManualMode \
	-N $NumberSoldiers \
	-p $percentile \
	-s $Stance \
	$wizardTop

    # Add wizard attributes
    addWizardAttrs $wizardTop 0

    drawHuman
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

::itcl::body HumanWizard::buildHumanXML {} {
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
