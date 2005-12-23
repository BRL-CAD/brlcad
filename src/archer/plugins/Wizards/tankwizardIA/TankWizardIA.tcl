##
# Portions Copyright (c) 2002 SURVICE Engineering Company. All Rights Reserved.
# This file contains Original Code and/or Modifications of Original Code as
# defined in and that are subject to the SURVICE Public Source License
# (Version 1.3, dated March 12, 2002).
#
#                 T A N K W I Z A R D I . T C L
#
# Author -
#	 Bob Parker
#
# Description:
#	 This is an Archer plugin for building tank geometry (zone based).
#

::itcl::class TankWizardIA {
    inherit Wizard

    constructor {_archer _wizardTop _wizardState _wizardOrigin _originUnits args} {}
    destructor {}

    public {
	# Override's for the Wizard class
	common wizardMajorType $Archer::pluginMajorTypeWizard
	common wizardMinorType $Archer::pluginMinorTypeMged
	common wizardName "Tank Wizard IA"
	common wizardVersion "1.0"
	common wizardClass TankWizardIA

	# Methods that override Wizard methods
	method setWizardState {_wizardState}

	method drawTank {}
	method buildTank {}
	method buildTankXML {}
    }

    protected {
	variable enableCategory 0
	variable useOriginalZones 1
	variable hullArmorId 11001
	variable hullInteriorId 11002
	variable leftSprocketId 12001
	variable leftTrackId 12002
	variable rightSprocketId 13001
	variable rightTrackId 13002
	variable leftRoadWheelInitialId 14000
	variable leftRoadWheelEndId 14000
	variable rightRoadWheelInitialId 15000
	variable rightRoadWheelEndId 15000
	variable leftIdlerWheelInitialId 16000
	variable leftIdlerWheelEndId 16000
	variable rightIdlerWheelInitialId 17000
	variable rightIdlerWheelEndId 17000
	variable turretId 18000
	variable gunBarrelId 19000
	variable zoneIdInitial 20000
	variable zoneId 20000
	variable frontZoneIdOffset 0
	variable centerZoneIdOffset 1
	variable rearZoneIdOffset 2
	variable floorZoneIdOffset 3

	variable tankCenter {0 0 0}
	variable local2base 1
	variable archer ""
	variable archersMged
	variable statusMsg ""
	variable vehicleClasses {"Light Ground Mobile" "Heavy Ground Mobile"}
	variable lightGroundMobileTypes {HUMMER "Truck (5 Ton)"}
	variable heavyGroundMobileTypes {Tank "Armored Personnel Carrier"}
	variable vehicleTypes {}
	variable weaponTypes {7.62mm 12.7mm "Artillery Frags" RPG NONE}

	variable tankHierarchy {
	    {leaf Category}
	    {branch Exterior {
		{leaf Gun}
		{branch Hull {
		    {leaf {Frontal Slope}} {leaf {Wheel Well Cutout}}
	        }}
		{leaf Tracks} {leaf Turret}
	    }}
	    {leaf Armor}
	}

	variable tankArrowNameMap {
	    {Armor armorArrow}
	    {Category categoryArrow}
	    {Exterior exteriorArrow}
	    {{Frontal Slope} frontSlopeArrow}
	    {Gun gunArrow}
	    {Hull tankHullArrow}
	    {Turret turretArrow}
	    {Tracks tracksArrow}
	    {{Wheel Well Cutout} wheelWellArrow}
	}

	variable localWarehouseHierarchy {
	    {branch {Transport } {
		{branch Trailers {
		    {leaf 3ST50}
		    {leaf 4ST60MD}
		    {leaf M15A1}
		    {leaf M15A2}
		    {leaf M100}
		    {leaf M101}
		    {leaf M101A1}
		    {leaf M101A2}
		    {leaf M104}
		    {leaf M104A1}
		    {leaf M104A2}
		    {leaf M105}
		    {leaf M105A1}
		    {leaf M105A2}
		    {leaf M106}
		    {leaf M106A1}
		    {leaf M106A2}
		    {leaf M106E1}
		    {leaf M107}
		    {leaf M107A1}
		    {leaf M107A2}
		    {leaf M118}
		    {leaf M118A1}
		    {leaf M119}
		    {leaf M119A1}
		    {leaf M127}
		    {leaf M127A1}
		    {leaf M127A1C}
		    {leaf M127A2C}
		    {leaf M131}
		    {leaf M149}
		    {leaf M149A1}
		    {leaf M149A2}
		    {leaf M172}
		    {leaf M270}
		    {leaf M270A1}
		    {leaf M332}
		    {leaf M345}
		    {leaf M353}
		    {leaf M390}
		    {leaf M390C}
		    {leaf M416}
		    {leaf M416A1}
		    {leaf M514}
		    {leaf M625}
		    {leaf M747}
		    {leaf M860A1}
		    {leaf M872}
		    {leaf M872A3}
		    {leaf M900}
		    {leaf M967}
		    {leaf M969}
		    {leaf M970}
		    {leaf M989A1}
		    {leaf M1000}
		    {leaf PLST}
		    {leaf XM1006}
	        }}
	    }}
	}

	variable remoteWarehouseHierarchy {
	    {branch {Field Fortifications} {
		{leaf {Caterpillar 30/30}}
		{leaf CEE}
		{leaf EXFODA}
		{leaf HME}
		{leaf HMMH}
		{leaf LCEE}
		{leaf SEE}
	    }}
	    {branch Maintenance {
	        {branch {Recovery Vehicles} {
		    {leaf ARV}
		    {leaf M88}
		    {leaf M88A1}
		    {leaf M88A1E1}
		    {leaf M113A2}
		    {leaf M578}
		    {leaf M984A1}
		    {leaf V-150S}
		    {leaf V-300}
	        }}
		{branch {Repair Vehicles} {
		    {leaf 66WT22}
		    {leaf {M60 Wrecker(2.5 Ton)}}
		    {leaf {M62 Wrecker (5 Ton)}}
		    {leaf {M553 (10 Ton)}}
		    {leaf {M108 Wrecker(2.5 Ton)}}
		    {leaf {M816 (5 Ton)}}
		    {leaf {M819 (5 Ton)}}
	        }}
	    }}
	    {branch Tank {
	        {branch {Main Battle} {
		    {leaf M60A2}
		    {leaf {XM-1 Abrams}}
	        }}
		{branch Light {
		    {leaf {M551 Sheridan}}
	        }}
	    }}
	    {branch Transport {
		{branch Amphibious {
		    {leaf {LARC}}
	        }}
		{branch {Light (up to 1000 Kg)} {
		    {leaf AMT600}
		    {leaf DJ-5}
		    {leaf {FAV (Scorpion)}}
		    {leaf Jeep}
		    {leaf {Jeep XJ}}
		    {leaf M37}
		    {leaf M38}
		    {leaf M151}
		    {leaf {M274 (Mechanical Mule)}}
		    {leaf {NMC-40 (Warrior)}}
		    {leaf SS200}
		    {leaf {Teledyne LFV (4x4)}}
		    {leaf {TPC Ramp-v}}
	        }}
		{branch "Materials Handling Equipment" {
		    {leaf 72-31F}
		    {leaf 72-31M}
		    {leaf {104T Mobile Container Handling Crane}}
		    {leaf {250T Mobile Container Handling Crane}}
		    {leaf AP308}
		    {leaf AT400}
		    {leaf {Caterpillar 988}}
		    {leaf {General Motors PLS}}
		    {leaf H-446A}
		    {leaf HL150T}
		    {leaf LACH}
		    {leaf M4K-B}
		    {leaf M6K-B}
		    {leaf M13K}
		    {leaf M878}
		    {leaf M878A1}
		    {leaf MC2500}
		    {leaf {Oshkosh PLS (10x10)}}
		    {leaf {PACCAR PLS}}
		    {leaf RT48MC}
		    {leaf RT875CC}
		    {leaf RTCST}
		    {leaf TMS300-5}
		    {leaf TMS760}
		    {leaf USDCH}
		    {leaf VRRTFLT}
	        }}
		{branch {Over-Snow} {
		    {leaf LMC1200}
		    {leaf LMC1500}
		    {leaf LMC3700C}
	        }}
		{branch {Tank Transporter} {
		    {leaf 66TT50}
		    {leaf 66TT60}
		    {leaf F5070}
		    {leaf M123}
		    {leaf M746}
		    {leaf M911}
		    {leaf RD8226SX}
	        }}
		{branch {Tracked Prime Movers} {
		    {leaf M548}
		    {leaf M973}
		    {leaf M992}
	        }}
		{branch Trucks {
		    {leaf AM715}
		    {leaf AM720}
		    {leaf ATTV}
		    {leaf CUCV}
		    {leaf M35}
		    {leaf M54}
		    {leaf M125}
		    {leaf M211}
		    {leaf M520}
		    {leaf M561}
		    {leaf M715}
		    {leaf M809}
		    {leaf M880}
		    {leaf M915}
		    {leaf M916}
		    {leaf M917}
		    {leaf M920}
		    {leaf M939}
		    {leaf {M998 (HMMWV)}}
		    {leaf MK48}
		    {leaf RM6866SX}
		    {leaf SS300}
	        }}
	    }}
	    {branch {Weapon Systems} {
		{branch {Mobile Surface-to-Air Missile System} {
		    {leaf {MIM-23 Hawk}}
		    {leaf {MIM-72 Chaparral (M48)}}
	        }}
		{branch {Anit-Aircraft Gun System} {
		    {leaf M163}
		    {leaf M167}
	        }}
		{branch Howitzer {
		    {leaf M102}
		    {leaf M109}
		    {leaf M110}
		    {leaf M198}
		    {leaf XM204}
	        }}
	    }}
	}

	variable closedIcon
	variable openIcon
	variable nodeIcon
	variable mkillIcon

	#XXX we need to add floor armor
	# units are in mm
	variable frontArmorType "7.62mm"
	variable rearArmorType "7.62mm"
	variable roofArmorType "7.62mm"
	variable sideArmorType "7.62mm"
	variable sponsonArmorType "7.62mm"
	variable armorThickness 2
	variable frontArmorThickness 2
	variable rearArmorThickness 2
	variable roofArmorThickness 2
	variable sideArmorThickness 2
	variable sponsonArmorThickness 2
	method setFrontArmorThickness {}
	method setRearArmorThickness {}
	method setRoofArmorThickness {}
	method setSideArmorThickness {}
	method setSponsonArmorThickness {}

	variable hullLength 340
	variable hullWidth 120
	variable hullHeight 72

	variable convHeight 24
	variable upperOffset 72
	variable lowerOffset 24

	variable wwDepth 24
	variable wwHeight 24

	variable numRoadWheels 4
	variable numIdlerWheels 4

	variable sprocketDiameter 24
	variable roadWheelDiameter 42
	variable idlerWheelDiameter 18

	variable turretLength 120
	variable turretWidth 90
	variable turretHeight 24
	variable turretOffset -36
	variable turretFrontOffset 24
	variable turretRearOffset 24
	variable turretSideOffset 12

	variable gunLength 200
	variable gunBaseInnerDiameter 5
	variable gunEndInnerDiameter 5
	variable gunBaseOuterDiameter 12
	variable gunEndOuterDiameter 11
	variable gunElevation 1

	variable extHalfLength
	variable extHalfWidth
	variable extHalfHeight
	variable wwHalfDepth
	variable wwHalfHeight
	variable turretHalfLength
	variable turretHalfWidth

	variable zoneLengthDelta
	variable zoneWidthDelta
	variable zoneUpperHeightDelta

	variable firstRoadWheelX
	variable lastRoadWheelX
	variable roadWheelZ
	variable roadWheelRadius
	variable driveSprocketX
	variable driveSprocketZ
	variable sprocketRadius
	variable idlerWheelX
	variable idlerWheelZ
	variable idlerWheelRadius
	variable leftTrackYMin
	variable leftTrackYMax
	variable rightTrackYMin
	variable rightTrackYMax
	variable trackWidth 24
	variable trackThickness 2
	variable trackClearance 2

	variable tankColor "194 201 80"
	variable zoneColor "255 0 0"

	method initWizardState {}
	method openArrow {arrowName}
	method buildParameter {parent}
	method buildParameterView {parent}
	method buildCategoryView {parent}
	method buildExteriorView {parent}
	method buildGunView {parent}
	method buildHullView {parent}
	method buildWheelWellView {parent}
	method buildFrontSlopeView {parent}
	method buildTrackView {parent}
	method buildTurretView {parent}
	method buildArmorView {parent}

	method toggle {parent child args}
	method changeClass {}
	method changeType {}
	method buildArrow {parent prefix text buildViewFunc}

	method addWizardAttrs {obj {onlyTop 1}}
	method buildHull {}
	method buildHullExterior {}
	method buildHullInterior {}
	method buildHullZones {}
	method buildLowerHullZones {}
	method buildUpperHullZones {}
	method buildHullZoneFromOriginal {name xdir id}
	method buildFloorHullZoneFromOriginal {id}
	method buildWheels {}
	method buildDriveSprockets {}
	method buildRoadWheels {}
	method buildIdlerWheels {}
	method buildTracks {}
	method buildTurret {}
	method buildGun {}
	method initRegionIds {}
    }

    private {
    }
}

## - constructor
#
#
#
::itcl::body TankWizardIA::constructor {_archer _wizardTop _wizardState _wizardOrigin _originUnits args} {
    global env

    lappend vehicleTypes $lightGroundMobileTypes $heavyGroundMobileTypes

    itk_component add pane {
	iwidgets::panedwindow $itk_interior.pane \
	    -orient vertical
    } {}

    buildParameter $itk_interior

    grid rowconfigure $itk_interior 0 -weight 1
    grid columnconfigure $itk_interior 0 -weight 1

    set archer $_archer
    set archersMged [Archer::pluginMged $archer]

    # process options
    eval itk_initialize $args

    #XXX wizardTop is temporarily hardwired
    #set wizardTop "simpleTank"
    set wizardTop $_wizardTop
    setWizardState $_wizardState
    set wizardOrigin $_wizardOrigin
    set wizardAction buildTank
    set wizardXmlAction buildTankXML
    set wizardUnits in

    set savedUnits [$archersMged units]
    $archersMged units $_originUnits
    set sf1 [$archersMged local2base]
    $archersMged units $wizardUnits
    set sf2 [$archersMged base2local]
    set sf [expr {$sf1 * $sf2}]
    set wizardOrigin [vectorScale $wizardOrigin $sf]
    $archersMged units $savedUnits
}

::itcl::body TankWizardIA::destructor {} {
    # nothing for now
}

::itcl::body TankWizardIA::setWizardState {_wizardState} {
    set wizardState $_wizardState
    initWizardState
}

::itcl::body TankWizardIA::initWizardState {} {
    foreach {vname val} $wizardState {
	if {[info exists $vname]} {
	    set $vname $val
	}
    }

    setFrontArmorThickness
    setRearArmorThickness
    setRoofArmorThickness
    setSideArmorThickness
    setSponsonArmorThickness
}

::itcl::body TankWizardIA::openArrow {arrowName} {
    $itk_component($arrowName) configure -togglestate open

    switch -- $arrowName {
	frontSlopeArrow -
	wheelWellArrow {
	    $itk_component(tankHullArrow) configure -togglestate open
	    $itk_component(exteriorArrow) configure -togglestate open
	}
	exteriorArrow {
	    $itk_component(frontSlopeArrow) configure -togglestate open
	    $itk_component(wheelWellArrow) configure -togglestate open
	    $itk_component(tankHullArrow) configure -togglestate open
	    $itk_component(turretArrow) configure -togglestate open
	    $itk_component(tracksArrow) configure -togglestate open
	}
	tankHullArrow {
	    $itk_component(frontSlopeArrow) configure -togglestate open
	    $itk_component(wheelWellArrow) configure -togglestate open
	    $itk_component(exteriorArrow) configure -togglestate open
	}
	gunArrow -
	tracksArrow -
	turretArrow {
	    $itk_component(exteriorArrow) configure -togglestate open
	}
    }
}

::itcl::body TankWizardIA::buildParameter {parent} {
    buildParameterView $parent

    grid rowconfigure $parent 0 -weight 1
    grid columnconfigure $parent 0 -weight 1
}

::itcl::body TankWizardIA::buildParameterView {parent} {
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

    #XXX wizardTop is temporarily hardwired
    #-state disabled
    itk_component add paramNameE {
	::entry $itk_component(paramNonArrowF).nameE \
	    -textvariable [::itcl::scope wizardTop] \
	    -disabledforeground black
    } {}

    # Create origin entry field
    itk_component add paramOriginL {
	::label $itk_component(paramNonArrowF).originL \
	    -text "Origin:" \
	    -anchor e
    } {}

    itk_component add paramOriginE {
	::entry $itk_component(paramNonArrowF).originE \
	    -textvariable [::itcl::scope wizardOrigin] 
    } {}

    set row 0
    grid $itk_component(paramNameL) $itk_component(paramNameE) \
	-row $row -stick nsew
    incr row
    grid $itk_component(paramOriginL) $itk_component(paramOriginE) \
	-row $row -stick nsew
    grid columnconfigure $itk_component(paramNonArrowF) 1 -weight 1

    buildArrow $newParent category Category buildCategoryView
    buildArrow $newParent exterior Exterior buildExteriorView
    buildArrow $newParent armor Armor buildArmorView

    itk_component add paramEmpty {
	frame $newParent.paramEmpty
    } {
	usual
    }

    set row 0
    grid $itk_component(paramNonArrowF) \
	-row $row -column 0 -sticky nsew
    incr row
    grid $itk_component(category) \
	-row $row -column 0 -sticky nsew
    incr row
    grid $itk_component(exterior) \
	-row $row -column 0 -sticky nsew
    incr row
    grid $itk_component(armor) \
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

::itcl::body TankWizardIA::buildCategoryView {parent} {
    itk_component add classL {
	::label $parent.classL -text "Class:" \
	    -anchor e
    } {}
    itk_component add classCB {
	iwidgets::combobox $parent.classCB
    } {}
    eval $itk_component(classCB) insert list end \
	$vehicleClasses

    itk_component add typeL {
	::label $parent.typeL -text "Type:" \
	    -anchor e
    } {}
    itk_component add typeCB {
	iwidgets::combobox $parent.typeCB
    } {}
    eval $itk_component(typeCB) insert list end \
	$heavyGroundMobileTypes

    # make initial selections
    $itk_component(classCB) selection set 1
    $itk_component(typeCB) selection set 0

    # set up callbacks
    $itk_component(classCB) configure -selectioncommand [::itcl::code $this changeClass]
    $itk_component(typeCB) configure -selectioncommand [::itcl::code $this changeType]

    # turn off editing
    $itk_component(classCB) configure -editable false
    $itk_component(typeCB) configure -editable false

    if {!$enableCategory} {
	$itk_component(classCB) component arrowBtn configure -state disable
	$itk_component(typeCB) component arrowBtn configure -state disable
    }

    # pack widgets
    set row 0
    grid $itk_component(classL) -row $row -column 0 -sticky e
    grid $itk_component(classCB) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(typeL) -row $row -column 0 -sticky e
    grid $itk_component(typeCB) -row $row -column 1 -sticky w

    grid columnconfigure $parent 1 -weight 1
}

::itcl::body TankWizardIA::buildExteriorView {parent} {
    # cannot use "hull"; it's already claimed by Itk
    buildArrow $parent gun "Gun" buildGunView
    buildArrow $parent tankHull "Hull" buildHullView
    buildArrow $parent turret "Turret" buildTurretView
    buildArrow $parent tracks "Tracks" buildTrackView

    # pack widgets
    set row 0
    grid $itk_component(gun) -row $row -column 1 -sticky nsew
    incr row
    grid $itk_component(tankHull) -row $row -column 1 -sticky nsew
    incr row
    grid $itk_component(tracks) -row $row -column 1 -sticky nsew
    incr row
    grid $itk_component(turret) -row $row -column 1 -sticky nsew

    grid columnconfigure $parent 1 -weight 1
}

::itcl::body TankWizardIA::buildGunView {parent} {
    itk_component add gunLengthL {
	::label $parent.gunLengthL -text "Length:" \
	    -anchor e
    } {}
    itk_component add gunLengthE {
	::entry $parent.gunLengthE -textvariable [::itcl::scope gunLength]
    } {}

    itk_component add gunBaseInnerDiameterL {
	::label $parent.gunBaseInnerDiameterL -text "Inner Diameter (Base):" \
	    -anchor e
    } {}
    itk_component add gunBaseInnerDiameterE {
	::entry $parent.gunBaseInnerDiameterE -textvariable [::itcl::scope gunBaseInnerDiameter]
    } {}

    itk_component add gunEndInnerDiameterL {
	::label $parent.gunEndInnerDiameterL -text "Inner Diameter (End):" \
	    -anchor e
    } {}
    itk_component add gunEndInnerDiameterE {
	::entry $parent.gunEndInnerDiameterE -textvariable [::itcl::scope gunEndInnerDiameter]
    } {}

    itk_component add gunBaseOuterDiameterL {
	::label $parent.gunBaseOuterDiameterL -text "Outer Diameter (Base):" \
	    -anchor e
    } {}
    itk_component add gunBaseOuterDiameterE {
	::entry $parent.gunBaseOuterDiameterE -textvariable [::itcl::scope gunBaseOuterDiameter]
    } {}

    itk_component add gunEndOuterDiameterL {
	::label $parent.gunEndOuterDiameterL -text "Outer Diameter (End):" \
	    -anchor e
    } {}
    itk_component add gunEndOuterDiameterE {
	::entry $parent.gunEndOuterDiameterE -textvariable [::itcl::scope gunEndOuterDiameter]
    } {}

    itk_component add gunElevationL {
	::label $parent.gunElevationL -text "Elevation (DEG):" \
	    -anchor e
    } {}
    itk_component add gunElevationE {
	::entry $parent.gunElevationE -textvariable [::itcl::scope gunElevation]
    } {}

    # pack widgets
    set row 0
    grid $itk_component(gunLengthL) -row $row -column 0 -sticky e
    grid $itk_component(gunLengthE) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(gunBaseInnerDiameterL) -row $row -column 0 -sticky e
    grid $itk_component(gunBaseInnerDiameterE) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(gunEndInnerDiameterL) -row $row -column 0 -sticky e
    grid $itk_component(gunEndInnerDiameterE) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(gunBaseOuterDiameterL) -row $row -column 0 -sticky e
    grid $itk_component(gunBaseOuterDiameterE) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(gunEndOuterDiameterL) -row $row -column 0 -sticky e
    grid $itk_component(gunEndOuterDiameterE) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(gunElevationL) -row $row -column 0 -sticky e
    grid $itk_component(gunElevationE) -row $row -column 1 -sticky w

    grid columnconfigure $parent 1 -weight 1
}

::itcl::body TankWizardIA::buildHullView {parent} {
    itk_component add hullLengthL {
	::label $parent.lengthL -text "Length:" \
	    -anchor e
    } {}
    itk_component add hullLengthE {
	::entry $parent.lengthE -textvariable [::itcl::scope hullLength]
    } {}

    itk_component add hullWidthL {
	::label $parent.widthL -text "Width:" \
	    -anchor e
    } {}
    itk_component add hullWidthE {
	::entry $parent.widthE -textvariable [::itcl::scope hullWidth]
    } {}

    itk_component add hullHeightL {
	::label $parent.heightL -text "Height:" \
	    -anchor e
    } {}
    itk_component add hullHeightE {
	::entry $parent.heightE -textvariable [::itcl::scope hullHeight]
    } {}

    buildArrow $parent wheelWell "Wheel Well Cutout" buildWheelWellView
    buildArrow $parent frontSlope "Frontal Slope" buildFrontSlopeView

    # pack widgets
    set row 0
    grid $itk_component(hullLengthL) -row $row -column 0 -sticky e
    grid $itk_component(hullLengthE) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(hullWidthL) -row $row -column 0 -sticky e
    grid $itk_component(hullWidthE) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(hullHeightL) -row $row -column 0 -sticky e
    grid $itk_component(hullHeightE) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(frontSlope) -row $row -column 1 -sticky nsew
    incr row
    grid $itk_component(wheelWell) -row $row -column 1 -sticky nsew

    grid columnconfigure $parent 1 -weight 1
}

::itcl::body TankWizardIA::buildWheelWellView {parent} {
    itk_component add wwDepthL {
	::label $parent.depthL -text "Depth:" \
	    -anchor e
    } {}
    itk_component add wwDepthE {
	::entry $parent.depthE -textvariable [::itcl::scope wwDepth]
    } {}

    itk_component add wwHeightL {
	::label $parent.heightL -text "Height:" \
	    -anchor e
    } {}
    itk_component add wwHeightE {
	::entry $parent.heightE -textvariable [::itcl::scope wwHeight]
    } {}

    # pack widgets
    set row 0
    grid $itk_component(wwDepthL) -row $row -column 0 -sticky e
    grid $itk_component(wwDepthE) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(wwHeightL) -row $row -column 0 -sticky e
    grid $itk_component(wwHeightE) -row $row -column 1 -sticky w

    grid columnconfigure $parent 1 -weight 1
}

::itcl::body TankWizardIA::buildFrontSlopeView {parent} {
    itk_component add convHeightL {
	::label $parent.convHeightL -text "Convergance Height:" \
	    -anchor e
    } {}
    itk_component add convHeightE {
	::entry $parent.convHeightE -textvariable [::itcl::scope convHeight]
    } {}

    itk_component add upperOffsetL {
	::label $parent.upperOffsetL -text "Upper Offset:" \
	    -anchor e
    } {}
    itk_component add upperOffsetE {
	::entry $parent.upperOffsetE -textvariable [::itcl::scope upperOffset]
    } {}

    itk_component add lowerOffsetL {
	::label $parent.lowerOffsetL -text "Lower Offset:" \
	    -anchor e
    } {}
    itk_component add lowerOffsetE {
	::entry $parent.lowerOffsetE -textvariable [::itcl::scope lowerOffset]
    } {}

    # pack widgets
    set row 0
    grid $itk_component(convHeightL) -row $row -column 0 -sticky e
    grid $itk_component(convHeightE) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(upperOffsetL) -row $row -column 0 -sticky e
    grid $itk_component(upperOffsetE) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(lowerOffsetL) -row $row -column 0 -sticky e
    grid $itk_component(lowerOffsetE) -row $row -column 1 -sticky w

    grid columnconfigure $parent 1 -weight 1
}

::itcl::body TankWizardIA::buildTrackView {parent} {
    itk_component add trackWidthL {
	::label $parent.trackwidthL -text "Width:" \
	    -anchor e
    } {}
    itk_component add trackWidthE {
	::entry $parent.trackWidthE -textvariable [::itcl::scope trackWidth]
    } {}

    itk_component add trackThicknessL {
	::label $parent.trackthicknessL -text "Thickness:" \
	    -anchor e
    } {}
    itk_component add trackThicknessE {
	::entry $parent.trackThicknessE -textvariable [::itcl::scope trackThickness]
    } {}

    itk_component add trackClearanceL {
	::label $parent.trackclearanceL -text "Clearance:" \
	    -anchor e
    } {}
    itk_component add trackClearanceE {
	::entry $parent.trackClearanceE -textvariable [::itcl::scope trackClearance]
    } {}

    itk_component add numRoadWheelsL {
	::label $parent.numRoadWheelsL -text "Number of Road Wheels:" \
	    -anchor e
    } {}
    itk_component add numRoadWheelsE {
	::entry $parent.numRoadWheelsE -textvariable [::itcl::scope numRoadWheels]
    } {}

    itk_component add roadWheelDiameterL {
	::label $parent.roadWheelDiameterL -text "Road Wheel Diameter:" \
	    -anchor e
    } {}
    itk_component add roadWheelDiameterE {
	::entry $parent.roadWheelDiameterE -textvariable [::itcl::scope roadWheelDiameter]
    } {}

    itk_component add numIdlerWheelsL {
	::label $parent.numIdlerWheelsL -text "Number of Idler Wheels:" \
	    -anchor e
    } {}
    itk_component add numIdlerWheelsE {
	::entry $parent.numIdlerWheelsE -textvariable [::itcl::scope numIdlerWheels]
    } {}

    itk_component add idlerWheelDiameterL {
	::label $parent.idlerWheelDiameterL -text "Idler Wheel Diameter:" \
	    -anchor e
    } {}
    itk_component add idlerWheelDiameterE {
	::entry $parent.idlerWheelDiameterE -textvariable [::itcl::scope idlerWheelDiameter]
    } {}

    itk_component add sprocketDiameterL {
	::label $parent.sprocketDiameterL -text "Drive Sprocket Diameter:" \
	    -anchor e
    } {}
    itk_component add sprocketDiameterE {
	::entry $parent.sprocketDiameterE -textvariable [::itcl::scope sprocketDiameter]
    } {}

    # pack widgets
    set row 0
    grid $itk_component(trackWidthL) -row $row -column 0 -sticky e
    grid $itk_component(trackWidthE) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(trackThicknessL) -row $row -column 0 -sticky e
    grid $itk_component(trackThicknessE) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(trackClearanceL) -row $row -column 0 -sticky e
    grid $itk_component(trackClearanceE) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(numRoadWheelsL) -row $row -column 0 -sticky e
    grid $itk_component(numRoadWheelsE) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(roadWheelDiameterL) -row $row -column 0 -sticky e
    grid $itk_component(roadWheelDiameterE) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(numIdlerWheelsL) -row $row -column 0 -sticky e
    grid $itk_component(numIdlerWheelsE) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(idlerWheelDiameterL) -row $row -column 0 -sticky e
    grid $itk_component(idlerWheelDiameterE) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(sprocketDiameterL) -row $row -column 0 -sticky e
    grid $itk_component(sprocketDiameterE) -row $row -column 1 -sticky w

    grid columnconfigure $parent 1 -weight 1
}

::itcl::body TankWizardIA::buildTurretView {parent} {
    itk_component add turretLengthL {
	::label $parent.turretLengthL -text "Length:" \
	    -anchor e
    } {}
    itk_component add turretLengthE {
	::entry $parent.turretLengthE -textvariable [::itcl::scope turretLength]
    } {}

    itk_component add turretWidthL {
	::label $parent.turretwidthL -text "Width:" \
	    -anchor e
    } {}
    itk_component add turretWidthE {
	::entry $parent.turretWidthE -textvariable [::itcl::scope turretWidth]
    } {}

    itk_component add turretHeightL {
	::label $parent.turretheightL -text "Height:" \
	    -anchor e
    } {}
    itk_component add turretHeightE {
	::entry $parent.turretHeightE -textvariable [::itcl::scope turretHeight]
    } {}

    itk_component add turretOffsetL {
	::label $parent.turretOffsetL -text "Offset:" \
	    -anchor e
    } {}
    itk_component add turretOffsetE {
	::entry $parent.turretOffsetE -textvariable [::itcl::scope turretOffset]
    } {}

    itk_component add turretFrontOffsetL {
	::label $parent.turretFrontOffsetL -text "Front Offset:" \
	    -anchor e
    } {}
    itk_component add turretFrontOffsetE {
	::entry $parent.turretFrontOffsetE -textvariable [::itcl::scope turretFrontOffset]
    } {}

    itk_component add turretRearOffsetL {
	::label $parent.turretRearOffsetL -text "Rear Offset:" \
	    -anchor e
    } {}
    itk_component add turretRearOffsetE {
	::entry $parent.turretRearOffsetE -textvariable [::itcl::scope turretRearOffset]
    } {}

    itk_component add turretSideOffsetL {
	::label $parent.turretSideOffsetL -text "SideOffset:" \
	    -anchor e
    } {}
    itk_component add turretSideOffsetE {
	::entry $parent.turretSideOffsetE -textvariable [::itcl::scope turretSideOffset]
    } {}

    # pack widgets
    set row 0
    grid $itk_component(turretLengthL) -row $row -column 0 -sticky e
    grid $itk_component(turretLengthE) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(turretWidthL) -row $row -column 0 -sticky e
    grid $itk_component(turretWidthE) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(turretHeightL) -row $row -column 0 -sticky e
    grid $itk_component(turretHeightE) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(turretOffsetL) -row $row -column 0 -sticky e
    grid $itk_component(turretOffsetE) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(turretFrontOffsetL) -row $row -column 0 -sticky e
    grid $itk_component(turretFrontOffsetE) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(turretRearOffsetL) -row $row -column 0 -sticky e
    grid $itk_component(turretRearOffsetE) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(turretSideOffsetL) -row $row -column 0 -sticky e
    grid $itk_component(turretSideOffsetE) -row $row -column 1 -sticky w

    grid columnconfigure $parent 1 -weight 1
}

::itcl::body TankWizardIA::buildArmorView {parent} {
    itk_component add frontArmorL {
	::label $parent.frontArmorL -text "Front:" \
	    -anchor e
    } {}
    itk_component add frontArmorCB {
	iwidgets::combobox $parent.frontArmorCB \
		-textvariable [::itcl::scope frontArmorType] \
		-selectioncommand [::itcl::code $this setFrontArmorThickness]
    } {
	usual
    }
    eval $itk_component(frontArmorCB) insert list end \
	$weaponTypes

    itk_component add sideArmorL {
	::label $parent.sideArmorL -text "Side:" \
	    -anchor e
    } {}
    itk_component add sideArmorCB {
	iwidgets::combobox $parent.sideArmorCB \
		-textvariable [::itcl::scope sideArmorType] \
		-selectioncommand [::itcl::code $this setSideArmorThickness]
    } {
	usual
    }
    eval $itk_component(sideArmorCB) insert list end \
	$weaponTypes

    itk_component add sponsonArmorL {
	::label $parent.sponsonArmorL -text "Sponson:" \
	    -anchor e
    } {}
    itk_component add sponsonArmorCB {
	iwidgets::combobox $parent.sponsonsArmorCB \
		-textvariable [::itcl::scope sponsonArmorType] \
		-selectioncommand [::itcl::code $this setSponsonArmorThickness]
    } {
	usual
    }
    eval $itk_component(sponsonArmorCB) insert list end \
	$weaponTypes

    itk_component add rearArmorL {
	::label $parent.rearArmorL -text "Rear:" \
	    -anchor e
    } {}
    itk_component add rearArmorCB {
	iwidgets::combobox $parent.rearArmorCB \
		-textvariable [::itcl::scope rearArmorType] \
		-selectioncommand [::itcl::code $this setRearArmorThickness]
    } {
	usual
    }
    eval $itk_component(rearArmorCB) insert list end \
	$weaponTypes

    itk_component add roofArmorL {
	::label $parent.roofArmorL -text "Roof:" \
	    -anchor e
    } {}
    itk_component add roofArmorCB {
	iwidgets::combobox $parent.roofArmorCB \
		-textvariable [::itcl::scope roofArmorType] \
		-selectioncommand [::itcl::code $this setRoofArmorThickness]
    } {
	usual
    }
    eval $itk_component(roofArmorCB) insert list end \
	$weaponTypes

    # make initial selections
    $itk_component(frontArmorCB) selection set 0
    $itk_component(rearArmorCB) selection set 0
    $itk_component(roofArmorCB) selection set 0
    $itk_component(sideArmorCB) selection set 0
    $itk_component(sponsonArmorCB) selection set 0

    # turn off editing
    $itk_component(frontArmorCB) configure -editable false
    $itk_component(rearArmorCB) configure -editable false
    $itk_component(roofArmorCB) configure -editable false
    $itk_component(sideArmorCB) configure -editable false
    $itk_component(sponsonArmorCB) configure -editable false

    # pack widgets
    set row 0
    grid $itk_component(frontArmorL) -row $row -column 0 -sticky e
    grid $itk_component(frontArmorCB) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(rearArmorL) -row $row -column 0 -sticky e
    grid $itk_component(rearArmorCB) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(roofArmorL) -row $row -column 0 -sticky e
    grid $itk_component(roofArmorCB) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(sideArmorL) -row $row -column 0 -sticky e
    grid $itk_component(sideArmorCB) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(sponsonArmorL) -row $row -column 0 -sticky e
    grid $itk_component(sponsonArmorCB) -row $row -column 1 -sticky w

    grid columnconfigure $parent 1 -weight 1
}

::itcl::body TankWizardIA::toggle {arrow view args} {
    set toggleState [$arrow cget -togglestate]
    switch -- $toggleState {
	closed {
	    grid forget $view
	}
	open {
	    eval grid $view $args
	}
    }
}

::itcl::body TankWizardIA::changeClass {} {
    set curr [$itk_component(classCB) curselection]

    $itk_component(typeCB) clear
    set types [lindex $vehicleTypes $curr]
    eval $itk_component(typeCB) insert list end $types

    # temporarily turn on editing
    $itk_component(typeCB) configure -editable true

    # initialize selections
    $itk_component(typeCB) selection set 0

    # turn off editing
    $itk_component(typeCB) configure -editable false
}

::itcl::body TankWizardIA::changeType {} {
}

::itcl::body TankWizardIA::buildArrow {parent prefix text buildViewFunc} {
    itk_component add $prefix {
	frame $parent.$prefix
    } {}
    itk_component add $prefix\Arrow {
	::swidgets::togglearrow $itk_component($prefix).arrow
    } {}
    itk_component add $prefix\Label {
	label $itk_component($prefix).label -text $text \
		-anchor w
    } {}
    itk_component add $prefix\View {
	frame $itk_component($prefix).$prefix\View
    } {}
    $buildViewFunc $itk_component($prefix\View)
    grid $itk_component($prefix\Arrow) -row 0 -column 0 -sticky e
    grid $itk_component($prefix\Label) -row 0 -column 1 -sticky w
    grid columnconfigure $itk_component($prefix) 1 -weight 1
    $itk_component($prefix\Arrow) configure -command [::itcl::code $this toggle \
	    $itk_component($prefix\Arrow) $itk_component($prefix\View) -row 1 \
	    -column 1 -sticky nsew]
}

::itcl::body TankWizardIA::addWizardAttrs {obj {onlyTop 1}} {
    if {$onlyTop} {
	$archersMged attr set $obj \
	    WizardTop $wizardTop
    } else {
	$archersMged attr set $obj \
	    WizardName $wizardName \
	    WizardClass $wizardClass \
	    WizardTop $wizardTop \
	    WizardState $wizardState \
	    WizardOrigin $wizardOrigin \
	    WizardUnits $wizardUnits \
	    WizardVersion $wizardVersion
    }
}

::itcl::body TankWizardIA::drawTank {} {
    $archersMged configure -autoViewEnable 0

    if {$useOriginalZones} {
	$archersMged draw $wizardTop
    } else {
	$archersMged detachObservers
	$archersMged draw -C $tankColor -m1 \
	    $wizardTop/$wizardTop\_exterior/$wizardTop\_sprockets
	$archersMged draw -C $tankColor -m1 \
	    $wizardTop/$wizardTop\_exterior/$wizardTop\_roadWheels
	$archersMged draw -C $tankColor -m1 \
	    $wizardTop/$wizardTop\_exterior/$wizardTop\_idlerWheels
	$archersMged draw -C $tankColor -m1 \
	    $wizardTop/$wizardTop\_exterior/$wizardTop\_tracks
	$archersMged draw -C $tankColor -m1 \
	    $wizardTop/$wizardTop\_exterior/$wizardTop\_gun.r
	$archersMged draw -C $tankColor -m1 -x0.25 \
	    $wizardTop/$wizardTop\_exterior/$wizardTop\_hull.r
	$archersMged draw -C $tankColor -m1 -x0.25 \
	    $wizardTop/$wizardTop\_exterior/$wizardTop\_turret.r
	$archersMged draw -C $zoneColor -m1 -x0.25 \
	    $wizardTop/$wizardTop\_interior
	$archersMged attachObservers
    }

    $archersMged refreshAll
    $archersMged configure -autoViewEnable 1
}

::itcl::body TankWizardIA::buildTank {} {
    SetWaitCursor

    initRegionIds
    $archer pluginUpdateStatusBar "Building Tank..."
    set savedUnits [$archersMged units]
    $archersMged units $wizardUnits
    set local2base [$archersMged local2base]

    set wizardState \
	[list \
	     frontArmorType $frontArmorType \
	     rearArmorType $rearArmorType \
	     roofArmorType $roofArmorType \
	     sideArmorType $sideArmorType \
	     sponsonArmorType $sponsonArmorType \
	     hullLength $hullLength \
	     hullWidth $hullWidth \
	     hullHeight $hullHeight \
	     convHeight $convHeight \
	     upperOffset $upperOffset \
	     lowerOffset $lowerOffset \
	     wwDepth $wwDepth \
	     wwHeight $wwHeight \
	     numRoadWheels $numRoadWheels \
	     numIdlerWheels $numIdlerWheels \
	     sprocketDiameter $sprocketDiameter \
	     roadWheelDiameter $roadWheelDiameter \
	     idlerWheelDiameter $idlerWheelDiameter \
	     turretLength $turretLength \
	     turretWidth $turretWidth \
	     turretHeight $turretHeight \
	     turretOffset $turretOffset \
	     turretFrontOffset $turretFrontOffset \
	     turretRearOffset $turretRearOffset \
	     turretSideOffset $turretSideOffset \
	     gunLength $gunLength \
	     gunBaseInnerDiameter $gunBaseInnerDiameter \
	     gunEndInnerDiameter $gunEndInnerDiameter \
	     gunBaseOuterDiameter $gunBaseOuterDiameter \
	     gunEndOuterDiameter $gunEndOuterDiameter \
	     gunElevation $gunElevation \
	     trackThickness $trackThickness \
	     trackClearance $trackClearance]

    set extHalfLength [expr $hullLength * 0.5]
    set extHalfWidth [expr $hullWidth * 0.5]
    set extHalfHeight [expr $hullHeight * 0.5]
    set wwHalfDepth [expr $wwDepth * 0.5]
    set wwHalfHeight [expr $wwHeight * 0.5]
    set turretHalfLength [expr $turretLength * 0.5]
    set turretHalfWidth [expr $turretWidth * 0.5]

    set v [list 0 0 \
	       [expr {$extHalfHeight \
			  + $roadWheelDiameter \
			  + $trackThickness}]]

    set tankCenter [vectorAdd $wizardOrigin $v]

    $archer pluginUpdateProgressBar 0.1
    buildHull
    $archer pluginUpdateProgressBar 0.3
    buildWheels
    $archer pluginUpdateProgressBar 0.5
    buildTracks
    $archer pluginUpdateProgressBar 0.7
    buildTurret
    $archer pluginUpdateProgressBar 0.9
    buildGun
    $archer pluginUpdateProgressBar 1.0

    $archersMged g $wizardTop\_exterior \
	$wizardTop\_sprockets \
	$wizardTop\_roadWheels \
	$wizardTop\_idlerWheels \
	$wizardTop\_tracks \
	$wizardTop\_turret.r \
	$wizardTop\_hull.r \
	$wizardTop\_gun
    $archersMged g $wizardTop\_interior \
	$wizardTop\_hullZones

    $archersMged g $wizardTop \
	$wizardTop\_exterior \
	$wizardTop\_interior

    # Add wizard attributes
    addWizardAttrs $wizardTop\_exterior
    addWizardAttrs $wizardTop\_interior
    addWizardAttrs $wizardTop 0

    drawTank
    $archer pluginUpdateSaveMode 1

    $archersMged units $savedUnits
    SetNormalCursor

    $archer pluginUpdateProgressBar 0.0
    $archer pluginUpdateStatusBar ""
    return $wizardTop
}

::itcl::body TankWizardIA::buildTankXML {} {
    append tankXML [beginSystemXML $wizardTop]

    append tankXML [beginSystemXML "Hull"]
    append tankXML [buildComponentXML "Hull Armor" $hullArmorId]

    append tankXML [beginZoneXML "Front Zone" [expr {$zoneIdInitial + $frontZoneIdOffset}]]
    append tankXML [beginSystemXML "Driver"]
    append tankXML [buildVirtualComponentXML "Driver Head" "Human Skull" 100 0.5 0.5 0.5]
    append tankXML [buildVirtualComponentXML "Driver Torso" "Human Body" 100 0.5 0.5 0.5]
    append tankXML [buildVirtualComponentXML "Driver Extremities" "Human Body" 100 0.5 0.5 0.5]
    append tankXML [endSystemXML]
    append tankXML [endZoneXML]

    append tankXML [beginZoneXML "Rear Zone" [expr {$zoneIdInitial + $rearZoneIdOffset}]]
    append tankXML [beginSystemXML "Loader"]
    append tankXML [buildVirtualComponentXML "Loader Head" "Human Skull" 100 0.5 0.5 0.5]
    append tankXML [buildVirtualComponentXML "Loader Torso" "Human Body" 100 0.5 0.5 0.5]
    append tankXML [buildVirtualComponentXML "Loader Extremities" "Human Body" 100 0.5 0.5 0.5]
    append tankXML [endSystemXML]
    append tankXML [beginSystemXML "Drive Train"]
    append tankXML [buildVirtualComponentXML "Engine" "Cast Iron" 85 0.5 0.5 0.5]
    append tankXML [buildVirtualComponentXML "Transmission" "Aluminum" 60 0.5 0.5 0.5]
    append tankXML [endSystemXML]
    append tankXML [endZoneXML]

    append tankXML [beginZoneXML "Floor Zone" [expr {$zoneIdInitial + $floorZoneIdOffset}]]
    append tankXML [beginSystemXML "Hydraulics"]
    append tankXML [buildVirtualComponentXML "Hydraulic Pump" "Cast Iron" 85 0.5 0.5 0.5]
    append tankXML [buildVirtualComponentXML "Hydraulic Lines" "Rolled Steel" 75 0.5 0.5 0.5]
    append tankXML [endSystemXML]
    append tankXML [beginSystemXML "Electric System"]
    append tankXML [buildVirtualComponentXML "Electric Lines" "Copper" 75 0.5 0.5 0.5]
    append tankXML [endSystemXML]
    append tankXML [endZoneXML]

    append tankXML [endSystemXML]

    append tankXML [beginSystemXML "Turret"]

    append tankXML [beginZoneXML "Center Zone" [expr {$zoneIdInitial + $centerZoneIdOffset}]]
    append tankXML [beginSystemXML "Commander"]
    append tankXML [buildVirtualComponentXML "Commander Head" "Human Skull" 100 0.5 0.5 0.5]
    append tankXML [buildVirtualComponentXML "Commander Torso" "Human Body" 100 0.5 0.5 0.5]
    append tankXML [buildVirtualComponentXML "Commander Extremities" "Human Body" 100 0.5 0.5 0.5]
    append tankXML [endSystemXML]
    append tankXML [beginSystemXML "Gunner"]
    append tankXML [buildVirtualComponentXML "Gunner Head" "Human Skull" 100 0.5 0.5 0.5]
    append tankXML [buildVirtualComponentXML "Gunner Torso" "Human Body" 100 0.5 0.5 0.5]
    append tankXML [buildVirtualComponentXML "Gunner Extremities" "Human Body" 100 0.5 0.5 0.5]
    append tankXML [endSystemXML]
    append tankXML [beginSystemXML "Fire Control System"]
    append tankXML [buildVirtualComponentXML "Azimuth Hand Crank" "Cast Iron" 100 0.5 0.5 0.5]
    append tankXML [buildVirtualComponentXML "Elevation Hand Crank" "Cast Iron" 100 0.5 0.5 0.5]
    append tankXML [beginSystemXML "Computerized"]
    append tankXML [buildVirtualComponentXML "Computer" "Aluminum" 15 0.5 0.5 0.5]
    append tankXML [endSystemXML]
    append tankXML [endSystemXML]
    append tankXML [endZoneXML]

    append tankXML [beginSystemXML "Gun System"]
    append tankXML [buildComponentXML "Gun Barrel" $gunBarrelId]
    append tankXML [endSystemXML]
    append tankXML [endSystemXML]
    append tankXML [beginSystemXML "Suspension"]
    append tankXML [beginSystemXML "Left Suspension"]
    append tankXML [buildComponentXML "Left Sprocket" $leftSprocketId]
    append tankXML [buildComponentXML "Left Track" $leftTrackId]
    append tankXML [buildWheelXML "Left Road" $leftRoadWheelInitialId $leftRoadWheelEndId]
    append tankXML [buildWheelXML "Left Idler" $leftIdlerWheelInitialId $leftIdlerWheelEndId]
    append tankXML [endSystemXML]
    append tankXML [beginSystemXML "Right Suspension"]
    append tankXML [buildComponentXML "Right Sprocket" $rightSprocketId]
    append tankXML [buildComponentXML "Right Track" $rightTrackId]
    append tankXML [buildWheelXML "Right Road" $rightRoadWheelInitialId $rightRoadWheelEndId]
    append tankXML [buildWheelXML "Right Idler" $rightIdlerWheelInitialId $rightIdlerWheelEndId]
    append tankXML [endSystemXML]
    append tankXML [endSystemXML]
    append tankXML [endSystemXML]
}

::itcl::body TankWizardIA::buildHull {} {
    buildHullInterior
    buildHullZones
    buildHullExterior
}

## - buildHullExterior
#
# Build hull exterior centered about (0, 0, 0).
#
::itcl::body TankWizardIA::buildHullExterior {} {
    # build rear of hull
    set v1 [list -$extHalfLength -$extHalfWidth -$extHalfHeight]
    set v2 [list -$extHalfLength $extHalfWidth -$extHalfHeight]
    set v3 [list -$extHalfLength $extHalfWidth $extHalfHeight]
    set v4 [list -$extHalfLength -$extHalfWidth $extHalfHeight]
    set v5 [list [expr {$extHalfLength - $lowerOffset}] -$extHalfWidth -$extHalfHeight]
    set v6 [list [expr {$extHalfLength - $lowerOffset}] $extHalfWidth -$extHalfHeight]
    set v7 [list [expr {$extHalfLength - $upperOffset}] $extHalfWidth $extHalfHeight]
    set v8 [list [expr {$extHalfLength - $upperOffset}] -$extHalfWidth $extHalfHeight]
    set v1 [vectorScale [vectorAdd $v1 $tankCenter] $local2base]
    set v2 [vectorScale [vectorAdd $v2 $tankCenter] $local2base]
    set v3 [vectorScale [vectorAdd $v3 $tankCenter] $local2base]
    set v4 [vectorScale [vectorAdd $v4 $tankCenter] $local2base]
    set v5 [vectorScale [vectorAdd $v5 $tankCenter] $local2base]
    set v6 [vectorScale [vectorAdd $v6 $tankCenter] $local2base]
    set v7 [vectorScale [vectorAdd $v7 $tankCenter] $local2base]
    set v8 [vectorScale [vectorAdd $v8 $tankCenter] $local2base]
    $archersMged put $wizardTop\_hull_rear.s arb8 \
	    V1 $v1 V2 $v2 V3 $v3 V4 $v4 \
	    V5 $v5 V6 $v6 V7 $v7 V8 $v8

    # build front of hull
    set v1 [list $extHalfLength -$extHalfWidth [expr {-$extHalfHeight + $convHeight}]]
    set v2 [list $extHalfLength $extHalfWidth [expr {-$extHalfHeight + $convHeight}]]
    set v1 [vectorScale [vectorAdd $v1 $tankCenter] $local2base]
    set v2 [vectorScale [vectorAdd $v2 $tankCenter] $local2base]
    $archersMged put $wizardTop\_hull_front.s arb8 \
	    V1 $v5 V2 $v8 V3 $v7 V4 $v6 \
	    V5 $v1 V6 $v1 V7 $v2 V8 $v2

    # make right side wheel well cut solid
    set v1 [list \
	    -$extHalfLength \
	    -$extHalfWidth \
	    -$extHalfHeight]
    set v2 [list \
	    -$extHalfLength \
	    [expr {-$extHalfWidth + $wwDepth}] \
	    -$extHalfHeight]
    set v3 [list \
	    -$extHalfLength \
	    [expr {-$extHalfWidth + $wwDepth}] \
	    [expr {-$extHalfHeight + $wwHeight}]]
    set v4 [list \
	    -$extHalfLength \
	    -$extHalfWidth \
	    [expr {-$extHalfHeight + $wwHeight}]]
    set v5 [list \
	    $extHalfLength \
	    -$extHalfWidth \
	    -$extHalfHeight]
    set v6 [list \
	    $extHalfLength \
	    [expr {-$extHalfWidth + $wwDepth}] \
	    -$extHalfHeight]
    set v7 [list \
	    $extHalfLength \
	    [expr {-$extHalfWidth + $wwDepth}] \
	    [expr {-$extHalfHeight + $wwHeight}]]
    set v8 [list \
	    $extHalfLength \
	    -$extHalfWidth \
	    [expr {-$extHalfHeight + $wwHeight}]]
    set v1 [vectorScale [vectorAdd $v1 $tankCenter] $local2base]
    set v2 [vectorScale [vectorAdd $v2 $tankCenter] $local2base]
    set v3 [vectorScale [vectorAdd $v3 $tankCenter] $local2base]
    set v4 [vectorScale [vectorAdd $v4 $tankCenter] $local2base]
    set v5 [vectorScale [vectorAdd $v5 $tankCenter] $local2base]
    set v6 [vectorScale [vectorAdd $v6 $tankCenter] $local2base]
    set v7 [vectorScale [vectorAdd $v7 $tankCenter] $local2base]
    set v8 [vectorScale [vectorAdd $v8 $tankCenter] $local2base]
    $archersMged put $wizardTop\_re_wwcut.s arb8 \
	    V1 $v1 V2 $v2 V3 $v3 V4 $v4 \
	    V5 $v5 V6 $v6 V7 $v7 V8 $v8

    # make left side wheel well cut solid
    set v1 [list -$extHalfLength $extHalfWidth -$extHalfHeight]
    set v2 [list -$extHalfLength [expr {$extHalfWidth - $wwDepth}] -$extHalfHeight]
    set v3 [list -$extHalfLength [expr {$extHalfWidth - $wwDepth}] [expr {-$extHalfHeight + $wwHeight}]]
    set v4 [list -$extHalfLength $extHalfWidth [expr {-$extHalfHeight + $wwHeight}]]
    set v5 [list $extHalfLength $extHalfWidth -$extHalfHeight]
    set v6 [list $extHalfLength [expr {$extHalfWidth - $wwDepth}] -$extHalfHeight]
    set v7 [list $extHalfLength [expr {$extHalfWidth - $wwDepth}] [expr {-$extHalfHeight + $wwHeight}]]
    set v8 [list $extHalfLength $extHalfWidth [expr {-$extHalfHeight + $wwHeight}]]
    set v1 [vectorScale [vectorAdd $v1 $tankCenter] $local2base]
    set v2 [vectorScale [vectorAdd $v2 $tankCenter] $local2base]
    set v3 [vectorScale [vectorAdd $v3 $tankCenter] $local2base]
    set v4 [vectorScale [vectorAdd $v4 $tankCenter] $local2base]
    set v5 [vectorScale [vectorAdd $v5 $tankCenter] $local2base]
    set v6 [vectorScale [vectorAdd $v6 $tankCenter] $local2base]
    set v7 [vectorScale [vectorAdd $v7 $tankCenter] $local2base]
    set v8 [vectorScale [vectorAdd $v8 $tankCenter] $local2base]
    $archersMged put $wizardTop\_le_wwcut.s arb8 \
	V1 $v1 V2 $v2 V3 $v3 V4 $v4 \
	V5 $v5 V6 $v6 V7 $v7 V8 $v8

    $archersMged put $wizardTop\_hull.r comb \
	region yes \
	id $hullArmorId \
	tree \
	[list - \
	     [list - \
		  [list - \
		       [list u \
			    [list l $wizardTop\_hull_front.s] \
			    [list l $wizardTop\_hull_rear.s]] \
		       [list l $wizardTop\_re_wwcut.s]] \
		  [list l $wizardTop\_le_wwcut.s]] \
	     [list l $wizardTop\_hullInterior.c]]

    addWizardAttrs $wizardTop\_hull_front.s
    addWizardAttrs $wizardTop\_hull_rear.s
    addWizardAttrs $wizardTop\_re_wwcut.s
    addWizardAttrs $wizardTop\_le_wwcut.s
    addWizardAttrs $wizardTop\_hull.r
}

## - buildHullInterior
#
# Build hull interior centered about (0, 0, 0).
#
::itcl::body TankWizardIA::buildHullInterior {} {

    # build rear of hullInterior
    set v1 [list \
		[expr {-$extHalfLength + $rearArmorThickness}] \
		[expr {-$extHalfWidth + $sponsonArmorThickness}] \
		[expr {-$extHalfHeight + $roofArmorThickness}]]
    set v2 [list \
		[expr {-$extHalfLength + $rearArmorThickness}] \
		[expr {$extHalfWidth - $sponsonArmorThickness}] \
		[expr {-$extHalfHeight + $roofArmorThickness}]]
    set v3 [list \
		[expr {-$extHalfLength + $rearArmorThickness}] \
		[expr {$extHalfWidth - $sponsonArmorThickness}] \
		[expr {$extHalfHeight - $roofArmorThickness}]]
    set v4 [list \
		[expr {-$extHalfLength + $rearArmorThickness}] \
		[expr {-$extHalfWidth + $sponsonArmorThickness}] \
		[expr {$extHalfHeight - $roofArmorThickness}]]

    set tx [expr {$upperOffset - $lowerOffset}]
    set theta [expr {atan2($hullHeight, $tx)}]
    set xoffset [expr {$roofArmorThickness / tan($theta)}]
    set v5 [list \
		[expr {$extHalfLength - $lowerOffset - $xoffset}] \
		[expr {-$extHalfWidth + $sponsonArmorThickness}] \
		[expr {-$extHalfHeight + $roofArmorThickness}]]
    set v6 [list \
		[expr {$extHalfLength - $lowerOffset - $xoffset}] \
		[expr {$extHalfWidth - $sponsonArmorThickness}] \
		[expr {-$extHalfHeight + $roofArmorThickness}]]

    set alpha [expr {atan2($tx, $hullHeight)}]
    set xoffset [expr {$roofArmorThickness * tan($alpha)}]
    set v7 [list \
		[expr {$extHalfLength - $upperOffset + $xoffset}] \
		[expr {$extHalfWidth - $sponsonArmorThickness}] \
		[expr {$extHalfHeight - $roofArmorThickness}]]
    set v8 [list \
		[expr {$extHalfLength - $upperOffset + $xoffset}] \
		[expr {-$extHalfWidth + $sponsonArmorThickness}] \
		[expr {$extHalfHeight - $roofArmorThickness}]]
    set v1 [vectorScale [vectorAdd $v1 $tankCenter] $local2base]
    set v2 [vectorScale [vectorAdd $v2 $tankCenter] $local2base]
    set v3 [vectorScale [vectorAdd $v3 $tankCenter] $local2base]
    set v4 [vectorScale [vectorAdd $v4 $tankCenter] $local2base]
    set v5 [vectorScale [vectorAdd $v5 $tankCenter] $local2base]
    set v6 [vectorScale [vectorAdd $v6 $tankCenter] $local2base]
    set v7 [vectorScale [vectorAdd $v7 $tankCenter] $local2base]
    set v8 [vectorScale [vectorAdd $v8 $tankCenter] $local2base]
    $archersMged put $wizardTop\_hullInterior_rear.s arb8 \
	V1 $v1 V2 $v2 V3 $v3 V4 $v4 \
	V5 $v5 V6 $v6 V7 $v7 V8 $v8

    # build front of hullInterior
    set pi_2 [expr {atan2(1, 0)}]
    set alpha [expr {atan2($lowerOffset, $convHeight)}]
    set theta [expr {$pi_2 - $alpha}]
    set xoffset [expr {$roofArmorThickness / tan($theta)}]
    set v1 [list \
		[expr {$extHalfLength - $frontArmorThickness - $xoffset}] \
		[expr {-$extHalfWidth + $sponsonArmorThickness}] \
		[expr {-$extHalfHeight + $convHeight}]]
    set v2 [list \
		[expr {$extHalfLength - $frontArmorThickness - $xoffset}] \
		[expr {$extHalfWidth - $sponsonArmorThickness}] \
		[expr {-$extHalfHeight + $convHeight}]]
    set v1 [vectorScale [vectorAdd $v1 $tankCenter] $local2base]
    set v2 [vectorScale [vectorAdd $v2 $tankCenter] $local2base]
    $archersMged put $wizardTop\_hullInterior_front.s arb8 \
	V1 $v5 V2 $v8 V3 $v7 V4 $v6 \
	V5 $v1 V6 $v1 V7 $v2 V8 $v2

    # make right side wheel well cut solid
    set v1 [list \
		-$extHalfLength \
		-$extHalfWidth \
		-$extHalfHeight]
    set v2 [list \
		-$extHalfLength \
		[expr {-$extHalfWidth + $wwDepth + $sideArmorThickness}] \
		-$extHalfHeight]
    set v3 [list \
		-$extHalfLength \
		[expr {-$extHalfWidth + $wwDepth + $sideArmorThickness}] \
		[expr {-$extHalfHeight + $wwHeight + $roofArmorThickness}]]
    set v4 [list \
		-$extHalfLength \
		-$extHalfWidth \
		[expr {-$extHalfHeight + $wwHeight + $roofArmorThickness}]]
    set v5 [list \
		$extHalfLength \
		-$extHalfWidth \
		-$extHalfHeight]
    set v6 [list \
		$extHalfLength \
		[expr {-$extHalfWidth + $wwDepth + $sideArmorThickness}] \
		-$extHalfHeight]
    set v7 [list \
		$extHalfLength \
		[expr {-$extHalfWidth + $wwDepth + $sideArmorThickness}] \
		[expr {-$extHalfHeight + $wwHeight + $roofArmorThickness}]]
    set v8 [list \
		$extHalfLength \
		-$extHalfWidth \
		[expr {-$extHalfHeight + $wwHeight + $roofArmorThickness}]]
    set v1 [vectorScale [vectorAdd $v1 $tankCenter] $local2base]
    set v2 [vectorScale [vectorAdd $v2 $tankCenter] $local2base]
    set v3 [vectorScale [vectorAdd $v3 $tankCenter] $local2base]
    set v4 [vectorScale [vectorAdd $v4 $tankCenter] $local2base]
    set v5 [vectorScale [vectorAdd $v5 $tankCenter] $local2base]
    set v6 [vectorScale [vectorAdd $v6 $tankCenter] $local2base]
    set v7 [vectorScale [vectorAdd $v7 $tankCenter] $local2base]
    set v8 [vectorScale [vectorAdd $v8 $tankCenter] $local2base]
    $archersMged put $wizardTop\_ri_wwcut.s arb8 \
	V1 $v1 V2 $v2 V3 $v3 V4 $v4 \
	V5 $v5 V6 $v6 V7 $v7 V8 $v8

    # make left side wheel well cut solid
    set v1 [list \
		-$extHalfLength \
		$extHalfWidth \
		-$extHalfHeight]
    set v2 [list \
		-$extHalfLength \
		[expr {$extHalfWidth - $wwDepth - $sideArmorThickness}] \
		-$extHalfHeight]
    set v3 [list \
		-$extHalfLength \
		[expr {$extHalfWidth - $wwDepth - $sideArmorThickness}] \
		[expr {-$extHalfHeight + $wwHeight + $roofArmorThickness}]]
    set v4 [list \
		-$extHalfLength \
		$extHalfWidth \
		[expr {-$extHalfHeight + $wwHeight + $roofArmorThickness}]]
    set v5 [list \
		$extHalfLength \
		$extHalfWidth \
		-$extHalfHeight]
    set v6 [list \
		$extHalfLength \
		[expr {$extHalfWidth - $wwDepth - $sideArmorThickness}] \
		-$extHalfHeight]
    set v7 [list \
		$extHalfLength \
		[expr {$extHalfWidth - $wwDepth - $sideArmorThickness}] \
		[expr {-$extHalfHeight + $wwHeight + $roofArmorThickness}]]
    set v8 [list \
		$extHalfLength \
		$extHalfWidth \
		[expr {-$extHalfHeight + $wwHeight + $roofArmorThickness}]]
    set v1 [vectorScale [vectorAdd $v1 $tankCenter] $local2base]
    set v2 [vectorScale [vectorAdd $v2 $tankCenter] $local2base]
    set v3 [vectorScale [vectorAdd $v3 $tankCenter] $local2base]
    set v4 [vectorScale [vectorAdd $v4 $tankCenter] $local2base]
    set v5 [vectorScale [vectorAdd $v5 $tankCenter] $local2base]
    set v6 [vectorScale [vectorAdd $v6 $tankCenter] $local2base]
    set v7 [vectorScale [vectorAdd $v7 $tankCenter] $local2base]
    set v8 [vectorScale [vectorAdd $v8 $tankCenter] $local2base]
    $archersMged put $wizardTop\_li_wwcut.s arb8 \
	V1 $v1 V2 $v2 V3 $v3 V4 $v4 \
	V5 $v5 V6 $v6 V7 $v7 V8 $v8

    #id $hullInteriorId
    $archersMged put $wizardTop\_hullInterior.c comb \
	region no \
	tree \
	[list - \
	     [list - \
		  [list u \
		       [list l $wizardTop\_hullInterior_front.s] \
		       [list l $wizardTop\_hullInterior_rear.s]] \
		  [list l $wizardTop\_ri_wwcut.s]] \
	     [list l $wizardTop\_li_wwcut.s]]

    addWizardAttrs $wizardTop\_hullInterior_front.s
    addWizardAttrs $wizardTop\_hullInterior_rear.s
    addWizardAttrs $wizardTop\_ri_wwcut.s
    addWizardAttrs $wizardTop\_li_wwcut.s
    addWizardAttrs $wizardTop\_hullInterior.c
}

## - buildHullZones
#
::itcl::body TankWizardIA::buildHullZones {} {
    set zoneLengthDelta [expr {$hullLength / 3.0}]
    set zoneWidthDelta [expr {$hullWidth / 3.0}]
    set zoneUpperHeightDelta [expr {($hullHeight - $convHeight) * 0.5}]

    buildLowerHullZones
    buildUpperHullZones

    if {!$useOriginalZones} {
	buildHullZoneFromOriginal "front" "f" [expr {$zoneIdInitial + $frontZoneIdOffset}]
	buildHullZoneFromOriginal "center" "c" [expr {$zoneIdInitial + $centerZoneIdOffset}]
	buildHullZoneFromOriginal "rear" "r" [expr {$zoneIdInitial + $rearZoneIdOffset}]
	buildFloorHullZoneFromOriginal [expr {$zoneIdInitial + $floorZoneIdOffset}]
    }

    addWizardAttrs $wizardTop\_hullZones
}

## - buildLowerHullZones
#
# Build zones moving from right-rear toward left-front.
#
::itcl::body TankWizardIA::buildLowerHullZones {} {
    set zdir l

    for {set i 0} {$i < 3} {incr i} {
	for {set j 0} {$j < 3} {incr j} {
	    set v1 [list \
		    [expr {-$extHalfLength + $i * $zoneLengthDelta}] \
		    [expr {-$extHalfWidth + $j * $zoneWidthDelta}] \
		    -$extHalfHeight]
	    set v2 [list \
		    [expr {-$extHalfLength + $i * $zoneLengthDelta}] \
		    [expr {-$extHalfWidth + ($j + 1) * $zoneWidthDelta}] \
		    -$extHalfHeight]
	    set v3 [list \
		    [expr {-$extHalfLength + $i * $zoneLengthDelta}] \
		    [expr {-$extHalfWidth + ($j + 1) * $zoneWidthDelta}] \
		    [expr {-$extHalfHeight + $wwHeight}]]
	    set v4 [list \
		    [expr {-$extHalfLength + $i * $zoneLengthDelta}] \
		    [expr {-$extHalfWidth + $j * $zoneWidthDelta}] \
		    [expr {-$extHalfHeight + $wwHeight}]]
	    set v5 [list \
		    [expr {-$extHalfLength + ($i + 1) * $zoneLengthDelta}] \
		    [expr {-$extHalfWidth + $j * $zoneWidthDelta}] \
		    -$extHalfHeight]
	    set v6 [list \
		    [expr {-$extHalfLength + ($i + 1) * $zoneLengthDelta}] \
		    [expr {-$extHalfWidth + ($j + 1) * $zoneWidthDelta}] \
		    -$extHalfHeight]
	    set v7 [list \
		    [expr {-$extHalfLength + ($i + 1) * $zoneLengthDelta}] \
		    [expr {-$extHalfWidth + ($j + 1) * $zoneWidthDelta}] \
		    [expr {-$extHalfHeight + $wwHeight}]]
	    set v8 [list \
		    [expr {-$extHalfLength + ($i + 1) * $zoneLengthDelta}] \
		    [expr {-$extHalfWidth + $j * $zoneWidthDelta}] \
		    [expr {-$extHalfHeight + $wwHeight}]]

	    set v1 [vectorScale [vectorAdd $v1 $tankCenter] $local2base]
	    set v2 [vectorScale [vectorAdd $v2 $tankCenter] $local2base]
	    set v3 [vectorScale [vectorAdd $v3 $tankCenter] $local2base]
	    set v4 [vectorScale [vectorAdd $v4 $tankCenter] $local2base]
	    set v5 [vectorScale [vectorAdd $v5 $tankCenter] $local2base]
	    set v6 [vectorScale [vectorAdd $v6 $tankCenter] $local2base]
	    set v7 [vectorScale [vectorAdd $v7 $tankCenter] $local2base]
	    set v8 [vectorScale [vectorAdd $v8 $tankCenter] $local2base]

	    switch -- $i {
		0 {
		    set xdir r
		}
		1 {
		    set xdir c
		}
		2 {
		    set xdir f
		}
	    }

	    switch -- $j {
		0 {
		    set ydir r
		}
		1 {
		    set ydir c
		}
		2 {
		    set ydir l
		}
	    }

	    $archersMged put $wizardTop\_$zdir$xdir$ydir\_hullZone.s arb8 \
		    V1 $v1 V2 $v2 V3 $v3 V4 $v4 \
		    V5 $v5 V6 $v6 V7 $v7 V8 $v8
	    $archersMged c $wizardTop\_$zdir$xdir$ydir\_hullZone.r \
		    $wizardTop\_hullInterior.c + $wizardTop\_$zdir$xdir$ydir\_hullZone.s

	    $archersMged adjust $wizardTop\_$zdir$xdir$ydir\_hullZone.r \
		region yes \
		id $zoneId \
		shader {plastic {tr 0.75}}
	    $archersMged g $wizardTop\_hullZones \
		$wizardTop\_$zdir$xdir$ydir\_hullZone.r

	    addWizardAttrs $wizardTop\_$zdir$xdir$ydir\_hullZone.s
	    addWizardAttrs $wizardTop\_$zdir$xdir$ydir\_hullZone.r

	    incr zoneId
	}
    }
}

## - buildUpperHullZones
#
# Build zones moving from right-rear toward left-front.
#
::itcl::body TankWizardIA::buildUpperHullZones {} {
    set zdir l

    for {set h 0} {$h < 2} {incr h} {
	for {set i 0} {$i < 3} {incr i} {
	    for {set j 0} {$j < 3} {incr j} {
		set v1 [list \
			[expr {-$extHalfLength + $i * $zoneLengthDelta}] \
			[expr {-$extHalfWidth + $j * $zoneWidthDelta}] \
			[expr {-$extHalfHeight + $wwHeight + $h * $zoneUpperHeightDelta}]]
		set v2 [list \
			[expr {-$extHalfLength + $i * $zoneLengthDelta}] \
			[expr {-$extHalfWidth + ($j + 1) * $zoneWidthDelta}] \
			[expr {-$extHalfHeight + $wwHeight + $h * $zoneUpperHeightDelta}]]
		set v3 [list \
			[expr {-$extHalfLength + $i * $zoneLengthDelta}] \
			[expr {-$extHalfWidth + ($j + 1) * $zoneWidthDelta}] \
			[expr {-$extHalfHeight + $wwHeight + ($h + 1) * $zoneUpperHeightDelta}]]
		set v4 [list \
			[expr {-$extHalfLength + $i * $zoneLengthDelta}] \
			[expr {-$extHalfWidth + $j * $zoneWidthDelta}] \
			[expr {-$extHalfHeight + $wwHeight + ($h + 1) * $zoneUpperHeightDelta}]]
		set v5 [list \
			[expr {-$extHalfLength + ($i + 1) * $zoneLengthDelta}] \
			[expr {-$extHalfWidth + $j * $zoneWidthDelta}] \
			[expr {-$extHalfHeight + $wwHeight + $h * $zoneUpperHeightDelta}]]
		set v6 [list \
			[expr {-$extHalfLength + ($i + 1) * $zoneLengthDelta}] \
			[expr {-$extHalfWidth + ($j + 1) * $zoneWidthDelta}] \
			[expr {-$extHalfHeight + $wwHeight + $h * $zoneUpperHeightDelta}]]
		set v7 [list \
			[expr {-$extHalfLength + ($i + 1) * $zoneLengthDelta}] \
			[expr {-$extHalfWidth + ($j + 1) * $zoneWidthDelta}] \
			[expr {-$extHalfHeight + $wwHeight + ($h + 1) * $zoneUpperHeightDelta}]]
		set v8 [list \
			[expr {-$extHalfLength + ($i + 1) * $zoneLengthDelta}] \
			[expr {-$extHalfWidth + $j * $zoneWidthDelta}] \
			[expr {-$extHalfHeight + $wwHeight + ($h + 1) * $zoneUpperHeightDelta}]]

		set v1 [vectorScale [vectorAdd $v1 $tankCenter] $local2base]
		set v2 [vectorScale [vectorAdd $v2 $tankCenter] $local2base]
		set v3 [vectorScale [vectorAdd $v3 $tankCenter] $local2base]
		set v4 [vectorScale [vectorAdd $v4 $tankCenter] $local2base]
		set v5 [vectorScale [vectorAdd $v5 $tankCenter] $local2base]
		set v6 [vectorScale [vectorAdd $v6 $tankCenter] $local2base]
		set v7 [vectorScale [vectorAdd $v7 $tankCenter] $local2base]
		set v8 [vectorScale [vectorAdd $v8 $tankCenter] $local2base]

		switch -- $h {
		    0 {
			set zdir c
		    }
		    1 {
			set zdir u
		    }
		}

		switch -- $i {
		    0 {
			set xdir r
		    }
		    1 {
			set xdir c
		    }
		    2 {
			set xdir f
		    }
		}

		switch -- $j {
		    0 {
			set ydir r
		    }
		    1 {
			set ydir c
		    }
		    2 {
			set ydir l
		    }
		}

		$archersMged put $wizardTop\_$zdir$xdir$ydir\_hullZone.s arb8 \
		    V1 $v1 V2 $v2 V3 $v3 V4 $v4 \
		    V5 $v5 V6 $v6 V7 $v7 V8 $v8
		$archersMged c $wizardTop\_$zdir$xdir$ydir\_hullZone.r \
		    $wizardTop\_hullInterior.c + $wizardTop\_$zdir$xdir$ydir\_hullZone.s

		$archersMged adjust $wizardTop\_$zdir$xdir$ydir\_hullZone.r \
		    region yes \
		    id $zoneId \
		    shader {plastic {tr 0.75}}
		$archersMged g $wizardTop\_hullZones \
		    $wizardTop\_$zdir$xdir$ydir\_hullZone.r

		addWizardAttrs $wizardTop\_$zdir$xdir$ydir\_hullZone.s
		addWizardAttrs $wizardTop\_$zdir$xdir$ydir\_hullZone.r

		incr zoneId
	    }
	}
    }
}

::itcl::body TankWizardIA::buildHullZoneFromOriginal {name xdir id} {
    $archersMged facetize $wizardTop\_$name\_hullZone.s \
	$wizardTop\_c[subst $xdir]r_hullZone.s \
	$wizardTop\_c[subst $xdir]c_hullZone.s \
	$wizardTop\_c[subst $xdir]l_hullZone.s \
	$wizardTop\_u[subst $xdir]r_hullZone.s \
	$wizardTop\_u[subst $xdir]c_hullZone.s \
	$wizardTop\_u[subst $xdir]l_hullZone.s

    $archersMged put $wizardTop\_$name\_hullZone.r comb \
	region yes \
	id $id \
	rgb $zoneColor \
	tree \
	[list l $wizardTop\_$name\_hullZone.s]

    $archersMged adjust $wizardTop\_$name\_hullZone.r \
	shader {plastic {tr 0.75}}

    $archersMged g $wizardTop\_hullZones \
	$wizardTop\_$name\_hullZone.r

    $archersMged rm $wizardTop\_hullZones \
	$wizardTop\_c[subst $xdir]r_hullZone.r \
	$wizardTop\_c[subst $xdir]c_hullZone.r \
	$wizardTop\_c[subst $xdir]l_hullZone.r \
	$wizardTop\_u[subst $xdir]r_hullZone.r \
	$wizardTop\_u[subst $xdir]c_hullZone.r \
	$wizardTop\_u[subst $xdir]l_hullZone.r

    $archersMged killtree \
	$wizardTop\_c[subst $xdir]r_hullZone.r \
	$wizardTop\_c[subst $xdir]c_hullZone.r \
	$wizardTop\_c[subst $xdir]l_hullZone.r \
	$wizardTop\_u[subst $xdir]r_hullZone.r \
	$wizardTop\_u[subst $xdir]c_hullZone.r \
	$wizardTop\_u[subst $xdir]l_hullZone.r

    addWizardAttrs $wizardTop\_$name\_hullZone.s
    addWizardAttrs $wizardTop\_$name\_hullZone.r
}

::itcl::body TankWizardIA::buildFloorHullZoneFromOriginal {id} {
    $archersMged facetize $wizardTop\_floor_hullZone.s \
	$wizardTop\_lrr_hullZone.s \
	$wizardTop\_lrc_hullZone.s \
	$wizardTop\_lrl_hullZone.s \
	$wizardTop\_lcr_hullZone.s \
	$wizardTop\_lcc_hullZone.s \
	$wizardTop\_lcl_hullZone.s \
	$wizardTop\_lfr_hullZone.s \
	$wizardTop\_lfc_hullZone.s \
	$wizardTop\_lfl_hullZone.s

    $archersMged put $wizardTop\_floor_hullZone.r comb \
	region yes \
	id $id \
	rgb $zoneColor \
	tree \
	[list l $wizardTop\_floor_hullZone.s]

    $archersMged adjust $wizardTop\_floor_hullZone.r \
	shader {plastic {tr 0.75}}

    $archersMged g $wizardTop\_hullZones \
	$wizardTop\_floor_hullZone.r

    $archersMged rm $wizardTop\_hullZones \
	$wizardTop\_lrr_hullZone.r \
	$wizardTop\_lrc_hullZone.r \
	$wizardTop\_lrl_hullZone.r \
	$wizardTop\_lcr_hullZone.r \
	$wizardTop\_lcc_hullZone.r \
	$wizardTop\_lcl_hullZone.r \
	$wizardTop\_lfr_hullZone.r \
	$wizardTop\_lfc_hullZone.r \
	$wizardTop\_lfl_hullZone.r

    $archersMged killtree \
	$wizardTop\_lrr_hullZone.r \
	$wizardTop\_lrc_hullZone.r \
	$wizardTop\_lrl_hullZone.r \
	$wizardTop\_lcr_hullZone.r \
	$wizardTop\_lcc_hullZone.r \
	$wizardTop\_lcl_hullZone.r \
	$wizardTop\_lfr_hullZone.r \
	$wizardTop\_lfc_hullZone.r \
	$wizardTop\_lfl_hullZone.r

    addWizardAttrs $wizardTop\_floor_hullZone.s
    addWizardAttrs $wizardTop\_floor_hullZone.r
}

::itcl::body TankWizardIA::buildWheels {} {
    buildDriveSprockets
    buildRoadWheels
    buildIdlerWheels
}

::itcl::body TankWizardIA::buildDriveSprockets {} {
    set sprocketRadius [expr {$sprocketDiameter * 0.5}]
    set driveSprocketX [expr {-$extHalfLength + $sprocketRadius + $trackThickness}]
    set ly $extHalfWidth
    set ry -$extHalfWidth
    set driveSprocketZ [expr {-$extHalfHeight + $wwHeight - $sprocketRadius - $trackClearance - $trackThickness}]
    set leftTrackYMin [expr {$ly - $wwDepth}]
    set leftTrackYMax $ly
    set rightTrackYMin $ry
    set rightTrackYMax [expr {$ry + $wwDepth}]


    set v [list $driveSprocketX $ly $driveSprocketZ]
    set v [vectorScale [vectorAdd $v $tankCenter] $local2base]

    set dsr [expr {$sprocketRadius * $local2base}]
    set dsd [expr {$wwDepth * $local2base}]

    $archersMged put $wizardTop\_l_sprocket.s tgc \
	V $v \
	H [list 0 -$dsd 0] \
	A [list $dsr 0 0] \
	B [list 0 0 $dsr] \
	C [list $dsr 0 0] \
	D [list 0 0 $dsr]
    $archersMged put $wizardTop\_l_sprocket.r comb \
	region yes \
	id $leftSprocketId \
	tree \
	[list l $wizardTop\_l_sprocket.s]

    addWizardAttrs $wizardTop\_l_sprocket.r
    addWizardAttrs $wizardTop\_l_sprocket.s

    set v [list $driveSprocketX $ry $driveSprocketZ]
    set v [vectorScale [vectorAdd $v $tankCenter] $local2base]

    $archersMged put $wizardTop\_r_sprocket.s tgc \
	V $v \
	H [list 0 $dsd 0] \
	A [list $dsr 0 0] \
	B [list 0 0 $dsr] \
	C [list $dsr 0 0] \
	D [list 0 0 $dsr]
    $archersMged put $wizardTop\_r_sprocket.r comb \
	region yes \
	id $rightSprocketId \
	tree \
	[list l $wizardTop\_r_sprocket.s]

    addWizardAttrs $wizardTop\_r_sprocket.r
    addWizardAttrs $wizardTop\_r_sprocket.s

    $archersMged put $wizardTop\_sprockets comb \
	region no tree \
	[list u \
	     [list l $wizardTop\_l_sprocket.r] \
	     [list l $wizardTop\_r_sprocket.r]]

    addWizardAttrs $wizardTop\_sprockets
}

::itcl::body TankWizardIA::buildRoadWheels {} {
    set roadWheelRadius [expr {$roadWheelDiameter * 0.5}]
    set tlen [expr {$hullLength - $lowerOffset}]
    set dx [expr {($tlen - $roadWheelDiameter - \
	    $idlerWheelDiameter - $sprocketDiameter - 2 * $trackThickness) / \
	    double($numRoadWheels - 1)}]

    set ly $extHalfWidth
    set ry -$extHalfWidth
    set roadWheelZ [expr {-$extHalfHeight - $roadWheelRadius}]

    set l_rwtree {}
    set r_rwtree {}

    for {set i 0} {$i < $numRoadWheels} {incr i} {
	set xoffset [expr {$sprocketDiameter + $trackThickness + $roadWheelRadius + $i * $dx}]
	set x [expr {-$extHalfLength + $xoffset}]

	if {$i == 0} {
	    set lastRoadWheelX $x
	}

	set v [list $x $ly $roadWheelZ]
	set v [vectorScale [vectorAdd $v $tankCenter] $local2base]

	set rwr [expr {$roadWheelRadius * $local2base}]
	set rwd [expr {$wwDepth * $local2base}]

	$archersMged put $wizardTop\_l_rwhl$i\.s tgc \
	    V $v \
	    H [list 0 -$rwd 0] \
	    A [list $rwr 0 0] \
	    B [list 0 0 $rwr] \
	    C [list $rwr 0 0] \
	    D [list 0 0 $rwr]

	set rid [expr {$leftRoadWheelInitialId + $i}]
	$archersMged put $wizardTop\_l_rwhl$i\.r comb \
	    region yes \
	    id $rid \
	    tree \
	    [list l $wizardTop\_l_rwhl$i\.s]

	addWizardAttrs $wizardTop\_l_rwhl$i\.s
	addWizardAttrs $wizardTop\_l_rwhl$i\.r

	set v [list $x $ry $roadWheelZ]
	set v [vectorScale [vectorAdd $v $tankCenter] $local2base]

	$archersMged put $wizardTop\_r_rwhl$i\.s tgc \
	    V $v \
	    H [list 0 $rwd 0] \
	    A [list $rwr 0 0] \
	    B [list 0 0 $rwr] \
	    C [list $rwr 0 0] \
	    D [list 0 0 $rwr]

	set rid [expr {$rightRoadWheelInitialId + $i}]
	$archersMged put $wizardTop\_r_rwhl$i\.r comb \
	    region yes \
	    id $rid \
	    tree \
	    [list l $wizardTop\_r_rwhl$i\.s]

	addWizardAttrs $wizardTop\_r_rwhl$i\.s
	addWizardAttrs $wizardTop\_r_rwhl$i\.r

	if {$l_rwtree == {}} {
	    set l_rwtree [list l $wizardTop\_l_rwhl$i\.r]
	    set r_rwtree [list l $wizardTop\_r_rwhl$i\.r]
	} else {
	    set l_rwtree [list u $l_rwtree [list l $wizardTop\_l_rwhl$i\.r]]
	    set r_rwtree [list u $r_rwtree [list l $wizardTop\_r_rwhl$i\.r]]
	}
    }

    set firstRoadWheelX $x

    if {$l_rwtree != {}} {
	$archersMged put $wizardTop\_l_roadWheels comb \
		region no tree $l_rwtree
	$archersMged put $wizardTop\_r_roadWheels comb \
		region no tree $r_rwtree
	$archersMged put $wizardTop\_roadWheels comb \
	    region no tree \
	    [list u \
		 [list l $wizardTop\_l_roadWheels] \
		 [list l $wizardTop\_r_roadWheels]]

	addWizardAttrs $wizardTop\_l_roadWheels
	addWizardAttrs $wizardTop\_r_roadWheels
	addWizardAttrs $wizardTop\_roadWheels
    }
}

::itcl::body TankWizardIA::buildIdlerWheels {} {
    set idlerWheelRadius [expr {$idlerWheelDiameter * 0.5}]
    set tlen [expr {$hullLength - $lowerOffset - $sprocketDiameter}]
    set dx [expr {($tlen - $idlerWheelRadius) / double($numIdlerWheels)}]
    set startx [expr {$extHalfLength - $lowerOffset - $idlerWheelRadius - $trackThickness}]

    set ly $extHalfWidth
    set ry -$extHalfWidth
    set idlerWheelZ [expr {-$extHalfHeight + $wwHeight - $idlerWheelRadius - $trackClearance - $trackThickness}]

    set l_iwtree {}
    set r_iwtree {}

    for {set i 0} {$i < $numIdlerWheels} {incr i} {
	set x [expr {$startx - $i * $dx}]

	if {$i == 0} {
	    set idlerWheelX $x
	}

	set v [list $x $ly $idlerWheelZ]
	set v [vectorScale [vectorAdd $v $tankCenter] $local2base]

	set iwr [expr {$idlerWheelRadius * $local2base}]
	set iwd [expr {$wwDepth * $local2base}]

	$archersMged put $wizardTop\_l_iwhl$i\.s tgc \
	    V $v \
	    H [list 0 -$iwd 0] \
	    A [list $iwr 0 0] \
	    B [list 0 0 $iwr] \
	    C [list $iwr 0 0] \
	    D [list 0 0 $iwr]

	set rid [expr {$leftIdlerWheelInitialId + $i}]
	$archersMged put $wizardTop\_l_iwhl$i\.r comb \
	    region yes \
	    id $rid \
	    tree \
	    [list l $wizardTop\_l_iwhl$i\.s]

	addWizardAttrs $wizardTop\_l_iwhl$i\.s
	addWizardAttrs $wizardTop\_l_iwhl$i\.r

	set v [list $x $ry $idlerWheelZ]
	set v [vectorScale [vectorAdd $v $tankCenter] $local2base]

	$archersMged put $wizardTop\_r_iwhl$i\.s tgc \
	    V $v \
	    H [list 0 $iwd 0] \
	    A [list $iwr 0 0] \
	    B [list 0 0 $iwr] \
	    C [list $iwr 0 0] \
	    D [list 0 0 $iwr]

	set rid [expr {$rightIdlerWheelInitialId + $i}]
	$archersMged put $wizardTop\_r_iwhl$i\.r comb \
	    region yes \
	    id $rid \
	    tree \
	    [list l $wizardTop\_r_iwhl$i\.s]

	addWizardAttrs $wizardTop\_r_iwhl$i\.s
	addWizardAttrs $wizardTop\_r_iwhl$i\.r

	if {$l_iwtree == {}} {
	    set l_iwtree [list l $wizardTop\_l_iwhl$i\.r]
	    set r_iwtree [list l $wizardTop\_r_iwhl$i\.r]
	} else {
	    set l_iwtree [list u $l_iwtree [list l $wizardTop\_l_iwhl$i\.r]]
	    set r_iwtree [list u $r_iwtree [list l $wizardTop\_r_iwhl$i\.r]]
	}
    }

    if {$l_iwtree != {}} {
	$archersMged put $wizardTop\_l_idlerWheels comb \
		region no tree $l_iwtree
	$archersMged put $wizardTop\_r_idlerWheels comb \
		region no tree $r_iwtree
	$archersMged put $wizardTop\_idlerWheels comb \
	    region no tree \
	    [list u \
		 [list l $wizardTop\_l_idlerWheels] \
		 [list l $wizardTop\_r_idlerWheels]]

	addWizardAttrs $wizardTop\_l_idlerWheels
	addWizardAttrs $wizardTop\_r_idlerWheels
	addWizardAttrs $wizardTop\_idlerWheels
    }
}

::itcl::body TankWizardIA::buildTracks {} {
    set x [lindex $tankCenter 0]
    set y [lindex $tankCenter 1]
    set z [lindex $tankCenter 2]

    $archersMged track $wizardTop\_l_track \
	[expr {$firstRoadWheelX + $x}] \
	[expr {$lastRoadWheelX + $x}] \
	[expr {$roadWheelZ + $z}]  \
	$roadWheelRadius \
	[expr {$driveSprocketX + $x}] \
	[expr {$driveSprocketZ + $z}] \
	$sprocketRadius \
	[expr {$idlerWheelX + $x}] \
	[expr {$idlerWheelZ + $z}] \
	$idlerWheelRadius \
	[expr {$leftTrackYMin + $y}] \
	[expr {$leftTrackYMax + $y}] \
	$trackThickness

    $archersMged track $wizardTop\_r_track \
	[expr {$firstRoadWheelX + $x}] \
	[expr {$lastRoadWheelX + $x}] \
	[expr {$roadWheelZ + $z}]  \
	$roadWheelRadius \
	[expr {$driveSprocketX + $x}] \
	[expr {$driveSprocketZ + $z}] \
	$sprocketRadius \
	[expr {$idlerWheelX + $x}] \
	[expr {$idlerWheelZ + $z}] \
	$idlerWheelRadius \
	[expr {$rightTrackYMin + $y}] \
	[expr {$rightTrackYMax + $y}] \
	$trackThickness

    $archersMged put $wizardTop\_tracks comb \
	region no tree \
	[list u \
	     [list l $wizardTop\_l_track] \
	     [list l $wizardTop\_r_track]]

    # Set region id for left track components
    set tobjs [$archersMged expand $wizardTop\_l_track\*]
    foreach tobj $tobjs {
	if {![catch {$archersMged get $tobj region} ret] && $ret == "yes"} {
	    $archersMged adjust $tobj id $leftTrackId
	}
	addWizardAttrs $tobj
    }

    # Set region id for right track components
    set tobjs [$archersMged expand $wizardTop\_r_track\*]
    foreach tobj $tobjs {
	if {![catch {$archersMged get $tobj region} ret] && $ret == "yes"} {
	    $archersMged adjust $tobj id $rightTrackId
	}
	addWizardAttrs $tobj
    }

    addWizardAttrs $wizardTop\_tracks
}

::itcl::body TankWizardIA::buildTurret {} {
    set v1 [list \
		[expr {-$turretHalfLength + $turretOffset}] \
		[expr {-$turretHalfWidth + $turretSideOffset}] \
		$extHalfHeight]
    set v2 [list \
		[expr {-$turretHalfLength + $turretOffset}] \
		[expr {$turretHalfWidth - $turretSideOffset}] \
		$extHalfHeight]
    set v3 [list \
		[expr {-$turretHalfLength + $turretRearOffset + $turretOffset}] \
		[expr {$turretHalfWidth - $turretSideOffset}] \
		[expr {$extHalfHeight + $turretHeight}]]
    set v4 [list \
		[expr {-$turretHalfLength + $turretRearOffset + $turretOffset}] \
		[expr {-$turretHalfWidth + $turretSideOffset}] \
		[expr {$extHalfHeight + $turretHeight}]]

    set v5 [list \
		[expr {$turretHalfLength + $turretOffset}] \
		[expr {-$turretHalfWidth + $turretSideOffset}] \
		$extHalfHeight]
    set v6 [list \
		[expr {$turretHalfLength + $turretOffset}] \
		[expr {$turretHalfWidth - $turretSideOffset}] \
		$extHalfHeight]
    set v7 [list \
		[expr {$turretHalfLength - $turretFrontOffset + $turretOffset}] \
		[expr {$turretHalfWidth - $turretSideOffset}] \
		[expr {$extHalfHeight + $turretHeight}]]
    set v8 [list \
		[expr {$turretHalfLength - $turretFrontOffset + $turretOffset}] \
		[expr {-$turretHalfWidth + $turretSideOffset}] \
		[expr {$extHalfHeight + $turretHeight}]]

    set v1 [vectorScale [vectorAdd $v1 $tankCenter] $local2base]
    set v2 [vectorScale [vectorAdd $v2 $tankCenter] $local2base]
    set v3 [vectorScale [vectorAdd $v3 $tankCenter] $local2base]
    set v4 [vectorScale [vectorAdd $v4 $tankCenter] $local2base]
    set v5 [vectorScale [vectorAdd $v5 $tankCenter] $local2base]
    set v6 [vectorScale [vectorAdd $v6 $tankCenter] $local2base]
    set v7 [vectorScale [vectorAdd $v7 $tankCenter] $local2base]
    set v8 [vectorScale [vectorAdd $v8 $tankCenter] $local2base]

    $archersMged put $wizardTop\_turret.s arb8 \
	V1 $v1 V2 $v2 V3 $v3 V4 $v4 \
	V5 $v5 V6 $v6 V7 $v7 V8 $v8

    # extra points for right side
    set _v1 [list \
		 [expr {$turretHalfLength - $turretFrontOffset + $turretOffset}] \
		 [expr {-$turretHalfWidth + $turretSideOffset}] \
		 $extHalfHeight]
    set _v2 [list \
		 [expr {$turretHalfLength - $turretFrontOffset + $turretOffset}] \
		 -$turretHalfWidth \
		 $extHalfHeight]
    set _v3 [list \
		 [expr {-$turretHalfLength + $turretRearOffset + $turretOffset}] \
		 -$turretHalfWidth \
		 $extHalfHeight]
    set _v4 [list \
		 [expr {-$turretHalfLength + $turretRearOffset + $turretOffset}] \
		 [expr {-$turretHalfWidth + $turretSideOffset}] \
		 $extHalfHeight]

    set _v1 [vectorScale [vectorAdd $_v1 $tankCenter] $local2base]
    set _v2 [vectorScale [vectorAdd $_v2 $tankCenter] $local2base]
    set _v3 [vectorScale [vectorAdd $_v3 $tankCenter] $local2base]
    set _v4 [vectorScale [vectorAdd $_v4 $tankCenter] $local2base]

    $archersMged put $wizardTop\_turret_r.s arb8 \
	V1 $_v1 V2 $_v2 V3 $_v3 V4 $_v4 \
	V5 $v8 V6 $v8 V7 $v4 V8 $v4
    $archersMged put $wizardTop\_turret_rr.s arb8 \
	V1 $v1 V2 $_v4 V3 $_v3 V4 $_v3 \
	V5 $v4 V6 $v4 V7 $v4 V8 $v4
    $archersMged put $wizardTop\_turret_rf.s arb8 \
	V1 $_v2 V2 $_v1 V3 $v5 V4 $v5 \
	V5 $v8 V6 $v8 V7 $v8 V8 $v8

    # extra points for left side
    set _v1 [list \
		 [expr {$turretHalfLength - $turretFrontOffset + $turretOffset}] \
		 $turretHalfWidth \
		 $extHalfHeight]
    set _v2 [list \
		 [expr {$turretHalfLength - $turretFrontOffset + $turretOffset}] \
		 [expr {$turretHalfWidth - $turretSideOffset}] \
		 $extHalfHeight]
    set _v3 [list \
		 [expr {-$turretHalfLength + $turretRearOffset + $turretOffset}] \
		 [expr {$turretHalfWidth - $turretSideOffset}] \
		 $extHalfHeight]
    set _v4 [list \
		 [expr {-$turretHalfLength + $turretRearOffset + $turretOffset}] \
		 $turretHalfWidth \
		 $extHalfHeight]

    set _v1 [vectorScale [vectorAdd $_v1 $tankCenter] $local2base]
    set _v2 [vectorScale [vectorAdd $_v2 $tankCenter] $local2base]
    set _v3 [vectorScale [vectorAdd $_v3 $tankCenter] $local2base]
    set _v4 [vectorScale [vectorAdd $_v4 $tankCenter] $local2base]

    $archersMged put $wizardTop\_turret_l.s arb8 \
	V1 $_v1 V2 $_v2 V3 $_v3 V4 $_v4 \
	V5 $v7 V6 $v7 V7 $v3 V8 $v3
    $archersMged put $wizardTop\_turret_lr.s arb8 \
	V1 $v2 V2 $_v3 V3 $_v4 V4 $_v4 \
	V5 $v3 V6 $v3 V7 $v3 V8 $v3
    $archersMged put $wizardTop\_turret_lf.s arb8 \
	V1 $_v2 V2 $_v1 V3 $v6 V4 $v6 \
	V5 $v7 V6 $v7 V7 $v7 V8 $v7

    $archersMged put $wizardTop\_turret.r comb \
	region yes \
	id $turretId \
	tree \
	[list u \
	     [list u \
		  [list u \
		       [list u \
			    [list u \
				 [list u \
				      [list l $wizardTop\_turret.s] \
				      [list l $wizardTop\_turret_r.s]] \
				 [list l $wizardTop\_turret_rr.s]] \
			    [list l $wizardTop\_turret_rf.s]] \
		       [list l $wizardTop\_turret_l.s]] \
		  [list l $wizardTop\_turret_lr.s]] \
	     [list l $wizardTop\_turret_lf.s]]

    addWizardAttrs $wizardTop\_turret.s
    addWizardAttrs $wizardTop\_turret_r.s
    addWizardAttrs $wizardTop\_turret_rr.s
    addWizardAttrs $wizardTop\_turret_rf.s
    addWizardAttrs $wizardTop\_turret_l.s
    addWizardAttrs $wizardTop\_turret_lr.s
    addWizardAttrs $wizardTop\_turret_lf.s
    addWizardAttrs $wizardTop\_turret.r
}

## - buildGun
#
::itcl::body TankWizardIA::buildGun {} {
    set v [list \
	       [expr {$turretHalfLength - 0.75 * $turretFrontOffset + $turretOffset}] \
	       0 \
	       [expr {$extHalfHeight + 0.25 * $turretHeight}]]
    set v [vectorScale [vectorAdd $v $tankCenter] $local2base]

    set gl [expr {$gunLength * $local2base}]

    set r1 [expr {$gunBaseOuterDiameter * 0.5 * $local2base}]
    set r2 [expr {$gunEndOuterDiameter * 0.5 * $local2base}]
    $archersMged put $wizardTop\_gun_tube.s tgc \
	V $v \
	H [list 0 0 $gl] \
	A [list $r1 0 0] \
	B [list 0 $r1 0] \
	C [list $r2 0 0] \
	D [list 0 $r2 0]

    set r1 [expr {$gunBaseInnerDiameter * 0.5 * $local2base}]
    set r2 [expr {$gunEndInnerDiameter * 0.5 * $local2base}]
    $archersMged put $wizardTop\_gun_tube_cut.s tgc \
	V $v \
	H [list 0 0 $gl] \
	A [list $r1 0 0] \
	B [list 0 $r1 0] \
	C [list $r2 0 0] \
	D [list 0 $r2 0]

    set mat [mat_angles 0 [expr {90 - $gunElevation}] 0]
    set mat [mat_xform_about_pt $mat $v]
    $archersMged put $wizardTop\_gun_tube.r comb \
	region yes \
	id $gunBarrelId \
	tree \
	[list - \
	     [list l $wizardTop\_gun_tube.s $mat] \
	     [list l $wizardTop\_gun_tube_cut.s $mat]]

    $archersMged g $wizardTop\_gun $wizardTop\_gun_tube.r

    addWizardAttrs $wizardTop\_gun_tube.s
    addWizardAttrs $wizardTop\_gun_tube_cut.s
    addWizardAttrs $wizardTop\_gun_tube.r
    addWizardAttrs $wizardTop\_gun
}

::itcl::body TankWizardIA::setFrontArmorThickness {} {
    switch -- $frontArmorType {
	"7.62mm" {
	    set frontArmorThickness 2
	}
	"12.7mm" {
	    set frontArmorThickness 4
	}
	"Artillery Frags" {
	    set frontArmorThickness 3
	}
	"RPG" {
	    set frontArmorThickness 6
	}
	"NONE" {
	    set frontArmorThickness 0
	}
    }
}

::itcl::body TankWizardIA::setRearArmorThickness {} {
    switch -- $rearArmorType {
	"7.62mm" {
	    set rearArmorThickness 2
	}
	"12.7mm" {
	    set rearArmorThickness 4
	}
	"Artillery Frags" {
	    set rearArmorThickness 3
	}
	"RPG" {
	    set rearArmorThickness 6
	}
	"NONE" {
	    set rearArmorThickness 0
	}
    }
}

::itcl::body TankWizardIA::setRoofArmorThickness {} {
    switch -- $roofArmorType {
	"7.62mm" {
	    set roofArmorThickness 2
	}
	"12.7mm" {
	    set roofArmorThickness 4
	}
	"Artillery Frags" {
	    set roofArmorThickness 3
	}
	"RPG" {
	    set roofArmorThickness 6
	}
	"NONE" {
	    set roofArmorThickness 0
	}
    }
}

::itcl::body TankWizardIA::setSideArmorThickness {} {
    switch -- $sideArmorType {
	"7.62mm" {
	    set sideArmorThickness 2
	}
	"12.7mm" {
	    set sideArmorThickness 4
	}
	"Artillery Frags" {
	    set sideArmorThickness 3
	}
	"RPG" {
	    set sideArmorThickness 6
	}
	"NONE" {
	    set sideArmorThickness 0
	}
    }
}

::itcl::body TankWizardIA::setSponsonArmorThickness {} {
    switch -- $sponsonArmorType {
	"7.62mm" {
	    set sponsonArmorThickness 2
	}
	"12.7mm" {
	    set sponsonArmorThickness 4
	}
	"Artillery Frags" {
	    set sponsonArmorThickness 3
	}
	"RPG" {
	    set sponsonArmorThickness 6
	}
	"NONE" {
	    set sponsonArmorThickness 0
	}
    }
}

::itcl::body TankWizardIA::initRegionIds {} {
    set rid [$archer pluginGetMinAllowableRid]
    set turretId $rid
    incr rid
    set gunBarrelId $rid
    incr rid
    set hullArmorId $rid
    incr rid
    set hullInteriorId $rid
    incr rid
    set leftSprocketId $rid
    incr rid
    set leftTrackId $rid
    incr rid
    set rightSprocketId $rid
    incr rid
    set rightTrackId $rid
    incr rid
    set leftRoadWheelInitialId $rid
    set rid [expr {$rid + $numRoadWheels - 1}]
    set leftRoadWheelEndId $rid
    incr rid
    set rightRoadWheelInitialId $rid
    set rid [expr {$rid + $numRoadWheels - 1}]
    set rightRoadWheelEndId $rid
    incr rid
    set leftIdlerWheelInitialId $rid
    set rid [expr {$rid + $numIdlerWheels - 1}]
    set leftIdlerWheelEndId $rid
    incr rid
    set rightIdlerWheelInitialId $rid
    set rid [expr {$rid + $numIdlerWheels - 1}]
    set rightIdlerWheelEndId $rid
    incr rid
    set zoneIdInitial $rid
    set zoneId $zoneIdInitial
}
