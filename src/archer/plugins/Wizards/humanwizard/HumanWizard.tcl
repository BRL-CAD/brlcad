#                H U M A N W I Z A R D . T C L
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
# Description:
#	 This is an Archer plugin for building human geometry.
#

::itcl::class HumanWizard {
    inherit Wizard

    constructor {_archer _wizardTop _wizardState _wizardOrigin _originUnits args} {}
    destructor {}

    public {
	# Override's for the Wizard class
	common wizardMajorType $Archer::pluginMajorTypeWizard
	common wizardMinorType $Archer::pluginMinorTypeMged
	common wizardName "Human Wizard"
	common wizardVersion "0.5"
	common wizardClass HumanWizard

	# Methods that override Wizard methods
	method setWizardState {_wizardState}

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
	variable autoMode 1
	variable boundingBoxes 0
	#68 Inches
	variable height 68
	variable locationx 0
	variable locationy 0
	variable locationz 0
	variable manualMode 0
	variable numberSoldiers 1
	variable percentile 50
	variable stance 0
	variable stanceString Standing

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

	method setstance {}

	method setstanceString {}
    }

    private {
    }
}

## - constructor
#
#
#
::itcl::body HumanWizard::constructor {_archer _wizardTop _wizardState _wizardOrigin _originUnits args} {
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

::itcl::body HumanWizard::setWizardState {_wizardState} {
    set wizardState $_wizardState
    initWizardState
}

::itcl::body HumanWizard::initWizardState {} {
    foreach {vname val} $wizardState {
	if {[info exists $vname]} {
	    set $vname $val
	}
    }

    setstanceString
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
	    -textvariable [::itcl::scope wizardTop]
    } {}

    # Create height entry field
    itk_component add paramheightL {
	::ttk::label $itk_component(paramNonArrowF).heightL \
	    -text "height (in):" \
	    -anchor e
    } {}
    itk_component add paramheightE {
	::ttk::entry $itk_component(paramNonArrowF).heightE \
	    -textvariable [::itcl::scope height]
    } {}

    # Create Location X entry field
    itk_component add paramlocationxL {
	::ttk::label $itk_component(paramNonArrowF).locationxL \
	    -text "Centerpoint x (in):" \
	    -anchor e
    } {}
    itk_component add paramlocationxE {
	::ttk::entry $itk_component(paramNonArrowF).locationxE \
	    -textvariable [::itcl::scope locationx]
    } {}

    # Create Location Y entry field
    itk_component add paramlocationyL {
	::ttk::label $itk_component(paramNonArrowF).locationyL \
	    -text "Centerpoint y (in):" \
	    -anchor e
    } {}
    itk_component add paramlocationyE {
	::ttk::entry $itk_component(paramNonArrowF).locationyE \
	    -textvariable [::itcl::scope locationy]
    } {}

    # Create Location Z entry field
    itk_component add paramlocationzL {
	::ttk::label $itk_component(paramNonArrowF).locationzL \
	    -text "Centerpoint z (in):" \
	    -anchor e
    } {}
    itk_component add paramlocationzE {
	::ttk::entry $itk_component(paramNonArrowF).locationzE \
	    -textvariable [::itcl::scope locationz]
    } {}

    # Create numberSoldiers entry field
    itk_component add paramnumberSoldiersL {
	::ttk::label $itk_component(paramNonArrowF).numberSoldiersL \
	    -text "Number of soldiers (to be squard):" \
	    -anchor e
    } {}
    itk_component add paramnumberSoldiersE {
	::ttk::entry $itk_component(paramNonArrowF).numberSoldiersE \
	    -textvariable [::itcl::scope numberSoldiers]
    } {}

    # Create percentile entry field
    itk_component add parampercentileL {
	::ttk::label $itk_component(paramNonArrowF).percentileL \
	    -text "Enter percentile:" \
	    -anchor e
    } {}
    itk_component add parampercentileE {
	::ttk::entry $itk_component(paramNonArrowF).percentileE \
	    -textvariable [::itcl::scope percentile]
    } {}
	
    # Create stance entry field
    itk_component add paramstanceL {
        ::ttk::label $itk_component(paramNonArrowF).stanceL \
            -text "stance:" \
            -anchor e
    } {}

    itk_component add paramstanceCB {
        ::ttk::combobox $itk_component(paramNonArrowF).stanceCB \
            -textvariable [::itcl::scope stanceString] \
            -state readonly \
            -values {Standing Sitting Driving ArmsOut FancySit Custom}
    } {}

    # Create empty label
    itk_component add emptyL {
	::ttk::label $itk_component(paramNonArrowF).emptyL \
	    -text "" \
	    -anchor e
    } {}

    # Create "autoMode" checkbutton
    itk_component add paramautoModeCB {
	::ttk::checkbutton $itk_component(paramNonArrowF).autoModeCB \
	    -text "Automode" \
	    -variable [::itcl::scope autoMode]
    } {}

    # Create "manualMode" checkbutton
    itk_component add parammanualModeCB {
        ::ttk::checkbutton $itk_component(paramNonArrowF).manualModeCB \
            -text "manualMode" \
            -variable [::itcl::scope manualMode]
    } {}

    # Create "boundingBoxes" checkbutton
    itk_component add paramBoundingBoxCB {
        ::ttk::checkbutton $itk_component(paramNonArrowF).boundingBoxCB \
            -text "boundingBoxes" \
            -variable [::itcl::scope boundingBoxes]
    } {}

    set row 0
    grid $itk_component(paramNameL) $itk_component(paramNameE) \
	-row $row -stick nsew
    incr row
    grid $itk_component(paramheightL) $itk_component(paramheightE) \
	-row $row -stick nsew
    incr row
    grid $itk_component(paramlocationxL) $itk_component(paramlocationxE) \
	-row $row -stick nsew
    incr row
    grid $itk_component(paramlocationyL) $itk_component(paramlocationyE) \
	-row $row -stick nsew
    incr row
    grid $itk_component(paramlocationzL) $itk_component(paramlocationzE) \
	-row $row -stick nsew
    incr row
    grid $itk_component(paramnumberSoldiersL) $itk_component(paramnumberSoldiersE) \
	-row $row -stick nsew
    incr row
    grid $itk_component(parampercentileL) $itk_component(parampercentileE) \
	-row $row -stick nsew
    incr row
    grid $itk_component(paramstanceL) $itk_component(paramstanceCB) \
	-row $row -stick nsew
    incr row
    grid $itk_component(emptyL) -columnspan 2 \
	-row $row -stick nsew
    incr row
    grid $itk_component(paramautoModeCB) -columnspan 2 \
	-row $row -stick nsew
    grid columnconfigure $itk_component(paramNonArrowF) 1 -weight 1

    incr row
    grid $itk_component(parammanualModeCB) -columnspan 2 \
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
	    WizardState $wizardState \
	    WizardOrigin $wizardOrigin \
	    WizardUnits $wizardUnits \
	    WizardVersion $wizardVersion
    }
}

::itcl::body HumanWizard::setstance {} {
    switch -- $stanceString {
	Standing {
	    set stance 0
	}
	Sitting {
	    set stance 1
	}
	Driving {
	    set stance 2
	}
	ArmsOut {
	    set stance 3
	}
	FancySit {
	    set stance 4
	}
	Captain {
	    set stance 5
	}
	Custom {
	    set stance 999
	}
    }
}

::itcl::body HumanWizard::setstanceString {} {
    switch -- $stance {
	0 {
	    set stanceString Standing
	}
	1 {
	    set stanceString Sitting
	}
	2 {
	    set stanceString Driving
	}
	3 {
	    set stanceString ArmsOut
	}
	4 {
	    set stanceString FancySit
	}
	5 {
	    set stanceString Captain
	}
	999 {
	    set stanceString Custom
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
    setstance
    $archer pluginUpdateStatusBar "Building human..."
    $archer pluginUpdateSaveMode 1
    $archer pluginUpdateProgressBar 0.2
    after 50
    $archer pluginUpdateProgressBar 0.4
    after 50

    set wizardState \
        [list \
             autoMode $autoMode \
             height $height \
             stance $stance]

#    set wizardState \
#	[list \
#	     autoMode $autoMode \
#	     boundingBoxes $boundingBoxes \
#	     height $height \
#	     locationx $locationx \
#	     locationy $locationy \
#	     locationz $locationz \
#	     manualMode $manualMode \
#	     numberSoldiers $numberSoldiers \
#	     percentile $percentile \
#	     stance $stance]

puts "$archersGed human -A -s$stance $wizardTop"
    $archersGed human -A -s$stance $wizardTop
#	-b \
#	-H $height \
#	-l \
#	-m \
#	-N $numberSoldiers \
#	-p $percentile \
#	-s $stance \ 
#	$wizardTop

    # Add wizard attributes
    addWizardAttrs $wizardTop 0

    drawHuman
    
    $archer pluginUpdateProgressBar 0.6
    after 100
    $archer pluginUpdateStatusBar "Almost Done..."
    $archer pluginUpdateProgressBar 0.8
    after 50
    $archer pluginUpdateStatusBar "Nearly finished..."
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
